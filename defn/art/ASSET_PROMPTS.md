# DEFN asset prompts

Every prompt that produced DEFN artwork or music, as files you can feed straight to `scripts/gen_art.py`, plus
the art direction they encode.

Two rules:

1. **`prompts/` is an archive.** Those files produced what ships. They are kept exactly as they were sent,
   typos and all, so a regeneration can be reproduced or diffed. Do not edit them; write a new file instead.
2. **`templates/` and `style/` are live.** New work should use them, and they can change, but the result must
   land inside the house style below.

New backgrounds are no longer authored as single images. [`MODULAR_BACKGROUNDS.md`](MODULAR_BACKGROUNDS.md)
describes the layer-set pipeline that replaces them; the style, palette, and reference rules below still
govern every layer it produces.

## Running them

```bash
python scripts/gen_art.py image --prompt defn/art/prompts/background_jungle_ruins.txt --ref defn/assets/backgrounds/background_beach_tiling.png -n 4
```

The script attaches the references, rolls `-n` variants, and writes a `.json` sidecar beside each result with
the prompt, the references, and an interaction id you can pass back with `--continue` to edit a keeper instead
of re-rolling it. `--dry-run` shows the request without spending anything. See its `--help` for the rest.

Aspect ratio and resolution are request fields the script sets, not prose. The
`ultra-high resolution, crisp 4K fidelity graphics` wording the older prompts carry is doing nothing; leave it
in the archived files, but drop it from new ones.

Where a prompt had separate "negative guidance" pasted alongside it, the two are joined in one file. This API
has no negative-prompt field, so what was sent alongside was only ever more prompt.

## The prompts

| File | Produced | References to attach |
|---|---|---|
| `prompts/background_desert_outpost.txt` | `background_desert_outpost_tiling.png`, Level 1 | `campaign/map_background_v2.jpg` (desert region only), one strong shipped background, one Spec Ops and one Guerrilla sprite |
| `prompts/background_jungle_ruins.txt` | `background_jungle_ruin_tiling.png`, Level 2 | 1 background, 2 characters |
| `prompts/background_summar_beach.txt` | `background_beach_tiling.png`, Level 3 | 2 backgrounds, 2 characters |
| `prompts/background_winter_forest.txt` | `background_winter_forest_tiling.png`, Level 4 | 2 backgrounds, 2 characters |
| `prompts/background_feldkirchen.txt` | `background_feldkirchen_tiling.png`, Level 5 and Endless | 3 backgrounds, 2 characters; one reference was a photograph of the real town, for architecture only |
| `prompts/campaign_map.txt` | `campaign/map_background_v2.jpg` | all five shipped gameplay backgrounds, as style and biome references only |
| `prompts/music_battle_theme.txt` | `music/theme01..04.mp3` | none; the four themes are re-rolls of this one brief |

Three carry a caveat worth reading before you regenerate:

- **Winter Forest** asks for `Saturated beach gold` and highlights `filtering through the canopy`, copied from
  the beach prompt. The result survived it. Replace that line with the Level 4 palette row below.
- **Campaign map** has drifted from `defn/data/campaign_map.json`: Jungle is now 24% / 80%, Feldkirchen
  66% / 77%, and an Endless node sits at 68.5% / 50%. Its `Winter Forst` is now `The Winter Forest`.
- **Desert Outpost** is the natural variant. The urban one was written, generated, and rejected; it is in
  `prompts/retired/desert_outpost_urban.txt` and would suit a future urban level.

### Retired

Kept for history. None of these produce anything in the game today.

| File | Why it is here |
|---|---|
| `prompts/retired/desert_outpost_urban.txt` | The urban Level 1 that lost to the natural variant |
| `prompts/retired/menu_background.txt` | Made `menu_background.png`, since removed; the backdrop is drawn in `src/adapters/godot/ui/menu_backdrop.cpp` from `data/ui_theme.json` |
| `prompts/retired/tiling_template_v1.txt` | The bulleted jungle prompt that seeded every later background, superseded by `templates/background_tiling.txt` |
| `prompts/retired/music_track_template_v1.txt` | The first variable-driven music brief, superseded by `templates/music_track.txt` |

## Templates and style statements

