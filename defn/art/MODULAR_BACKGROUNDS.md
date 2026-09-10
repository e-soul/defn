# Modular backgrounds

Gameplay backgrounds built as per-biome **layer sets**: a few seamless horizontal strata composed at load time
into a parallax stack, instead of one monolithic image.

**Status.** Working end to end for three biomes: port-terminal in endless mode, desert-outpost on campaign
Level 1, and jungle-ruin on Level 2. Both campaign sets replace their single backgrounds outright. Levels
3–5 still use their single images and are unchanged. Stamps — scattered props on top of the strata — are
designed but not built.

The desert set was the first one built by following this document rather than writing it, which is the only
useful test of it. Sky, clouds and the standing band came out usable on the first roll; the far band and the
ground each needed one prompt revision and a re-roll, both for reasons now written down below.

[`ASSET_PROMPTS.md`](ASSET_PROMPTS.md) remains the authority on style, palette, and references; everything here
inherits from it. This file covers what is different about authoring a *layer* rather than a scene.

---

## Running the pipeline

Four stages. The first three are per-image and involve picking a keeper; the last is per-biome and mechanical.

**1. Generate.** One prompt per layer, in `art/prompts/<biome>/`. Roll two or three variants.

```bash
python scripts/gen_art.py image --prompt defn/art/prompts/port_terminal/mid.txt --aspect 4:1 --size 4K -n 3 --out build/art/port_terminal --name mid
```

Alpha-bearing layers are drawn over a flat chroma field; opaque ones are not. Magenta is the default, but pick
the key against the biome palette — the desert set used green, because its rock is too close to magenta to key
cleanly. `4:1` is the aspect ceiling. Attach a reference only when the layer genuinely shares subject matter
with it.

**2. Key**, for every layer that needs transparency. Opaque layers skip this.

```bash
python scripts/cut_layer.py build/art/port_terminal/mid_01.jpg --key magenta --no-crop --report --preview build/art/port_terminal/keyed -o build/art/port_terminal/mid_cut.png
```

Look at the checkerboard preview for halos before going further, and at the `ambiguous` percentage: under 1% is
healthy, and anything in the tens means the backdrop was graded rather than flat. Crop off the pixel or two of
pure backdrop the model leaves around the frame before the next step, or the cut degenerates on two empty edge
columns.

**3. Cut a wrapping strip.** Never trust the model's seamlessness; this is what makes it true.

```bash
python scripts/make_tileable.py build/art/port_terminal/mid_cut.png --width 6000 --tolerance 0.30 --no-flatten -o build/art/port_terminal/mid_tileable.png
```

Pass `--no-flatten` for anything already keyed, and for an opaque layer whose raw `check_tiling.py` tilt is
already near zero — flattening a strip with no trend to fit makes it worse, not better. A `SEAM` verdict here
means re-roll, not a longer feather. Measure with `--report --feather 0` first, because the default crossfade
makes the join columns agree by construction and hides exactly the step you are looking for; then roll the
strip by half its width and look at the join.

**4. Build the set.** Sizes every layer, seats the standing bands with a contact shadow, verifies the wraps,
and prints the level data.

```bash
python scripts/build_layer_set.py defn/art/layer_sets/port_terminal.json
```

Paste the printed `background_layers` block into the level's JSON — `data/endless.json` for endless,
`data/levels/level_NN.json` for a campaign level, where it replaces the `background` key rather than joining
it. Then re-import so Godot picks up the changed textures, and relaunch:

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

And the desert-outpost values, which are the same five planes with the subject matter swapped:

| Layer | Alpha | `scroll_scale` | `height_ratio` | `bottom_ratio` | Tile | Content |
|---|---|---|---|---|---|---|
| `sky` | no | 0.05 | 1.000 | 1.000 | 3418 | vertical wash, deep cerulean down to a cream-gold basin haze |
| `clouds` | yes | 0.15 | 0.420 | 0.420 | 2275 | flat-bottomed fair-weather masses, `autoscroll` 5.0 |
| `far` | yes | 0.30 | 0.340 | 0.540 | 2616 | mesas and escarpments across the basin, low contrast |
| `ground` | no | **1.00** | 0.510 | 1.000 | 2219 | a worn dirt road, smooth and quiet, the walkable floor |
| `mid` | yes | **1.00** | 0.528 | 0.708 | 2697 | rock graded by depth: fine scatter near, big outcrops far |

