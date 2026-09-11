extends SceneTree

## Plays a scripted shot against the real game so it can be recorded or photographed.
##
## Two things make this work. Nothing in the game polls the mouse -- every click path reads the position
## off the event itself -- so synthesized events drive the genuine article: card hover and press states,
## unit hover, the selection ring, the destination marker. And Movie Maker mode pins the frame delta to
## exactly 1/fps, so the shot is scheduled in frames rather than seconds; under a plain run the loop is
## uncapped (~3000 fps here) and anything timed in wall-clock is meaningless.
##
##   godot --path defn --write-movie <abs>.avi --fixed-fps 60 --script tools/capture_runner.gd -- \
##       --shot res://tools/shots/level_03_opening.json
##
## Add --recon to skip recording and print a per-second table of where the fight actually is; that is
## how the timings in a shot file get chosen.

const CURSOR_SCRIPT := "res://tools/capture_cursor.gd"
const BOOT_FRAMES := 20
const DEFAULT_TRAVEL := 0.34
const AFFORD_WAIT_SECONDS := 12.0

var _fps := 60
var _shot: Dictionary = {}
var _shot_path := ""
var _stills_dir := ""
var _recon := false
var _recon_seconds := 60
var _cursor_enabled := true
var _level_override := ""

var _frame := 0
var _cursor: Control = null
var _cover: ColorRect = null
var _hud: Node = null
var _entities: Node = null
var _camera: Camera2D = null
var _cards: Dictionary = {}

var _travel_from := Vector2.ZERO
var _travel_start := 0
var _travel_end := 0
var _travel_spec: Dictionary = {}
var _point := Vector2(1600.0, 900.0)


func _initialize() -> void:
	call_deferred("_run")


# ---------------------------------------------------------------- arguments


func _parse_args() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index := 0
	while index < args.size():
		var flag: String = args[index]
		var value: String = args[index + 1] if index + 1 < args.size() else ""
		match flag:
			"--shot":
				_shot_path = value
				index += 2
			"--stills-dir":
				_stills_dir = value
				index += 2
			"--fps":
				_fps = int(value)
				index += 2
			"--level":
				_level_override = value
				index += 2
			"--recon":
				_recon = true
				index += 1
			"--recon-seconds":
				_recon_seconds = int(value)
				index += 2
			"--no-cursor":
				_cursor_enabled = false
				index += 1
			_:
				index += 1


func _load_shot() -> bool:
	if _shot_path == "":
		return _recon
	if not FileAccess.file_exists(_shot_path):
		printerr("[capture] shot file not found: ", _shot_path)
		return false
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(_shot_path))
	if typeof(parsed) != TYPE_DICTIONARY:
		printerr("[capture] shot file is not a JSON object: ", _shot_path)
		return false
	_shot = parsed
	_fps = int(_shot.get("fps", _fps))
	return true


# ---------------------------------------------------------------- boot


func _frames(count: int) -> void:
	for i in range(count):
		await process_frame


func _boot() -> bool:
	Input.use_accumulated_input = false
	Input.set_mouse_mode(Input.MOUSE_MODE_HIDDEN)
	seed(int(_shot.get("seed", 20260911)))

	# Movie Maker starts recording at process start, so the half-built menu and the scene swap land in
	# the first second of every take. The cover hides the boot and then fades up into the shot.
	_build_cover()

	# The menu scene is what builds CampaignService; jumping straight to the game scene leaves the
	# level selection with nowhere to land.
	change_scene_to_file("res://scenes/menu.tscn")
	await _frames(BOOT_FRAMES)
	if not Engine.has_singleton("CampaignService"):
		printerr("[capture] CampaignService missing after the menu scene loaded")
		return false

	# --level lets a recon pass name a level before any shot file exists for it, and lets one shot's
	# choreography be tried on another level.
	var level: String = _level_override if _level_override != "" else str(_shot.get("level", "level_03"))
	var campaign: Object = Engine.get_singleton("CampaignService")
	campaign.call("set_current_level_id", level)
	var selected: String = str(campaign.call("get_current_level_id"))
	if selected != level:
		# A locked or misspelled id is refused and the previous selection silently stands, which would
		# otherwise be discovered only when the footage shows the wrong beach.
		printerr("[capture] level %s was refused; current selection is %s" % [level, selected])
		return false

	change_scene_to_file("res://scenes/game.tscn")
	await _frames(BOOT_FRAMES)

	_hud = root.find_child("HUD", true, false)
	_entities = root.find_child("EntityContainer", true, false)
	_camera = root.find_child("Camera", true, false)
	if _hud == null or _entities == null:
		printerr("[capture] game scene did not build")
		return false

	_index_cards()
	if _cursor_enabled:
		_build_cursor()
	return true