`templates/` holds fill-in-the-blank starting points: `background_tiling.txt`, `music_track.txt`, and
`image_skeleton.txt`, which is the section order to assemble a new image prompt in so content instructions do
not dilute the style constraints.

`style/` holds the copy-ready statements. Pick **exactly one** and put it at the top of a new prompt. Do not
concatenate several — their proportion, line-weight, camera, and detail rules differ, and mixing them weakens
the result.

| File | For |
|---|---|
| `style/characters.txt` | Characters and enemy sprites |
| `style/characters_frames.txt` | Appended to the above for an animation sequence |
| `style/props.txt` | Weapons, equipment, pickups, small props |
| `style/structures.txt` | Structures, defenses, large props |
| `style/environments.txt` | Gameplay environments and tiling backgrounds |
| `style/scene_illustrations.txt` | Mission previews, campaign art, other scene illustrations |
| `style/icons.txt` | Icons and portraits |

Two of these describe work that does not exist yet. No DEFN character has ever been generated — every unit
uses the gameart2d Soldier bundle — and the shipped `assets/ui/` glyphs are hand-authored SVG in a flat
geometric style, deliberately outside the illustration language. Both statements are targets, not descriptions.

**Defaults for a normal battle track:** standard battlefield mission; concentrated tactical pressure with a
growing sense of momentum; medium intensity; 2–3 minutes; 112–122 BPM; electronic drums, low synthesizer bass,
restrained snare and tom percussion, sparse synth melody, subtle distorted guitar textures; a short low synth
pulse and minimal percussion that immediately establishes forward motion; looping ending.

---

## The house style

Use this as the anchor at the top of any new image prompt, then specialize it with a `style/` statement.

> DEFN is a contemporary-to-near-future military belt-scroller drawn as authored 2D cartoon illustration:
> clean dark hand-inked contours, crisp graphic silhouettes, simplified chunky geometry, broad flat
> local-color regions, restrained two- or three-value cel shading, sparse purposeful texture, and gently
> irregular hand-drawn edges. Depth comes from overlap, relative scale, value grouping, warm-versus-cool
> separation, and atmospheric desaturation — never from lens behavior.

Words do not replace references. Attach the most relevant existing DEFN assets and treat them as the final
authority for proportion, contour weight, palette, rendering, and level of detail. A composition reference may
define content or camera only; it must never override the DEFN rendering language.

### A note on "vector art"

The June 2025 prompts asked for *"clean 2D vector art, sharp digital illustration"*. That wording is
superseded. What the set converged on — and what the strongest members (Jungle Ruins, The Winter Forest,
Desert Outpost, the campaign map) look like — is **hand-inked cartoon illustration** with slightly irregular
contours and a faint tooth to the fills. Summar Beach is the flattest, most literally vector-like member and
is the outlier, not the target. New prompts should ask for the hand-inked language above.

### Shape and silhouette

- Design from the silhouette inward. A character role, weapon class, prop function, building type, or terrain
  form should stay recognizable when reduced to a flat shape.
- Prefer a few large interlocking masses over many small parts.
- Use slightly chunky thickness and mild exaggeration for readability. Avoid needle-thin barrels, fragile
  railings, hairline cables, anatomically narrow joints, or tiny architectural trim except as a sparse accent.
- Keep silhouettes asymmetrical enough to feel designed, while retaining an easy-to-read center of mass.

### Line language

- Clean charcoal-to-near-black outlines with gently varied, rounded or tapered turns, as if inked by hand.
  Not mechanical CAD lines, not sketchy repeated strokes.
- On character sprites the outer contour and major overlaps are roughly two to three times more prominent than
  small interior marks. Equipment layers stay outlined even when their colors differ.
- On props and the tower, strong outer and structural contours with thinner seams, bolts, cracks, and glass
  highlights.
- On backgrounds, scale line weight to depth: clearest and darkest in foreground and midground, thinner and
  less contrasty in distant planes. Do not give every leaf, brick, or mountain a character's sticker outline.
- Every interior line should clarify volume, material, damage, articulation, or overlap.

### Color and shading

- Establish a dominant biome or faction palette, then limit accent hues. Large color regions read before local
  detail.
- Base color plus one shadow tone, and when useful one highlight tone. Hard-edged cel shapes may soften only in
  sky, distant haze, glass, visor reflections, and restrained atmospheric transitions.
