# WS105 — Config Studio B1 model layer

Completed on `codex/config-binds-b1`, starting at `6342c32` on the supplied
Config Studio integration lineage. No engine source, view, build list, remote,
tag or release changed. The model commits, in order, are `4b1f2dd`, `1e9f326`
and `a4b2007`; this report is a separate final commit.

The unit supplies engine key identities, layout and device snapshots, capture
lifecycle, catalog action rows, conflict proposals and F2-backed binding edits.
It does not claim completion of the List/Visual views or live UI acceptance.

## Engine and layout evidence

`tests/launcher/engine_keymap.py` compiles the production
`sdl_to_keynum` and `sdl_button_to_keynum` functions from
`src/unix/linux_input.c` into a temporary, data-free oracle. Swift agrees on
1,029 cases: 256 byte values, five non-ASCII Unicode samples, 512 masked
scancodes and 256 SDL mouse-button values. The fixture also checks all 98
names/numbers from `src/PC/client_mp/cl_keys_mp.c` and canonical spelling
round trips. Backspace uses SDL code 8 and engine number 127; SDL Delete uses
code 127 and engine number 162. These differences were caught during the red
run rather than inferred from scancode numbers.

The optional `input_reference.py` check reads the private Mac 1.3 `keynames`
object and verifies that all 98 names/numbers equal the current source. It
prints only a receipt. `Key_StringToKeynum` disassembly was inspected privately:
its one-byte path sign-extends the character. Literal `§` cannot be treated as
key `0xa7`; `0xa7` names F1. No reference bytes or disassembly are committed.

