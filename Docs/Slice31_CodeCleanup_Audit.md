# Slice 31 Code Cleanup Audit

Branch: `refactor/slice-31-cleanup`

Goal: clean and refactor the current Slice 1-31 technical showroom before starting Slice 32. Do not add nature-map, horse, mounted traversal, wildlife, or other Slice 32+ work on this branch.

## Cleanup rules

1. Preserve current Slice 31 behavior first.
2. Do not delete Blueprint or `.uasset` content unless the C++ reference chain is verified.
3. Prefer small compile-safe commits over large rewrites.
4. Keep `main` stable; merge only after Unreal Editor compile and PIE validation.
5. Remove prototype/template debt only after confirming it is not used by starter-template gameplay or existing Blueprint assets.

## Current high-level findings

### 1. `AAmbientDirector` is doing too much

`Source/Ambient_UE5/AmbientDirector.h` and `.cpp` currently contain multiple responsibilities in one actor:

- world sensing and region detection
- candidate placement validation
- encounter definition selection/scoring
- authored-point and EQS spawn location selection
- pacing/budget/history checks
- prototype encounter runtime state machine
- save/load/restore logic
- on-screen debug dashboard and debug drawing

This is the main refactor target. Do not rewrite it at once. Split it in stages.

### 2. Debug code is mixed into runtime code

The Director currently owns both gameplay logic and debug visualization/dashboard printing. This is useful for the technical showroom, but should be isolated before the real vertical slice.

Preferred direction:

- keep `FAmbientWorldState` and debug structs as data
- move formatting/drawing into a dedicated debug helper later
- keep debug toggles Blueprint-facing
- make production defaults conservative after Blueprint settings are verified

### 3. Save game header depends on `AmbientDirector.h`

`AmbientDirectorSaveGame.h` includes `AmbientDirector.h` because the save snapshot uses `EAmbientEncounterRuntimeState` and `FAmbientEncounterHistoryEntry`. This couples save data to the full Director actor header.

Preferred direction:

- move shared runtime/save structs and enums into a lightweight types header
- keep `AmbientDirectorSaveGame.h` independent from the Director actor class

Candidate future header:

```text
AmbientDirectorTypes.h
```

Likely contents:

- `EAmbientEncounterRuntimeState`
- `EAmbientDirectorDebugVisualizationMode`
- `FAmbientEncounterHistoryEntry`
- `FAmbientWorldState`
- `FAmbientEncounterSelectionDebugEntry`

### 4. Hard-coded showroom defaults should become data/config defaults

Several core C++ classes still default to showroom-specific IDs/tags:

- `Region.Showroom`
- `Point.Showroom`
- `Encounter.Prototype.Showroom`
- `EP.Showroom.01`

This is acceptable for Slice 1-31, but should not become the permanent default for Slice 32+.

Preferred direction:

- preserve current assets for the showroom
- make C++ defaults neutral where possible
- move showroom-specific values into Blueprint/DataAsset defaults

### 5. Build configuration still contains starter-template include paths

`Source/Ambient_UE5/Ambient_UE5.Build.cs` contains include paths for several template variants:

- `Variant_Platforming`
- `Variant_Combat`
- `Variant_SideScrolling`

Do not delete these blindly. First verify whether the initial UE template C++ classes still depend on them. If they are unused, remove them in a dedicated compile-tested commit.

### 6. Low-risk code hygiene candidates

These are safe-looking but still require compile validation:

- rename local typo `MakrerRotation` to `MarkerRotation` in `AAmbientDirector::UpdateCandidateMarker`
- replace magic sentinel `-999999.0f` in selection logic with an explicit `bFoundBestCandidate` flag
- move unreachable TODO in `GetCurrentEncounterBudgetUse()` above the return or convert it to a clear future-work comment
- remove duplicate blank lines and generated-template comments
- replace `LogTemp` with the project log category where appropriate

## Recommended cleanup order

### Pass 1 — Zero-risk documentation and policy

- Add this audit.
- Freeze Slice 31 scope.
- Confirm compile baseline in Unreal Editor.

### Pass 2 — Small code hygiene

- Fix local typos.
- Remove stale comments.
- Replace obvious magic numbers in local logic.
- Keep behavior identical.

### Pass 3 — Header dependency cleanup

- Introduce `AmbientDirectorTypes.h`.
- Move shared structs/enums out of `AmbientDirector.h`.
- Make `AmbientDirectorSaveGame.h` include the lightweight types header instead of the actor header.

### Pass 4 — Director responsibility split

Split only after Pass 2 and Pass 3 compile.

Suggested structure:

```text
AmbientDirector.h/.cpp                  Actor orchestration only
AmbientDirectorTypes.h                  Shared enums/structs
AmbientDirectorSelection.*              Definition scoring and selection helpers
AmbientDirectorPlacement.*              Spawn location validation helpers
AmbientDirectorDebug.*                  Debug text/draw helpers
AmbientDirectorSaveGame.*               Save object only
```

Do not make all helper files UObject classes unless reflection is needed. Prefer plain structs/functions for low overhead.

### Pass 5 — Template/starter cleanup

- Check starter-template C++ classes and Blueprints.
- Remove unused variant include paths/dependencies only after compile.
- Remove or archive unused prototype content after asset references are verified in the Editor.

## Do not start yet on this branch

- Slice 32 nature map
- horse integration
- traversal state expansion
- mounted encounter table
- wildlife encounter table
- portfolio video packaging

Those belong after the Slice 31 cleanup branch is merged.
