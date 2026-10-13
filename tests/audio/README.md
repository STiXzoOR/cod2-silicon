# Sound environment contracts

Run `python3 tests/audio/run.py --build build-macos` and repeat with the CoD2x
build. Preserve the actual Release compile database as
`compile_commands.engine.json` before fixture runners alter build flags.

The command fixture compiles the production command body and
`SND_SetEnvironmentEffects`, with their actual room-name table. ASan/UBSan and
assertions check the resulting effect priority, room, goals and fade rates.
Invalid arguments must leave sound state unchanged and print a diagnostic.
Command arguments and room-driver output are fixture boundaries; no sound
device or running game is needed.

The runner also invokes the existing reverb reference probe, requiring all
ten 2D/3D cases to agree with the independently inspected Mac 1.3 branches.
It does not use `--expect-difference`. Probe results use a temporary directory
outside the repository; compiled fixtures remain in the selected build folder.

These tests establish command/state and wet-level selection contracts. They
do not prove acoustic output, environmental asset coverage or voice fidelity.