`mid` is 0.50 of screen height before its contact shadow and based at 0.66 — on the top line of the belt band
itself, not just above it. That is what makes the formations read as objects standing right behind the
fighting line rather than as scenery a few metres back, and it is worth more than any amount of drawing
detail: at 0.40 based at 0.65 the same artwork looked like a distant rockery. Sizing it up also separated its
tile from the ground's, which matters because they share a scroll rate of 1.00 and two same-rate layers with
near-equal periods stay in step for screens at a time.

`far` grew to 0.34 to keep up: with the rock band that much taller, a shorter distant band would have shown
only in the gaps. Its top edge is at 0.18 against the rock band's 0.16, so the mesas no longer clear the rock
and are read entirely *between* the masses — which is fine, and is what the gaps are for, but it is a real
change in what that layer is doing and the reason it is worth keeping generous.

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

### Jungle Ruins: Level 2

The third set uses the same five planes, with peach dusk behind cool sage forest, warm ochre limestone ruins,
and a quiet ochre-grey earth road. Its recipe is [`layer_sets/jungle_ruin.json`](layer_sets/jungle_ruin.json).
Tile widths below use the rounded geometry stored in the level JSON, measured in Godot at 1080p.

| Layer | `scroll_scale` | `height_ratio` | `bottom_ratio` | Tile |
|---|---|---|---|---|
| `sky` | 0.05 | 1.000 | 1.000 | 4086 |
| `clouds` | 0.15 | 0.240 | 0.320 | 2911 |
| `far` | 0.30 | 0.400 | 0.570 | 2082 |
| `ground` | **1.00** | 0.460 | 1.000 | 1999 |
| `mid` | **1.00** | 0.512 | 0.662 | 2498 |

Clouds drift at 5 pixels per second. The ruin artwork itself is 0.50 high and based at 0.65; its shallow
contact shadow uses depth 0.025, alpha 0.18 and a floor-derived tint. The bank's common opaque span measures
0.504–0.586, so the floor edge at 0.540 is hidden on 100% of columns. The far layer extends to 0.570,
underlapping the floor. A 122px soldier composite checked scale at the top, middle and bottom of the unchanged
0.66–0.79 belt. Godot review checked the start and a seam-crossing camera position, all five texture loads,
and cloud motion while stationary.

Every shipped layer passes the normal seam threshold. No waiver, flattening or feathering was used. The
manifest records exact source filenames and processing steps; prompts include rejected versions rather than
overwriting the generation history. The old single jungle image remains in the assets repository, but is no
longer selected by Level 2 or either export preset.

Two additions to the prompt lessons came from this set:

* **Sparse clouds need explicit separation.** Both initial cloud rolls repeated a shorter bank internally.
  Six individually described silhouettes, one loose band, empty space between every cloud and empty outer
  margins produced a useful drifting layer. Keep those intentional margins when keying; cropping them away
  would cut into complete clouds rather than remove a stray backdrop border.
* **Plain margins can become painted panels.** Numeric margin requirements produced vertical side panels in
  the sky and floor. The sky's first crop passed the seam metric while retaining a visible interior panel;
  a tighter crop removed it and still wrapped. The floor needed a shorter prompt describing one continuous
  evenly lit surface, without dividing the canvas into percentages. Check the whole strip, not only its join.

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

**The combat floor is the one layer drawn without contours.** Everything else in the house style is built on
clean dark hand-inked outlines, and on the floor they are a liability: every outlined shape becomes a raised
edge, and a floor full of them reads as a relief carving rather than as ground. The desert ground was first
drawn the way the port apron was — cracked clay plates, gravel beds, scattered stones, each with its own
contour — and it was handsome in isolation, tiled perfectly, and was wrong in the game: busy, restless and
embossed, competing with the units standing on it. Redrawn as a worn dirt road with *no contour lines
anywhere*, built only from broad soft areas of slightly different tone with long fades between them and two
suggested wheel-worn bands, it disappears behind the action the way a floor should. State the exception
explicitly in the prompt, and list what it rules out — cracked plates, crazing, gravel, pebbles, tread marks,
ruts with defined edges — because "smooth" on its own is not enough.

