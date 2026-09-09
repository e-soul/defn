# Modular backgrounds

Gameplay backgrounds built as per-biome **layer sets**: a few seamless horizontal strata composed at load time
into a parallax stack, instead of one monolithic image.

**Status.** Working end to end and shipping for endless mode, on the port-terminal set. The five campaign
levels still use their single images and are unchanged. Stamps — scattered props on top of the strata — are
designed but not built.

[`ASSET_PROMPTS.md`](ASSET_PROMPTS.md) remains the authority on style, palette, and references; everything here
inherits from it. This file covers what is different about authoring a *layer* rather than a scene.

---

## Running the pipeline

Four stages. The first three are per-image and involve picking a keeper; the last is per-biome and mechanical.

**1. Generate.** One prompt per layer, in `art/prompts/<biome>/`. Roll two or three variants.

```bash
python scripts/gen_art.py image --prompt defn/art/prompts/port_terminal/mid.txt --aspect 4:1 --size 4K -n 3 --out build/art/port_terminal --name mid
```

Alpha-bearing layers are drawn over flat magenta; opaque ones are not. `4:1` is the aspect ceiling. Attach a
reference only when the layer genuinely shares subject matter with it.

**2. Key**, for every layer that needs transparency. Opaque layers skip this.

```bash
python scripts/cut_layer.py build/art/port_terminal/mid_01.jpg --key magenta --no-crop --report --preview build/art/port_terminal/keyed -o build/art/port_terminal/mid_cut.png
```

Look at the checkerboard preview for halos before going further.

**3. Cut a wrapping strip.** Never trust the model's seamlessness; this is what makes it true.

```bash
python scripts/make_tileable.py build/art/port_terminal/mid_cut.png --width 6000 --tolerance 0.30 --no-flatten -o build/art/port_terminal/mid_tileable.png
```

Pass `--no-flatten` for anything already keyed; omit it for opaque layers so the lateral lighting trend is
fitted out. A `SEAM` verdict here means re-roll, not a longer feather.

**4. Build the set.** Sizes every layer, seats the standing bands with a contact shadow, verifies the wraps,
and prints the level data.

```bash
python scripts/build_layer_set.py defn/art/layer_sets/port_terminal.json
```

Paste the printed `background_layers` block into the level's JSON — `data/endless.json` for endless. Then
re-import so Godot picks up the changed textures, and relaunch:

```bash
"$GODOT_BIN" --headless --path defn --import
```

Data and asset changes need no rebuild. Only C++ changes do.

### Where things live

| | |
|---|---|
| Prompts, one per layer | `art/prompts/<biome>/{sky,far,ground,mid}.txt` |
| Build recipe and geometry | `art/layer_sets/<biome>.json` |
| Generations and intermediates | `build/art/<biome>/` — not committed |
| Shipped assets | `assets/backgrounds/<biome>/*.png` |
| Runtime geometry | the level's `background_layers` |

Sources under `build/` are not committed, so what records the work is the prompt, the `.json` sidecar
`gen_art.py` writes beside every generation, the manifest, and the shipped PNG. Rebuilding from nothing means
going back to the prompts.

Every new asset must also be listed in `export_presets.cfg`, or it renders in a debug run and is missing from
the packaged build. `python scripts/check_export_presets.py` catches that, and runs as part of `scons test_all`.

---

## The model: strata plus stamps

The tiling template in [`templates/background_tiling.txt`](templates/background_tiling.txt) already describes a
background as horizontal depth planes with the far edge at the 50% mark, storytelling confined above it, and
line weight scaling with depth. That contract *is* a layer stack; it had simply been baked flat every time.

### Strata

Continuous horizontal bands. Each tiles, each scrolls at its own rate. Listed in draw order, back to front,
with the port-terminal values as a worked example:

| Layer | Alpha | `scroll_scale` | `height_ratio` | `bottom_ratio` | Content |
|---|---|---|---|---|---|
| `sky` | no | 0.05 | 1.000 | 1.000 | vertical wash only — no clouds, no horizon |
| `clouds` | yes | 0.15 | 0.420 | 0.420 | soft cloud masses, `autoscroll` 6.0 |
| `far` | yes | 0.35 | 0.280 | 0.460 | distant skyline across the water, low contrast |
| `ground` | no | **1.00** | 0.500 | 1.000 | the walkable floor |
| `mid` | yes | **1.00** | 0.433 | 0.663 | the band standing on the floor |

