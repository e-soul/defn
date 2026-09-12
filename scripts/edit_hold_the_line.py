#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause

"""Cut an effects-only capture into a preset 53-second, 1080p60 gameplay showcase."""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FPS = 60
WIDTH, HEIGHT = 1920, 1080
FADE_SECONDS = 0.45
END_CARD_SECONDS = 4
TOTAL_SECONDS = 53
TOTAL_FRAMES = round(TOTAL_SECONDS * FPS)


@dataclass(frozen=True)
class Caption:
    text: str
    label: str
    start: float
    end: float


@dataclass(frozen=True)
class Variation:
    shot: str
    output_name: str
    music_name: str
    title: str
    metadata_title: str
    captions: tuple[Caption, ...]
    # Shot-relative seconds: 7s hook, 7s deployment, 35s uninterrupted battle.
    clips: tuple[tuple[float, float], ...] = ((26.0, 33.0), (1.2, 8.2), (12.5, 47.5))
    music_volume: float = 0.36
    logo_only: bool = False

    @property
    def required_source_seconds(self) -> float:
        return max(end for _, end in self.clips)

    def validate(self) -> None:
        if len(self.clips) != 3:
            raise ValueError("a variation must have hook, deployment and continuous battle clips")
        for (start, end), duration in zip(self.clips, (7, 7, 35), strict=True):
            if not all(math.isfinite(value) for value in (start, end)) or start < 0:
                raise ValueError("clip times must be finite and nonnegative")
            if not math.isclose(end - start, duration, abs_tol=1e-6, rel_tol=0):
                raise ValueError("clip lengths must remain 7, 7 and 35 seconds")
            if any(not math.isclose(value * FPS, round(value * FPS), abs_tol=1e-6, rel_tol=0)
                   for value in (start, end)):
                raise ValueError("clip boundaries must align to 60 fps frames")
        if len(self.captions) != 3:
            raise ValueError("a variation must have three captions")
        for caption in self.captions:
            if (not all(math.isfinite(value) for value in (caption.start, caption.end))
                    or not 0 <= caption.start < caption.end <= TOTAL_SECONDS - END_CARD_SECONDS
                    or caption.end - caption.start < 0.4):
                raise ValueError("captions must fit gameplay and allow both 0.2-second fades")


# Change clips or individual Caption times here to retime a variation without
# changing the shared edit, typography or continuous music timeline.
VARIATIONS: dict[str, Variation] = {
    "hold_the_line": Variation(
        shot="hold_the_line",
        output_name="give_ground_hold_the_line.mp4",
        music_name="theme01.mp3",
        title="GIVE GROUND. HOLD THE LINE.",
        metadata_title="DEFN - Give ground. Hold the line.",
        captions=(
            Caption("BUILD YOUR LINE.", "01 / DEPLOY", 7.6, 13.2),
            Caption("GIVE GROUND.", "02 / REPOSITION", 15.8, 21.0),
            Caption("BRING REINFORCEMENTS.", "03 / REINFORCE", 24.8, 29.0),
        ),
    ),
    "jungle_line": Variation(
        shot="jungle_line",
        output_name="jungle_line.mp4",
        music_name="theme02.mp3",
        music_volume=0.24,
        title="HOLD THE JUNGLE LINE.",
        metadata_title="DEFN - Hold the jungle line.",
        captions=(
            Caption("BUILD YOUR LINE.", "01 / DEPLOY", 7.6, 13.2),
            Caption("MAKE ROOM TO REGROUP.", "02 / REPOSITION", 15.8, 21.0),
            Caption("KEEP SUPPORT COMING.", "03 / REINFORCE", 29.8, 34.0),
        ),
    ),
    "winter_fireline": Variation(
        shot="winter_fireline",
        output_name="winter_fireline.mp4",
        music_name="theme03.mp3",
        music_volume=0.24,
        title="HOLD THE WINTER LINE.",
        logo_only=True,
        metadata_title="DEFN - Hold the winter line.",
        captions=(
            Caption("BUILD A MIXED LINE.", "01 / DEPLOY", 7.6, 13.2),
            Caption("HOLD THROUGH THE WAVE.", "02 / DEFEND", 15.8, 21.0),
            Caption("REINFORCE THE FRONT.", "03 / REINFORCE", 24.8, 29.0),
        ),
    ),
    "town_crossfire": Variation(
        shot="town_crossfire",
        output_name="town_crossfire.mp4",
        music_name="theme04.mp3",
        music_volume=0.24,
        title="HOLD THE SQUARE.",
        logo_only=True,
        metadata_title="DEFN - Hold the square.",
        captions=(
            Caption("BRING HEAVY SUPPORT.", "01 / DEPLOY", 7.6, 13.2),
            Caption("KEEP THE LINE FIRING.", "02 / ENGAGE", 15.8, 21.0),
            Caption("ANSWER THE NEXT WAVE.", "03 / REINFORCE", 24.8, 29.0),
        ),
    ),
}


def probe(executable: str, path: Path) -> dict:
    result = subprocess.run(
        [executable, "-v", "error", "-show_streams", "-show_format", "-of", "json", str(path)],
        check=True, capture_output=True, text=True,
    )
    return json.loads(result.stdout)


