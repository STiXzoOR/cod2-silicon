# R3 generation 13 preflight

The parent accepted an exact preflight combining the four separately verified
cleanup proposals against generation 12. The disposable application creates
268 instruction records covering 880 bytes, replaces five old records and one
four-byte pointer definition, and updates nine function bodies. It classifies
862 previously unknown bytes. Its projected totals are 619,324 instructions
and 10,473 unknown bytes across 4,436 intervals. Generation 12 remains the
accepted exported baseline until the separate generation 13 campaign completes.

All 619,056 unaffected instruction records, 389,760 references, 108,649 DATA
references, 8,115 function identities and 4,621 original region histories are
preserved. The 79 added references match original operands. Parent review
confirmed 28 new default labels and 13 display-only changes with unchanged
symbol identities. Clearing the conflicting pointer definition preserves its
dynamic symbol and incoming/outgoing DATA records.

Two unwind calls receive only conditional, local no-return analysis overrides;
their original CALL bytes and global callee metadata remain unchanged. The
cursor instruction matches its independently derived complete processor record.
Two computed calls remain targetless, and excluded trailing NOPs remain unknown.
A separate query captures four additional typed conflicts without changing them.

Original-byte and control-flow checks cover all proposed instruction boundaries
and 51 ordered exception-table rows. The inherited finite service, ABI and
unwinding qualifications remain binding; the VAO proof does not export a complete
stack-write trace.

Worker and parent fresh external replays reproduce the complete metadata and
exact changes, with all eight commands and 16 mutation negatives passing.
Every source and copied project file remains byte-identical. Parent verification
also checks all 297 worker files and their read-only modes. An initial query
helper compilation failure is preserved and was rejected by completion checks.
Parent acceptance receipt SHA-256: `05b9711035ab76b379ba8036ffe8b0b98957b814d6f093bcb5be9822afbea8eb`.

No native implementation changed, so native suites were not rerun for this
preflight report. Complete export, generated-C effects, downstream impact and
whole-process validation remain open. This does not certify generation 13,
Metal, or completion of R3–R6.
