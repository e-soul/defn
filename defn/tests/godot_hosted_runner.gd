extends SceneTree


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if not ClassDB.class_exists("DefnHostedTestRunner"):
		printerr("Hosted test runner class was not registered. Build with with_hosted_tests=yes or run the hosted_test SCons target.")
		quit(2)
		return

	var result_variant: Variant = ClassDB.class_call_static("DefnHostedTestRunner", "run_registered_tests")
	if typeof(result_variant) != TYPE_DICTIONARY:
		printerr("Hosted test runner returned an invalid result payload.")
		quit(2)
		return

	var result: Dictionary = result_variant
	var failures: Array = result.get("failures", [])

	if result.get("success", false):
		print("%d hosted test(s) passed" % int(result.get("passed", 0)))
		quit(0)
		return

	printerr("Hosted tests failed")
	for failure in failures:
		printerr("[FAIL] %s: %s" % [failure.get("name", ""), failure.get("message", "")])

	quit(1)