func _index_cards() -> void:
	_cards.clear()
	for child in _hud.get_children():
		if not (child is HBoxContainer):
			continue
		for card in child.get_children():
			if card is Button:
				_cards[_card_unit_id(card)] = card
		if not _cards.is_empty():
			return


## The HUD keeps unit ids on the C++ side only, so the card is matched by its title -- "Breacher" is
## the card for `breacher`.
func _card_unit_id(card: Button) -> String:
	for node in card.find_children("*", "Label", true, false):
		var text: String = (node as Label).text
		if text != "":
			return text.to_lower()
	return ""


func _build_cursor() -> void:
	var layer := CanvasLayer.new()
	layer.layer = 128
	layer.name = "CaptureCursorLayer"
	root.add_child(layer)
	_cursor = load(CURSOR_SCRIPT).new()
	_cursor.name = "CaptureCursor"
	layer.add_child(_cursor)
	var cursor_config: Dictionary = _shot.get("cursor", {})
	_cursor.chip_enabled = bool(cursor_config.get("input_chip", true))
	var start: Variant = cursor_config.get("start", null)
	if typeof(start) == TYPE_ARRAY and (start as Array).size() == 2:
		_point = Vector2(float(start[0]), float(start[1]))
	_cursor.point = _point


func _build_cover() -> void:
	var layer := CanvasLayer.new()
	layer.layer = 200
	layer.name = "CaptureCoverLayer"
	root.add_child(layer)
	_cover = ColorRect.new()
	_cover.name = "CaptureCover"
	_cover.color = Color(0.0, 0.0, 0.0, 1.0)
	_cover.set_anchors_preset(Control.PRESET_FULL_RECT)
	_cover.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(_cover)


func _set_cover_alpha(alpha: float) -> void:
	if _cover != null:
		_cover.color = Color(0.0, 0.0, 0.0, clampf(alpha, 0.0, 1.0))


# ---------------------------------------------------------------- input synthesis


## Events are aimed in window space: the root viewport here is the physical window (4K on a HiDPI
## display) while Control rects are in the 1920x1080 canvas, and push_input undoes the stretch. Feeding
## canvas coordinates straight in lands the click at half the intended position, which silently hits
## nothing at all.
func _to_window(canvas_point: Vector2) -> Vector2:
	return root.get_screen_transform() * canvas_point


