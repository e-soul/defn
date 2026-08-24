extends SceneTree


func _parse_args() -> Dictionary:
	var parsed := {"seeds": 25, "out": ""}
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index := 0
	while index < args.size():
		var flag: String = args[index]
		var value: String = args[index + 1] if index + 1 < args.size() else ""
		match flag:
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
	if not ClassDB.class_exists("DefnBalanceRunner"):
		printerr("Balance runner class was not registered. Build with with_hosted_tests=yes or run the balance SCons target.")
		quit(2)
		return

	var args := _parse_args()
	print("Measuring over %d seed(s)" % int(args["seeds"]))

	var result_variant: Variant = ClassDB.class_call_static("DefnBalanceRunner", "measure", args)
	if typeof(result_variant) != TYPE_DICTIONARY or not bool((result_variant as Dictionary).get("success", false)):
		printerr("Balance measurement failed.")
		quit(1)
		return

	quit(0)
