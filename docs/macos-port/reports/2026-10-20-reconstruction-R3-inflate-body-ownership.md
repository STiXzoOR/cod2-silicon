# Original inflate function-body ownership

An independently checked, unapplied proposal assigns 510 already decoded instruction records, totaling 1,772 bytes, to their original inflate functions. The proposal adds no instructions, removes no data or references and changes no native implementation.

| Original function | Existing records to include | Body bytes to add |
| --- | ---: | ---: |
| inflate_blocks | 494 | 1,720 |
| inflate_codes | 16 | 52 |
| **Total** | **510** | **1,772** |

Paired original debug records establish both function extents. Neither has a matching or overlapping unwind FDE; the complete unwind inventory and this absence remain explicit. LLVM and Capstone agree on 1,876 original records. The proposal retains all 339 local/incoming references and the complete relevant identities and histories. The parent checked candidate ownership against all 8,115 existing function bodies and independently verified original candidate bytes, ordered switch-table entries and debug extents.

Static control-flow paths are qualified by normal call return and preserved ABI/frame/memory state. Fifty-eight proposed records depend on computed callbacks returning. Nine initialized callback cases cover 50 original instruction addresses; 1,826 decoded addresses remain unexecuted in this unit. The synthetic services demonstrate their stated return mechanics, without implementing real allocation, free or unwind behavior. These limits do not become full-function execution claims.

Fresh parent replay reproduced all 12 evidence files and rejected eleven semantic and source-binding mutations. The repaired packet pins consumed source contents against the original commit while recording current Git state separately. All eleven original substantive evidence files remain byte-identical. The parent verified all 76 manifested files of the repaired unit and all 134 manifested files of the preserved original unit. The native platform intentionally uses SDK zlib, whose parity is not established here. No native tests were run because this unit changes private evidence only.

Private evidence identifiers:

- Unit seal: `039d559a716b02e417f51d4a237c767b1cddf3847804467f0e2d515a0c9a9802`.
- Output manifest: `3f4248d177696cef3d2510e2b8a1552e12adea06291094c25e85b2de15b94d4c`.
- Unapplied proposal: `5c2c21e368b41440c9ae6452d69eca495724da9f02e1271a509cbd968ddfee8f`.
- Parent acceptance: `c635e1545e16b8905083ffe4f003d873fc48d327418ddcdf79e4d57e8581b255`.

A separate metadata preflight and application are required before these body unions enter an accepted reconstruction generation. No unknown-byte denominator is reduced by this proposal. R3 and full reconstruction remain incomplete.
