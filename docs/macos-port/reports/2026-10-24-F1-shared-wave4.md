# F1 combat, contact and missile integration

Baseline `bd18d2f`; implementation evidence is in the dated combat-trigger,
server-contact and missile module reports. This batch fixes thirteen confirmed
behavior differences across four engine sources. It is not completion of F1.

Both stock and CoD2x client/dedicated builds pass. All six damage groups, the
contact/visibility fixture and all nine missile groups pass ASan/UBSan. Existing
grenade integration, held-grenade death, use-list and snapshot fixtures pass
in both configurations. Four engine files are byte-identical to the baseline
with `COD2_X64` undefined. Whitespace checks pass. Linux object/binary identity
remains unavailable; known legacy ABI and inactive CoD2x baselines remain red.

Live acceptance used read-only installed data, separate private homes and
unique localhost ports, with one game running at a time:

- Stock Toujane: 2000 frames, 3776 live player rows starting at frame116,
  zero script errors in private qconsole_mp.log.
- CoD2x Carentan: 600 frames, 824 live player rows starting at frame112,
  zero script errors.

The stock replay first differs from wave3 at frame298 in quantized state;
raw player state first differs at frame349 (grenade damage/knockback). To locate
the first change, a private executable substituted only the pre-change missile
translation unit into the current stock build. No integration object, source
or binary was overwritten. Two short 295-frame replays produced final dumps
at frame298. The old-missile variant matches wave3's entire state/player prefix;
the current variant matches wave4's prefix. The only entity differing at that
frame is grenade117, in position, bounce velocity and spin. This attributes the
first changed live state to missile physics; it does not establish complete
original-server or Mac-runtime parity. The known earlier native/Linux replay
difference remains outside this batch's claim.

Private build, fixture and live evidence is under
`~/Library/Application Support/CoD2-native-decomp/F1-shared-wave4.noindex/`,
with focused regression evidence in F1-combat-trigger, F1-server-contact and
F1-missile sibling directories. The isolated comparison binaries and logs
remain private. Review outcome is recorded below before integration.

Review completed with no actionable findings. Correctness and a fresh local
adversarial review covered the bounded change; the attempted Claude peer
exhausted its turn budget without an artifact, so the required local fallback
provided the adversarial lens. Review was static; the checks above were run by
the implementing agents. Receipt: `20261024-wave4-4anwinae`. No review fixes
or unresolved findings remain for this batch. Broader F1/F2 work continues.