Two knock-on effects are worth expecting. A floor this quiet throws all the visual interest onto the standing
band, which is where it belongs but does mean that band has to carry it. And a mass sitting on a smooth pale
floor has nothing to hide an unconvincing base, which is what forced the contact shadow up to 0.42.

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

**Check a standing band's scale by compositing a real unit sprite into the frame.** Nothing else settles it.
Units are drawn at a fixed size — a DEFN soldier is 122px on a 1080 viewport, 11.3% of screen height — so
whether the rock looks right is a measurable question, not a matter of taste, and it is invisible while you are
looking at the layer on its own. Dropped into the frame at true scale, the first desert band turned out to be
three figures tall with chest-high "rubble", which is why the characters read as miniatures in front of a
cliff.

The prompt will not fix absolute size on its own. "A standing soldier would be one seventh of the frame
height", with every object sized against that figure, was ignored by six consecutive rolls: the model draws
rock at whatever size composes well. What it *does* respond to is a demand for a wide range of sizes and for a
stated place in the frame for each of them.

**Grade the band by depth, and the scale problem and the height problem solve each other.** Sizing the whole
band as one near object forces a choice between two bad outcomes: keep it tall and the rock dwarfs the units,
shrink it until the rock is right and the layer becomes a low strip with no parallax to speak of, and its tile
narrows with it — at 0.338 of screen the floor beneath it was down to 1980px of period against a 1920
viewport.

Both go away if height in the frame is treated as distance, which is what it is. Small loose stones along the
bottom edge where the layer meets the road, boulders through the middle, big outcrops along the top, with the
contour lightening, the interior detail thinning and the colour cooling as it recedes. The near stones set the
scale against the unit sprite; the large masses are large *because they are far away and are genuinely large
landforms*, so the band can be half the screen tall without anything looking wrong. The desert band went from
0.338 back to 0.528 on that basis, its tile from 2059 to 2697, and the floor's from 1980 to 2219.

Whatever the height ends up being, re-derive the rest of the stack from it, because what the band covers
changes with it:

* The floor's own far edge is a straight horizontal line, and it is invisible only while the rock covers it.
  Measure the window — the lowest point of the rock's top silhouette and the highest of its feet — and put the
  floor's edge inside it. Here that window is 0.391 to 0.591, so the floor sits at 0.510 with its edge at
  0.490, verified at 0.00% of columns exposed.
* The distant band must then reach *below* the floor's edge, or bare sky shows in the strip between them.
* All of it is arithmetic on measurements, so do it with a script rather than by eye. Every one of these
  numbers was wrong at least once when it was chosen by looking.

**What matters at a standing band's base is the shape of the boundary, not whether the band is continuous.**
This took three attempts to get right and each one taught a different half of it.

The container band works as an edge-to-edge opaque run because a quay full of containers *is* one: it meets
the floor along a real straight line. Desert rock is not, and drawing it that way gave the whole band a shared
sand apron whose bottom edge met the floor along a dead straight horizontal rule across the screen. It read as
a shelf someone had built. The obvious diagnosis — that continuity was the problem — was wrong, and the
redraw that followed from it, a run of **separated masses** with the floor showing between them, fixed the
shelf and introduced something worse: with no opaque body behind them, any mass the model drew high in the
band had its foot high on screen, nothing near underneath it, and hung in mid-air. About five per cent of that
run was floating, and no prompt would confine the feet — "every foot inside the bottom sixth of the frame" was
ignored by every roll of three sweeps. Extending the floor upward to meet them only moved the straight edge up
the screen, where it read as a ledge in its own right.

What works is a **continuous bank with two jagged boundaries**:

* **The body is opaque across the full width.** Nothing can float, because every rock is either standing on
  the floor or resting on rocks that are. This also hands back the floor's far edge — the mass covers it on
  every column, so the floor only has to reach the highest foot, and its own straight top edge is never on
  screen. Both bounds are worth measuring rather than eyeballing: on the desert band the mass spans 0.444 to
  0.558 of screen on *every* column, and the floor's edge has to sit inside that window.
