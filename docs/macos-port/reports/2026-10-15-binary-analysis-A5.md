# A5 — bounded integration verification

2026-10-15. Existing independent reference contracts and production-source
fixtures passed on source-tree object `4ec6695155930f24b78a31fb389755e9085cd6f8`.
No runtime source changed and no entire binary function was newly certified
semantically equivalent by this pass.

Verification includes the owned-reference water/lighting contracts, native water
and fixes13 fixtures, six fidelity groups, shader setup, 24 renderer sanitizer
fixtures and three FX suites. Stock and CODX water/fixes13/fidelity runs passed.
The renderer checks cover state, buffer representations, material/font loading,
shader arguments, image formats, visibility, lighting and render-target math.
These bounded checks do not exercise live GPU resource retirement.

GSC storage, VM and compiler checks, movement, collision, audio and message
reference checks passed. Audio compared ten cases and message serialization
compared 32 cases with zero differences. Protocol, runtime-policy, demo and
playback fixtures passed. The policy fixture uses its existing non-native stub
configuration; it is not proof of a live native network session.

Two initial protocol/demo harness invocations failed because of incorrect
fixture macros/include paths. Restoring the repository runners' intended flags
passed; both initial failures and corrected commands remain in private receipts.
Script runners were adapted in memory only to redirect build paths privately.
Their original hashes and adaptations are recorded. Fixture assertions were
enabled separately from the preserved engine compile database.

A complete fresh stock arm64 `cod2_macos` build passed. It used the host SDK 27.0;
the linker warned that the installed SDL library targets macOS 27 while the
application targets 26, and reported an alignment reduction. Consequently this
is a successful build, not macOS 26 packaging certification. A full CODX engine
build was not performed; CODX coverage here is the named fixture suites.

Private commands, logs, exit statuses and domain coverage are retained in the
external workspace under `receipts/A5-*` and `analysis/behavior-coverage.json`.
The core tooling, fidelity tooling and ABI unit suites also passed. The A4
round-trip and ABI results are complementary evidence, not additional behavior
proof inferred from decompiler output.

Remaining integration checks are explicit: original-i386 runtime differential
execution, Metal lifecycle and visual parity, screenshot/map-loading comparisons,
long gameplay/network/demo soaks and deployment-compatible packaging. No game
was launched. This phase delivers the available offline verification pass;
these live checks must accompany the corresponding Metal implementation slices.
