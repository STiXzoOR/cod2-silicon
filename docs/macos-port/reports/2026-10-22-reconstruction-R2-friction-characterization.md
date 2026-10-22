# R2 friction characterization — 2026-10-22

An independent parent replay reproduces 61 exact-state differences between
original Mac MP 1.3 `PM_Friction` and each native build. This accepts the bounded
characterization, not a correction or whole-game equivalence. The source baseline
is `aca861317791e8d0a3534bf9b42061908f1a7895`.

The frozen domain contains 672 combinations of movement flags, timer boundaries,
slowdown enablement, velocity and frame time, plus 128 walking, surface, movement
type and flag combinations. All 800 states have initialized objects and valid
dvars. Both stock and CODX match 739 cases and differ in the same 61:

- 33 velocity differences where both landing and jump flags are set.
- 20 differences where native code clears the jump flag and jump origin at timer
  1801, while the original preserves them. The timer itself remains unchanged.
- Eight velocity differences for initialized type-4 walking states. Live gameplay
  reachability of those states is not established.

Original instructions test the landing flag before the jump flag and execute
`Jump_ReduceFriction` only in the latter arm. Native code currently applies both.
The original also permits ordinary ground friction for the initialized type-4
walking states before adding flight friction. The separately verified flight
coefficient is unchanged by this characterization.

The original binary is SHA-256
`15efec53aa25a3bc686745edbb32ad4b565b9e82606e1cc5349c61753a708f12`.
Raw text symbols and paired STABS function bounds, matching FDEs where present,
and two instruction decoders bind the tested definitions. All 103 friction and
32 reduction-helper instructions execute across the domain; no imported call is
modeled. The original helper's x87 return and caller's float store execute.

Native probes include complete production movement, jump and slide sources,
using stock/CODX engine flags with contraction disabled, assertions and ASan/UBSan.
Comparison checks exact float bits, flags, timer and jump origin, plus full
architecture-specific allocations, guards, permitted writes and FP controls.
Ten controls exercise altered results, missing cases/traces, sentinel corruption,
an effective mutation of original mapped data and an ineffective mutation.

A separate diagnostic runs actual native `PmoveSingle` with modeled flat-ground
collision and fixture services. Its production ground, timer, walking and jump
helpers reach friction with both flags set and timer 1127. Original friction on
that captured state differs in velocity. Original caller flow is corroborated
statically, not executed; this is not a live original-game comparison.

The parent freshly executes the entire 800-case original/native campaign and
caller diagnostic, then independently reconstructs the domain, checks raw bytes,
complete state and return ABI, and recomputes mismatch partitions. Sixteen complete
observation artifacts match the worker byte-for-byte. Caller static corroboration
also matches after separately validating its output-root path and identical target
content. A first overly strict path comparison failed and is retained. Worker
setup/compiler failures and its reuse of the completed original observations are
also retained; the parent campaign reused no original execution results.

Private evidence remains under the analysis store's `reconstruction/phase-r2`:

- `20261022-friction-original-native-characterization`: worker seal
  `2daa15a0c98fea8c9d88f0ebc29e4fa6027af1f0ee8273ce0dcf464c4a88ab9f`.
- `20261022-friction-parent-authoritative-replay`: parent replay seal
  `618be645a0a0e75c4cbbe95859b43e45f9119d0f1594d41e8f554cc27d9c2e71`.
- `20261022-friction-parent-audit`: independent audit seal
  `01c60f55f82d9d66de1f801c3ccf4475136be650e29f01138ec435f9ddb6c2ae`.

No production source, accepted discovery generation or denominator changes here.
The next unit is a native-only correction with regression evidence. Legacy object
identity and whole-process behavior remain separate, unproved gates.
