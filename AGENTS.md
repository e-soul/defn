# DEFN — Project Instructions

## Overview

This is a **Godot 4.6** game built as a **Belt Scroller** (2.5D side-scroller). It uses **godot-cpp** (GDExtension) with **modern C++23**.

## Design Goals

- **Code-first**: Everything is implemented in C++ code. The Godot editor should only be used when something cannot be done in code.
- **Modern C++23**: Use modern language features (structured bindings, std::optional, std::span, constexpr, concepts, etc.) where they improve clarity.
- **When Making Changes**: Follow modern game development architectural patterns — component-based design, data-driven configuration, separation of concerns, and clean resource management. Always use the appropriate high-level API from Godot.

## Tech Stack

- **Engine**: Godot 4.6
- **Language**: C++23 via godot-cpp (GDExtension)
- **Build**: SCons
- **Data**: JSON for level and unit definitions (`defn/data/`)

## Project Structure

- `defn/` - Godot project directory
- `defn/src/` - All C++ game source (entities, managers, HUD)
- `defn/assets/` - Art and sprite assets (submodule)
- `godot-cpp/` - GDExtension bindings (submodule, do not modify)

## Build Instructions

```powershell
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process
.\.venv\Scripts\Activate.ps1
cd defn
scons with_tidy=True
```

Note: Fix any clang-tidy issues reported by the build.

## Format Code

```powershell
cd defn
git ls-files '**.h' '**.cpp' | ForEach-Object { clang-format -i $_ }
```
