$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

$forbiddenNodeInclude = '#include\s+<godot_cpp/classes/(node|node2d|canvas_layer|scene_tree|timer|object|file_access|json)'
$violations = @()

foreach ($root in @("src/domain", "src/application", "src/adapters/presenters")) {
    if (-not (Test-Path $root)) {
        $violations += "Missing architecture audit root: $root"
        continue
    }

    $sourceFiles = Get-ChildItem -Path $root -Recurse -Include *.h, *.cpp
    foreach ($sourceFile in $sourceFiles) {
        $relativePath = Resolve-Path -Relative $sourceFile.FullName
        $matches = Select-String -Path $sourceFile.FullName -Pattern $forbiddenNodeInclude
        foreach ($match in $matches) {
            $violations += "${relativePath}:$($match.LineNumber): forbidden Godot framework include: $($match.Line.Trim())"
        }
    }
}

$combatLogicAttackTargetIncludes = Select-String -Path "src/domain/combat/combat_logic.h", "src/domain/combat/combat_logic.cpp" -Pattern 'attack_target\.h|AttackTarget\s*\*'
foreach ($match in $combatLogicAttackTargetIncludes) {
    $violations += "$($match.Path):$($match.LineNumber): combat logic must use EntityId instead of AttackTarget*: $($match.Line.Trim())"
}

if ($violations.Count -gt 0) {
    Write-Error ($violations -join [Environment]::NewLine)
    exit 1
}

Write-Host "Architecture audit passed."