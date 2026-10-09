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
| `python scripts/capture.py --shot responsive_interactions --stills --size 390x844 --fixture max_roster` | Verifies gestures, rotation, pause and navigation through real input | PNGs and explicit checks |
| `python scripts/capture.py --shot audio_restoration --video --stills --size 960x540 --fixture max_roster --no-cursor` | Records accepted card taps and combat with music muted | AVI with mixed effects audio and a combat still |

`--shot` defaults to `level_03_opening`; with no mode flag, `--video` is assumed. `--level` overrides the level a
shot names, and is how a recon pass picks one before any shot file exists for it. Godot is found via `--godot` or
`GODOT_BIN`, the same as the SCons targets — on this machine `devenv_defn.bat` sets it. Everything lands in
`build/capture/`, which is git-ignored.

ffmpeg is optional. Without it the AVI is still written and the script prints the command to convert it by hand.

`--size` requests logical UI dimensions. The runner converts them to physical
window pixels using the window's current content transform after the menu boots.
World targets are converted through the battlefield viewport and its host
rectangle; UI targets use their own control rectangles. This keeps click mapping
correct after resize and at different desktop densities. The world remains
1920 × 1080 while captures include the full-screen UI around it. With `--size`,
the wrapper measures the actual render pixels and stages a private project with
matching Movie Maker dimensions; it shares imported resources and leaves the
game's project settings untouched. Movie dimensions are fixed at startup, so
use fixed-size shots for video and stills passes for rotation/resize shots.

`--fixture fresh`, `--fixture max_roster` and `--fixture rewards` redirect Godot user storage beneath
the chosen output directory. They do not overwrite the player's save. The latter
unlocks the campaign and all four friendly types and supplies a large career
score. The rewards fixture supplies strong, explicitly artificial upgrade counts
for a real first-clear reward draft; `responsive_results` deploys two units and
requires a visible VICTORY title before photographing the result. Run it with
`--fixture rewards`. `responsive_interactions` includes drag/tap count checks, release after
resize, selection, pause/resume, menu navigation and the wide campaign map. `click`, `key`, `wheel`, `press_card`,
`drag`, `release` and `resize` synthesize events or change the window; `key` defaults
to Escape and also accepts `key: "Tab"`. `check`
observes state and makes capture failures fatal. Still/check actions are ordered
by their scheduled action time. Tools and shot fixtures remain excluded from
exports.

`responsive_ui` also checks the original HUD headings inside the window, requires
a landscape battlefield at least 70% of stage height, and verifies both pause
actions fit. `buttons_visible` checks complete button rectangles, including the
desktop campaign's Replay and Back actions. Pause lookup uses its accessibility
name when the visible control is the pause symbol. Use `--no-cursor` for layout
inspection so the capture-only LMB/RMB indicators cannot cover real controls.

`desktop_reference --fixture max_roster --no-cursor` captures Feldkirchen in the
default 1920 × 1080 client window. It checks full battlefield height, the three
original content-sized top plates, 13-point HUD labels, and four 190 × 110 cards
with 80 × 80 portraits and 15/13-point title/cost text in reference coordinates.
It also checks deployment, selection and pause/resume through synthesized input.
These reference-coordinate checks remain valid at different client sizes and DPI.
`campaign_carousel --stills --size 390x783 --fixture fresh --no-cursor` captures
the mobile campaign through real arrow clicks and an illustration swipe. It checks
locked missions, complete briefing labels, selection after rotation, cancellation
of a gesture across resize, Back, and explicit deployment. `campaign_carousel_endless`
with `--fixture max_roster` captures Replay and the optional standing engagement,
including its explicit Begin Watch action. Both include 864 × 230 landscape.
`labels_visible` and `buttons_disabled` observe the real controls; `press`, `drag`
and `release` can exercise a gesture at canvas coordinates.
`menu_panels --size 960x540 --fixture max_roster --no-cursor` captures the first
pause opening and the main menu on desktop, portrait and landscape, then checks
the options footer in a short window. `options_panels --size 1280x800 --fixture fresh --no-cursor`
captures all desktop settings together and checks Back in a short window.
`buttons_visible` also checks ancestor
clipping, so a button inside a collapsed scroll viewport fails the capture.
`responsive_interactions` uses Escape for its desktop landscape pause, then
exercises the visible Pause control after returning to portrait.

`texture_tiles --stills --size 960x540 --fixture max_roster --no-cursor` captures
a frozen background for source-versus-tiled comparison. `--project-dir` can select
an isolated project prepared by `scripts/stage_web_project.py` with `include_tools=True`.
Use a smaller tile limit such as 1024 during verification to put many tile joins
inside the frame. `--level` selects each campaign background.

