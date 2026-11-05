# gfx_d3d/r_surfbounds module reconciliation

Updated 2026-11-05.

**4/4 reconciled: 4 equal.** Mac 1.3 i386 is the authority.

All four bodies were hand compared: model, cached, triangle and dispatched bounds match. The dispatch table at Mac 0x34a420 was decoded from binary data: null at 0–2, model at 3–4, cached at 5, triangles at 6 and null at 7. Entity clamping to 2046 and sort-sign selection match. The triangle body was restarted at its exact symbol at 0xfb6e4 to avoid a full-disassembly alignment error.

## Verification and limits

No new tests, CI or replays were run. The arm64 Release dedicated target builds. These bodies were reviewed against the binary and did not require code changes. The eight changed source files have identical port-disabled preprocessed bodies in five configurations; none uses `__LINE__`. Current GL behavior was audited; Metal plans do not retire these paths.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-gfx-world.noindex/`.
