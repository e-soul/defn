# The Capture Rig

How to record gameplay video and screenshots of the real game — for the itch.io page, a trailer, a devlog, or a
bug report — from a shot file that can be replayed verbatim after the art changes.

- The runner: [`tools/capture_runner.gd`](tools/capture_runner.gd)
- The drawn pointer: [`tools/capture_cursor.gd`](tools/capture_cursor.gd)
- Shot files: [`tools/shots/`](tools/shots/)
- The wrapper: [`scripts/capture.py`](../scripts/capture.py)

Nothing here ships. `defn/tools/` is not named by `export_files`, so the exported game does not carry it, and
`scons packaging` stays green.

---

## What it is

A shot file names a level and a list of timed actions. The runner boots the real game, then plays those actions by
**synthesizing mouse events** — not by calling handlers — so the footage shows the genuine article: card hover and
press states, unit hover, the selection ring, the destination marker, real energy spend. A pointer is drawn into
the frame because the OS cursor is composited by the desktop and never reaches a recording.

Recording runs under Godot's Movie Maker mode, which pins the frame delta to exactly `1/fps` and decouples
rendering from real time. A 38-second shot renders in about three minutes and comes out as smooth 60 fps with
audio in sync; a slow machine changes how long it takes, never what comes out.

The point of a shot file is that footage becomes **re-runnable**. Change a background, re-run the shot, get the
same take with the new art.

## The commands

| Command | Does | Output |
|---|---|---|
| `python scripts/capture.py --recon --level <id>` | Prints where the fight actually is, second by second | A table on stdout |
| `python scripts/capture.py --shot <name> --stills` | Plays the shot, saves its marked frames | 4K PNGs + 1920-wide copies |
| `python scripts/capture.py --shot <name> --video` | Records the shot | MJPEG AVI, then MP4 (and GIF) if ffmpeg is present |
| `python scripts/capture.py --shot <name> --encode-only --gif 13.65 6` | Re-encodes what is already recorded | MP4 + cover GIF |

`--shot` defaults to `level_03_opening`; with no mode flag, `--video` is assumed. `--level` overrides the level a
shot names, and is how a recon pass picks one before any shot file exists for it. Godot is found via `--godot` or
`GODOT_BIN`, the same as the SCons targets — on this machine `devenv_defn.bat` sets it. Everything lands in
`build/capture/`, which is git-ignored.

ffmpeg is optional. Without it the AVI is still written and the script prints the command to convert it by hand.

```
python scripts/capture.py --shot level_03_opening --video --stills --gif 13.65 6
```

## Authoring a shot

**1. Recon the level.** Timings should come from measurement, not from guessing when two lines meet.

```
python scripts/capture.py --recon --level level_03 --recon-seconds 45
```

It deploys a few units on a fixed schedule and prints, per second: friendly and hostile counts, camera x, the
leftmost hostile, the rightmost friendly, and which cards are affordable. On `level_03` that says wave 1 spawns
between 4.0 s and 8.4 s, the lines meet around 17–22 s at world x 650–700, and the camera never scrolls, so the
whole engagement stays in frame.

**2. Write the shot file** in `tools/shots/`, using those numbers. See
[`level_03_opening.json`](tools/shots/level_03_opening.json), whose `_notes` explain every timing it picked.

**3. Dry-run it with `--stills`.** A stills pass skips Movie Maker and runs as fast as the machine manages, so it
is the cheap way to see whether the choreography reads. Put a `still` next to any beat you are unsure of.

**4. Record**, then encode. Re-cutting a GIF or trying another x264 setting is `--encode-only`; it does not pay
for the render again.

### Shot file keys

