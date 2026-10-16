# Recovered Windows reference versus native Classic

2026-10-16, reconstruction R5 diagnostic capture slice. Full reconstruction,
retail Steam startup and Metal certification remain incomplete.

After [recovering the earlier Windows reference](2026-10-16-reference-recovery.md),
three native CoD2x runs loaded Toujane DM, selected the British team and Enfield,
and captured the requested courtyard, rooftop and sky views. All native runs
exited through `quit` with status 0. Their exact executable SHA-256, arguments,
screenshots and draw traces are retained privately.

## Configuration and diagnostic results

Native execution used the R2 CoD2x arm64 binary and the existing Classic OpenGL
renderer. The existing shader extractor rebuilt the private Mac shader cache
from the owned i386 reference. This is not a Metal test. The Windows side uses
the diagnostic 1.3 executable with CoD2x and the recovered Wine/D9VK environment.

Both sides requested 1280x720, FOV 80, picmip 0 for all texture classes, gamma 1,
one AA sample, hidden HUD/gun and noclip. The second native run supplied graphics
settings before initialization and waited for a draw-trace receipt at each pose
before requesting its framebuffer screenshot.

The existing `compare_images.swift` compares every RGB pixel without alignment,
cropping or colour correction. These are diagnostic image distances, not pass
thresholds or percentages of engine equivalence:

| View | Native run 1 MAE / 255 | Native run 2 MAE / 255 | Run 2 RMSE / 255 |
| --- | ---: | ---: | ---: |
| Rooftop | 1.787312 | 1.787266 | 4.119476 |
| Sky | 0.768325 | 0.768325 | 1.491516 |

A third native run captured courtyard last, after eye-height settling. Its
draw origin is exactly `[2569, 2274, 182]`, matching the delayed Windows
`viewpos` receipt. The resulting courtyard MAE is **2.092074 / 255** and RMSE
is **3.957709 / 255**. Native runs 1 and 2 instead recorded Z=181.897598 and
180.688507 for that first capture; their courtyard scores are excluded from
accepted-position comparisons. Longer elapsed time alone is not an acceptance
check: verify the recorded origin.

Native rooftop and sky traces report origins `[3051, 2178, 221]` and
`[3051, 2178, 351]`; Windows `viewpos` reports the same integer positions.
Exact Windows pitch/matrix values have not been independently captured. Both
rooftop commands requested yaw 180, but integer `viewpos` output is 179 on
Windows and 180 on native. Do not turn approximate camera correspondence into
an exact-input equivalence claim. Courtyard has the same integer yaw discrepancy.
Sky uses held lookup on Windows and explicit
pitch -85 on native. Clouds may also differ with time.

## Rejected evidence and procedure correction

The initial Windows courtyard capture was taken at its spawn position
`1430 330 129`, despite requesting `2569 2274 181`. The first camera command
was issued during the spawn sequence. Its screenshot looked valid, but its
`viewpos` receipt invalidates the comparison. Two native runs reached the
requested courtyard location and rendered a different scene. This mismatch is
not evidence of a native renderer defect.

The initial courtyard reference and the console-covered first rooftop image
remain private as rejected evidence. Capture acceptance must verify the actual
camera after spawn, not merely that a screenshot exists or that a command was
sent. The delayed Windows courtyard recapture reports `[2569, 2274, 182]` and
is the reference for the third native run's comparison above.

## Retail investigation remains separate

A virtual desktop alone did not repair the current Steam environment. Direct
launch additionally required the owning Steam client's `Steam.dll` and the game
working directory. With those resolved, DX9 created the graphics device and
render targets and initialized sound, but still entered Wine's crash handler.
Skipping the intro did not resolve that failure. No additional renderer change
is justified by calling every startup failure a black-screen graphics defect.
The exact cause remains unresolved, with failed logs preserved.

## Private evidence and next gates

The analysis workspace's `reconstruction/ws8-recovery/` contains
`native-codx-captures/`, `native-codx-captures-02/`, `native-codx-captures-03/`,
the shader cache, reference captures, `native-reference-comparison-02.json`
and `courtyard-comparison-03.json`. Retail follow-ups are in
`reconstruction/phase-r5/`. None of these binary-derived artifacts is published.

Next gates are accepted camera-state receipts, richer paired draw/state traces,
combat and lifecycle scenarios, and integration of the preserved Metal backend
against current engine behavior. Three close still images do not certify those
behaviors, all maps, untouched retail, or the original Mac application.