- Tint shadows toward the scene palette rather than indiscriminate black. Keep outlines darker than adjacent
  shadows.
- Place small saturated accents against larger muted fields: bright eyes, hazard stripes, insignia-sized color
  patches, ammunition, indicator lights, warm windows.
- Glossy white or pale-blue highlights only on eyes, eyewear, visors, windows, polished optics, and glass.

### Detail and material

- Material comes from shape language: concrete uses broad panels, seams, a few chips and cracks; metal uses
  plates, dark joints, narrow highlight strips; fabric a few angular folds; glass flat blue-gray or pale values
  with diagonal highlight bands; foliage grouped leaf silhouettes and selective veins; snow broad cap shapes
  with pale blue undersides.
- Keep distress authored and sparse. A handful of large readable damage marks; no procedural grunge, texture
  overlays, pores, sand grains, fine scratches, or uniform edge wear.
- Avoid dense repeated detail. A row of windows, stones, boards, leaves, or panels should vary slightly and
  merge into larger rhythm groups.

### Mood and subject

- A readable action cartoon for a military game: capable, lively, approachable, with serious tactical stakes
  but no oppressive realism.
- Technology is contemporary-to-near-future and purpose-driven. A viewer should understand what a device does
  from its main components.
- Cultural, geographic, and faction identity comes from architecture, vegetation, climate, clothing layers,
  equipment configuration, and controlled palette — not caricature or decorative overload.
- Violence may be implied through poses, damage, smoke, and tactical context; gore and horror are outside the
  style.

## Palette by asset

Relationships, not fixed swatches. Match the supplied reference for the chosen biome.

| Asset | Dominant fields | Shadow / outline tendency | Controlled accents |
|---|---|---|---|
| Level 1 — Desert Outpost | warm amber and ochre sand, coral-orange soil, mauve-rose sandstone ledges, gray weathered asphalt | dark plum, brown-black, smoky maroon | pale gold sky, sunlit rock faces, olive scrub tufts |
| Level 2 — Jungle Ruins | sunset peach sky, ochre limestone, sage and olive foliage, deep blue-green shadow masses | charcoal green, deep teal | pale yellow backlight behind the ruins, warm stone faces, restrained moss |
| Level 3 — Summar Beach | cyan sky, turquoise-to-deep-blue water, warm golden sand, olive dune grass | dark teal, deep green, warm charcoal | white surf bands and cloud shapes, small shell and prop accents |
| Level 4 — The Winter Forest | ice-blue and slate mountains, snow white, dark pine, ochre-gold grass | deep blue-green, cool charcoal, muted violet | pink-to-amber dusk sky, pale blue snow shade |
| Level 5 — Feldkirchen | pale blue sky, cream and pastel facades, terracotta roofs, soft green hills, dust-gray square | warm charcoal, muted brown, slate | sunlit yellow church walls, red roofs, warm windows |
| Campaign map | all five biome palettes in one frame, along an S-flow | per biome | storm-gray cloud mass over the snow region |
| Military structures (`tower.png`) | warm off-white concrete, olive-drab, gunmetal, blue-gray glass | charcoal, deep olive-gray | hazard yellow-and-black stripe, olive flag with a yellow star, lamp housing |

## References

Paths relative to the repository root.