| Key | Default | Meaning |
|---|---|---|
| `name` | `shot` | Names the output files |
| `level` | `level_03` | Campaign level id — `level_03`, never `3` |
| `seed` | `20260911` | Seeds the global RNG, which is all the live game uses |
| `fps` | 60 | Frames per second; also the shot's clock |
| `duration_seconds` | 30 | Length of the shot proper, excluding boot |
| `fade_in` / `fade_out` | 0.5 / 0.6 | Seconds of fade at each end |
| `cursor.input_chip` | `true` | The LMB/RMB pill in the corner |
| `cursor.start` | — | Where the pointer begins, in canvas coordinates |
| `timeline` | `[]` | The actions |
| `background_only` | `false` | Freeze the loaded match and hide everything except its background and camera, including the cursor |
| `mute_music` | `false` | Silence the background music player for an effects-only recording; leave gameplay and UI sounds intact |

Every timeline entry takes `at` (seconds from the start of the shot) and an optional `travel` (seconds of cursor
approach before the click, default 0.34).

| Action | Fields | Does |
|---|---|---|
| `deploy` | `unit` | Clicks that unit's deploy card |
| `select` | `target` | Left-clicks a friendly: `newest`, `oldest`, `rightmost`, `leftmost` |
| `move` | `ahead`, `from` | Left-clicks ground to reposition the selected unit — **`ahead` must be negative** |
| `deselect` | `ahead`, `from` | Right-clicks empty ground |
| `still` | `label` | Saves a PNG of that frame |
| `park` | `canvas` | Moves the pointer somewhere without clicking |
| `hold` | — | Does nothing |

Targets are re-resolved **every frame of the travel**, so the pointer tracks a walking unit the way a hand would.
Coordinates are canvas-space (1920×1080), which is also what the HUD's rects are in.

### Campaign background previews

The `background_preview` shot captures the real opening parallax composition without the tower, units, HUD or
pointer. For each campaign level, run:

```
python scripts/capture.py --shot background_preview --level level_01 --stills --still-width 960 --out-dir build/capture/previews/level_01
```

Repeat for `level_02` through `level_05`. Convert each `background_preview_background_960.png` to a high-quality
RGB JPEG at **960 x 540**, replacing that level's existing `assets/campaign/*_preview.jpg` as named by
`data/campaign_map.json`. Keep the full-resolution captures under `build/capture/`, not in the shipped assets.
Re-import the project after replacing the previews.

---

## Five things that will bite

**Synthesized events are in window space, not canvas space.** Controls live in the 1920×1080 canvas, but the root
viewport is the physical window — 3840×2160 on a HiDPI display — and `push_input` undoes the stretch. Feeding a
Control's own rect coordinates straight to `Input.parse_input_event` lands the click at half position, where it
hits nothing at all and reports no error. Always `root.get_screen_transform() * canvas_point`; `_to_window()` in
the runner does this.

**Schedule in frames, never in wall-clock time.** Under a `--script` main loop the frame rate is uncapped — about
3000 fps here — so awaiting 60 frames is 20 ms of game time, not a second. A probe that waits "twelve seconds"
this way sees a quarter-second of match and concludes the game is frozen; it is not. `--fixed-fps` (which both
capture modes pass) pins the delta, and Movie Maker pins it at exactly `1/fps`.

**A reposition only goes backwards.** `request_reposition` refuses any destination to the right of the unit: the
order turns a unit around, walks it back and suspends its combat. An order to the right films as a click that did
nothing. This is why `move` takes a negative `ahead`, and why the beat worth showing is pulling the front unit out
as a wave closes.

**Movie Maker records from process start.** The half-built menu and the scene swap would otherwise open every
take. The runner puts an opaque cover up before the first scene loads and fades it up when the shot begins, so
recording time is shot time **plus about 0.65 s of boot**. The runner prints the exact offset:
`shot starts at recorded frame 39 (0.65s)`. A `--gif` span or any trim is measured from the head of the file, so
add that offset.

**Godot's stdout is swallowed when it is launched from Python on Windows.** The plain editor binary detaches from
the parent console, which hides everything the runner prints — including the warning that a deploy never became
affordable. `capture.py` switches to the `_console` build next to it automatically; if you invoke Godot yourself
from a script, do the same.

