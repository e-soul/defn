#!/usr/bin/env python3
# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
"""Generate DEFN backgrounds and music through the Gemini API instead of the AI Studio web UI.

The slow part of art creation is not one generation, it is the sweep: a background prompt needs the same
references attached every time, four or five variants before one tiles cleanly, and then a refinement turn or
two on the keeper. Doing that by hand means re-uploading `background_jungle_ruin_tiling.png` and two sprites
into a fresh AI Studio tab for every roll, and losing the exact prompt that produced the one you kept.

This script takes the prompt from a file, attaches the references once, rolls `-n` variants in a batch, and
writes a `.json` sidecar next to every result recording the model, the full prompt, the reference paths, and
the interaction id. The id is the part the web UI does not give you: pass it back with `--continue` and the
model edits that image conversationally rather than generating a new one, which is how a seam or a stray rock
gets fixed without re-rolling the whole background.

    python scripts/gen_art.py image --prompt prompt.txt --ref defn/assets/backgrounds/background_jungle_ruin_tiling.png -n 4
    python scripts/gen_art.py image --prompt "Flatten the rocks below the halfway line." --continue <id>
    python scripts/gen_art.py music --prompt theme05.txt -n 3
    python scripts/gen_art.py image --prompt prompt.txt --ref bg.png --dry-run

`--prompt` takes a file path or a literal string; the prompts in `defn_game_asset_prompts.md` are meant to be
saved to a file and passed by path. Aspect ratio and resolution are request fields here, not prose, so the
`ultra-high resolution, crisp 4K fidelity graphics` wording that older prompts carry is doing nothing and can
be dropped from new ones.

Neither output format is selectable. Images come back as JPEG, so a background lands as a `.jpg` rather than
matching the `.png` the shipped ones happen to use -- Godot imports either, and the campaign previews are
already JPEG. Music comes back as MP3, which is what `assets/music` already holds.

The interactions API exposes no temperature, top-p, or top-k; the sampling knobs the older `generate_content`
path had are simply not in its request. What it does have is `--size` and `--aspect` for the output frame,
`--thinking` for how long the model plans before drawing, and `--seed`, which is accepted but did not
reproduce a repeat run in testing. Variation therefore comes from `-n`, not from a temperature dial.

There is deliberately no knob for how much detail the model reads from an attached reference. The SDK types
carry a per-image `resolution` of low/medium/high/ultra_high, but both image models reject it outright --
"Media resolution is not supported for model 'gemini-3.1-flash-image'", and the same for `gemini-3-pro-image`.
It belongs to the text models that read images, not to the ones that draw them. Do not re-add it.

Needs `GEMINI_API_KEY` in the environment; `devenv_defn.bat` sets it. Requires the `google-genai` package.

Model names move faster than this script does. Both defaults are overridable with `--model`, and the SDK
passes an unrecognized name straight through to the service, so a newer model works without an edit here.
"""

import argparse
import base64
import datetime
import json
import os
import pathlib
import sys
import time

try:
    from google import genai

    # The interactions API raises from its own error hierarchy, and `google.genai.errors` -- the one the older
    # `generate_content` path uses -- does not catch any of it. Nothing public re-exports these yet, so reach
    # into the private module and fall back to the legacy classes if a future release moves them.
    from google.genai._gaos.lib import compat_errors as api_errors
except ImportError:  # pragma: no cover - SDK layout changed or package missing
    try:
        from google import genai
        from google.genai import errors as api_errors
    except ImportError as exc:
        sys.exit(f"{exc}. Install with: pip install google-genai")

IMAGE_MODEL = "gemini-3.1-flash-image"
MUSIC_MODEL = "lyria-3.5"

# The service accepts far more -- 10 object, 4 character, and 3 style references on Flash -- but every extra
# reference dilutes the rest, and DEFN has never needed more than four.
REFERENCE_WARN_AT = 6

