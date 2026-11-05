# Mac/Tools/MacFolders reconciliation

Updated 2026-11-05. **9/9 reconciled: 9 intentional.**

Mac 0x8bf6 obtains an executable parent HFS directory ID. Mac 0x8dce searches the localized data folder beside executable/application directories. The other seven exports wrap cached volume/bundle FSRefs or convert those paths to FSRefs. All nine bodies were inspected, including lookup failures, the 0x400-byte item wrappers, slash/backslash handling and the 0x50-byte bundle-reference copy.

The native launcher supplies `fs_basepath` and `fs_homepath` in `launcher/Core.swift`; `mac_common.c` and the filesystem use POSIX paths. No external native source caller reaches any MacFolders export. Generated native data was preprocessed: the apparent `virtualKeyConvert` relocation to `GetDataFolderItemRef` is in the pointer-size-4 fallback only. LP64 emits scalar key bytes.

The retained functions include empty success stubs and old process/catalog declarations; they are not working LP64 folder providers. In particular, restoring a CF bundle return in another unit does not validate their int-typed CF declarations. SDK URL application launching is owned by unix-runtime; its pending b8facc4 handles the packaged outer-app layout. That dependency was inspected read-only and is not part of this commit. No old HFS provider or launch source was edited.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.
