# Mac/Tools/MacBuilder module reconciliation

Updated 2026-11-05.

**18/18 reconciled: 18 intentional.** Each assigned Mac body was read, including the seven missing reconstructed bodies. The verdict concerns the retired Carbon nib layer, not functional equality with native UI.

The Mac callers were identified individually. MacDisplay initialization built the display-selection nib and installed its user pane; the console built a nib with a TXN text view and edit hook; CCallOfDutyEngine initialization built the serial-entry nib and five edit hooks. The selected native path instead starts through `linux_common.c` / `mac_main.c`, enumerates SDL display modes and creates its SDL window in `macos_display.c`, reads terminal input and writes stdout through `mac_syscon.c`, and uses the launcher's `KeyStore` and `SetupView` for serial entry. The native event pump consumes SDL events directly. No native source call to a MacBuilder public API remains; old function-relative data relocations are not UI callers.

The retained bodies have the Mac control signature, text tags, font flags, focus kinds and callback tags. Their providers do not implement Carbon controls: `GetControlByID` and related functions are inert in `link_stubs.c`; nib creation, TXN and UPP names include non-executable placeholder arrays. `GetControlBounds` does not populate its output, and `PtInRect` is a zero-returning Carbon stub. Thus even the eleven structurally matching bodies cannot be claimed as usable native helpers. The seven missing bodies were not filled with calls into that unusable stack. Ledger evidence names each former responsibility and its selected replacement.

`StPortState` construction/destruction are owned by the separate mac-smalltools audit and were not changed. No shared headers or code changed in this module. This does not certify the original splash drawing path, which remains separate lifecycle work; the launcher's setup UI is not asserted to reproduce that splash.

The selected MacBuilder source compiled in the background arm64 Release `cod2_macos_ded` build with two jobs. These retired helpers were not exercised. No new test, replay or full client build was run. No entries remain unchecked.

Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-display.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