# The last four are Flash-only. Pass one to a Pro model and the service rejects the request.
ASPECT_RATIOS = ("1:1", "2:3", "3:2", "3:4", "4:3", "4:5", "5:4", "9:16", "16:9", "21:9", "1:4", "4:1", "1:8", "8:1")

IMAGE_SIZES = ("512", "1K", "2K", "4K")
THINKING_LEVELS = ("minimal", "low", "medium", "high")

REFERENCE_MIME = {
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".webp": "image/webp",
    ".heic": "image/heic",
    ".heif": "image/heif",
}

OUTPUT_EXTENSION = {
    "image/png": ".png",
    "image/jpeg": ".jpg",
    "image/webp": ".webp",
    "audio/mp3": ".mp3",
    "audio/mpeg": ".mp3",
    "audio/wav": ".wav",
    "audio/ogg": ".ogg",
    "audio/ogg_opus": ".ogg",
}

RETRIES = 3
RETRY_BACKOFF_SECONDS = 5

# Worth another roll: the service was busy, throttled, or the connection died. A 400 or a 401 will not improve.
RETRYABLE_ERRORS = tuple(
    cls
    for cls in (
        getattr(api_errors, name, None)
        for name in ("RateLimitError", "InternalServerError", "APITimeoutError", "APIConnectionError", "ServerError")
    )
    if isinstance(cls, type)
)
API_ERROR = getattr(api_errors, "APIError", Exception)
# A 4K Pro image or a full-length track is minutes of work, and the default client timeout is far below that.
REQUEST_TIMEOUT_SECONDS = 900


def api_key():
    """The key, from whichever of the usual names is set. `devenv_defn.bat` sets GEMINI_API_KEY."""
    for name in ("GEMINI_API_KEY", "GOOGLE_API_KEY", "GENAI_API_KEY"):
        value = os.environ.get(name)
        if value:
            return value
    sys.exit("No API key. Set GEMINI_API_KEY (devenv_defn.bat does) and run from that shell.")


def load_prompt(value):
    """A path is read; anything else is the prompt itself. Returns the text and where it came from."""
    try:
        path = pathlib.Path(value)
        if path.is_file():
            text = path.read_text(encoding="utf-8").strip()
            if not text:
                sys.exit(f"{path} is empty.")
            return text, str(path)
    except OSError:
        pass  # Too long or otherwise unusable as a path, so it is a literal prompt.
    return value.strip(), "command line"


def reference_part(path):
    """One attached image, base64 inline. Anything the service will not accept fails here, not mid-batch."""
    if not path.is_file():
        sys.exit(f"Reference not found: {path}")
    mime = REFERENCE_MIME.get(path.suffix.lower())
    if mime is None:
        sys.exit(f"Unsupported reference type {path.suffix!r}: {path}. Use {', '.join(sorted(REFERENCE_MIME))}.")
    return {
        "type": "image",
        "mime_type": mime,
        "data": base64.b64encode(path.read_bytes()).decode("ascii"),
    }


def build_input(prompt, references):
    """Text first, then references, which is the order the prompting guide uses."""
    if not references:
        return prompt
    return [{"type": "text", "text": prompt}] + [reference_part(path) for path in references]


def create_interaction(client, request, label):
    """One generation, retrying only what is worth retrying. A 4xx is a bad request and will not improve."""
    for attempt in range(1, RETRIES + 1):
        try:
            return client.interactions.create(timeout=REQUEST_TIMEOUT_SECONDS, **request)
        except RETRYABLE_ERRORS as exc:
            if attempt == RETRIES:
                raise
            wait = RETRY_BACKOFF_SECONDS * attempt
            code = getattr(exc, "code", "?")
            print(f"  {label}: server error {code}, retrying in {wait}s", file=sys.stderr)
            time.sleep(wait)
    raise AssertionError("unreachable")


