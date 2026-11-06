# F1 tail motion and attachment failures

Moving tails now use the preceding frame's origin to calculate their endpoint,
and bolted tail endpoints are transformed into world space once. Particle,
tail, oriented-particle, line, cylinder, and cloud updates now return false when
an existing bolt is invalid or its bone orientation lookup fails, allowing the
normal FX lifecycle to remove them. Previously they continued as unbolted
effects using local coordinates.

## Mac evidence

The Linux 1.3 guide identified the candidates; the Mac assembly confirms each:

- `__ZN4Tail6UpdateEv`, `0xa5d1a–0xa5d35`, copies all three origin components
  into `mEndpoint` before `Particle::UpdateOrigin` at `0xa5d42`. Native omitted
  that copy, so a turning tail retained its original endpoint direction.
- `__ZN4Tail15CalcNewEndpointEPK13orientation_t`, `0xa12bc–0xa130a`, calculates
  the local endpoint and converts it into the renderer's world endpoint.
  Its caller invokes it at `0xa6076` and immediately returns true at
  `0xa607b–0xa6087`. Native instead transformed that result a second time.
- Tail's invalid-handle branch at `0xa6093` and failed orientation lookup at
  `0xa60b7–0xa60ce` return false. Particle has the same behavior in
  `__ZN8Particle6UpdateEv` at `0xa7c2a` and `0xa7c4e–0xa7c65`.
  A failed lookup still marks both bolt indices invalid, as before.

The same confirmed early-return correction now covers four further functions:

| Mac update symbol | Invalid-handle branch | Bone lookup | False return |
| --- | --- | --- | --- |
| `__ZN16OrientedParticle6UpdateEv` | `0xa7427` | `0xa744f` | `0xa746a` |
| `__ZN4Line6UpdateEv` | `0xa6431` | `0xa6459` | `0xa6474` |
| `__ZN8Cylinder6UpdateEv` | `0xa579d` | `0xa57c5` | `0xa57e0` |
| `__ZN5Cloud6UpdateEv` | `0xa6b1e` | `0xa6b46` | `0xa6b61` |

In each function, failed lookup marks both bolt indices `-1` before returning
false. Cached valid attachments continue normally. The correction adds only
native early returns after the existing lookup, before `Particle_UpdateOrigin`.

Emitter's attachment failure was corrected with its preceding caller update.
Light already matches the original (`0xa2d21`, lookup `0xa2d49`, false return
`0xa2d64`). Flash intentionally performs no bolt lookup: its complete Mac
update (`0xa2038–0xa20b3`) only updates normalized time and light color. Neither
Light nor Flash changes here.

This batch changes `Tail_Update`, `Particle_Update`, and the four update
functions in the table. Preceding particle velocity and emitter corrections
remain intact. This completes the bounded attachment correction, without
claiming the complete FX module is reconciled. Reference assembly stays outside
the repository in `F1-fx-tail-motion.noindex`.

## Checks

The existing production fixture adds two successive, perpendicular motion
steps for unbolted and rotated/translated bolted tails. Fixed numeric assertions
check previous local origins, current world origins, and world endpoints. A
zero-frame-time update preserves the rendered endpoint. Both update functions
also exercise an already-invalid bolt, a failed fresh bone lookup, and a valid
cached orientation, including no-render-update assertions on failure.

The original tail/particle fixture reported 24 failed checks before correction.
The additional four-function fixture executes each actual update with an
already-invalid attachment, failed fresh bone lookup, and cached valid
orientation. Invalid cases must return false without moving the local origin
or changing any renderer state; valid cases advance local x from 10 to 11 and
produce the fixed world origin `(80, 211, 330)`. All channel instances are
initialized by the real production initializers.

The new scenarios independently reported **24 failed checks** before the four
new guards (`red-attachments.log`); their cached-valid controls already passed.
The combined fixture passes with stock and CoD2x engine flags under ASan/UBSan,
retaining all preceding lifecycle, scheduler, particle-physics, and emitter
checks:

```sh
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-tail-motion.noindex/green-attachments-stock"
taskpolicy -b nice -n 19 python3 tests/platform/run_fx_primitives.py \
  --compile-db /Users/stix/Projects/cod2-native-wt/decomp-f1-script/build-macos-codx/compile_commands.engine.json \
  --output "$HOME/Library/Application Support/CoD2-native-decomp/F1-fx-tail-motion.noindex/green-attachments-codx"
git diff --check
```

`unifdef -UCOD2_X64` outputs compare byte-identically with `cmp` against
`bd18d2f`. This preserves the disabled source, not proof of unchanged original
i386 objects. Full builds, live game validation and reference captures remain
integration work; this worker did not run them.
