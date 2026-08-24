extends SceneTree

var _runner: Node = null
var _frames: int = 0


func _initialize() -> void:
	if not ClassDB.class_exists("DefnConformanceRunner"):
		printerr("Conformance runner class was not registered. Build with with_hosted_tests=yes or run the conformance SCons target.")
		quit(2)
		return

	_runner = ClassDB.instantiate("DefnConformanceRunner")
	root.add_child(_runner)


func _process(_delta: float) -> bool:
	if _runner == null:
		return true

	_frames += 1
	if _frames % 1200 == 0:
		print("... %d frames simulated" % _frames)
	if not _runner.is_finished():
		return false

	var result: Dictionary = _runner.get_result()
	root.remove_child(_runner)
	_runner.free()
	_runner = null
	var failures: Array = result.get("failures", [])
	var scenarios: int = int(result.get("scenarios", 0))

	if bool(result.get("success", false)):
		print("%d conformance scenario(s) agree" % scenarios)
		quit(0)
		return true

	printerr("Conformance failed after %d scenario(s)" % scenarios)
	for failure in failures:
		printerr("  %s" % failure)
	quit(1)
	return true