def payload_of(interaction, kind):
    """The generated bytes plus their mime type, or a readable failure."""
    content = interaction.output_image if kind == "image" else interaction.output_audio
    if content is None or not content.data:
        status = interaction.status or "unknown"
        errors = interaction.errors or []
        detail = "; ".join(str(getattr(err, "message", err)) for err in errors) if errors else "no content returned"
        return None, f"status {status}: {detail}"
    return (base64.b64decode(content.data), content.mime_type or ""), None


def usage_of(interaction):
    """Token counts if the SDK exposes them, so a sweep's cost is visible in the sidecars."""
    usage = getattr(interaction, "usage", None)
    if usage is None:
        return None
    try:
        return usage.model_dump(exclude_none=True)
    except AttributeError:
        return None


def write_result(out_dir, stem, index, data, mime, sidecar):
    """The asset and its sidecar, sharing a name so a keeper carries its own provenance."""
    extension = OUTPUT_EXTENSION.get(mime, ".bin")
    if extension == ".bin":
        print(f"  unrecognized output type {mime!r}, saving as .bin", file=sys.stderr)
    base = f"{stem}_{index:02d}"
    asset = out_dir / (base + extension)
    asset.write_bytes(data)
    (out_dir / (base + ".json")).write_text(json.dumps(sidecar, indent=2) + "\n", encoding="utf-8")
    return asset


def run(args, kind):
    prompt, prompt_source = load_prompt(args.prompt)
    references = [pathlib.Path(ref) for ref in getattr(args, "ref", [])]
    if len(references) > REFERENCE_WARN_AT:
        print(
            f"warning: {len(references)} references attached. Past about {REFERENCE_WARN_AT} they dilute each "
            f"other; the guidance is to weight one primary reference and keep the rest few.",
            file=sys.stderr,
        )

    if kind == "image":
        response_format = {
            "type": "image",
            "aspect_ratio": args.aspect,
            "image_size": args.size,
        }
    else:
        # Ask for plain audio and nothing else. Lyria returns MP3, which is what `assets/music` already holds.
        # Naming a type is worse than pointless: on `lyria-3-pro-preview` both `audio/mp3` and `audio/wav` come
        # back as a 400, "MIME type ... is not supported", so the request fails for asking for what it returns.
        response_format = {"type": "audio"}

    base_request = {
        "model": args.model,
        "input": build_input(prompt, references),
        "response_format": response_format,
    }
    if getattr(args, "continue_from", None):
        base_request["previous_interaction_id"] = args.continue_from
    if getattr(args, "thinking", None):
        base_request["generation_config"] = {"thinking_level": args.thinking}

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    stem = f"{args.name}_{stamp}" if args.name else stamp

    print(f"model      {args.model}")
    print(f"format     {json.dumps(response_format)}")
    print(f"prompt     {len(prompt)} chars from {prompt_source}")
    for path in references:
        print(f"reference  {path}")
    if base_request.get("previous_interaction_id"):
        print(f"continuing {base_request['previous_interaction_id']}")
    if getattr(args, "thinking", None):
        print(f"thinking   {args.thinking}")
    if getattr(args, "seed", None) is not None:
        print(f"seed       {args.seed}" + (f"..{args.seed + args.count - 1}" if args.count > 1 else ""))
    print(f"variants   {args.count}")

    if args.dry_run:
        print("\n--dry-run: nothing sent.\n")
        print(prompt)
        return 0

    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    failures = 0
    for index in range(1, args.count + 1):
        label = f"{index}/{args.count}"
        started = time.monotonic()
        request = dict(base_request)
        seed = None
        if getattr(args, "seed", None) is not None:
            # Walked across the sweep so a fixed value cannot collapse `-n 4` into four attempts at one image.
            # Whether it buys anything is another matter: the field is accepted, but two runs at seed 4242 with
            # an identical prompt came back as visibly different drawings, so treat it as recorded intent
            # rather than a guarantee. It costs nothing to send and may start working.
            seed = args.seed + index - 1
            request["generation_config"] = dict(request.get("generation_config", {}), seed=seed)
        print(f"[{label}] generating...", flush=True)
        try:
            interaction = create_interaction(client_for(), request, label)
        except API_ERROR as exc:
            print(f"[{label}] failed: {type(exc).__name__}: {exc}", file=sys.stderr)
            failures += 1
            continue

        result, problem = payload_of(interaction, kind)
        if problem:
            print(f"[{label}] failed: {problem}", file=sys.stderr)
            failures += 1
            continue

        data, mime = result
        sidecar = {
            "generated": datetime.datetime.now().isoformat(timespec="seconds"),
            "prompt_source": prompt_source,
            "model": args.model,
            "kind": kind,
            "interaction_id": interaction.id,
            "previous_interaction_id": request.get("previous_interaction_id"),
            "response_format": response_format,
            "generation_config": request.get("generation_config"),
            "seed": seed,
            "references": [str(path) for path in references],
            "output_mime_type": mime,
            "usage": usage_of(interaction),
            "notes": interaction.output_text or None,
            "prompt": prompt,
        }
        asset = write_result(out_dir, stem, index, data, mime, sidecar)
        elapsed = time.monotonic() - started
        print(f"[{label}] {asset} ({len(data) / 1_048_576:.1f} MiB, {elapsed:.0f}s)")
        if interaction.id:
            print(f"        refine: --continue {interaction.id}")

    if failures:
        print(f"\n{failures} of {args.count} failed.", file=sys.stderr)
    return 1 if failures == args.count else 0


