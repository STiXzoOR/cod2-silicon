# F1 attachment and filesystem integration

Baseline `e31321c`. This batch restores six effect update returns when an
existing attachment is invalid, takes the tail endpoint from the preceding
motion position, and removes a second world transform. It also fixes the
filesystem short-path lookup base and the zero-byte general IWD seek result.
The command module reconciliation report confirms existing behavior and its
intentional native/CoD2x differences; cmd.c does not change.

Worker fixtures observed 24 failures for tail/particle motion and another
24 for the other four attachment paths. The filesystem fixture observed
14 failures covering lookup and stored/deflated archive seek statuses.
The integrated stock and CoD2x FX and filesystem fixtures pass under
ASan/UBSan, including the preceding emitter overflow regression. Both full
client/dedicated builds pass. Both changed engine files remain byte-identical
under unifdef -UCOD2_X64 against e31321c; whitespace checks pass.

Private evidence: F1-shared-wave8.noindex, F1-fx-tail-motion.noindex and
F1-files.noindex. Module reports record original Mac evidence and exact
fixture commands. Original i386 object comparison remains unavailable.
Both stock and CoD2x clients completed one scripted Toujane combat cycle
(34.2 and 34.1 seconds), including firing, reloading, movement, fragmentation
and smoke grenades. Both shut down cleanly with no script errors. Logs retain
the existing missing beep, missing UI materials and unknown shock dvar
warnings. Smoke screenshots show the map, weapon and smoke rendering in both
builds; these are smoke checks, not original-reference capture comparisons.
No server replay was repeated for this client FX and filesystem batch.

The actual five-rubric simplification pass made no edits. Four proposed proof
comment removals were skipped because the proof files were protected.
Actual ce-code-review completed with a Ready to merge verdict and no
introduced findings (receipt 20261024-wave8-x0vgfwny). The local reviewer
and independent Claude pass examined the ten changed files.

The zero-motion tail question was closed against the original instructions:
CalcNewEndpoint branches directly to return at 0xa1286 when length is zero;
Tail::Update snapshots origin before motion and returns after that helper.
Preserving the prior rendered endpoint is original behavior. Review also
identified a separate pre-existing Cylinder endpoint defect: it uses the
Tail helper and transforms twice, whereas the Mac constructs origin plus
length times its forward axis and transforms once. That correction is the
next active FX unit; this batch claims only Cylinder attachment-failure parity.
Original-server and original-renderer parity remain unverified.
