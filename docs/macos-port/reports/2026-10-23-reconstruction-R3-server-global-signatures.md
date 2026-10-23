# R3: full export with server signatures

The known server tail override and two aggregate signatures pass a prospectively
frozen full-export contract. Both the worker and fresh parent replay complete
554 probes, 8,115 ordered status records and 7,481 C outputs, with headless and
effective exit codes zero. Each audit passes 42,049 checks with no rejection.
This accepts the measured export surfaces; generation 15 remains authoritative.

The application uses the installed accepted decompiler and disposable copies of
the closed generation-15 project. The annotations and exact nine added type
records, including the unused `/undefined *32` dependency, are unchanged from
the [bounded preflight](2026-10-23-reconstruction-R3-server-dependency.md).
Known function records are projected through the existing full-export serializers
before execution. Unlisted changes remain rejected; no policy is relaxed after
either campaign.

Only the complete C outputs for IDs `15967e`, `159c8c` and `15a836` change, exactly
matching their three frozen hashes. Every other complete C output, final status
and diagnostic matches the accepted baseline. All 7,481 C functions complete on
their first 30-second attempt. The policy still permits a 90-second retry only
after first-attempt failure, with the unchanged 64 MiB limit and full attempt
records retained.

Two stored function records change as expected; old variables remain in exact
order after the new parameters. The parent independently binds the bytes of all
619,351 instructions to the original image and verifies the 389,849 reference
records against the previously accepted tail application. Relative to generation
15, the only instruction/reference change remains the known `CALL_RETURN`
override at `0x15b2ca`; signature annotation adds none. The 169,281 recursive-data
records pass the existing narrowly qualified default-type UID comparison.
Raw export records keep their original evidence bindings; a separate derived
ledger binds the final discovery records without promoting a generation.

The 100,589 exported symbols remain exactly equal and ordered. **This is a
memory/external-address census, not a complete database symbol census.** Parent
inspection of the installed source independently confirms that
`getAllSymbols(true)` reaches `getAllMemoryAndExternalAddresses()` through its
address iterator. Existing function snapshots record parameter names, types,
storage, source, first use and comments, but omit complete variable symbol IDs.

The following remain unmeasured: parameter/local symbol IDs and full records,
complete database label history, and internal datatype identities, source-archive
fields and timestamps beyond the historical serializer. A new instrumented
application must query function namespaces, `Variable.getSymbol()` and complete
`getLabelHistory()`, with a separately frozen datatype metadata contract if that
coverage is claimed. These omissions are not waived by the passing export audit.

Both campaigns use `-readOnly`: source and copied closed projects remain
byte-identical, and the retained copies do not contain the transient annotations.
A future metadata audit must apply them again in a fresh session. No native
source or installed tool changes, and no generation promotion occurs here.

Ten effective static comparator negatives, an unchanged positive and a distinct
ineffective-mutation check pass, including a parent rerun. These are comparator
controls, not JVM mutation campaigns. The worker retains one failed post-run
helper caused by a syntax typo. The first parent inventory read failed before
any replay because its reader assumed an object rather than a list; a separately
saved one-line reader correction preserves every hash, census and mode check.
Neither failure changes the campaign harness or frozen acceptance policy.

Private evidence is under `reconstruction/phase-r3/`:

- Worker: `20261023-server-global-signature-application`, inventory SHA-256
  `f508fdeef46b2b70993bbd1e6c228b967e801b903fe4d898a29418e9a8f2e1b2`.
- Parent replay: `20261023-server-global-parent-fresh`, inventory SHA-256
  `384af30e9384d374cc49d2feea914e475a67f423ec9cacebec358fbd9720974a`.
- Independent audit: `20261023-server-global-parent-independent`, inventory
  SHA-256 `d7b9cae420cd1d62a9cc65ae7f80dfb8632b7bf5fcb9ea8c4119fc2d11e00a5c`.
- Exact scope-source bindings, controls and command receipts:
  `20261023-eighteenth-wave-parent-verification`.

Native full suites were not repeated for this report-only change. The completed
CI run at code commit `455333e` passed its harness and Apple Silicon jobs; its
x86 comparison job failed while compiling the old baseline, before comparing
objects. The overall workflow success is not byte-identity evidence.
