# Mac/Tools/MacFolders reconciliation

Updated 2026-11-05. **9/9 reconciled: 9 intentional.**

Mac 0x8bf6 obtains an executable parent HFS directory ID. Mac 0x8dce searches the localized data folder beside executable/application directories. The other seven exports wrap cached volume/bundle FSRefs or convert those paths to FSRefs. All nine bodies were inspected, including lookup failures, the 0x400-byte item wrappers, slash/backslash handling and the 0x50-byte bundle-reference copy.

The native launcher supplies `fs_basepath` and `fs_homepath` in `launcher/Core.swift`; `mac_common.c` and the filesystem use POSIX paths. No external native source caller reaches any MacFolders export. Generated native data was preprocessed: the apparent `virtualKeyConvert` relocation to `GetDataFolderItemRef` is in the pointer-size-4 fallback only. LP64 emits scalar key bytes.

The retained functions include empty success stubs and old process/catalog declarations; they are not working LP64 folder providers. In particular, restoring a CF bundle return in another unit does not validate their int-typed CF declarations. SDK URL application launching is owned by unix-runtime; its pending 5ee587b handles the packaged outer-app layout. That dependency was inspected read-only and is not part of this commit. No old HFS provider or launch source was edited.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.

## Integrated batch 13 verification

Engine source f0d941a passed both dedicated builds, local CI 02–07/09–13, native-only source guards, parent/fresh independent review and both 2,000-frame replays. Initial CI 08 failed three existing display smoke assertions: interval 0 now requests synchronized interval 1. The isolated fixture correction passed fresh independent review and all three sanitizer cases; after integration the complete CI 08 retry passed both client-variable modes. Only that fixture changed after the engine builds/replays. Its six expected guard differences are separate from the native-only source changes. No changed source uses __LINE__. Replays exactly match both 819d282 baselines and each other in players/state hashes/entities; all six Linux logic fields agree, with maximum origin drift 0.11792 and velocity drift 2.0. Existing audio/voice/thread sanitizer fixtures support their tested paths; the existing thread target required SDL linkage. No new tests or i386 byte-parity claim.

This platform batch closes 471 entries and leaves 11 explicit timing/splash/lighting/mutex/ring/FBO boundaries unchecked. The separately reviewed AbsoluteTime follow-up is not included in this source revision. Dedicated replay does not exercise client audio, dialogs, display hardware or lifecycle/error branches; unreached corrections remain compiled and reviewed. Hosted optional ILP32 comparison fails compiling its unchanged reference; native validation is distinct.
