extends SceneTree


func _parse_args() -> Dictionary:
	# Flag -> key. Every flag here consumes exactly one value, so the cursor advance is done once below rather than
	# per case. A per-case `index += 2` is how this went wrong before: each flag added between an existing case's
	# body and its increment silently stole that increment, leaving the donor flag unable to advance the cursor at
	# all -- which is not a parse error but an infinite loop, indistinguishable from a slow sweep.
	const FLOAT_LIST_FLAGS := {
		"--base-budget": "base_budget",
		"--escalation": "escalation",
		"--bounty-decay": "bounty_decay",
		"--bounty-decay-curve": "bounty_decay_curve",
		"--bounty-floor": "bounty_floor",
		"--escalation-curve": "escalation_curve",
		"--hostile-damage-growth": "hostile_damage_growth",
		"--hostile-damage-cap": "hostile_damage_cap",
		"--elite-fraction-cap": "elite_fraction_cap",
		"--elite-hp-growth": "elite_hp_growth",
		"--elite-hp": "elite_hp",
		"--elite-first-wave": "elite_first_wave",
		"--wave-interval": "wave_interval",
		"--interval-growth": "interval_growth",
		"--supply-start": "supply_start",
		"--supply-growth": "supply_growth",
		"--supply-cap": "supply_cap",
		"--energy-cap": "energy_cap",
	}

	var parsed := {"seeds": 5, "out": "", "max_seconds": 1800.0, "base_budget": [], "escalation": [], "bounty_decay": [], "bounty_decay_curve": [], "bounty_floor": [], "escalation_curve": [], "hostile_damage_growth": [], "hostile_damage_cap": [], "elite_fraction_cap": [], "elite_hp_growth": [], "elite_hp": [], "elite_first_wave": [], "wave_interval": [], "interval_growth": [], "supply_start": [], "supply_growth": [], "supply_cap": [], "energy_cap": [], "policy_labels": []}
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index := 0
	while index < args.size():
		var flag: String = args[index]
		var value: String = args[index + 1] if index + 1 < args.size() else ""
		if FLOAT_LIST_FLAGS.has(flag):
			parsed[FLOAT_LIST_FLAGS[flag]] = _parse_floats(value)
			index += 2
		elif flag == "--seeds":
			parsed["seeds"] = int(value)
			index += 2
		elif flag == "--out":
			parsed["out"] = value
			index += 2
		elif flag == "--max-seconds":
			parsed["max_seconds"] = float(value)
			index += 2
		elif flag == "--policies":
			parsed["policy_labels"] = Array(value.split(",", false))
			index += 2
		else:
			index += 1
	return parsed


func _parse_floats(value: String) -> Array:
	var values: Array = []
	for part in value.split(",", false):
		values.append(float(part))
	return values


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if not ClassDB.class_exists("DefnEndlessRunner"):
		printerr("Endless runner class was not registered. Build with with_hosted_tests=yes or run the endless SCons target.")
		quit(2)
		return

	var args := _parse_args()
	print("Sweeping endless over %d seed(s)" % int(args["seeds"]))

	var result_variant: Variant = ClassDB.class_call_static("DefnEndlessRunner", "run_sweep", args)
	if typeof(result_variant) != TYPE_DICTIONARY or not bool((result_variant as Dictionary).get("success", false)):
		printerr("Endless sweep failed.")
		quit(1)
		return

	var result: Dictionary = result_variant
	print("%d run(s), deepest wave %d" % [int(result.get("runs", 0)), int(result.get("max_wave", 0))])
	quit(0)
