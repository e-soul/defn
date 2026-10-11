# Copyright (c) 2026 e-soul.org
# SPDX-License-Identifier: BSD-2-Clause
extends SceneTree

## Runs against the exported PCK and release GDExtension, outside the source project.
## In particular, do not call the C++ layout methods directly: Godot must dispatch them.

func _initialize() -> void:
	call_deferred("_run")


func _settle() -> void:
	for frame in range(30):
		await process_frame


func _button(label: String) -> Button:
	for node in current_scene.find_children("*", "Button", true, false):
		if node.text == label:
			return node
	return null


func _check_buttons(labels: Array[String]) -> bool:
	for label in labels:
		var button := _button(label)
		if button == null or not button.is_visible_in_tree() or button.size.x <= 0 or button.size.y <= 0:
			push_error("Native export: missing or empty button: " + label)
			return false
		var rect := button.get_global_rect()
		if not root.get_visible_rect().grow(1).encloses(rect):
			push_error("Native export: button outside viewport: " + label)
			return false
		var ancestor := button.get_parent()
		while ancestor != null:
			if ancestor is Control and ancestor.clip_contents and not ancestor.get_global_rect().grow(1).encloses(rect):
				push_error("Native export: clipped button: " + label)
				return false
			ancestor = ancestor.get_parent()
	return true


func _click(label: String) -> void:
	var button := _button(label)
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.position = button.get_global_rect().get_center()
		event.pressed = pressed
		Input.parse_input_event(event)
		await process_frame
	await _settle()


func _run() -> void:
	root.size = Vector2i(1280, 720)
	if change_scene_to_file(ProjectSettings.get_setting("application/run/main_scene")) != OK:
		quit(1)
		return
	await _settle()
	var screen := current_scene.find_child("ScreenRoot", true, false)
	if screen == null or not screen.is_processing():
		push_error("Native export: menu layout callback was not registered")
		quit(1)
		return
	if not _check_buttons(["Play", "Options", "Quit"]):
		quit(1)
		return
	await _click("Play")
	if not _check_buttons(["Campaign", "Progress", "Main Menu"]):
		quit(1)
		return
	await _click("Main Menu")
	await _click("Options")
	if not _check_buttons(["Back"]):
		quit(1)
		return
	await _click("Back")
	root.size = Vector2i(667, 330)
	await _settle()
	if not _check_buttons(["Play", "Options", "Quit"]):
		quit(1)
		return
	unload_current_scene()
	await _settle()
	print("NATIVE_EXPORT_SMOKE_PASS")
	quit()
