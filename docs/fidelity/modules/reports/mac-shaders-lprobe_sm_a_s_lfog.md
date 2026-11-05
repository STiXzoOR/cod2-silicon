# Mac/Shaders/lprobe_sm_a_s_lfog reconciliation

Updated 2026-11-05.

1 entry remains unchecked: D3DXCompileShader at Mac 0x118aa6. This ledger attributes the whole compiler/map initializer to a shader filename; it is not a standalone shader implementation.

The original entry begins embedded shader-map initialization. Its lookup tail at 0x134129–0x134353 treats the first argument as a filename, appends a to .vs names (n on card enum 2) or e to .ps names, and returns an owned buffer containing the lookup key, separator and payload. A missing key still returns success with null shader and clears the other outputs.

Actual pending shader source 51608e5 uses lp64_shader_cache.h and locally extracted vsa/pse programs with constant-table markers; native shader extraction reads the original table and verifies an 834-asset manifest. This is a concrete native provider, but full lookup/table/vendor parity was not established here. Missing or invalid cache entries return 0x88760b59, whereas the original missing-key path succeeds with null output. Without the cache, D3DXCompileShader still selects generic HLSL templates. These specific contract gaps remain unresolved; future Metal plans do not remove them. No shared shader source or original payload was copied into this change.

The parent owns provider integration and runtime validation. No compiler parity or full-table manual comparison is claimed, and no new tests, full CI or replays were run. Private evidence: ~/Library/Application Support/CoD2-native-decomp/recon-mac-color-math.noindex/.
