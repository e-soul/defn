extends SceneTree


func _parse_args() -> Dictionary:
	var parsed := {"spec": "", "seeds": 0, "separation": 0.0, "friendly_spacing": 0.0, "out": "res://build/matrix.jsonl"}
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index := 0
	while index < args.size():
		var flag: String = args[index]
		var value: String = args[index + 1] if index + 1 < args.size() else ""
		match flag:
			"--spec":
				parsed["spec"] = value
				index += 2
			"--seeds":
				parsed["seeds"] = int(value)
				index += 2
			"--separation":
				parsed["separation"] = float(value)
				index += 2
			"--friendly-spacing":
				parsed["friendly_spacing"] = float(value)
				index += 2
			"--out":
				parsed["out"] = value
				index += 2
			_:
				index += 1
	if int(parsed["seeds"]) <= 0:
		parsed.erase("seeds")
	if float(parsed["separation"]) <= 0.0:
		parsed.erase("separation")
	if float(parsed["friendly_spacing"]) <= 0.0:
		parsed.erase("friendly_spacing")
	return parsed


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if not ClassDB.class_exists("DefnMatrixRunner"):
		printerr("Matrix runner class was not registered. Build with with_hosted_tests=yes or run the matrix SCons target.")
		quit(2)
		return

	var args := _parse_args()
	var spec: String = str(args["spec"])
	print("Measuring the payoff matrix (%s)" % ("built-in mixes" if spec == "" else spec))

	var result_variant: Variant = ClassDB.class_call_static("DefnMatrixRunner", "measure", args)
	if typeof(result_variant) != TYPE_DICTIONARY:
		printerr("Matrix runner returned an invalid result payload.")
		quit(2)
		return

	var result: Dictionary = result_variant
	if not result.get("success", false):
		printerr("Matrix measurement failed: %s" % result.get("message", ""))
		quit(1)
		return

	var out_path: String = str(result.get("out", ""))
	print("%d cell(s), %d row(s) over %d seed(s)" % [int(result.get("cells", 0)), int(result.get("rows", 0)), int(result.get("seeds", 0))])
	if out_path != "":
		print("Wrote %s" % out_path)
		print("Analyse with: python scripts/analyze_matrix.py defn/build/matrix.jsonl")
	quit(0)