## What comes out

For the 38-second `level_03_opening` at 60 fps:

| Artifact | Size | Notes |
|---|---|---|
| `level_03_opening.avi` | 499 MB | MJPEG, straight from the movie writer, with PCM audio |
| `level_03_opening.mp4` | 32 MB | x264 CRF 16, `yuv420p`, AAC 192k — headroom for YouTube's own re-encode |
| `level_03_opening.gif` | 1.6 MB | 630 px wide, 15 fps, generated palette — fits an itch.io cover |
| `stills/*.png` | ~7 MB each | 3840×2160, plus a 1920-wide downscale of each |

Two resolutions, on purpose. **Video records at the project viewport size — 1920×1080 — regardless of the window
or the display's DPI**, so a recording is identical on any machine. **Stills come off the window backbuffer**,
which on a HiDPI display is 4K; downsampling those to 1080p is visibly sharper than grabbing 1080p directly.

Audio is captured and stays in sync because it is rendered offline alongside the video, not sampled live.

## "Give ground. Hold the line." showcase

[`hold_the_line.json`](tools/shots/hold_the_line.json) records a 60-second source take on Summar Beach.
[`edit_hold_the_line.py`](../scripts/edit_hold_the_line.py) cuts it to **53 seconds at 1080p60**:

| Final time | Beat |
|---|---|
| 0-7 s | Combat-first hook, taken from later in the same engagement |
| 7-14 s | Fade back to three opening deployments; "BUILD YOUR LINE." |
| 14-49 s | Uninterrupted approach, retreat order, resumed fighting and reinforcements |
| 49-53 s | DEFN end card; continuous soundtrack finishes with a two-second fade |

The edit skips the quiet march between deployment and contact, but never speeds up combat or cuts inside the
retreat. Its payoff is holding through the opening rush, not a claimed mission victory. Three short captions sit
above the battlefield as bright, square-edged editorial cards with numbered action labels, rather than dark
HUD-like panels. Their colors and the end card use the UI palette. Each scene fades out and the next fades in
over 0.45 seconds per side, with matching sound-effect fades; the opening and ending also fade.
Music runs on a separate, uninterrupted timeline across every scene transition, including the end card.
The edit uses the game's `theme01.mp3` at a linear gain of 0.36, with fades only at the beginning and end.
No external music or art is added.

From the repository root:

```
python scripts/capture.py --shot hold_the_line --stills
python scripts/capture.py --shot hold_the_line --video --out-dir build/capture/sfx
python scripts/edit_hold_the_line.py --boot-frames 39
```

Use the actual `shot starts at recorded frame` value from the recording log if it differs from 39.
The edit needs ffmpeg, ffprobe and Pillow; it uses installed Arial Bold on Windows, or an explicit
`--font <font-file>` elsewhere. Its output is `build/capture/give_ground_hold_the_line.mp4`.
Re-running just the edit does not launch Godot. Effects-only source files live in `build/capture/sfx/`.
The shot sets `mute_music: true`; do not feed the older mixed recording to the editor, or music will double.
`--music` selects a different local track and `--music-volume` adjusts its linear gain. The music must cover
the whole edit; short tracks are rejected rather than silently looped. The final mix uses a non-boosting peak
limiter, and only music continues under the end card.
The editor checks the source format and verifies 3,180 output frames and 53 seconds of audio.

### Additional showcase variations

The same editor supports three more effects-only shots. Each preset uses
`build\capture\sfx\<variation>.avi` and writes `build\capture\<variation>.mp4`;
record each shot with `mute_music: true` before editing.

| `--variation` | Level | Continuous music (gain 0.24) | End-card title |
| --- | --- | --- | --- |
| `jungle_line` | `level_02` (Jungle Ruins) | `theme02.mp3` | HOLD THE JUNGLE LINE. |
| `winter_fireline` | `level_04` (The Winter Forest) | `theme03.mp3` | DEFN |
| `town_crossfire` | `level_05` (Feldkirchen) | `theme04.mp3` | DEFN |

