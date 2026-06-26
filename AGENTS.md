# DEFN — Project Instructions

## Overview

This is a **Godot** game built as a **Belt Scroller** (2.5D side-scroller). It uses **godot-cpp** (GDExtension).

## Design Goals

- **Code-first**: Everything is implemented in C++ code. The Godot editor should only be used when something cannot be done in code.
- **Modern C++23**: Use modern language features where they improve clarity.
- **When Making Changes**: Follow modern game development architectural patterns — component-based design, data-driven configuration, separation of concerns, and clean resource management. Always use the appropriate high-level API from Godot.

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
