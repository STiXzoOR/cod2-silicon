# Mac/Tools/MacGameEngine reconciliation

Updated 2026-11-05.

**24/24 reconciled: 24 intentional.** The class is replaced by native main/WinMain, SDL input/window handling, terminal console and `Sys_Quit`. Each assigned body and its actual replacement/caller boundary was checked; engine-side responsibilities were not excluded by filename.

Read the constructor, all three destructors, `Run`, `Initialize`, timer/event callbacks and state methods. Mac initializes object/handler/modifier/quit/running fields, removes installed handlers in destructors, checks old OS/QuickTime/data-volume prerequisites and adjusts the stack limit before entering a Carbon timer/event loop. Native does not instantiate this object or retain its vtable in compiled native data. Its seven empty base input virtuals therefore need no additional dispatch bodies; SDL emits engine events directly. Quit/running flags are replaced by SDL queued quit and dedicated signal checks. The pending Unix-runtime change documents why terminal fatal errors must not wait for the native no-op Carbon pump.

`HandleEvent` at 0x264d4 was read across running/system-lock, key/text, mouse, menu, application and window branches. The 11-entry mouse jump table at 0x328228 was decoded: button-down/up are kinds 1/2, motion 5/6 and wheel 10, with other entries returning. Native `linux_input.c` supplies SDL key/button/UTF8/wheel/focus handling, rawmouse ownership and relative/menu-coordinate delivery; native display owns its window. No Carbon callback/vtable invokes the old dispatch. `ProcessAllEvents`/`Quit` compatibility symbols in `linux_common.c` are no-ops with no selected native source callers.

**Historical live splash gap (resolved by the batch19 follow-up below):** Mac `DrawSplashScreen` at 0x25cbc resolves `cod.png`, creates/release URL/provider/PNG objects and clears/draws the full display through a QuickDraw CGContext. The non-dedicated native startup still calls `Sys_ShowSplashWindow`, which calls the empty `linux_common.c` drawing provider. No launcher/bootstrap code or report establishes an equivalent. This is a known missing live implementation, left unchecked pending the appropriate native drawing owner; the empty provider is not classified as intentional.

The arm64 Release `cod2_macos_ded` target compiles and links with `taskpolicy -b nice -n 19` and two jobs. All six inactive-source configurations match the assignment base. No new tests, full CI, replays or pushes were run. Source changes are compiled and reviewed, not exercised. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-lifecycle.noindex/`.

## Integrated batch 13 verification

Engine source 28b9601 passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both d873794 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.

## Batch 19 follow-up

The former live splash gap is now reconciled by Mac-Tools-MacGameEngine-startup-splash.md: the native AppKit BMP owner uses the canonical post-parser fs_basepath, hides at renderer takeover and owns destruction. This follows the Windows bitmap policy with an explicit native timing/lifetime adaptation. Actual startup visibility remains compiled/reviewed only.