| Purpose | Path |
|---|---|
| Characters (friendly) | `defn/assets/Spec_Ops_-_Game_Sprites/png/Soldier1..4` |
| Characters (hostile) | `defn/assets/The_Guerrila_-_Game_Sprites/png/Soldier1..4` |
| Characters (the hound) | `defn/assets/The_Mercenaries_-_Game_Sprites/png/Soldier4` |
| Structure | `defn/assets/tower.png`, `defn/assets/tower_destroyed.png` |
| Gameplay backgrounds | `defn/assets/backgrounds/background_*_tiling.png` |
| Campaign map | `defn/assets/campaign/map_background_v2.jpg` |
| Mission previews (960 × 540) | `defn/assets/campaign/*_preview.jpg` |
| Music | `defn/assets/music/theme01..04.mp3` |
| Pack the characters come from | [The Soldier Game Sprite Bundle](https://www.gameart2d.com/soldier_game_sprite_bundle.html) |

Not references:

- `defn/assets/backgrounds/middle_east_ruin_tiling.png` and `middle_east_ruins.png` are the retired Level 1
  background and its source art. Keep them as a record of what was replaced; do not feed them to a generation
  as a quality or style reference.
- `defn/assets/ui/**/*.svg` are hand-authored vector glyphs, outside the illustration style.
- There is no menu background image; the backdrop is procedural.

### Protocol

1. **Pick the primary reference by asset class.** Character sprites for characters, `tower.png` for standalone
   military construction, the closest shipped background for environments.
2. **Add one cross-class reference for cohesion.** A character with the tower aligns military color and
   material language; a tower with a background aligns contour density and lighting.
3. **Use composition references only for composition.** Tell the model explicitly to ignore their lighting,
   material treatment, camera optics, line quality, texture, and realism.
4. **Name each reference and what it governs** in the prompt itself. A generation usually carries three
   references pulling in different directions, and the relationship instruction is what resolves them.
5. **Do not average conflicting proportions.** Character references govern character anatomy; environment
   references govern environmental scale, depth, and contour density.
6. **Keep the count low.** The service accepts far more, but every extra reference dilutes the rest. DEFN has
   never needed more than four.
7. **Generate without text or UI.** Labels, icons, mission markers, and borders are added in Godot.

### Runtime numbers

Check these rather than trusting an archived prompt; the numbers in those were correct when they were sent.

- `defn/data/campaign_map.json` — mission anchor positions, previews, dossier content.
- `defn/data/levels/level_0*.json` — each level's background path and `belt_width` band. Unit feet land between
  **66% and 82.5%** of image height (Jungle Ruins 66%–79%). The whole lower half must read as walkable, but
  that narrower band is where sprites actually stand, so keep it the flattest part of the floor.
- `defn/data/music_playlist.json` — track list, volume, gap between tracks.

## Prompting the models

Both are Google models: **Nano Banana 2** for images, **Lyria 3.5** for music.

- **Write a scene, not a keyword list.** Specificity buys control, and the models respond to narrative
  description. The archived prompts are the right length and register.
- **Prefer positive framing.** Say `one continuous, visually open combat floor`, not `no obstacles`; `every
  plane in sharp focus`, not `no depth of field`. A residual exclusion list still helps — the archived prompts
  show it working — but put it last and keep it to real failure modes for that subject.
- **Iterate instead of re-rolling.** `--continue` edits the image you already have. State what must stay
  identical, not only what changes.
- **Music needs numbers and the word "instrumental".** There is no vocals-off switch. BPM, key, and mood
  adjectives measurably help; `[Intro]` / `[Build]` / `[Peak]` / `[Release]` / `[Outro]` tags give structure.
- **Lyria has no editing turn.** Every attempt is a fresh roll and results vary between identical calls, so the
  prompt must be self-contained. The four shipped themes came out of exactly that process.
- **No temperature.** The interactions API exposes no temperature, top-p, or top-k. Variation comes from `-n`.

## Acceptance checklist

- At thumbnail and gameplay size, the subject and its function are immediately legible.
- It reads as authored 2D cartoon illustration, never photography, realistic concept art, 3D/PBR, or an
  outline-filtered render.
- Silhouette, large color masses, and value grouping work before small details are visible.
- Exterior, structural, interior, and distant line weights form a clear hierarchy.
- Most forms use a flat base color plus one shadow and at most one restrained highlight family.
- Detail density suits the class: boldest and simplest for sprites and icons, intermediate for props, finer and
  depth-scaled for environments.
- Characters keep the compact adult proportion system and do not drift toward children, realistic anatomy, or
  generic anime anatomy.
- Equipment and structures have recognizable practical functions without excessive mechanisms or greebling.
- Materials come from designed shapes, not photographic texture or procedural noise.
- Palette is limited and harmonized, with saturation and contrast reserved for focal information.
- For a background: the whole lower half reads as open walkable ground, the 66%–82.5% foot band is the flattest
  part of it, and the edges wrap without a visible seam when repeated four times.
- No accidental text, logo-like glyphs, watermarks, borders, frames, duplicate anatomy, malformed weapons, or
  baked UI.
- When references disagree, the correct DEFN asset class stays authoritative.