`autoscroll` is horizontal drift in pixels per second, independent of the camera, and only the cloud layer has
it. It is the only thing on screen that keeps moving while the player stands still, which is most of a match,
and that makes it the cheapest motion in the stack. It also means the clouds belong in their own layer rather
than painted into the sky: a cloud in the sky wash would sit still while these drift past it.

Four rules that are not negotiable.

**`ground` stays at 1.00.** Units live in its coordinate space. A floor that slides relative to the sprites
standing on it reads as ice.

**Anything sharing a contact line with the ground also scrolls at 1.00.** The ground is a rigid texture moving
at 1.00, so every mark painted on it moves at 1.00 — including its own far edge. A band planted on that edge is
planted on something moving at 1.00, and any other rate makes the floor visibly slide out from under it. `mid`
was first set to 0.60 on the reasoning that it is further away, and in the engine the belt slid under the
containers exactly as the arithmetic says it must. Distance does not earn a slower rate; *not touching the
ground* earns a slower rate.

**The ground is drawn before anything standing on it.** The intuition that the midground band is "behind" the
floor is wrong: it stands on the floor, so the floor is painted first and the band drawn onto it, exactly as a
unit sprite is. Reversing them does not look like a layering bug — the ground silently paints over everything
below its own far edge, clipping the bottom third of the band and all of its contact shadow. It reads as "the
structures are too small and too far away", and resizing cannot fix it, because whatever is added below the far
edge is clipped too.

**Give the strata unequal widths.** The composite period becomes their least common multiple rather than one
screen. Most of the repetition fix comes from this alone, before a single stamp is placed.

There is deliberately no foreground layer at `scroll_scale > 1.0`. `belt_width` is `[0.66, 0.825]`, so a unit's
feet land as low as 82.5% of screen height, and anything drawn below that occludes legs during a fight.

**How much floor to show** follows from the draw-order rule: the visible floor is bounded above by whatever
stands on it, not by the ground layer's own far edge. The ground can keep its full drawn height and generous
scale, because the standing band covers the far part of it. What matters is where that band's base sits, and
the answer is at or just above the top of the belt band — 0.65 against a belt starting at 0.66 here, which
matches `background_jungle_ruin_tiling.png`, where the structures are based at 0.62–0.66.

### Stamps

Discrete alpha cut-outs — wrecks, huts, crane masses, individual trees, rock forms — placed at arbitrary x by a
deterministic scatter. Designed, not built. This is where variability and life are meant to come from.

The division of labour is the point: **the layers that must tile are the simple ones** (sky, haze, ridge,
floor), and everything with interesting silhouette becomes a stamp whose x position is free, so tiling never
constrains it.

When built, placement belongs in the domain layer as pure logic — a `BackgroundScatterPlanner` taking the layer
set, a seed, the belt band and the camera extent, returning `{stamp_id, layer, x, flip, variant}` with no Godot
types, unit-testable in the native suite. Determinism matters so the hosted and conformance suites stay stable,
so a campaign level looks the same every time it is played, and so endless — the mode that is *about*
repetition — is the one free to reseed per run. Constraints it must enforce: never inside the belt band,
per-layer y ranges, minimum spacing per class.

### The ground is not a tileset

Worth stating because it is the obvious thing to try. Do not break the combat floor into repeating tiles. A
cel-shaded surface with hand-drawn marks shows grid repetition badly, and the floor is the one plane the player
looks at for a whole match. One wide seamless strip per biome; put the variability above it.

---

## Prompt rules for layers

Additions to the house style, all of them learned by getting them wrong first.

**Light flat across the width.** No lateral gradient, no directional sun, no raking light, no large cast
shadow. State it as a *tiling requirement*, not an atmospheric preference, because that is what it is: a strip
lit from one side has two ends differing by the whole gradient and no crop can reconcile them. Feathering does
not save it either — the ramp concentrates the mismatch into a short band whose end reads as an edge. Vertical
gradients are fine and wanted; say so explicitly, or the flat-lighting rule and the depth falloff read as
contradicting each other.