* **The top boundary is a strongly varied silhouette** with deep, frequent dips, because that is the only
  place the distant band is seen. Say so in the prompt, or the dips come out shallow and that layer is wasted.
* **The bottom boundary is irregular and restless** — a scalloped line of rock feet, backdrop coming up
  between neighbouring boulders, no constant height for more than a short stretch. A straight edge is the
  defect; a wandering one made of real silhouettes reads as rocks lying on the ground.

**Make that bottom boundary out of the rocks' own feet, and let it carry its contour.** There is a strong
temptation to fill the lower zone with something — sand, dry earth, a pale apron — so that the band has a
tidy base to sit on. Every version that did read as a shelf, because a filled zone has to end *somewhere* and
wherever it ends is a boundary between two different surfaces. Filling it with the floor layer's own colour
gets closest: sample the floor where the two will meet, put the RGB triple in the prompt, and four rolls land
within about 10/255. But it then needs the inked contour cutting off that boundary in the build, because the
model outlines every boundary it draws — eight rolls across two sweeps outlined it, including four after the
contradicting sentence was removed from the prompt — and the trim has to take the anti-aliased shell as well
as the opaque pixels or it leaves a grey hairline tracing the whole width. Two mechanisms and a colour match,
all to hide a boundary that did not need to exist.

Drawing no fill at all is simpler and better. The lower edge becomes the rounded undersides of boulders, the
bases of shelves and the corners of slabs, with backdrop immediately below each of them, and composited over
the floor the rocks read as resting on the dirt. The contour is then *wanted* along that edge rather than
being a defect — it closes each silhouette exactly as a unit sprite's does, which is what makes an object look
like it is standing on the road instead of a hole cut in a sheet. Nothing needs colour-matching because
nothing but rock touches the floor, and the build-time trim goes away with the zone it existed for.

The two rules that make this safe are the ones already stated: the mass stays opaque above that edge, so no
rock is unsupported, and the feet stay inside a shallow zone so the floor can reach them.

Two corollaries from the failed attempts are worth keeping, because they are true whatever the band's shape
is:

* **No pale footing, pad or sand skirt under a mass.** It was tried, on the reasoning that a small drift
  hugging each foot would stop the object reading as a sticker. It does the opposite: a pale pad is a shape
  like any other, so the model outlines its top edge where it meets the rock and leaves the bottom open where
  it fades out — and a shape outlined along the top and open along the bottom reads as a flat disc the rock is
  standing on.
* **Every stratum's bottom edge must be overlapped by the layer drawn after it.** The `far` band sat at 0.48
  against a floor starting at 0.50, and the 2%-of-screen strip between them had been hidden by the original
  apron the whole time; the moment the band stopped covering it, bare sky showed through as a pale line across
  the full width. Re-check that arithmetic across the whole stack after any change to what a band covers.

The contact shadow changed with all this too. It follows each column's own lowest opaque pixel rather than the
bottom of the canvas, which is what let it work at all when the band was separated masses — a full-width
rectangle paints a bar across the gaps such a layer exists to have — and it degrades to exactly that rectangle
when every column is opaque. Its strength is a function of what it is doing: 0.42 seats free-standing masses
that have nothing else holding them down, 0.20 was right while the band ended in a zone of floor-coloured
ground, and 0.30 suits rock feet resting on the road.

**A sparse layer is materially harder to wrap, and the metric cannot help.** Two transparent columns match
perfectly, so an empty-to-empty join scores 0.00/0.00 while looking wrong: the two empty margins meet as a gap,
and if the model drew a formation straddling the two edges — which it does by default, because that is what
"make it seamless" looks like from the inside — that gap slices a rock in half. Cropping to the alpha bounding
box makes the two halves abut instead, and then the wrap is only as good as the model's own registration: 2.4
and 7.7 per 255 on two rolls, both visible as a step in the strata at 1:1. What works is to forbid the
straddling formation outright and ask for a *wide* empty band at each side — about a twentieth of the width —
so the join lands in open ground and simply reads as one more gap between formations.

