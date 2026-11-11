# cgame/cg_shellshock module reconciliation

Updated 2026-11-04.

**6/6 reconciled: 1 equal, 5 fixed.** The six functions were compared by hand with the Mac 1.3 i386 disassembly. Native changes are guarded by `COD2_X64`; the legacy source is unchanged.

## Corrections

**Camera perturbation had no data and an invalid matrix layout.** Native declared a zero-filled `cg_perturbations[131]`, so the interpolation in `CG_UpdateShellShock` could never produce a camera kick. The native table now contains the engine constants at Mac 0x33c1a0, including the three wraparound pairs. All 262 float values were checked, and the compiled client object's table matches them. `CG_PerturbCamera` now supplies one contiguous matrix with forward, cross and up rows, matching the stack basis passed to `MatrixMultiply` at 0x1d6ccd. Three separately declared vectors did not establish that layout.

**The sound envelope selected the wrong phase.** Mac 0x1d7574–0x1d759e uses the fade-in/full-volume branch while the remaining sound time is at least the fade-out duration, then divides the remaining time by that duration. Native selected the branches in reverse. Abort playback now uses `shellshock_end_abort` (Mac string 0x2e36d4), and the silent loop uses `shellshock_loop_silent` (0x2e36fc), replacing the different native alias names. The zero-loop-fade branch deliberately continues using the sound envelope value, as Mac 0x1d7abd does.

**Saved-screen fading used a different arithmetic order.** Mac 0x1d6ee1–0x1d6ef5 divides remaining time by fade duration before multiplying by effect time. Native multiplied first. The division now happens first, with the same half-unit addition and `floorf` conversion.

**Two smaller branches differed.** The default-file failure now prints the complete diagnostic referenced at 0x1d6e54. The view-kick rate comparison at 0x1d6fb7–0x1d6fc6 now retains Mac's 1.0 fallback for unordered inputs. The 3000 ms view fade remains unchanged.

## Coverage and native boundaries

`CG_SaveShellShockDvars` matches the 29-name serialization, path formatting, open failure, write length and close sequence at 0x1d6cda. All 29 names at Mac 0x34d680 match native in order, as do all 29 dvar sources read by `CG_SetShellShockParmsFromDvars`. That setter's millisecond conversions, room-name bound, float-bit copies, channel placement and clamping were also compared. The update function's reset, loop-end timing, mouse fade, smoothstep amplitude and cubic interpolation were compared in addition to the corrected paths.

The loader retains its documented native copy of the requested filename before file services and dvar parsing can reuse `va` storage (WS41). The sound calls retain the pointer-width API and `imp_vec3_origin` listener address established by WS12. The providers in `snd.c` and `win32/snd_driver.c` are live engine functions feeding the existing native audio backend; this reconciliation does not replace that backend. Screen saving/blending and turn limits use the actual `cl_cgame_mp.c` providers. No shared header or provider changed.

## Verification and limits

The arm64 Release dedicated target built at low priority with two jobs. It excludes cgame and sound, so the changed client translation unit was also compiled separately with its actual Release compilation command; it produced no warnings. The existing ASan `shellshock_file` fixture passed unchanged, exercising the loader's stable filename and the 29-name table/count.

Five inactive legacy preprocessing configurations match the baseline, and `unifdef -UCOD2_X64` output is byte-for-byte identical. This file has no `__LINE__` uses. No new tests, full CI, replays, client link or game session were run. The audio, screen-blend, camera and unordered-input corrections are compiled and reviewed but not exercised. Ordinary bounded engine clock/duration values and the stable client-global lifetime are assumed, as in the existing native callers. Nothing remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-shellshock.noindex/`.

## Integrated batch verification

Integrated source 6b334b3 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
