# F1 shared wave6 integration

Baseline `d6b7d32`. The corpse landing buffer now holds all three output angles.
Four particle functions restore Mac stopping, bolt-space velocity and secondary
channel flag behavior. Module reports contain the interpreted Mac evidence.

Stock and CoD2x client/dedicated builds pass. Corpse and particle production
fixtures pass ASan/UBSan in both configurations after observed baseline failures.
The stock particle binary contains all new checks but predates the final success
message and relocation of an unused label into the legacy branch; neither
changed executed logic. The final CoD2x fixture and both full builds pass.
The two engine files remain byte-identical under `unifdef -UCOD2_X64` against
baseline. Whitespace checks pass. Linux binary identity remains unverified.

Native client acceptance uses a private copy of tools/macos/combat.py with a
60fps cap and distinct localhost ports. Each process uses a new private home,
read-only installed data and the shared GUI lease; only one game runs at a time.
The stock Toujane client completed one 34.2-second combat cycle with primary
and pistol firing, reloading, movement, a frag and smoke, then exited 0. Captures
show world geometry, textures, weapon, muzzle flash and smoke. Console output
has no script errors; it reports a missing `sound/misc/beep.wav`.
No original-renderer image equality is claimed by these native smoke captures.

Private evidence is under `F1-shared-wave6.noindex`, `F1-items.noindex` and
`F1-fx-primitives.noindex` in the established decomp evidence directory.
CoD2x client, server replay and review results are recorded below before
integration. The emitter follow-up and broader F1/F2 plan remain ongoing.

CoD2x Toujane also completed one 34.3-second combat cycle and exited 0. Its
smoke capture shows the map, weapon and active smoke; the console has no script
errors and the same missing beep sound. Both GUI leases were released.

The 2000-frame stock Toujane replay also passes: 3776 live player rows from
frame116, zero script errors in the private qconsole_mp.log, and both state
and raw player files identical to wave5. The corpse sanitizer fixture remains
the direct evidence for the landing-buffer defect; this replay establishes
that the recorded simulation is unchanged.

Correctness and an independent Claude adversarial review completed with no
actionable findings (receipt `20261024-wave6-g96pu3dc`). The review identified
a nonblocking question about bolted late-spawn catch-up, which is being checked
separately against Mac before any whole-module claim. An optional test using
different random weights for each channel was declined for this batch: those
reads are unchanged, while the corrected independent flags and coordinate
branches already have failing-before/passing-after coverage. No review code
changes were required.
