# xanim/xanim_load module reconciliation

Updated 2026-11-04.

**4/4 reconciled: 3 equal, 1 fixed.** Mac 1.3 is the authority. The native loader and both quaternion readers were compared with the complete Mac bodies and inline blocks, including notification loading. Linux supplied split helper names only.

The delta quaternion and translation frame counts now use unsigned comparisons, matching Mac 0xba4af and 0xbadd1. The previous signed comparisons selected the wrong index stream when a frame count crossed 32767.

Indexed channels now reserve `sizeof(channel) + 2 * (count - 1)`, including the Mac tail space. Mac uses bases 10 for quaternions and 14 for translations, while copying indices at offset 8. Native had retained only the index offset in its allocation sizes. The native formula accounts for pointer alignment and preserves the existing frame-zero allocation fixes. Both byte-index and short-index branches were checked.

The comparison also covered version 14, flags, loop-frame adjustment, frequency, bone names and bitsets, every static/animated quaternion and translation branch, quaternion sign continuity, notification types/times, the `end` sentinel and file cleanup. Native hunk callbacks zero their allocations; no indeterminate storage was introduced. Corrupt high-bit bone counts and overflowing notification counts remain outside the valid asset/allocation contract.

The arm64 Release `cod2_macos_ded` target compiled at low priority with two jobs. Changes are compiled and reviewed, not exercised. No tests or replays were added or run. Preprocessed output with `COD2_X64` undefined is identical to the base source.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-xanim-model.noindex/`.
