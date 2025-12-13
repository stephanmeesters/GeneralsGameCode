To generate the snapshot schema manually for testing:

1. Ensure `compile_commands.json` exists (e.g., `cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build`).
2. Run the generator from the repo root:
   ```bash
   python3 scripts/cpp/generate_snapshot_schema.py \
     --output /tmp/GeneratedSnapshotSchema.h \
     --source-root GeneralsMD/Code \
     --source-root Core/GameEngine \
     --compile-commands build/compile_commands.json \
     --gamestate GeneralsMD/Code/GameEngine/Source/Common/System/SaveGame/GameState.cpp
   ```
3. Open `/tmp/GeneratedSnapshotSchema.h` to inspect the emitted schema. If you skip `--compile-commands`, it will walk the source roots directly.

The generated header now also includes:
- `SnapshotSchema::SCHEMAS`: `unordered_map<string_view, span<const SnapshotSchemaField>>` keyed by class name.
- `SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS`: map from save-game block name (parsed from `GameState::init`) to the corresponding schema view when available.