Winter and town use `logo_only=True`: their end cards show only centered DEFN text,
without a divider, tagline or subtitle. Other presets retain their existing end cards.

These three tracks are roughly 4–5 dB louder than the beach track over the opening
53 seconds, so their preset gain is intentionally reduced to 0.24 to balance music
against gameplay effects. The original beach preset retains 0.36; `--music-volume`
still takes precedence over either default.

From the repository root, using each recording's actual boot-frame value:

```powershell
python scripts\capture.py --shot jungle_line --video --out-dir build\capture\sfx
python scripts\capture.py --shot winter_fireline --video --out-dir build\capture\sfx
python scripts\capture.py --shot town_crossfire --video --out-dir build\capture\sfx
python scripts\edit_hold_the_line.py --variation jungle_line --boot-frames 39
python scripts\edit_hold_the_line.py --variation winter_fireline --boot-frames 39
python scripts\edit_hold_the_line.py --variation town_crossfire --boot-frames 39
```

Omitting `--variation` (or choosing `hold_the_line`) retains the original beach edit.
Explicit `--source`, `--output`, `--music` and `--music-volume` always override preset defaults.
All variations keep the 53-second structure: 7-second hook, 7-second deployment,
35-second continuous battle and 4-second title. Initial shot-relative clips are
`(26, 33)`, `(1.2, 8.2)` and `(12.5, 47.5)`, with captions on the finished edit's
clock at `7.6–13.2`, `15.8–21` and `24.8–29` seconds. Scene fades, caption styling
and uninterrupted music are shared with the beach edit.
The jungle variation includes a reposition order; its final caption runs at `29.8–34` seconds
to match the recorded reinforcement click after the energy wait. It shows a pressured defence,
not a claimed victory. Inspect the actual movie as well as the dry-run stills: combat outcomes
and affordability timings can differ between takes.

`VARIATIONS` in the editor holds typed `Variation` and `Caption` values: add a preset's
`clips=` to adjust its source windows, or change its caption times independently.
Validation preserves the three clip lengths, frame-aligned boundaries and caption
fade room, and requires source duration through the latest clip endpoint plus boot frames
(not merely the final clip in edit order). Generated caption/end-card PNGs are retained
beside the output in `<output-stem>_titles` for inspection and regenerated on subsequent edits.

Check the marked stills again after balance or progression changes: an unaffordable deployment delays all later
events. Captures use the current local campaign save, including unlocks and upgrades, and a run that finishes a
match can update that save. The shots aim to end before mission completion, but an early defeat
can still update the save.

## Extending it

- **A new action**: add a case to `_target_spec()` for what it aims at and to the `match action` block in
  `_play()` for what it does. Anything reachable by mouse is already reachable; nothing needs binding on the C++
  side.
- **A new target kind**: add a branch to `_resolve()`. It is called every frame during a travel, so it may track
  something that moves.
- **Deploy cards are matched by their title label** — "Breacher" is the card for `breacher` — because the HUD
  keeps unit ids on the C++ side only. A card whose display name stops matching its id needs an explicit map here.
- **A deploy whose card is dark waits before the pointer sets off**, for up to 12 s, then skips with a warning.
  So a balance change slides a shot's timing instead of breaking it — but schedule the tactical beats *before* any
  deploy that might wait, because a waiting event holds up the ones behind it.

## Known limits

- Video is 1080p; the movie writer follows the project viewport, so 4K video would mean overriding that at
  startup. Stills are already 4K.
- The camera is wherever the game puts it. On levels whose scroll triggers do fire, a shot cannot currently frame
  a specific spot on the belt.
- One pointer, one action at a time. Events are strictly sequential.
- Shots start from the level, not from the menu. Menu navigation is clickable the same way, but no shot does it
  yet.