`responsive_overflow --size 667x330 --fixture max_roster` checks horizontal
scrolling (zero deployments) followed by a tap on the revealed Operator (one).
At 320 logical units, the result footer wraps its actions; that width is a
best-effort stress target rather than a comfortable gameplay target.

`python scripts/capture_responsive_web.py http://127.0.0.1:8000/index.html`
uses Playwright against the real export, with a temporary browser context and
an IDBFS save fixture. It captures navigation, mobile volume, disabled Progress,
maximum-roster drag/tap, resize, pause, fullscreen and results. Install
`scripts/requirements-web-test.txt` and Playwright Chromium first. Its coordinates
are the current narrow UI anatomy: inspect the PNGs as well as its runtime log.
`--cdp http://127.0.0.1:9223` attaches to a dedicated test Chrome (including Android
Chrome forwarded by adb); use a test origin because it replaces that origin's
campaign save. Browser captures complement the Godot rig; they do not invoke
game handlers. Fullscreen captures fail if the shell's exit button overlaps the
canvas. Android rotation remains a device operation.

`--options --fixture fresh` captures the mobile volume panel in portrait, landscape
and a 230-unit-tall stage. It changes volume through touch, observes its normal
IDBFS settings file, uses Back, and reopens and reloads the panel to check persistence.
It also taps the disabled Progress action in both orientations and a short landscape,
then uses Main Menu to verify the surrounding actions remain accessible.

`--shell` captures the portrait rotation hint beside Fullscreen, including a narrow
320-unit viewport, fullscreen entry/exit and rotation. It checks hint visibility,
header overflow and separation from the fullscreen button. Orientation lock behavior
uses the actual browser API; emulated browsers may reject it and remain in portrait.

`--promotion` uses a desktop pointer and the rewards fixture, deploys two units
through normal mouse input, and captures the solid SVG promotion star after real
combat. Inspect the 90- and 110-second stills to verify the Web export renders the
star without relying on system fonts.

`python scripts/capture_responsive_web.py http://127.0.0.1:8151/index.html
--campaign-carousel --fixture fresh` captures touch swipes, touch arrows, rotation,
all locked cards and the disabled deployment gate in the real release export.
Repeat with `--fixture max_roster` to check the sixth, uncounted endless card and
Begin Watch. Add `--cdp`, `--android-device` and `--adb` to exercise actual Android
Chrome rotation. Use an isolated test origin because the fixture replaces its save.

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

For Port Terminal, select the unlocked endless mode through the real menu:

```
python scripts/capture.py --shot background_preview --level endless --stills --size 1920x1080 --still-width 960 --fixture max_roster --out-dir build/capture/previews/port_terminal
```

Save its RGB JPEG as `assets/campaign/port_terminal_preview.jpg`. The endless preview uses the same real
background layers, camera and painter order as the match, without gameplay or UI baked into the image.

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

Without `--size`, Movie Maker uses the project viewport size (1920×1080).
With `--size`, video records the complete requested window at its render density:
390×844 logical units at 200% scaling produce a 780×1688 movie. **Stills come
off the window backbuffer**, preserving the same density; optional downscaled
copies retain the captured aspect ratio. Inspect a recorded frame as well as the
stills, because Movie Maker fixes its dimensions before the runner starts.

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
- **Deploy cards carry `unit_id` metadata** so discovery survives reflow and name changes. Older fixtures fall
  back to their title label.
- **A deploy whose card is dark waits before the pointer sets off**, for up to 12 s, then skips with a warning.
  So a balance change slides a shot's timing instead of breaking it — but schedule the tactical beats *before* any
  deploy that might wait, because a waiting event holds up the ones behind it.

## Known limits

- Video follows the requested logical root size. Density-aware stills use the physical window size.
- The camera is wherever the game puts it. On levels whose scroll triggers do fire, a shot cannot currently frame
  a specific spot on the belt.
- One pointer, one action at a time. Events are strictly sequential.
- Shots start from a level. `responsive_interactions` then visits menus through the real pause action.

The native runner disables VSync while using a fixed frame delta, so synthesized
input remains frame-scheduled and background captures can finish without display
refresh throttling.

## UI regression scenarios

Native and browser entry points share scripts/capture_fixtures.py. fresh, max_roster,
rewards and rescue create isolated saves; filesystem and IDBFS installation remain
separate. Fixture upgrades stress presentation and make the victory/reward replay
reliable; they do not represent normal progression balance.