_client = None


def client_for():
    """One client for the whole run, built lazily so --dry-run needs no key."""
    global _client
    if _client is None:
        _client = genai.Client(api_key=api_key())
    return _client


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate DEFN art and music through the Gemini API.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("\n\n", 2)[-1],
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    def common(sub, default_name):
        sub.add_argument("--prompt", required=True, help="prompt text, or a path to a file holding it")
        sub.add_argument("-n", "--count", type=int, default=1, help="variants to generate (default 1)")
        sub.add_argument("--out", default="build/art", help="output directory (default build/art)")
        sub.add_argument("--name", default=default_name, help="filename stem, timestamped per run")
        sub.add_argument("--model", help="override the model")
        sub.add_argument("--dry-run", action="store_true", help="print the request and the prompt, send nothing")

    image = subparsers.add_parser("image", help="generate images (Nano Banana)")
    common(image, "image")
    image.add_argument("--ref", action="append", default=[], metavar="PATH", help="reference image, repeatable")
    image.add_argument("--aspect", default="16:9", choices=ASPECT_RATIOS, help="aspect ratio (default 16:9)")
    image.add_argument("--size", default="4K", choices=IMAGE_SIZES, help="resolution (default 4K)")
    image.add_argument(
        "--thinking",
        choices=THINKING_LEVELS,
        help="how long the model plans before drawing; the service picks a default if unset",
    )
    image.add_argument(
        "--seed",
        type=int,
        metavar="N",
        help="seed the first variant, walking to N+count-1 across the sweep; accepted but did not reproduce in testing",
    )
    image.add_argument(
        "--continue",
        dest="continue_from",
        metavar="INTERACTION_ID",
        help="edit the image from an earlier interaction instead of generating a new one",
    )

    music = subparsers.add_parser("music", help="generate music (Lyria)")
    common(music, "theme")
    music.add_argument("--ref", action="append", default=[], metavar="PATH", help="image to compose from, repeatable")

    args = parser.parse_args(argv)
    if args.count < 1:
        parser.error("--count must be at least 1")
    if args.model is None:
        args.model = IMAGE_MODEL if args.command == "image" else MUSIC_MODEL
    return run(args, args.command)


if __name__ == "__main__":
    sys.exit(main())