**Never name a thing you do not want drawn.** A ground prompt asking for "stencilled bay numbering worn to
unreadable ghosts" produced crisp, legible bay numbers. Keep the positive text free of it entirely, and let the
exclusion list forbid numerals, letters and character-like marks explicitly, in any orientation.

**Ask for a hard cel edge against the chroma field, positively.** Left to itself the model treats the backdrop
as atmosphere and feathers the artwork into it — the first container band dissolved into 150px of magenta and
green smear along its top silhouette. Put the requirement in the positive section, not only in the exclusions.
A single `--continue` turn demanding a hard edge fixed it completely: soft-edge fraction 2.0% to 0.6%, halo
+168 to +39. **Refinement, not re-rolling, is the tool for this.**

**Describe a projection as mechanics, not as a camera.** "A gently elevated three-quarter view" is resolved
however the model likes, and both of its defaults are wrong: converging diagonals with a vanishing point, or a
flat orthographic grid that reads as paving standing up. What works is three separate statements — feature
direction, feature spacing, and how that spacing changes with height.

**Express the elevated angle as vertical foreshortening, never as convergence.** Lateral features stay parallel
to the frame edges and their spacing shrinks steadily from bottom to top; depth features stay exactly vertical
and parallel. That progressive compression is the single cue that makes a surface read as a floor lying down
rather than a wall standing up.

**Prefer an organic surface vocabulary on the ground.** Straight lines are a tiling liability: under
foreshortening a regular horizontal rhythm is exactly what the eye locks onto and counts. The paved apron with
expansion joints and painted bars showed its repeat badly and cut poorly; the dirt-and-gravel version described
by soft irregular patches wraps comfortably while keeping 8050 of 8256 available pixels.
`background_winter_forest_tiling.png` and `background_beach_tiling.png` are the models to look at — but
describe what you learn from them in words rather than attaching them, or their palette comes too.

**Name the biome's own dryness explicitly.** A ground prompt that lists patches, gravel and tufts without
saying what kind of place it is drifts toward a verge or a meadow. "This is an arid, hard-used industrial yard,
not a meadow, a verge, or a field", followed by an explicit statement that nothing on the surface is green and
there is no grass, moss, snow or pure white, is what pins it.

**Distance means lower contrast, never haze.** "Atmospheric desaturation" and "hard cel edge" are contradictory
instructions, and the model resolves them by dissolving the artwork into the backdrop. Say that distance means
lower contrast and a lighter, cooler value, and *never* softness, blur, haze, mist, fog, fading or
transparency.

**An attached reference is reproduced, not referenced.** A sky generated with the port apron attached "as the
authority for palette temperature" came back with the apron, lane markings and all, across its bottom third; a
far band with the container band attached came back carrying containers. Attach a reference only when the layer
genuinely shares subject matter with it, and state the palette in words otherwise. Every layer in the shipped
port set was ultimately generated with **no reference at all**, and each came out cleaner for it. The house
style is described well enough in prose to carry a layer on its own.

A caution on diagnosing this, though: not everything that looks like contamination is. The first ground was
blamed for olive-green tufts leaked from a winter-forest reference, and the prompt turned out to have asked for
"dry olive weed tufts" in as many words. Check what was actually requested before blaming the reference.

**4:1 is the aspect ceiling.** 8:1 was tried to buy a longer period and degraded badly — both rolls smeared
through the middle, one to near-pure streaks with no structure. The same prompt at 4:1 was clean first time.
Ultra-wide generation is not a route to a long period.

**Uniformity is not the same thing as landmark-free.** A first cloud layer asked for masses "all of similar
size and weight" within "a narrow range of size, height and value", reasoning that variety would produce
something memorable. It produced a mechanical, evenly-spaced run instead. What actually makes an element
announce itself when a band repeats is distinctive *value* — being darker or more saturated than its
neighbours. Size, spacing and overlap can vary freely and should, so constrain only the value and let
everything else differ.

Note the period cost, though: a layer drawn with bigger masses has a taller content band, so at a given
on-screen height its tile is *shorter*. Enlarging the clouds took their tile from 1790px to 1410px, and it
came back to 1974px only by drawing them larger on screen too.