def make_titles(directory: Path, font_path: Path, variation: Variation) -> list[Path]:
    theme = json.loads((ROOT / "defn" / "data" / "ui_theme.json").read_text(encoding="utf-8"))
    palette = theme["palette"]

    def color(role: str, alpha: int = 255) -> tuple[int, int, int, int]:
        return (*[round(channel * 255) for channel in palette[role][:3]], alpha)

    def centered(draw: ImageDraw.ImageDraw, text: str, y: int, size: int, role: str) -> None:
        font = ImageFont.truetype(str(font_path), size)
        draw.text((WIDTH // 2, y), text, font=font, fill=color(role), anchor="mt")

    paths = []
    for index, caption in enumerate(variation.captions):
        text, label = caption.text, caption.label
        image = Image.new("RGBA", (WIDTH, 170))
        draw = ImageDraw.Draw(image)
        font = ImageFont.truetype(str(font_path), 52)
        label_font = ImageFont.truetype(str(font_path), 22)
        half_width = round(draw.textlength(text, font=font) / 2) + 40
        left, right = WIDTH // 2 - half_width, WIDTH // 2 + half_width
        draw.rectangle((left + 8, 20, right + 8, 156), fill=color("neutral_ink", 150))
        draw.rectangle((left, 12, right, 148), fill=color("accent_strong"))
        draw.rectangle((left, 12, left + 8, 148), fill=color("accent"))
        draw.text((left + 40, 31), label, font=label_font, fill=color("neutral_ink"), anchor="lt")
        draw.text((left + 40, 72), text, font=font, fill=color("neutral_ink"), anchor="lt")
        path = directory / f"caption_{index}.png"
        image.save(path)
        paths.append(path)

    image = Image.new("RGBA", (WIDTH, HEIGHT), color("neutral_ink"))
    draw = ImageDraw.Draw(image)
    if variation.logo_only:
        font = ImageFont.truetype(str(font_path), 148)
        draw.text((WIDTH // 2, HEIGHT // 2), "DEFN", font=font, fill=color("text_primary"), anchor="mm")
    else:
        centered(draw, "DEFN", 365, 148, "text_primary")
        draw.line((780, 554, 1140, 554), fill=color("accent"), width=4)
        centered(draw, variation.title, 605, 44, "accent")
        centered(draw, "A tactical belt-scroller", 700, 28, "text_secondary")
    path = directory / "end_card.png"
    image.save(path)
    paths.append(path)
    return paths


def filter_graph(boot_frames: int, music_volume: float, variation: Variation) -> str:
    parts = ["[0:v]split=3[source0][source1][source2]", "[0:a]asplit=3[sound0][sound1][sound2]"]
    for index, (start, end) in enumerate(variation.clips):
        first = boot_frames + round(start * FPS)
        last = boot_frames + round(end * FPS)
        duration = end - start
        parts.append(
            f"[source{index}]trim=start_frame={first}:end_frame={last},"
            f"setpts=PTS-STARTPTS,setsar=1,fade=t=in:d={FADE_SECONDS},"
            f"fade=t=out:st={duration - FADE_SECONDS}:d={FADE_SECONDS}[v{index}]"
        )
        parts.append(
            f"[sound{index}]atrim=start={first / FPS}:end={last / FPS},asetpts=PTS-STARTPTS,"
            f"afade=t=in:d={FADE_SECONDS},"
            f"afade=t=out:st={duration - FADE_SECONDS}:d={FADE_SECONDS}[a{index}]"
        )
    parts.append("[v0][v1][v2]concat=n=3:v=1:a=0[game]")
    previous = "game"
    for index, caption in enumerate(variation.captions):
        start, end = caption.start, caption.end
        duration = end - start
        parts.append(
            f"[{index + 1}:v]format=rgba,trim=duration={duration},setpts=PTS-STARTPTS,"
            f"fade=t=in:d=0.2:alpha=1,fade=t=out:st={duration - 0.2}:d=0.2:alpha=1,"
            f"setpts=PTS+{start}/TB[caption{index}]"
        )
        parts.append(
            f"[{previous}][caption{index}]overlay=x=0:y=140:eof_action=pass:repeatlast=0"
            f"[captioned{index}]"
        )
        previous = f"captioned{index}"
    parts.append(
        f"[4:v]trim=end_frame={END_CARD_SECONDS * FPS},setpts=PTS-STARTPTS,setsar=1,"
        f"fade=t=in:d={FADE_SECONDS},"
        f"fade=t=out:st={END_CARD_SECONDS - FADE_SECONDS}:d={FADE_SECONDS}[card]"
    )
    parts.append(f"[{previous}][card]concat=n=2:v=1:a=0,format=yuv420p[outv]")
    parts.append(
        f"[a0][a1][a2]concat=n=3:v=0:a=1,apad=pad_dur={END_CARD_SECONDS}[effects]"
    )
    # Music uses the finished video's clock, never the source clip boundaries.
    parts.append(
        f"[5:a]atrim=duration={TOTAL_SECONDS},asetpts=PTS-STARTPTS,volume={music_volume},"
        f"afade=t=in:d=0.45,afade=t=out:st={TOTAL_SECONDS - 2}:d=2[music]"
    )
    parts.append(
        "[effects][music]amix=inputs=2:duration=longest:normalize=0,"
        "alimiter=limit=0.95:level=false:latency=true[outa]"
    )
    return ";".join(parts)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variation", choices=VARIATIONS, default="hold_the_line",
                        help="edit preset (default: hold_the_line, the original beach edit)")
    parser.add_argument("--source", type=Path,
                        help="override preset's effects-only capture; must be recorded with mute_music=true")
    parser.add_argument("--music", type=Path, help="override preset's continuous local music track")
    parser.add_argument("--music-volume", type=float,
                        help="override preset's linear music gain (beach: 0.36; other presets: 0.24)")
    parser.add_argument("--output", type=Path, help="override preset's output MP4 path")
    parser.add_argument("--boot-frames", type=int, default=39, help="recorded frame printed by the capture runner")
    parser.add_argument("--font", type=Path, default=Path(os.environ.get("WINDIR", "C:\\Windows")) / "Fonts" / "arialbd.ttf")
    args = parser.parse_args()
    variation = VARIATIONS[args.variation]
    try:
        variation.validate()
    except ValueError as error:
        parser.error(f"invalid {args.variation} preset: {error}")
    if args.source is None:
        args.source = ROOT / "build" / "capture" / "sfx" / f"{variation.shot}.avi"
    if args.music is None:
        args.music = ROOT / "defn" / "assets" / "music" / variation.music_name
    if args.output is None:
        args.output = ROOT / "build" / "capture" / variation.output_name
    if args.music_volume is None:
        args.music_volume = variation.music_volume
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        parser.error("ffmpeg and ffprobe must be on PATH")
    if not args.source.is_file() or not args.font.is_file():
        parser.error("source and font must exist; pass --source and --font as needed")
    if not args.music.is_file():
        parser.error("music must exist; pass --music as needed")
    if not math.isfinite(args.music_volume) or not 0 < args.music_volume <= 1:
        parser.error("--music-volume must be greater than zero and at most one")
    if args.boot_frames < 0:
        parser.error("--boot-frames cannot be negative")
    if args.output.resolve() in (args.source.resolve(), args.music.resolve()):
        parser.error("output must not overwrite a source")
    source = probe(ffprobe, args.source)
    video = next((stream for stream in source["streams"] if stream["codec_type"] == "video"), None)
    if video is None:
        parser.error("source must contain a video stream")
    if (video["width"], video["height"], video["r_frame_rate"]) != (WIDTH, HEIGHT, "60/1"):
        parser.error("source must be 1920x1080 at 60 fps")
    if not any(stream["codec_type"] == "audio" for stream in source["streams"]):
        parser.error("source must contain the recorded game audio")
    required = args.boot_frames / FPS + variation.required_source_seconds
    if float(source["format"]["duration"]) < required:
        parser.error(f"source must be at least {required:.3f} seconds")
    music = probe(ffprobe, args.music)
    if not any(stream["codec_type"] == "audio" for stream in music["streams"]):
        parser.error("music must contain an audio stream")
    if float(music["format"]["duration"]) < TOTAL_SECONDS:
        parser.error(f"music must be at least {TOTAL_SECONDS:g} seconds; no automatic looping or gaps")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    title_directory = args.output.parent / f"{args.output.stem}_titles"
    title_directory.mkdir(parents=True, exist_ok=True)
    titles = make_titles(title_directory, args.font, variation)
    command = [ffmpeg, "-hide_banner", "-loglevel", "warning", "-y", "-i", str(args.source)]
    for title in titles:
        command.extend(["-loop", "1", "-framerate", str(FPS), "-i", str(title)])
    command.extend(["-i", str(args.music)])
    command.extend([
        "-filter_complex", filter_graph(args.boot_frames, args.music_volume, variation),
        "-map", "[outv]", "-map", "[outa]", "-t", str(TOTAL_SECONDS),
        "-c:v", "libx264", "-crf", "16", "-preset", "slow", "-pix_fmt", "yuv420p",
        "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart",
        "-metadata", f"title={variation.metadata_title}",
        str(args.output),
    ])
    subprocess.run(command, check=True)

    output = probe(ffprobe, args.output)
    video = next(stream for stream in output["streams"] if stream["codec_type"] == "video")
    audio = next(stream for stream in output["streams"] if stream["codec_type"] == "audio")
    if int(video["nb_frames"]) != TOTAL_FRAMES or abs(float(audio["duration"]) - TOTAL_SECONDS) > 0.05:
        raise RuntimeError(f"The encoded video did not meet the {TOTAL_FRAMES}-frame / {TOTAL_SECONDS:g}-second A/V contract")
    print(f"{args.output}: {TOTAL_SECONDS:g} seconds, {TOTAL_FRAMES} frames, 1920x1080, 60 fps, game audio")


if __name__ == "__main__":
    main()
