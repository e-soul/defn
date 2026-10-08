# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Stage a Web project with lossless background tiles and its own Godot import cache.

Nothing in the source project is changed. Only resources selected by the export
preset are copied, so unused artwork is not imported into the temporary project.
"""

from __future__ import annotations

import json
import re
import shutil
from pathlib import Path

DEFAULT_TILE_LIMIT = 4096
# Linear sampling needs the neighboring source pixels beyond each tile's drawn region.
TILE_BORDER = 2
EXPORT_PRESET = "defn_web_release"


def export_resources(text: str, preset: str) -> tuple[re.Match, list[str]]:
    for block in re.finditer(r"\[preset\.\d+\]\n(.*?)(?=\n\[preset\.|\Z)", text, re.S):
        if f'name="{preset}"' not in block.group(1):
            continue
        files = re.search(r"export_files=PackedStringArray\((.*?)\)", block.group(1), re.S)
        if files is None:
            raise ValueError(f"{preset} must explicitly select its resources")
        return block, re.findall(r'"([^"]+)"', files.group(1))
    raise ValueError(f"Missing export preset: {preset}")


def resource_path(project: Path, resource: str) -> Path:
    if not resource.startswith("res://"):
        raise ValueError(f"Not a project resource: {resource}")
    path = (project / resource[6:]).resolve()
    if not path.is_relative_to(project.resolve()):
        raise ValueError(f"Resource escapes the project: {resource}")
    return path


def padded_tile(image, x: int, y: int, width: int, height: int):
    """Copy pixels exactly; wrap horizontal guards at the parallax repeat boundary."""
    from PIL import Image

    border = TILE_BORDER
    tile = Image.new("RGBA", (width + 2 * border, height + 2 * border))
    tile.paste(image.crop((x, y, x + width, y + height)), (border, border))
    for offset in range(border):
        left = (x - border + offset) % image.width
        right = (x + width + offset) % image.width
        tile.paste(image.crop((left, y, left + 1, y + height)), (offset, border))
        tile.paste(image.crop((right, y, right + 1, y + height)), (border + width + offset, border))
    for offset in range(border):
        top = max(0, y - border + offset)
        bottom = min(image.height - 1, y + height + offset)
        # Reuse horizontal guards while copying the neighboring row without interpolation.
        for target, source_y in ((offset, top), (border + height + offset, bottom)):
            row = Image.new("RGBA", (tile.width, 1))
            row.paste(image.crop((x, source_y, x + width, source_y + 1)), (border, 0))
            for column in range(border):
                for destination, source_x in ((column, (x - border + column) % image.width),
                                              (border + width + column, (x + width + column) % image.width)):
                    row.putpixel((destination, 0), image.getpixel((source_x, source_y)))
            tile.paste(row, (0, target))
    return tile


def background_tiles(source: Path, stage: Path, resource: str, limit: int) -> tuple[str, list[str]]:
    from PIL import Image

    original = resource_path(source, resource)
    relative = Path(resource[6:]).relative_to("assets/backgrounds")
    base = stage / "generated/background_tiles" / relative.with_suffix("")
    base.mkdir(parents=True, exist_ok=True)
    sidecar = original.with_suffix(original.suffix + ".import").read_text(encoding="utf-8")
    params = sidecar.split("[params]", 1)[1]
    if "mipmaps/generate=true" in params:
        raise ValueError(f"Tiled backgrounds require non-mipmapped source imports: {resource}")
    params = re.sub(r"process/size_limit=\d+", "process/size_limit=0", params)
    textures = []
    sprites = []
    core = limit - 2 * TILE_BORDER
    with Image.open(original) as loaded:
        image = loaded.convert("RGBA")
        size = image.size
        for y in range(0, image.height, core):
            for x in range(0, image.width, core):
                width, height = min(core, image.width - x), min(core, image.height - y)
                index = len(textures)
                target = base / f"tile_{index}.png"
                padded_tile(image, x, y, width, height).save(target)
                target.with_suffix(".png.import").write_text(
                    '[remap]\nimporter="texture"\ntype="CompressedTexture2D"\n\n[params]' + params,
                    encoding="utf-8",
                )
                textures.append("res://" + target.relative_to(stage).as_posix())
                sprites.append(
                    f'[node name="Tile{index}" type="Sprite2D" parent="."]\n'
                    f'position = Vector2({x}, {y})\ntexture = ExtResource("{index}")\n'
                    f'centered = false\nregion_enabled = true\n'
                    f'region_rect = Rect2({TILE_BORDER}, {TILE_BORDER}, {width}, {height})\n'
                    'region_filter_clip_enabled = false\n'
                )
    scene = base / "background.tscn"
    external = "\n".join(
        f'[ext_resource type="Texture2D" path={json.dumps(path)} id="{index}"]'
        for index, path in enumerate(textures)
    )
    scene.write_text(
        f'[gd_scene load_steps={len(textures) + 1} format=3]\n\n{external}\n\n'
        f'[node name="BackgroundTiles" type="Node2D"]\n'
        f'metadata/source_size = Vector2({size[0]}, {size[1]})\n\n' + "\n".join(sprites),
        encoding="utf-8",
    )
    return "res://" + scene.relative_to(stage).as_posix(), textures


def stage_web_project(source: Path, stage: Path, limit: int = DEFAULT_TILE_LIMIT, *, include_tools: bool = False) -> dict:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError(
            "Web texture staging needs Pillow. Install it with: "
            "python -m pip install -r scripts/requirements-web-build.txt"
        ) from error

    source, stage = source.resolve(), stage.resolve()
    if stage == source or stage.is_relative_to(source) or source.is_relative_to(stage):
        raise ValueError("Web staging must be separate from the source project")
    if stage.exists() and any(stage.iterdir()):
        raise ValueError("Web staging must start in an empty directory")
    if limit <= 2 * TILE_BORDER or limit > DEFAULT_TILE_LIMIT:
        raise ValueError(f"Texture tile limit must be between {2 * TILE_BORDER + 1} and {DEFAULT_TILE_LIMIT}")
    stage.mkdir(parents=True, exist_ok=True)
    presets = (source / "export_presets.cfg").read_text(encoding="utf-8")
    block, selected = export_resources(presets, EXPORT_PRESET)
    replacements = {}
    tile_resources = []
    for resource in selected:
        path = resource_path(source, resource)
        if resource.startswith("res://assets/backgrounds/") and path.suffix == ".png":
            with Image.open(path) as image:
                oversized = max(image.size) > limit
            if oversized:
                scene, textures = background_tiles(source, stage, resource, limit)
                replacements[resource] = scene
                tile_resources.extend(textures)
                continue
        destination = resource_path(stage, resource)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        sidecar = path.with_suffix(path.suffix + ".import")
        if sidecar.exists():
            shutil.copy2(sidecar, destination.with_suffix(destination.suffix + ".import"))
    for name in ("project.godot", "defn_core.gdextension"):
        shutil.copy2(source / name, stage / name)
    shutil.copytree(source / "bin", stage / "bin", ignore=shutil.ignore_patterns("*.pdb", "*.exp", "*.lib"), dirs_exist_ok=True)
    shutil.copytree(source / "export_templates", stage / "export_templates", dirs_exist_ok=True)
    if include_tools:
        shutil.copytree(source / "tools", stage / "tools", ignore=shutil.ignore_patterns("*.uid"))
    # Rewrite only the private snapshot; source data and all native imports retain their original paths.
    for path in stage.rglob("*"):
        if path.suffix not in (".json", ".tscn", ".tres", ".godot", ".html"):
            continue
        text = path.read_text(encoding="utf-8")
        for original, tiled in replacements.items():
            text = text.replace(original, tiled)
        path.write_text(text, encoding="utf-8")
    shipped = [replacements.get(path, path) for path in selected] + tile_resources
    export_files = "export_files=PackedStringArray(" + ", ".join(json.dumps(path) for path in shipped) + ")"
    web_block = re.sub(r"export_files=PackedStringArray\(.*?\)", lambda _: export_files, block.group(), flags=re.S)
    (stage / "export_presets.cfg").write_text(
        presets[:block.start()] + web_block + presets[block.end():], encoding="utf-8",
    )
    for resource in shipped:
        if not resource_path(stage, resource).is_file():
            raise ValueError(f"Missing staged export resource: {resource}")
    report = {"tile_limit": limit, "backgrounds": replacements, "tile_count": len(tile_resources)}
    (stage / "texture_tiles.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Web textures: {len(replacements)} full-resolution backgrounds in {len(tile_resources)} tiles (limit {limit})", flush=True)
    return report