func _send_motion(canvas_point: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	var window_point := _to_window(canvas_point)
	event.position = window_point
	event.global_position = window_point
	Input.parse_input_event(event)


func _send_click(canvas_point: Vector2, button: int) -> void:
	var window_point := _to_window(canvas_point)
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.position = window_point
		event.global_position = window_point
		event.button_index = button
		event.pressed = pressed
		Input.parse_input_event(event)
	if _cursor != null:
		_cursor.click(button)


# ---------------------------------------------------------------- target resolution


func _units(group: String) -> Array:
	var live: Array = []
	for unit in get_nodes_in_group(group):
		if is_instance_valid(unit) and not unit.is_queued_for_deletion():
			live.append(unit)
	return live


func _canvas_position(node: Node2D) -> Vector2:
	return node.get_global_transform_with_canvas().origin


## Canvas point for whatever the action is aimed at, re-resolved every frame of the travel so the
## cursor tracks a walking unit the way a hand would.
func _resolve(spec: Dictionary) -> Vector2:
	if spec.has("canvas"):
		var raw: Array = spec["canvas"]
		return Vector2(float(raw[0]), float(raw[1]))

	if spec.has("card"):
		var unit_id: String = str(spec["card"])
		if not _cards.has(unit_id):
			printerr("[capture] no deploy card for ", unit_id, " (have: ", _cards.keys(), ")")
			return _point
		return (_cards[unit_id] as Button).get_global_rect().get_center()

	if spec.has("friendly"):
		var unit := _pick_unit("friendlies", str(spec["friendly"]))
		if unit == null:
			return _point
		# Aimed at the torso rather than the origin at the feet, which is where a player would click.
		return _canvas_position(unit) + Vector2(0.0, float(spec.get("offset_y", -34.0)))

	if spec.has("ground_ahead"):
		var anchor := _pick_unit("friendlies", str(spec.get("from", "rightmost")))
		var base_point: Vector2 = _canvas_position(anchor) if anchor != null else _point
		var ahead := base_point + Vector2(float(spec["ground_ahead"]), float(spec.get("offset_y", -10.0)))
		var bounds := root.get_visible_rect().size
		return Vector2(clampf(ahead.x, 90.0, bounds.x - 90.0), clampf(ahead.y, 120.0, bounds.y - 190.0))

	printerr("[capture] unrecognised target: ", spec)
	return _point


func _pick_unit(group: String, which: String) -> Node2D:
	var units := _units(group)
	if units.is_empty():
		return null
	match which:
		"newest":
			return units[units.size() - 1]
		"oldest":
			return units[0]
		"rightmost":
			var best: Node2D = units[0]
			for unit in units:
				if (unit as Node2D).global_position.x > best.global_position.x:
					best = unit
			return best
		"leftmost":
			var best_left: Node2D = units[0]
			for unit in units:
				if (unit as Node2D).global_position.x < best_left.global_position.x:
					best_left = unit
			return best_left
	return units[units.size() - 1]


# ---------------------------------------------------------------- the shot


func _compile_events() -> Array[Dictionary]:
	var events: Array[Dictionary] = []
	var timeline: Array = _shot.get("timeline", [])
	for raw in timeline:
		var entry: Dictionary = raw
		var fire := int(round(float(entry.get("at", 0.0)) * _fps))
		var travel := int(round(float(entry.get("travel", DEFAULT_TRAVEL)) * _fps))
		events.append(
			{
				"fire": fire,
				"travel_start": maxi(0, fire - travel),
				"action": str(entry.get("action", "hold")),
				"travelling": false,
				"entry": entry,
			}
		)
	events.sort_custom(func(a, b): return int(a["travel_start"]) < int(b["travel_start"]))
	return events


func _target_spec(entry: Dictionary) -> Dictionary:
	match str(entry.get("action", "")):
		"deploy":
			return {"card": str(entry.get("unit", ""))}
		"select":
			return {"friendly": str(entry.get("target", "rightmost"))}
		"move", "deselect":
			return {"ground_ahead": float(entry.get("ahead", 300.0)), "from": str(entry.get("from", "rightmost"))}
		"park":
			return {"canvas": entry.get("canvas", [1600.0, 900.0])}
	return {}


func _begin_travel(spec: Dictionary, start_frame: int, end_frame: int) -> void:
	_travel_spec = spec
	_travel_from = _point
	_travel_start = start_frame
	_travel_end = maxi(end_frame, start_frame + 1)


func _advance_cursor() -> void:
	if not _travel_spec.is_empty():
		var span := float(_travel_end - _travel_start)
		var t: float = clampf(float(_frame - _travel_start) / span, 0.0, 1.0)
		var eased: float = 1.0 - pow(1.0 - t, 3.0)
		_point = _travel_from.lerp(_resolve(_travel_spec), eased)
	if _cursor != null:
		_cursor.point = _point
		_cursor.tick()
	_send_motion(_point)


func _card_ready(unit_id: String) -> bool:
	if not _cards.has(unit_id):
		return false
	return not (_cards[unit_id] as Button).disabled


func _save_still(label: String) -> void:
	if _stills_dir == "":
		return
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	var name: String = str(_shot.get("name", "shot"))
	var path := "%s/%s_%s.png" % [_stills_dir.rstrip("/"), name, label]
	var error := image.save_png(path)
	if error != OK:
		printerr("[capture] could not write still ", path, " (", error, ")")
	else:
		print("[capture] still %s (%dx%d)" % [path, image.get_width(), image.get_height()])


func _play() -> void:
	var events := _compile_events()
	var total := int(round(float(_shot.get("duration_seconds", 30.0)) * _fps))
	var index := 0
	var afford_deadline := 0

	var fade_in := int(round(float(_shot.get("fade_in", 0.5)) * _fps))
	var fade_out := int(round(float(_shot.get("fade_out", 0.6)) * _fps))

	print("[capture] playing %s: %d event(s) over %d frames @ %d fps" % [str(_shot.get("name", "shot")), events.size(), total, _fps])
	# Recording time is shot time plus the boot the cover is hiding; a trim or a --gif span is measured
	# from the head of the file, not from here.
	print("[capture] shot starts at recorded frame %d (%.2fs)" % [Engine.get_frames_drawn(), Engine.get_frames_drawn() / float(_fps)])

	while _frame < total:
		while index < events.size():
			var event: Dictionary = events[index]
			var entry: Dictionary = event["entry"]
			var action: String = event["action"]

			if _frame < int(event["travel_start"]):
				break

			# A deploy fired against a card the energy cannot pay for is a click that does nothing and
			# reads as a bug in the footage. The wait happens before the cursor sets off, so it idles
			# wherever it was rather than loitering on a dimmed card -- and the approach then lands just
			# as the card lights, which is the read a player would recognise.
			if not bool(event["travelling"]):
				afford_deadline = int(event["fire"]) + int(AFFORD_WAIT_SECONDS * _fps)
				if action == "deploy" and not _card_ready(str(entry.get("unit", ""))):
					if _frame < afford_deadline:
						break
					printerr("[capture] %s never became affordable; skipping" % str(entry.get("unit", "")))
					index += 1
					continue
				var spec := _target_spec(entry)
				if not spec.is_empty():
					var travel_frames := int(event["fire"]) - int(event["travel_start"])
					event["fire"] = maxi(int(event["fire"]), _frame + travel_frames)
					_begin_travel(spec, _frame, int(event["fire"]))
				event["travelling"] = true

			if _frame < int(event["fire"]):
				break

			match action:
				"deploy", "select", "move":
					_send_click(_point, MOUSE_BUTTON_LEFT)
					print("[capture] %6.2fs %s %s" % [_frame / float(_fps), action, str(entry.get("unit", entry.get("target", entry.get("ahead", ""))))])
				"deselect":
					_send_click(_point, MOUSE_BUTTON_RIGHT)
					print("[capture] %6.2fs deselect" % (_frame / float(_fps)))
				"still":
					await _save_still(str(entry.get("label", "frame%d" % _frame)))
				"park", "hold":
					pass
				_:
					printerr("[capture] unknown action: ", action)
			if not _target_spec(entry).is_empty():
				_travel_spec = {}
			index += 1

		if fade_in > 0 and _frame <= fade_in:
			_set_cover_alpha(1.0 - float(_frame) / float(fade_in))
		elif fade_out > 0 and _frame >= total - fade_out:
			_set_cover_alpha(float(_frame - (total - fade_out)) / float(fade_out))

		_advance_cursor()
		await process_frame
		_frame += 1

	print("[capture] finished at frame ", _frame)


# ---------------------------------------------------------------- recon


## Recording a shot blind means guessing when the two lines meet. This prints the fight so the shot
## file can be written against numbers instead.
func _recon_pass() -> void:
	print("[capture] recon: t | friendlies | hostiles | camera.x | left hostile | right friendly | cards ready")
	var deploys := [
		{"at": 2, "unit": "breacher"},
		{"at": 4, "unit": "marksman"},
		{"at": 12, "unit": "impact"},
		{"at": 18, "unit": "breacher"},
	]
	var pending := 0

	for second in range(_recon_seconds):
		while pending < deploys.size() and int(deploys[pending]["at"]) <= second:
			var unit_id: String = str(deploys[pending]["unit"])
			if _cards.has(unit_id) and not (_cards[unit_id] as Button).disabled:
				var center: Vector2 = (_cards[unit_id] as Button).get_global_rect().get_center()
				_point = center
				_send_motion(center)
				await process_frame
				_send_click(center, MOUSE_BUTTON_LEFT)
				print("[capture] recon deployed %s at t=%d" % [unit_id, second])
			pending += 1
		await _frames(_fps)

		var friendlies := _units("friendlies")
		var hostiles := _units("hostiles")
		var ready: Array = []
		for unit_id in _cards:
			if not (_cards[unit_id] as Button).disabled:
				ready.append(unit_id)
		print(
			(
				"[capture] recon %3ds | %2d | %2d | %7.0f | %8s | %8s | %s"
				% [
					second + 1,
					friendlies.size(),
					hostiles.size(),
					_camera.global_position.x if _camera != null else 0.0,
					("%.0f" % _pick_unit("hostiles", "leftmost").global_position.x) if not hostiles.is_empty() else "-",
					("%.0f" % _pick_unit("friendlies", "rightmost").global_position.x) if not friendlies.is_empty() else "-",
					", ".join(ready),
				]
			)
		)


# ---------------------------------------------------------------- entry point


func _run() -> void:
	_parse_args()
	if not _load_shot():
		quit(2)
		return
	if not await _boot():
		quit(2)
		return
	if _recon:
		await _recon_pass()
	else:
		await _play()
	quit(0)
