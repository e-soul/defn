# DEFN — Project Instructions

## Overview

This is a **Godot** game built as a **Belt Scroller** (2.5D side-scroller). It uses **godot-cpp** (GDExtension).

## Requirements

- **Code-first**: Everything is implemented in C++ code. The Godot editor should only be used when something cannot be done in code.
- **Modern C++23**: Use modern language features where they improve clarity.
- **When Making Changes**: Follow the target architecture described in @ARCH.md. If structural changes are really needed, reflect them in @ARCH.md. Keep the @README.md short and up-to-date.

## Build Instructions

```powershell
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process
.\.venv\Scripts\Activate.ps1
cd defn
scons with_tidy=True test_all
```

Note: Fix any clang-tidy issues reported by the build.

## Format Code

```powershell
cd defn
git ls-files '**.h' '**.cpp' | ForEach-Object { clang-format -i $_ }
```
