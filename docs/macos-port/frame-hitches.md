# Local slow-frame diagnostics

When local match summaries are enabled, `match_summary.c` consumes the preceding
frame's renderer counters at the same boundary as its raw frame time. Each active,
non-demo frame longer than 100 ms writes one `CoD2 slow frame:` event to stderr,
including elapsed match milliseconds, frame number, duration and renderer work.
Capture stderr when investigating a session.

The existing `main/stats/matches.jsonl` record also stores `hitchCount`,
`hitchesOmitted`, `hitches`, and aggregate `rendererActivity`. A hitch has `atMS`,
`frame`, `msec`, and `renderer`. Each renderer entry is `[callCount, cpuWallMS]`:

| Entry | Work measured |
| --- | --- |
| `programUpload` | ARB vertex/fragment program submission to the GL driver |
| `textureUpload` | 2D/3D texture creation and updates, including compressed uploads |
| `textureCreate` | D3D texture, cube and volume creation |
| `targetCreate` | D3D target and platform color/depth/MSAA allocation |
| `resolve` | Full MSAA framebuffer resolve |
| `screenCopy` | Framebuffer-to-texture copies |
| `present` | SDL OpenGL swap |
| `videoRestart` | An accepted `vid_restart`, including teardown/recreation |
| `draw` | Native indexed draw preparation and submission, including driver waits |

The timings are CPU wall time, not GPU execution time. Categories can overlap
(for example a texture upload inside a draw, or allocations inside a restart),
so do not add them. The draw category includes first-use driver work that may be
deferred beyond program upload. An unexplained remainder can still be gameplay,
network, filesystem, OS scheduling, or renderer work outside these call sites.
No shader warm-up or other speculative hitch fix is applied.

The match record retains up to the first eight hitches, fewer if escaped server
metadata needs the existing 8192-byte line budget. `hitchesOmitted` makes that
limit explicit. Every hitch still goes to stderr; preserve the engine log for a
complete trace. Existing schema-1 readers can ignore the additional fields.
The existing 200-match retention and atomic publication are unchanged.

Ordinary frames perform no diagnostic file writes. Counters use monotonic clocks
and relaxed atomic additions; standalone renderer tools have an unattached
collector. Startup `+set cl_matchSummaries 0` disables both timing and output for
the entire process, respecting the existing frozen local privacy choice.
