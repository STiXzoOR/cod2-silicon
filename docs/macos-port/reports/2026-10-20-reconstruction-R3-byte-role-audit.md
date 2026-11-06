# Remaining original executable-region byte roles

The generation13 audit accounts for all 10,473 still-unknown bytes without removing any from the denominator. It retains all 4,621 region histories, 8,115 function histories and the complete instruction/reference census. This is static evidence; no native or reconstruction-project behavior changed.

| Original location | Unknown bytes retained |
| --- | ---: |
| TEXT outside sections | 1,858 |
| TEXT text | 4,601 |
| TEXT coalesced text | 229 |
| IMPORT jump-table slots | 2,534 |
| IMPORT outside sections | 1,251 |
| **Total** | **10,473** |

The audit preserves 786 references into unknown spans: 749 DATA references and 37 calls. DATA provenance includes 716 original instruction operands, 29 serialized symbol-table fields and four other mapped words. Numeric equality does not establish an executable root. All three stored fallthroughs into unknown spans remain explicit, including an instruction decoded from two zero-valued bytes between two original function extents. The preceding original instruction jumps elsewhere; that alone does not prove the gap unreachable or authorize deleting the stored record.

The inherited import-placeholder classification retains all 633 slots and its three metadata discrepancies. It does not establish live loader binding. NOP encodings, section-gap placement and lack of matching function extents remain qualified observations. No unknown bytes are certified as padding or unreachable; arbitrary computed-target reachability remains unresolved.

Fresh parent replay of the repaired packet reproduced all 21 stable evidence files exactly and rejected nine semantic mutations plus source-write controls. The repair separates observed current Git state from the original dispatch identity; all 17 original substantive artifacts remain byte-identical. The parent independently checked original section/file mapping, all 10,473 per-byte records, complete reference and instruction streams, raw function extents, all incoming-reference identities and unchanged histories. The repaired sealed unit contains 64 manifested files and preserves all 78 files of the original unit. No native tests were run because this unit changes only private evidence.

Private evidence identifiers:

- Unit seal: `b1c17561e91480bc6f2eb6e5d53afdacc8c3cc5dd7561cf601bfcc144c45e7e7`.
- Output manifest: `bff63703e0ad1650e756d2dc5d401858b81ef4b759ac44d258ac828cdb112525`.
- Parent acceptance: `bed082c133ad7ffa4c34b71faa4be37cd407cf258647d88769bca9f7ba6d5828`.

R3 closure and whole-game reconstruction remain incomplete. The audit supplies provenance for subsequent decisions and authorizes no metadata mutation.