**Softness in a keyed layer means low contrast, not blurred edges.** A cloud with a genuinely soft edge keys
into a wide semi-transparent fringe, and with 4:2:0 chroma that fringe carries the backdrop's colour. Ask
instead for loose irregular outlines, a single flat fill with no interior shading, and a value close to the
sky behind it — the boundary stays a clean cel edge for the keyer while the layer reads as soft.

**Period is set by the content band, not the canvas.** A layer's on-screen tile width is

```
source_width x (on-screen height / content height)
```

so what governs period is how *shallow* the drawn band is within its canvas. A band asked to fill "the lower
two thirds of the frame" shrank to 861px on screen and repeated 2.2 times per viewport. Ask for a shallow band
and give it a generous on-screen height; both widen the tile.

**A layer hidden behind another must clear it, or it is wasted.** The far band was placed spanning 0.24–0.48
while the container run spans 0.23–0.66, so every part of it sat behind the containers and it showed only
through the gaps. Raising it to 0.18–0.46 lets its tallest masses rise above the container tops, and the
skyline it was drawn for finally reads. Check the arithmetic against the layer in front before blaming the
artwork: a band can be perfectly good and contribute nothing.

Its content should also be designed for how it is seen. Mostly-occluded means the top-edge silhouette carries
almost all the value, and the rest is read in narrow vertical slivers between the near masses — so the band
must stay interesting at a *fine horizontal scale*, and broad slow forms spanning a quarter of the width are
wasted there.

**Scale the standing band as a near object, and draw it as one.** It shares the ground plane with the units
and is only metres behind them. The container band was first authored at 0.194 of frame height and read as
small and distant; jungle ruins, the reference to match, spans 40–47%. Sizing it up is not enough on its own —
the first band was also *drawn* as a distant one, with "noticeably finer linework and much less interior
detail", and blown up to half the screen it looked thin and underdescribed. A near band wants foreground
weight: outer silhouettes and major overlaps two to three times the weight of interior marks, hardware drawn as
chunky oversized shapes, and a handful of large authored dents rather than fine wear.

**Forbid internal periodicity explicitly.** Asking for a seamless wrap pushes the model to satisfy it the easy
way — by making the whole run periodic, so the same distinctive stack appears two or three times across the
image. That is far worse than an edge seam, because both copies are on screen at once. The instruction that
works says the seam is a property of the two outer edges continuing into one another and must never be
achieved by making the interior repeat, and that no arrangement, silhouette or colour sequence may occur
twice.

---

## Tooling

Four scripts, all offline apart from `gen_art.py`. Each carries its full reasoning in its own docstring.

**`scripts/check_tiling.py`** — measures how well a layer wraps. Two absolute floors sit under the ratios,
below which no ratio is computed, because a ratio is meaningless when both terms are near zero: a smooth image
has almost no horizontal variation, so its baseline collapses and a fraction-of-a-unit join divides out to a
large number. An unclouded sky wash scored 2.57 on a 0.5/255 join; the cloud band scored 2.64 on a 1.9/255 join
that was confirmed invisible by rendering the join and looking at it. The floors differ — 0.6 for `step`, 2.0
for `local` — because a coherent offset held across the whole edge is far easier to see than the same amount of
scattered difference. Every genuine failure in the corpus survives both. It answers one narrow question — do the pixel
columns either side of the join agree — and nothing else. It cannot tell you whether a layer is good, whether
the stack reads as one place, or whether the repeat is obvious; those are judged by looking at it in the game,
and no measurement substitutes for that. What it is for is the defect that survives looking: a seam narrow
enough that it passes casual inspection and then sits on screen for a whole match. It reports three numbers: `local`, the mean absolute difference across the join over the median interior column
difference, which catches structural mismatch; `step`, the same on the *signed* mean, which catches a coherent
tonal shift that texture hides from an absolute mean; and `tilt`, the lateral brightness gradient, reported as
a diagnostic rather than a criterion, because content is allowed to be heavier on one side.

Running it over the shipped set was the first thing it did, and **three of the five campaign backgrounds do not
actually wrap** — all three for the same reason, being lit from the side:

```
                                      local   step    tilt
background_desert_outpost_tiling.png   0.64   1.24    -0.6   wraps
middle_east_ruin_tiling.png            0.63   0.52    +4.2   wraps
background_jungle_ruin_tiling.png      0.69   0.47    -3.6   wraps
background_winter_forest_tiling.png    2.32   6.48    +7.0   seam
background_beach_tiling.png            2.43  14.07    -4.5   seam
background_feldkirchen_tiling.png      3.19  17.29    -4.0   seam
```

Feldkirchen shows why `step` earns its place: its mean column difference across the join is 4.8 out of 255, but
rows 855–971 differ by 140–206, a hard vertical value step down a building's face. Averaged over the frame it
disappears; on screen it is a line.

Two failure modes no edge metric can catch, which is why this stays a hand-run script rather than a build gate:
a *compositional* seam, where matching pixels say nothing about a landmark that arrives every screen — jungle
ruins wraps at 0.69 with its dominant arch straddling the join — and a crossfade hiding a step by concentrating
it into a visible ramp just inside the edge. Both of those are visible to a person and invisible to the metric,
and the reverse is true of the seams it did find, so the two are complementary and neither is sufficient.

**`scripts/make_tileable.py`** — cuts a wrapping strip out of an over-wide generation by finding the window
that already wraps. It scores candidates on *premultiplied* colour, matching what the verdict measures: colour
under a transparent pixel is invisible and, on a keyed layer, is whatever the despill happened to leave there,
so scoring raw RGB spends the search optimising a difference nobody can see. On the cloud band that mattered —
raw scoring reported a cost of 22/255 for a cut whose real wrap was perfect and 2.85 for one that was not. Among candidates that wrap about equally well it takes the *widest*, not the cheapest: a
narrower width has more candidate positions and so wins on cost by sheer number of tries, and width is period.
Optionally fits out the lateral brightness trend first, which rescues a genuine illumination ramp but cannot
rescue a large directional shadow — that is content, and no low-order fit represents it.

**`scripts/cut_layer.py`** — keys a layer off its chroma backdrop. `--report` includes an **ambiguous**
percentage, which is the palette-clash check: artwork drawn in a hue too near the key comes out
semi-transparent in the middle of a solid object, and that is invisible in the source and obvious in game. A
run of container variants measured 0.8–0.9%; the one that had drawn several containers in mauve measured
3.5%, and was discarded on that number alone. The matte is built in the (Cb, Cr) plane so
the decision ignores luminance and an inked contour, dark but nearly neutral, stays far from any saturated key.
Distance is normalised by the key's own chroma length, so thresholds mean "how much of this pixel is backdrop"
rather than an absolute number needing retuning per key. The fringe is then *solved* rather than approximated:
an anti-aliased edge arrives as `art*a + key*(1-a)`, and with `a` estimated and the key known, `art` can be
recovered exactly.

**`scripts/build_layer_set.py`** — the per-biome build. Sizes each layer, appends contact shadows, verifies
wraps, and prints the level's `background_layers` block. It exists because that sizing is arithmetic nobody
should do twice by hand: a contact shadow hangs below the artwork, so both `height_ratio` and `bottom_ratio`
shift by amounts that depend on the shadow depth, and getting one wrong shows up in the game as "that looks a
bit off" rather than as an error.

### The alpha problem, and what it costs

Neither image model emits an alpha channel — `gen_art.py` returns JPEG. So alpha-bearing layers are generated
over flat magenta and keyed. The house style is unusually friendly to this: broad flat local-colour regions,
hard cel edges, dark inked contours, and a narrow per-biome palette that leaves room to pick a key well outside
it. Validated against ground truth by compositing real DEFN sprites over magenta and recovering them: **0.9/255
mean alpha error** on clean input.

Two things degrade that in practice.

**The API returns 4:2:0 JPEG.** Chroma is stored at half resolution, so the backdrop bleeds into every contour
before the file is opened. This is the one place the pipeline fights the file format rather than the model, and
it cannot be trimmed away: eroding the matte took the residual cast from +39 to +25 to +20 per 255 and stalled,
because a 4:2:0 encoder spreads chroma error across a whole subsampled block. `--erode 1` is the default, as
the point where most of the cast is gone and almost none of the contour is. Removing the rest needs the
discarded chroma reconstructed — a joint upsample guided by the full-resolution luma, which JPEG does preserve.
Not implemented; the right step if the residue ever reads in game.