That instruction is in the prompt now and the model still would not follow it: eleven rolls across three
sweeps all drew content to both edges. Eleven `--continue` turns asking for the two bands to be cleared did
reach 0.00/0.00 — and each one paid for it somewhere else, damaging a boulder cluster into a blur, washing the
artwork with green, clearing only one of the two edges, or chopping a formation flat against the frame. The
shipped layer is therefore the roll whose two halves *do* meet, cropped by one column at each side so they
abut, with a 2.4/255 residual recorded as an `accepted_seam` waiver in the manifest. Regenerating this layer
is worth doing when a roll finally comes back with the empty bands; nothing else about it needs to change.

**A seam waiver is per-layer and reviewed, never a threshold.** `build_layer_set.py` fails the build on any
`SEAM`, with one exception: a layer may carry an `accepted_seam` block naming the exact `local` and `step` it
is allowed, and the reason. It exists because the ratios divide by how much the image varies column to column,
and a layer of separated masses over transparency barely varies — the desert rock band's interior baseline is
1.08 against the container band's 3.97, so the same absolute join reads nearly four times worse. The absolute
figure is the one that decides, the ratios do not report it, and so the waiver has to state it and the layer
has to have been looked at. Do not raise the global threshold to make a layer pass; every other layer in the
set still fails the build outright.

**Scale the standing band as a near object, and draw it as one.** It shares the ground plane with the units
and is only metres behind them. The container band was first authored at 0.194 of frame height and read as
small and distant; jungle ruins, the reference to match, spans 40–47%. Sizing it up is not enough on its own —
the first band was also *drawn* as a distant one, with "noticeably finer linework and much less interior
detail", and blown up to half the screen it looked thin and underdescribed. A near band wants foreground
weight: outer silhouettes and major overlaps two to three times the weight of interior marks, hardware drawn as
chunky oversized shapes, and a handful of large authored dents rather than fine wear.

**Build the wrap into the composition rather than hoping for it.** This is the single highest-value line in a
layer prompt and it was missing from the first desert set. Asking for seamlessness describes the property you
want; asking for **plain, identical margins** describes how to get it: *the left-most 4% and the right-most 4%
of the frame are the same quiet, evenly toned open ground, same value, same colour, nothing crossing either
boundary, and the two margins interchangeable with one another.* Three ground rolls and three far rolls without
that line all seamed, at `step` 6 to 58 with no window anywhere in the frame that wrapped honestly. The very
next roll of each, with the line added and nothing else changed, wrapped natively — the ground at `local` 1.27
`step` 0.00, the far band at 0.00/0.00 on all three variants. It costs two sentences and it converts the wrap
from something you search for into something the model draws.

Add the value-uniformity half of it for opaque layers: *any tall narrow vertical slice of this image, taken
anywhere across it, must average to the same value and the same colour as any other; there is no vignette, no
corner darkening, no brighter middle, and no drift from one side to the other, however gradual.* A gradual
lateral drift is invisible in the generation and becomes a hard vertical step the moment the two ends meet.

**Put a layout requirement first when it is arithmetic, and say why.** The far band's "occupy roughly the lower
third of the frame" sat two thirds of the way down the first prompt, phrased as composition. All three rolls
filled the canvas edge to edge, which would have made its tile 1296px against a 1920 viewport. Moving it to the
top of the prompt, stating it as thirds — *the upper two thirds are pure flat backdrop and hold no artwork
whatever* — and giving the reason as period rather than taste, fixed it in one roll. The model follows a
constraint it has been told the purpose of; it treats an unexplained proportion as a suggestion.

**A refinement turn changes more than you asked.** `--continue` is still the right tool for a hard edge or a
stray mark, but it is not a colour picker. Asking the keeper mid band to warm its grey sand and *change nothing
else* came back twice: once with the sand warmed, two boulders turned grey, a rock spire added and a patch of
backdrop bleeding into the artwork, and once with the whole composition zoomed. Both were rejected and the
original kept. The narrower the edit, the better the odds — "make this silhouette a hard edge" survives, "keep
everything and recolour one region" does not.

