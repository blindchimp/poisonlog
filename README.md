# PoisonLog

Date: 2026-10-20

**Vibe coded** — this project was built largely by AI-assisted, stream-of-consciousness coding.

A Qt 6 / QML desktop application for recording where herbicides and pesticides are applied on a piece of land.

## Overview

- Set up a map by entering an address; the app geocodes it and captures a top-down map view (roughly one acre around the main structure).
- Draw one or more rectangles on the saved map to mark treated areas.
- Attach notes, a date, a tag name, and a color to the marked areas.
- Commit records to a local SQLite database.
- Browse committed records in a side list sorted newest to oldest; selecting one displays its rectangles and notes.
- Partially entered work is preserved across record selection and application restarts, with options to resume or discard it.

## Requirements

- CMake 3.25+
- Qt 6.12 (Core, Gui, Qml, Quick, QuickControls2, Sql, Network, Test)
- A C++20 compiler

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

```sh
./build/poisonlog
```

## Tests

```sh
cd build && ctest
```

## Project Layout

- `src/` — C++ backend (database, models, geocoder, tile provider, app state)
- `qml/` — QML UI (map setup, map canvas, record list, editor panel)
- `tests/` — Qt Test suites for the database and app store
