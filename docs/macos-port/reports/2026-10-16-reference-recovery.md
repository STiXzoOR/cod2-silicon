# Windows rendering reference recovery

2026-10-16, `codex/metal-u1-integration`, starting at `10f6a4a` with a clean tree.

The earlier WS8/WS15 Windows rendering reference runs again in a separate
private environment. Menu rendering, Toujane DM client initialization, joining
a team, selecting a weapon, camera commands and three game framebuffer captures
were exercised. This completes a reference-recovery slice of R5, not the R5
campaign or full reconstruction.

## What was recovered

The missing original setup was documented in [WS8](WS8-wine-baseline.md) and
[WS15](WS15-render.md). The recent Steam investigation had failed to reuse it.
The recovered configuration uses:

- Sikarugir Wine 10, Highball engine `x64-sikarugir10.0_6-r19`.
- Official D9VK `v1.10.3-20250511`, 32-bit D3D9 DLL.
- The existing diagnostic Windows CD 1.3 executable and CoD2x 1.4.6.8.
- A private copy of the owned game assets and a 1280x720 Wine virtual desktop.
- The unchanged `tools/wine/play-cod2x.sh`, with `--reference --renderer d9vk
  --desktop 1280x720 --resolution 1280x720 --dx9 --local --fullscreen --fps 60`.

The D9VK archive, CoD2x release archive, diagnostic executable and both Miles
proxy/original DLLs match all corresponding SHA-256 values in WS8. The launcher
clears the Wine builtin marker only in its copied renderer DLL, as already
documented. The diagnostic executable is unchanged. Highball 0.10.15's existing
CLI installed the pinned r19 manifest; this is not a claim to have recreated
every byte of the old prefix. No source renderer implementation was changed.

## Observations

The game rendered profile selection and loaded `mp_toujane` deathmatch.
`CL_InitCGame` reported 62.83 seconds; this is one initialization observation,
not a benchmark. Console commands selected the British team and Enfield, enabled
noclip and hid the HUD and gun. Game-generated JPEGs at 1280x720 visibly contain
world geometry, textured surfaces, lighting and sky:

- `recovery-courtyard.jpg`: requested pose `2569 2274 181 180`.
- `recovery-rooftop-clean.jpg`: requested pose `3051 2178 220 180`.
- `recovery-sky.jpg`: requested position `3051 2178 350 0`, then held/released
  lookup. Pitch has not been independently measured for comparison acceptance.

An earlier rooftop capture contains the console bar and is excluded. New
captures have not been scored against native output or the missing old images.
Menu interaction and clean capture are verified; combat, network interoperability,
repeatability, lifecycle and long-run stability remain unverified.

## Evidence and remaining diagnosis

Private evidence lives under the analysis workspace's
`reconstruction/ws8-recovery/`: pinned installation logs, identities, launch
command, console log, capture configs and `reference-recovery-result.json`.
The recovered prefix and all game imagery remain outside Git.

The failed retail attempt and recovered reference differ in executable, patch,
renderer, engine revision and desktop configuration. Success here does not
identify which difference caused retail failure. The recent retail logs showed
repeated fullscreen-mode failures followed by a Metal residency-set assertion;
other attempted settings exposed an error-formatting page fault. Retail diagnosis
must isolate those differences rather than call this recovery a Steam fix.

This is a Windows visual reference with CoD2x loaded. It is not untouched retail
execution, Mac i386 process execution, or evidence of complete native equivalence.