**"Every group must be unique" is not enough for a scattered layer; name the failure.** A band of separated
masses fails uniqueness in a way a continuous run does not, and it fails it the same way every time: the model
draws a shape, then draws a near-copy of it *directly above or below* the first, and does it again for the
neighbouring group. The result reads as a pattern stamped twice rather than as a place, and it is far more
obvious than a repeat across the width because both copies sit in the same glance. What fixes it is naming the
arrangement rather than restating the principle — never place a formation directly above, below or behind
another that resembles it; do not organise them into rows, tiers or a grid; and cap any distinctive motif
explicitly ("the deep undercut or cave-mouth appears at most once in the whole image"), because that is the
shape the model most likes to duplicate. Add a line on spacing too: some masses alone with open ground either
side, some clustered tight, gaps of obviously different widths, never an even rhythm.

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

**Measure a wrap with `--feather 0`, then look at the join.** The default 16-column crossfade blends the two
edge columns into each other, so the join columns agree *exactly* and `make_tileable.py`'s own verdict reads
`local 0.00 step 0.00` on a strip that still has a visible line down it. Every ground window of the first
desert roll reported `ok` that way and every one of them showed a vertical tone step when the strip was rolled
by half its width and looked at. Probe with `--report --feather 0` to get an honest number, and render the join
before believing any of them:

```bash
python scripts/make_tileable.py build/art/<biome>/ground_01.jpg --width 7600 --tolerance 0.1 --feather 0 --report
```

It is also worth running `check_tiling.py` on the **raw generation** before cutting anything. That answers a
question the cut cannot: whether the model's own two edges already meet. When they do — and with the margin
rule above they usually do — the whole frame is the strip, and every narrower window is worse, because there is
no second place in the image where the content happens to match. On the desert set the near-full windows scored
1.2–2.3 raw edge cost against 22–28 for anything narrower.

**`--flatten` can introduce the tilt it exists to remove.** It is on by default for opaque layers and it fits a
low-order polynomial to the lateral brightness trend. When there is no trend to fit, the fit chases content
instead: on a ground roll that wrapped natively at `local` 1.27 with a tilt of +0.3/255, flattening produced a
tilt of −12.5/255 and took the best window to `local` 7.85, `step` 121. Check the raw generation's tilt first
and pass `--no-flatten` when it is already near zero. Flatten is for a strip that measurably ramps, not a
default to leave on.

**Raise `--low` when the backdrop is not quite the key colour.** The named keys are literal — `green` is RGB
0,255,0 — and the model returns something a few units off, `(12, 240, 18)` on one desert roll. Every pixel of
the empty field then sits a little way from the key and keys to alpha 2-5 rather than 0, which is a faint veil
over the whole layer, a `soft edge` reading in the tens of percent, and, on a sparse layer, an interior
baseline collapsed to nearly nothing. Omitting `--key` altogether makes `cut_layer.py` detect the backdrop from
the border, which fixes most of it; nudging `--low` from 0.12 to 0.20 finished the job, taking one layer from
21% soft edge and 27% ambiguous to 0.7% and 0.85% without eating any silhouette. Check the numbers rather than
the picture here: a 2%-alpha veil is invisible in a checkerboard preview and visible over artwork in game.

**Trim a keyed layer's transparent border before cutting it.** The model leaves a pixel or two of pure backdrop
around the frame, which keys to fully transparent columns at both edges. Those columns then match perfectly —
they are both empty — so the search reports a raw edge cost of 0.00, takes the whole frame, and produces a
strip whose real join is wherever the artwork actually ends. Crop to the alpha bounding box in x, and to the
last non-empty row in y, before `make_tileable.py` sees it.

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

**Choose the key against the biome's palette, not by habit.** Magenta is the default because the port
palette — rust, gunmetal, slate, amber, sand — sits on the warm-neutral side of chroma space. The desert does
not: its sandstone runs through rose-brown, clay-red and mauve-grey, which are close enough to magenta that
solid rock would key out semi-transparent in the middle of an object. Both of that set's alpha layers were
therefore generated over **pure green** instead, with the prompt forbidding green, olive, khaki, sage and teal
in the artwork — an easy exclusion in a desert, where nothing is green anyway. `cut_layer.py --key green`
measured 0.24%, 0.30% and 0.47% ambiguous across the keepers, against the 3.5% that condemned a container roll.
The rule generalises: name the biome's dominant hues, pick the named key furthest from them in chroma, and
exclude that key's hue family in the prompt.

