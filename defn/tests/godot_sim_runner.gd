extends SceneTree


func _parse_args() -> Dictionary:
	var parsed := {
		"scenario": "res://scenarios/level_01_greedy.json",
		"seeds": 1,
		"out": "",
	}
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index := 0
	while index < args.size():
		var flag: String = args[index]
		var value: String = args[index + 1] if index + 1 < args.size() else ""
		match flag:
			"--scenario":
				parsed["scenario"] = value
				index += 2
			"--seeds":
				parsed["seeds"] = int(value)
				index += 2
			"--out":
				parsed["out"] = value
				index += 2
			_:
				index += 1
	return parsed


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if not ClassDB.class_exists("DefnSimRunner"):
		printerr("Simulation runner class was not registered. Build with with_hosted_tests=yes or run the sim SCons target.")
		quit(2)
		return

	var args := _parse_args()
	print("Running %d seed(s) of %s" % [int(args["seeds"]), args["scenario"]])

	var result_variant: Variant = ClassDB.class_call_static("DefnSimRunner", "run_sweep", args)
	if typeof(result_variant) != TYPE_DICTIONARY:
		printerr("Simulation runner returned an invalid result payload.")
		quit(2)
		return

	var result: Dictionary = result_variant
	if not result.get("success", false):
		printerr("Simulation sweep failed: %s" % result.get("message", ""))
		quit(1)
		return

	var runs: int = int(result.get("runs", 0))
	var victories: int = int(result.get("victories", 0))
	var out_path: String = str(result.get("out", ""))
	print("%d run(s), %d victory/victories (%.0f%% win rate)" % [runs, victories, 100.0 * float(victories) / float(max(runs, 1))])
	if out_path != "":
		print("Wrote %s" % out_path)
	quit(0)
