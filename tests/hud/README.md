# Turret and spectator HUD contracts

The fixture includes the complete production `cg_draw_mp.c` translation unit,
then invokes `CG_DrawTurretCrossHair`, `CG_DrawSpectatorMessage` and the unchanged
`CG_DrawFollow`. Expected state, commands, geometry and text parameters come from
the hand-reviewed owned Mac 1.3 reference documented in WS71, independently of
the production implementation.

Turret draws run through the active production `CL_DrawStretchPic` adapter and
the complete production `screen_placement_mp.c`, ending at a captured
`R_AddCmdDrawStretchPic`. Three display sizes (640×480, 1280×720 and 2560×1440),
full-screen and half-height split-screen viewports, registered material, next
entity state, pause/draw/alpha gates and friendly color are checked. A conflicting
`eFlags` value ensures the target color comes from `pm_flags`.

Spectator draws capture `UI_DrawText` calls. Synthetic localization and control
bindings check primary command preference, alternative bindings, unbound-key
localization, snapshot selection, descriptive-text and catcher gates, the
reference font scale, hint line placement and draw parameters. The CoD2x build
also checks the existing spectated-name suppression option. These dependencies
are test inputs; this fixture does not establish font rasterization, real
localization-file contents, physical text rendering or keyboard input behavior.

```sh
taskpolicy -b nice -n 19 python3 tests/hud/run.py \
  --compile-commands build-macos/compile_commands.json \
  --output "$HOME/Library/Application Support/CoD2-native/hud-stock"
taskpolicy -b nice -n 19 python3 tests/hud/run.py \
  --compile-commands build-macos-codx/compile_commands.json \
  --output "$HOME/Library/Application Support/CoD2-native/hud-codx"
python3 tests/hud/reference.py \
  --binary "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
```

The runner rebases paths from a sibling engine compile database to this checkout,
uses the original engine flags for a production object compile, enables
ASan/UBSan and assertions only for fixture objects, and retains logs/results
outside the checkout. It never changes the caller's compile database. Use
`--baseline <Git revision>` to exercise older production HUD source; acceptance
requires a zero exit code and all selected cases passing. The pre-restoration
baseline fails seven groups; the unchanged follow-policy control passes.

The optional private-reference check validates the exact reference digest,
global import identities, reviewed literals and nested STABS offsets. It does
not execute i386 code or independently prove every branch. Neither fixture
requires game data or opens a window. Live mounted-turret/spectator checks and
screenshots remain a separate integration requirement.

The scoreboard border fixture extracts the active production border-buffer
declaration, `CalcBorderLines`, and the border draw block from
`CG_DrawScoreboard`. ASan checks the real buffer capacity; captured
`UI_DrawHandlePic` calls must emit six thin borders inside the existing panel,
with four horizontal and two vertical lines and unchanged fade alpha. These
geometric expectations are independent of the producer's rectangle values.
The original Mac consumer's six records and field order were inspected for
WS93. This fixture covers CPU geometry, not rendered pixels or keyboard input.

```sh
taskpolicy -b nice -n 19 python3 tests/hud/scoreboard.py \
  --compile-commands build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/scoreboard-stock"
taskpolicy -b nice -n 19 python3 tests/hud/scoreboard.py \
  --compile-commands build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native/scoreboard-codx"
```

Use `--baseline f5b83ad` to reproduce the stack overflow. Increasing capacity
alone still fails the independent panel bounds check because the old consumer
shifts every rectangle by one float. Outputs and sanitizer logs remain outside
the checkout; the compile database is never modified.