| Native shot | Unique contract |
| --- | --- |
| desktop_reference | Original battlefield/HUD reference fit and DPI-scaled input. |
| responsive_ui | Portrait metrics and deploy-grid composition. |
| responsive_overflow | Maximum roster, tray capacity and scroll navigation. |
| responsive_interactions | Swipe/tap, pause, menus and resize cancellation. |
| responsive_results | Compact result content and reward/owned navigation. |
| menu_panels | Content-fitted menu, options and progression chrome. |
| desktop_score_victory | First-clear reward gate, selection and enabled navigation. |
| desktop_score_defeat | Defeat statistics and retry/campaign actions. |
| desktop_score_rescue | Later-frontier rescue reward after defeat. |
| desktop_score_endless | Run total, best run, records and fresh-run action. |
| campaign_carousel | Locked browsing, selection, arrows/swipes and rotation. |
| campaign_carousel_endless | Distinct unlocked endless entry and deployment. |
| texture_tiles | Tiled-background pixels, painter order and composition. |
| audio_restoration | Accepted deployment, battle and UI audio after transitions. |
| field_promotion | World-space promotion star visibility, size and gold fill after real combat. Use --fixture rewards. |
| background_preview | Clean campaign thumbnails from the real parallax composition. |

From the repository root, replay fixed sizes with video and stills:

~~~powershell
python scripts/capture.py --shot desktop_reference --video --stills --size 1920x1080 --fixture max_roster --no-cursor --out-dir build/capture/ui-review/desktop
python scripts/capture.py --shot responsive_ui --video --stills --size 390x844 --fixture max_roster --no-cursor --out-dir build/capture/ui-review/portrait
python scripts/capture.py --shot desktop_score_victory --video --stills --size 1920x1080 --fixture rewards --no-cursor --out-dir build/capture/ui-review/victory
~~~

Use desktop_score_defeat with max_roster, desktop_score_rescue with rescue, and
desktop_score_endless with max_roster and --level endless for the other outcomes.
Resize flows use stills. Keep outputs under build/capture, outside source/assets.

Build/serve the actual Web export as described in [WEB_BUILD.md](WEB_BUILD.md), then:

~~~powershell
python scripts/capture_responsive_web.py http://127.0.0.1:8137/index.html --phone-landscape --out-dir build/capture/ui-review/web-landscape
python scripts/capture_responsive_web.py http://127.0.0.1:8137/index.html --campaign-carousel --fixture max_roster --out-dir build/capture/ui-review/web-campaign
python scripts/capture_responsive_web.py http://127.0.0.1:8137/index.html --score-screens victory --score-interactions --out-dir build/capture/ui-review/web-victory
~~~

For Web defeat use --score-screens defeat with --fixture max_roster, fresh or rescue;
use --score-screens endless for a run. --score-interactions covers reward Back,
selection, owned paging, 864x230 stage stress and item preservation through rotation.
The landscape flow covers compact menus, overflow, accepted taps, card enabled
states, pause and portrait restoration.

BrowserCapture shares error collection, canvas coordinates, touch dispatch,
screenshots, bounded shell/log readiness and rotation. Scenarios still send ordinary
browser input. Simulation/outcome and rendered-frame waits retain settling intervals
where the engine exposes no reliable browser signal. Inspect PNGs and movies:
successful navigation and a clean error log alone do not prove the expected outcome.

Owned Chromium contexts emulate 390x844 and 667x375 browser viewports at DPR 3;
the shell reserves its toolbar outside the game. These are browser emulation,
not Android evidence. For a dedicated connected Android test browser, add
--cdp http://127.0.0.1:9223 --android-device emulator-5554 --adb <adb executable>.
Disable automatic rotation on that test device; the helper changes actual device
orientation. Use a private test origin and owned ADB forwards/reverse mappings.
browser-inputs.json in score captures records primary coarse and any fine pointer
capabilities; a phone with a secondary mouse must retain mobile results.

The bounded matrix includes native normal/high DPI, portrait, landscape and a
230-unit stage, nonzero safe insets, coarse-primary plus secondary mouse, campaign
selection/lock/endless, swipe/pause, and distinct score outcomes. Pure/hosted context
tests supplement captures for coordinate/capability transitions. Record unavailable
platform cases explicitly.

Historical reports and debug.log are archived under
D:/dev/src/defn-local/ui-redesign-history. The dated EVIDENCE_INDEX and SHA-256
manifest preserve original capture locations. Fresh runs belong with their build
revision, initial working-tree status, exact commands and observed limitations.
