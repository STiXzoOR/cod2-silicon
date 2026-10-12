# Config Studio catalog

The committed resource describes **readiness integration `048ff5e`**, not the
older engine on this foundation branch. This tool uses Python's standard library
and Clang's preprocessor. It reads source; it neither builds nor runs the game,
and consumes no game data, player profiles, reference binary or private tools.

```sh
python3 -m unittest discover -s tools/catalog -v
python3 tools/catalog/catalog.py generate \
  --source-root /path/to/source-export --source-revision 048ff5e \
  --output launcher/Resources/ConfigCatalog.json
python3 tools/catalog/catalog.py check \
  --source-root /path/to/source-export --catalog launcher/Resources/ConfigCatalog.json --mode stock
python3 tools/catalog/catalog.py check \
  --source-root /path/to/source-export --catalog launcher/Resources/ConfigCatalog.json --mode cod2x
sh tests/launcher/config_catalog.sh
```

Export a revision with `git archive`; do not change a sibling worktree. Generate
twice to distinct paths and use `cmp` to verify deterministic bytes. A source
root with different registrations must fail against this resource. Source line
shifts alone do not fail coverage. Authored copy may not replace factual fields,
claim an unknown registration, or shadow a registered name with a supplemental
entry. When generating an older source's comparison resource, remove copy for
names absent from that source in a **scratch copy file** and pass `--copy`.

## Facts and scope

- `variants.stock` and `variants.cod2x` hold **all registration sites**, including
  source-local implementation headers, renderer import calls and reviewed
  table/alias indirections. Header sites retain their own path and line and are
  deduplicated across translation units. The native client
  source selection follows `cmake/macos-arm64.cmake`; feature headers and gates
  are preprocessed with `COD2_X64=1`, `COD2_APPLE_SDK=1`, macOS/arm64 and optionally
  `COD2_CODX=1`. The scope includes hosted-server registrations in that client,
  not dedicated-executable-only registrations. No Metal or optional gamepad,
  splitscreen, script-debugger or diagnostic feature is enabled.
- Type, default, limits, enum choices and flag words describe **constructor
  inputs**. Enum defaults are indices; colour defaults are normalized constructor
  components. This is not a runtime dvar dump. Defaults can be replaced by
  configuration, hardware setup, scripts or server policy. Single-precision
  constructor text is retained as its numeric source value, not promised as an
  exact live floating-point serialization.
- `literal` facts have a value. `dynamic` facts have no invented value and retain
  the source expression. Display mode lists, home/install paths, hardware identity,
  game-type counts and protocol selection are examples. The loader returns
  `ambiguous` when multiple sites disagree, rather than guessing startup order.
- Flags are OR-ed across sites for restrictions. A runtime conditional flag word
  keeps its expression and possible words; its aggregate is conservative and
  marked `flagsDynamic`. ROM (`0x40`), NOWRITE (`0x10`) and CHEAT (`0x80`) never
  become editable controls. Saved means ARCHIVE (`0x1`); the separate `0x200`
  flag is called `savedBit` without claiming a config-writing effect.
- Explicit command registrations and game `ClientCommand`/`ConsoleCommand`
  dispatch sites are separate mechanisms. Commands available only by dispatch
  have `availability: dispatchOnly`. Coverage checks both sets. Generic runtime
  factories accept arbitrary names; they live in `dynamicRegistrations`, not
  fabricated finite entries. Unknown name indirections fail closed.
- `referenceOnly`, `scriptOrMod` and `unavailable` entries have **no native
  variants** and no asserted native defaults. They cannot satisfy coverage for a
  real registration. The supplemental list is illustrative, not an inventory of
  every possible mod-defined name or every Windows CoD2x-only setting.
- The gate rejects missing/extra names and mismatched type/default/range/flags/
  choices, command mechanisms/site counts and dynamic-factory evidence. It is
  static registration coverage, not a whole-program execution trace. New source
  selection rules or new registration wrappers/indirections need review of this
  bounded reader; supported CMake glob definitions are checked explicitly. Table
  names require a constant index or a reviewed bounded iteration. Cast-sensitive
  arithmetic remains dynamic. The C parser does not claim to interpret arbitrary C.

## Copy and consumers

`copy.json` owns original plain titles, explanations, menu-exposure hints and
Guided choices. Registration evidence is generated independently. All actual
menu-exposed settings identified in the supplied research have authored copy;
remaining Expert references honestly say that their effect is not documented.
Menu labels are authored action labels, not bundled game menus or translations.
Competitive explanations are checked against `cod2x_runtime.c` and upstream
CoD2x 1.4.6.8 `mss32/competitive.cpp`; they are separate from registration defaults.

`ConfigCatalog` is immutable, Codable and Sendable. Use `setting(named:)`,
`command(named:)`, `settings(in:)`, `commands(in:)`, `variant(for:)` and
`guidedSettings(in:mode:)`. Use a variant's consensus facts and its `editable`
property, or the setting's `isEditable(in:)`; do not pick the first site as a live
default or trust the resource's editable hint on its own. Control kinds include
numeric/vector fields for Expert as well as switches, menus, text and colour.
The ranges live on the mode's sites/consensus, so a stock range cannot silently
become a CoD2x range. Machine display controls remain machine settings at F3.

F3 must add this Swift file to launcher build lists and copy the JSON into the
bundle. `ConfigCatalog.load(bundle:)` accepts either a root resource or a
`Resources` subdirectory and throws if missing. This F4 unit adds no UI, disk
editing, build integration, rule sets or optimizer recommendations.
