extends Control

## Pointer drawn into the frame for capture runs.
##
## Movie Maker records the rendered viewport, and the OS cursor is composited by the desktop on top of
## the window, so it never reaches the recording. Under autoplay there is no pointer at all. This draws
## one, in the same canvas space the synthesized events are aimed at, plus the click tell that tells a
## viewer a press happened rather than leaving them to infer it from the consequence.

const RING_LIFETIME := 26
const PRESS_LIFETIME := 7
const CHIP_FLASH := 20
const ARROW_SCALE := 3.0

# Tip at the origin; the notch at index 2/5 is what makes this read as a cursor rather than a triangle.
var _arrow_points := PackedVector2Array(
	[
		Vector2(0.0, 0.0),
		Vector2(0.0, 16.0),
		Vector2(3.5, 12.5),
		Vector2(5.8, 18.0),
		Vector2(8.0, 17.0),
		Vector2(5.7, 11.7),
		Vector2(10.5, 11.5),
	]
)

var point := Vector2(960.0, 540.0)
var chip_enabled := true

var _frame := 0
var _rings: Array[Dictionary] = []
var _press_frame := -1000
var _chip_flash := {MOUSE_BUTTON_LEFT: -1000, MOUSE_BUTTON_RIGHT: -1000}
var _ink := Color(0.021, 0.027, 0.049)
var _paper := Color(0.948, 0.953, 0.972)
var _accent := Color(0.982, 0.811, 0.298)
var _line := Color(0.504, 0.542, 0.696)
var _font: Font = null


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_font = ThemeDB.fallback_font
	_load_palette()


## Keeps the cursor tinted like the rest of the UI, so the overlay does not look bolted on.
func _load_palette() -> void:
	if not FileAccess.file_exists("res://data/ui_theme.json"):
		return
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://data/ui_theme.json"))
	if typeof(parsed) != TYPE_DICTIONARY:
		return
	var palette: Variant = (parsed as Dictionary).get("palette", {})
	if typeof(palette) != TYPE_DICTIONARY:
		return
	_ink = _palette_color(palette, "neutral_ink", _ink)
	_paper = _palette_color(palette, "text_primary", _paper)
	_accent = _palette_color(palette, "accent", _accent)
	_line = _palette_color(palette, "neutral_line_strong", _line)


func _palette_color(palette: Dictionary, key: String, fallback: Color) -> Color:
	var raw: Variant = palette.get(key, null)
	if typeof(raw) != TYPE_ARRAY or (raw as Array).size() < 3:
		return fallback
	var values: Array = raw
	for value in values:
		if typeof(value) != TYPE_FLOAT and typeof(value) != TYPE_INT:
			return fallback
	var alpha: float = float(values[3]) if values.size() > 3 else 1.0
	return Color(float(values[0]), float(values[1]), float(values[2]), alpha)


## Advanced once per captured frame by the runner, so the animation is tied to frames rather than
## wall-clock time -- the whole point of Movie Maker mode is that those two differ.
func tick() -> void:
	_frame += 1
	var live: Array[Dictionary] = []
	for ring in _rings:
		if _frame - int(ring["frame"]) < RING_LIFETIME:
			live.append(ring)
	_rings = live
	queue_redraw()


func click(button: int) -> void:
	_press_frame = _frame
	_chip_flash[button] = _frame
	_rings.append({"frame": _frame, "pos": point, "button": button})
	queue_redraw()


func _ring_color(button: int) -> Color:
	return _accent if button == MOUSE_BUTTON_LEFT else _paper


func _draw() -> void:
	for ring in _rings:
		_draw_ring(ring)
	_draw_arrow()
	if chip_enabled:
		_draw_chip()


func _draw_ring(ring: Dictionary) -> void:
	var age: float = float(_frame - int(ring["frame"]))
	var t: float = clampf(age / float(RING_LIFETIME), 0.0, 1.0)
	var eased: float = 1.0 - pow(1.0 - t, 3.0)
	var radius: float = lerpf(9.0, 52.0, eased)
	var alpha: float = pow(1.0 - t, 1.6)
	var color: Color = _ring_color(int(ring["button"]))
	color.a = alpha
	var center: Vector2 = ring["pos"]

	# A dark companion ring keeps the tell readable over pale sand as well as over the dark HUD pods.
	var shadow := _ink
	shadow.a = alpha * 0.5
	draw_arc(center, radius, 0.0, TAU, 64, shadow, lerpf(7.0, 2.5, t), true)
	draw_arc(center, radius, 0.0, TAU, 64, color, lerpf(5.0, 1.5, t), true)


func _draw_arrow() -> void:
	var press := 0.0
	var age := _frame - _press_frame
	if age >= 0 and age < PRESS_LIFETIME:
		press = 1.0 - float(age) / float(PRESS_LIFETIME)
	var scale := ARROW_SCALE * (1.0 - 0.16 * press)

	var body := PackedVector2Array()
	var shadow := PackedVector2Array()
	for p in _arrow_points:
		body.append(point + p * scale)
		shadow.append(point + p * scale + Vector2(2.5, 3.5))

	_fill_polygon(shadow, Color(_ink.r, _ink.g, _ink.b, 0.38))
	_fill_polygon(body, _paper)

	var outline := body.duplicate()
	outline.append(body[0])
	draw_polyline(outline, _ink, 2.4, true)


## draw_colored_polygon fans from the first vertex, which turns the cursor's notch inside out; the
## triangulation is what keeps the arrow an arrow.
func _fill_polygon(points: PackedVector2Array, color: Color) -> void:
	var indices := Geometry2D.triangulate_polygon(points)
	for i in range(0, indices.size(), 3):
		draw_colored_polygon(PackedVector2Array([points[indices[i]], points[indices[i + 1]], points[indices[i + 2]]]), color)


func _draw_chip() -> void:
	if _font == null:
		return
	var size := get_viewport_rect().size
	var pill := Vector2(84.0, 38.0)
	var origin := Vector2(size.x - 30.0 - pill.x * 2.0 - 10.0, size.y - 30.0 - pill.y)
	_draw_pill(Rect2(origin, pill), "LMB", MOUSE_BUTTON_LEFT)
	_draw_pill(Rect2(origin + Vector2(pill.x + 10.0, 0.0), pill), "RMB", MOUSE_BUTTON_RIGHT)


func _draw_pill(rect: Rect2, label: String, button: int) -> void:
	var age := _frame - int(_chip_flash[button])
	var lit: float = clampf(1.0 - float(age) / float(CHIP_FLASH), 0.0, 1.0) if age >= 0 else 0.0

	var box := StyleBoxFlat.new()
	box.bg_color = Color(_ink.r, _ink.g, _ink.b, 0.55).lerp(_ring_color(button), lit * 0.85)
	box.border_color = _line.lerp(_ring_color(button), lit)
	box.set_border_width_all(2)
	box.set_corner_radius_all(10)
	draw_style_box(box, rect)

	var text_color := _paper.lerp(_ink, lit)
	var font_size := 20
	var text_height := _font.get_height(font_size)
	var baseline := rect.position.y + (rect.size.y + text_height) * 0.5 - _font.get_descent(font_size)
	draw_string(_font, Vector2(rect.position.x, baseline), label, HORIZONTAL_ALIGNMENT_CENTER, rect.size.x, font_size, text_color)