The macOS layout policy was checked against the releases pinned by
`scripts/build-sdl.sh`: [SDL 3.4.10 keyboard options](https://github.com/libsdl-org/SDL/blob/release-3.4.10/src/events/SDL_keyboard.c),
[Cocoa translation](https://github.com/libsdl-org/SDL/blob/release-3.4.10/src/video/cocoa/SDL_cocoakeyboard.m),
[Darwin virtual-key mapping](https://github.com/libsdl-org/SDL/blob/release-3.4.10/src/events/scancodes_darwin.h),
[default keymap](https://github.com/libsdl-org/SDL/blob/release-3.4.10/src/events/SDL_keymap.c)
and [sdl2-compat 2.32.72](https://github.com/libsdl-org/sdl2-compat/blob/release-2.32.72/src/sdl2_compat.c).
This is an independent Swift implementation of the factual mappings/policy.

Synthetic snapshots compare ABC, Greek, AZERTY, QWERTZ, Dvorak and JIS at
representative letter, punctuation, number-row and special-key positions.
Greek retains Greek labels while SDL's Latin fallback uses QWERTY identities.
French number fallback requires all ten unshifted symbols and shifted digits;
partial matches remain unsupported. ISO swaps positions 10/50. JIS extra
keys, Fn, non-ASCII Latin-layout symbols and unsupported SDL keys retain
explicit outcomes. The pinned Darwin table gives F13/F14 positions
PrintScreen/ScrollLock, which this engine does not bind, and F15 gives Pause.
Command is supported as engine key 150; the canvas's blanket unavailable
Command styling must not override this evidence.

Custom uppercase base-layout codes are explicitly unavailable because `bind`
folds ASCII uppercase but `CL_KeyEvent` does not. Custom control-key
translations follow SDL when provided. Ordinary Return/Space and keypad
Enter retain distinct expected identities.

## API for the UI owner

All declarations live in `launcher/InputDevices.swift` and
`launcher/ConfigBindings.swift`. No SwiftUI views are supplied.

| API | Contract |
| --- | --- |
| `EngineKey(number:)`, `EngineKey(spelling:)` | Failable canonical binding identity, `number`, lowercase `spelling`, stable `id`; never persist a label or virtual-key position as the binding identity. |
| `InputSourceSnapshot` | `id`, `name`, `shape`, raw `labels` and `shiftedLabels`; `label(virtualKey:)` returns readable labels; `mapping(virtualKey:)` returns `.available`, `.console` or `.unavailable`. |
| `InputKeyMapping` | `engineKey` includes console identity for inspection; only `.available` permits capture. `isReserved` and `explanation` expose console/unavailable reasons. |
| `EngineInputMapping` | Raw SDL-to-engine conversion, AppKit button conversion and wheel direction. UI capture should use the typed snapshot mapping, which also checks bindability. |
| `InputDevicesModel(provider:)` | Main-actor observable `inputSource`, `devices`, `overrides`; `startObserving()`, `stopObserving()`, `refresh()`, `setOverride(_:for:)`, `presentation(for:)`. Production provider is `SystemInputDeviceProvider()`. |
| `InputDevicePresentation` | Shape, size and modifier choices each expose a value, basis (`detected`, `guessed`, `userOverride`, `unknown`) and explanation. Button count is optional descriptor evidence, not a guess. |
| `ConfigBindings(document:catalog:mode:)` | Immutable snapshot with `actions`, `entries`, `row(for:)` and `entry(for:)`; action titles come from current bindable, mode-available catalog menu labels. |
| `ConfigBindingRow` | `action`, all `keys`, `primary`, `secondary`, `additional`. Keys are in engine number order; the document has no independent primary/secondary preference. Edits may therefore change which remaining key appears first. Extras are preserved. |
| `ConfigBindingEntry` | Exact `binding` bytes, `text`, optional exact single `actionName`, and `hasReleaseTail`. Leading whitespace and complex scripts remain opaque rather than being misrepresented as simple actions. |
| `prepareEdit(read:action:slot:key:)` | Proposes a primary/secondary edit; nil key clears that slot. Duplicate assignment to another slot and console keys throw. |
| `prepareKeyEdit(read:key:binding:)` | Visual inspector can edit or clear any key, including opaque/additional binds. Nil/empty binding clears it. F1 enforces byte, quote, line and buffer limits. B2 remains responsible for structured chains and full I1 safety review. |
| `ConfigBindingEdit` | Immutable `profileID`, optional `action`, affected `oldKey`/`key`, exact displaced `conflict`, `before` version, `preview`, `hasChanges`, `summary`. Construction stays within the model file. |
| `ConfigBindings.apply(_:to:replaceConflict:runningGame:)` | Returns one F2 history transaction, or nil for a no-op. A displaced binding requires `replaceConflict: true` after review. Expected config/metadata bytes and dialect must still match. |
| `ConfigBindingCapture(source:mapping:)` | Main-actor observable `state`, `held`, `released`, `unavailable`, `isMonitoring`; `start(runningGame:)`, `cancel(_:)`, `gameDidStart()`. Mapping closure should read the current device model snapshot. |
| `BindingCaptureEventSource` | Injectable main-actor source with `start(handler)` and `stop()`. Handler returns `.consume`/`.passThrough`. Tests use a synthetic source exclusively. |
| `AppKitBindingEventSource(window:)` | Production local monitor scoped to the editor window; key/button/wheel conversion, app/window focus notifications and removal of all installed tokens on stop. No global input monitor or permission prompt. |

Recommended capture composition:

```swift
let devices = InputDevicesModel(provider: SystemInputDeviceProvider())
let capture = ConfigBindingCapture(
    source: AppKitBindingEventSource(window: editingWindow),
    mapping: { devices.inputSource.mapping(virtualKey: $0) }
)
```

Observe `state` for the captured `EngineKey`. A captured down event publishes
the result immediately but keeps the source until every captured held input
is released. Keep its owner alive during that drain so Return/Space repeats
and up events remain consumed. Wheels and Caps Lock are pulses. Escape,
explicit cancel, focus loss, restart and game start clear held state and stop
the source. Late callbacks from an old generation pass through. Teardown also
queues source cleanup if the owner is dropped. No game commands or synthetic
engine releases are executed by this launcher model.

The UI must call `gameDidStart()` on game ownership changes and provide the
real running-game callback to its existing F2 store. `runningGame: false`
cannot override F2's callback for a write. Close/deactivate cleanup is supplied
by the source; view dismissal should explicitly cancel capture too.

Binding locks use `bind:<engine spelling>`; aliases such as `bind:0x71` protect
the same key as `bind:q`. `action:<command>` and bare command names also lock
the action. Source and destination keys, including empty/shadowed assignments,
are checked. Locks are rechecked at apply, and metadata byte comparison prevents
a concurrent lock change from being overwritten. Existing setting locks and
all metadata are preserved.

`hasReleaseTail` follows I1's raw leading-plus/semicolon warning condition.
It is an inspection warning, not a replacement for I1/B2 press/release and
transitive script review. Unresolved `exec`/`vstr` commands remain in the F1
document; these rows describe direct document state, not executed game state.

Shape/size/modifier overrides affect device presentation and survive refresh
in the model. They do not change SDL identity or rewrite a profile. Persisting
machine preferences belongs to the integration owner; this unit does not
invent a new sidecar schema. Registry inventory is refreshed initially, on
input-source notifications or explicit `refresh()`; the UI can refresh when
its device selection changes. Registry IDs are inventory identities, not
promised durable hardware identifiers. DPI and polling measurements are not
invented here; F2 already exposes user DPI metadata.

The production provider enumerates only keyboard/pointer/touchpad HID usages,
not unrelated vendor collections sharing a product name. It reads registry
metadata/HID elements without `IOHIDDeviceOpen`. Per-device keyboard shape is
used when reported; otherwise the current macOS shape is explicitly a guess.
No prior launcher IOKit enumeration was present in this checkout to reuse.

## Verification and receipts

All commands below exited zero. New runners use Swift 6, complete strict
concurrency, warnings as errors, macOS 13 arm64 target, `-j 2`, and background
`taskpolicy -b nice -n 19` compilation/execution. Temporary compiler outputs
and synthetic profile homes are removed by their traps.

```sh
sh tests/launcher/input_devices.sh --system-probe
sh tests/launcher/config_bindings.sh
taskpolicy -b nice -n 19 python3 tests/launcher/input_reference.py \
  "$HOME/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
taskpolicy -b nice -n 19 sh tests/launcher/config_document.sh
taskpolicy -b nice -n 19 sh tests/launcher/config_profiles.sh
taskpolicy -b nice -n 19 sh tests/launcher/config_catalog.sh
taskpolicy -b nice -n 19 sh tests/launcher/config_import.sh
```

Results: converter/name checks described above; six representative layout
families and unsupported outcomes; input-source/device refresh and labelled
overrides; capture down/release, layout change during a held capture,
Return/Space consumption, Caps Lock pulse, Mouse1–5/wheel, unsupported buttons,
stale callbacks and cancel/focus/game cleanup; action rows/extras, opaque plus
tails, reviewed replacement, generic clears, exact-byte preservation, 1023-byte
boundary, NUL rejection, locks including empty binds, history and disk/running
guards. F1 passed; F2 passed 14 cases; F4 passed with 733 settings/235 commands;
I1 passed safety/dependencies, source/URL/containment/size and Windows discovery.

The read-only system probe returned ABC/ANSI, 66 translated labels, two keyboard
HID devices (one is virtual), one trackpad, zero mice and no reported button
counts. It proves these APIs can read this host's current metadata, not that
six physical keyboards or every layout position were exercised. No live
source switch or local monitor was installed during tests. Production monitor
code is compiled, and lifecycle behavior is proven at the injected source
boundary; actual view/monitor interaction remains the UI owner's acceptance.

Red receipts precede implementation for converter, layouts, source refresh,
capture startup, action models and generic edits. Additional assertion-red
receipts cover readable labels, Caps Lock latch cleanup, vendor HID collection
filtering, custom uppercase layouts and empty-binding locks. Compiler errors
encountered while establishing interfaces/strict concurrency were corrected
and are not counted as assertion passes.

Evidence directory:
`~/Library/Application Support/CoD2-native-current-readiness/WS105.noindex/`.
It holds the red/green logs, final runner logs, reference receipt, private
parser inspection and fetched-source SHA-256 receipt. Download/compiler scratch
directories were removed. No app bundle, game window, benchmark lease, game
data mutation, real player config write or LaunchServices registration occurred.

## Preservation and composition bounds

```sh
git diff --name-only 6342c32 -- src build/lp64_gen CMakeLists.txt
taskpolicy -b nice -n 19 python3 tools/abi/legacy.py --base 6342c32
taskpolicy -b nice -n 19 python3 tests/fixes13/legacy.py --base 6342c32
taskpolicy -b nice -n 19 python3 tests/cod2x/check_inactive_gates.py --base 6342c32
git diff --check
```

The engine-path diff is empty. The three relative guards report zero changed
engine files and zero mismatches (5 OFF, 10 legacy and 17 inactive
configurations respectively). No engine file exists in this unit's
`unifdef -UCOD2_X64`/`cmp` obligation set. These empty unit-relative checks do
not prove x86 object/binary equality or turn the inherited full gates green.
Engine build flags, including the locked host revision, were not changed.

The wave's known reds remain: legacy ABI 10 differences (five generated
`data_native.c`, excluded by CMake OFF, and five test-only `server_control.c`);
inactive CoD2x gates 436 differences across intentional stock/native repairs
and synthetic x64-defined-zero configurations; x86 Linux comparison requires
unavailable Linux multilib. These are supplied baseline counts, not fresh full
gate results. The predecessor WS104 snapshot appearance failure also remains
reported red. This model-only unit did not rerun or weaken the full engine or
rendered launcher gates.

Compose the three code commits and this report after the supplied integration
base. Add both new Swift model files to the launcher build/typecheck lists
alongside F1/F2/F4, use this API from `ConfigBindsView.swift`, and keep the actual
process guard/capture owner lifecycle in the shared Studio owner. This unit
leaves those build-list/view changes to their named owners, as requested.
There are no remaining model blockers or uncommitted unit files.