The `--report` ambiguous percentage is also how a bad backdrop is caught. One far-band variant came back with a
*graded* green field rather than a flat one; it measured 43% ambiguous and 36% soft edge, and was discarded on
those two numbers without anyone looking at it.

**The API returns 4:2:0 JPEG.** Chroma is stored at half resolution, so the backdrop bleeds into every contour
before the file is opened. This is the one place the pipeline fights the file format rather than the model, and
it cannot be trimmed away: eroding the matte took the residual cast from +39 to +25 to +20 per 255 and stalled,
because a 4:2:0 encoder spreads chroma error across a whole subsampled block. `--erode 1` is the default, as
the point where most of the cast is gone and almost none of the contour is. Removing the rest needs the
discarded chroma reconstructed — a joint upsample guided by the full-resolution luma, which JPEG does preserve.
Not implemented; the right step if the residue ever reads in game.

**A contact shadow follows the artwork, not the canvas.** See the standing-band lesson above: appending a full-width
band below the layer is right only when the layer is opaque edge to edge. `contact_shadow` now starts each
column's fade at that column's own lowest opaque pixel and blurs the field horizontally, so a boulder gets a
pool at its foot, a gap gets nothing, and a mass standing further back gets its shadow higher up the frame. On
a fully opaque band every column's base is the bottom row, so it reduces exactly to the rectangle it replaces
and the port-terminal set is unaffected.

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

The desert set costs **78.4 MiB of VRAM** and 19.5 MiB on disk, against 23.7 and 8.2
for the single image it replaced. Textures import lossless and uncompressed (`compress/mode=0`,
`vram_texture` false), so the stored pixel count is the VRAM cost directly.

The jungle set stores five textures totaling **16.1 MiB on disk**, with **78.3 MiB** of RGBA pixel storage.
It uses the same lossless, non-VRAM-compressed import settings and no mipmaps.

**Known wart.** The layer list lives inline in the level file — `data/endless.json`,
`data/levels/level_01.json` and `data/levels/level_02.json` — duplicating geometry that
`art/layer_sets/<biome>.json` also holds. A shared `data/backgrounds/<biome>.json` that both the loader
and the build script read is the right home as soon as a second level wants the *same* set. Until then,
changing geometry means editing the manifest, re-running the build, and pasting the printed block.

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

The `background_*_tiling.png` files are to be replaced eventually, and two of them now have been:
`background_desert_outpost_tiling.png` and `background_jungle_ruin_tiling.png` no longer ship, since Levels 1
and 2 carry layer sets instead. The desert file stays in the assets repository because `data/lab/tempo_*.json`
still names it, and those fixtures are never packaged; the old jungle image stays as an archive. Three remain.
They are not inputs to this pipeline and not style references for it; new sets are drawn
from scratch against the house style, as those were.

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
- **Ground period.** The ground tile is 2122px in the port set and 2157px in the desert one, against a 1920
  viewport, so close to the same frame arrives each screen. The 4:1 generation ceiling caps how much better a
  single strip can get; the real answer is stamps. The container band is the better-off one at 2451px, and that
  came from a shallower drawn band, not a wider canvas.
- **Residual key tint.** A few small magenta patches survive inside container faces where the model painted
  magenta-tinted metal — interior artwork, not an edge, so the matte cannot find it. A `--continue` pass on the
  keeper would clear it. The desert set has one such pixel across three keyed layers, which is the difference a
  well-chosen key makes.
- **The desert `mid` tile is the narrowest in the set** at 2347px, and it is a sparse layer, so its repeat is
  carried by a handful of distinctive formations rather than by texture. That makes it the layer stamps would
  help most: the scatter planner would let the same rock vocabulary be placed at free x positions instead of
  being baked into a strip that has to wrap.
- **The other three campaign biomes**, and retiring the remaining monolithic backgrounds.
