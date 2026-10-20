# Native recording queue copy offsets

The dormant recording queue applied the byte count twice when advancing a
`short *` source. A 161-sample input copied the wrong tail after its first
160-sample frame; exact-size inputs also produced an AddressSanitizer heap
buffer overread. Both copies now advance a byte pointer under `COD2_X64`.
The inactive legacy branches are unchanged.

The actual source passes 90 cases in both stock and CoD2x configurations, with
larger buffers and exact-size allocations, using engine optimization flags
plus AddressSanitizer and UndefinedBehaviorSanitizer. The owned original i386
function, an independent stream model and native output agree on samples,
volume, return values, offsets, partial buffers, encoded bytes and modeled
service order. The original source produces 65 mismatches per variant; both
exact-size reproductions abort under ASan. Restoring either faulty copy alone
produces 55 or 40 mismatches. Sixteen comparator controls reject altered or
missing observations.

The finite domain uses mono 16-bit PCM, 160-sample frames and small values.
Caller-range gains of 0.5–1.5 are covered; other gains are synthetic direct-call
stress cases. The campaign executes 154 of 164 original boundaries. Invalid
frame/length branches remain unexecuted. Codec, network and talk-state services
are models, and standalone floating-point controls do not certify the engine's
runtime state. No whole-function or whole-game equivalence is claimed.

This translation unit remains excluded from the macOS client. Microphone
capture remains disabled. The new data-free regression fixture is documented
in `tests/voice/README.md`; owned-binary comparison is optional and keeps
original-derived observations outside Git.

Twenty inactive legacy source checks pass. These checks do not prove x86
artifact equality; the existing multilib CI compiler failure remains unresolved.

Private verification: `phase-r3/20261020-recording-queue-repair-proof`.
Receipt SHA256: `366a361db1cbaf268543b26e5abfb931cb63f386767d84b77a5c19e1fb635d93`.
Seal SHA256: `154f6dde63bc13d4bf4466674426548bca91c2770f3736d33e3702acdc224fb5`.

A fresh parent replay reproduced all 24 observation files across four runs.
Replay receipt SHA256: `967c83d647a9cbc4e860f0eb2cf5012b5339d52132b5f5da1239954c8ef53f1b`.