**A contact shadow has almost no margin for error.** The base of a standing band is a long horizontal run, so
*any* consistent darkening along it reads as a drawn rule rather than as shading. The first attempt used depth
0.055 at alpha 0.42 in near-black *and* darkened the artwork's own bottom rows by up to 30%; on screen that was
a 45px dark stripe across the full width, and the first thing anyone noticed. What works is a third of the
depth, alpha under 0.2, a colour sampled from the surface the shadow falls on rather than from black, and the
artwork itself left untouched.

---

## In the engine

- `domain/content/background_layer.h` — `path`, `scroll_scale`, `height_ratio`, `bottom_ratio`, all as
  fractions of viewport height, so each layer's stored resolution is its own business.
- `LevelDefinition::background_layers`, parsed by `LevelLoader`. Endless gets it for free: its synthesised
  level already travels through that same parser.
- `GameBackgroundBuilder::build_stack` — one `Parallax2D` per plane under a single node, tree order as draw
  order, a layer that fails to load skipped rather than failing the whole stack.
- Layers replace `background` outright when present rather than drawing over it.

Cost at 1080p for the port-terminal set: **44.6 MiB of VRAM** across four layers against 23.7 for one current
background, and 12.4 MiB on disk against 8.6. The sky is stored at 1x density because it is a smooth wash with
no hard edges; the container band is the expensive one, at nearly half the screen.

**Known wart.** The layer list lives inline in `data/endless.json`, duplicating geometry that
`art/layer_sets/port_terminal.json` also holds. A shared `data/backgrounds/<biome>.json` that both the loader
and the build script read is the right home as soon as a second level wants the same set. Until then, changing
geometry means editing the manifest, re-running the build, and pasting the printed block.

Level background paths are still not checked by `content_validator` — they never were — but
`check_export_presets.py` does see them.

---

## Why this exists

Two defects, both measured in the code rather than inferred from the art.

**There was no parallax.** `game_background_builder.cpp` set `scroll_scale` to `(1.0, 1.0)` on a single
`Parallax2D` holding a single `Sprite2D`. Sky, distant ridge and combat floor all travelled at exactly camera
speed as one rigid sheet, so depth was painted into the image and then contradicted by the motion.

**The visual period was one screen.** The shipped textures are 3840 x 2160 and `viewport_height` is 1080, so
`display_width` came out at exactly `viewport_width`. The identical frame arrived again every single screen of
scrolling — the shortest period the tiling can have.

Neither is fixable by regenerating a better 3840 x 2160 image, and both dissolve the moment the background
stops being one texture.

The five `background_*_tiling.png` files are to be replaced eventually. They are not inputs to this pipeline
and not style references for it; new sets are drawn from scratch against the house style, as those were.

---

## Acceptance checklist

Everything in `ASSET_PROMPTS.md` still applies. A layer set adds:

- every stratum passes `check_tiling.py` — a pre-filter before the eye, not a substitute for it
- the composite still puts one continuous open floor across the lower half, with the 66%–82.5% foot band the
  flattest part of it
- line weight hierarchy holds **across** layers, not only within one — a `far` stratum drawn alone will almost
  always come back too contrasty
- the stack reads as one illustration at gameplay size, not as collaged planes
- palette coherence survives independent generation of each layer
- the standing band's base sits at or just above the top of the belt band, and its contact shadow does not read
  as a line
- no layer's tile is narrower than the viewport, and no two tiles share a width

---

## Still open

- **Stamps and the scatter planner.** The largest remaining piece, and where the promised variability lives.
- **Ground period.** The ground tile is 2122px against a 1920 viewport, so close to the same frame arrives each
  screen. The 4:1 generation ceiling caps how much better a single strip can get; the real answer is stamps.
  The container band is the better-off one at 2451px, and that came from a shallower drawn band, not a wider
  canvas.
- **Residual key tint.** A few small magenta patches survive inside container faces where the model painted
  magenta-tinted metal — interior artwork, not an edge, so the matte cannot find it. A `--continue` pass on the
  keeper would clear it.
- **The other five biomes**, and retiring the monolithic backgrounds.
