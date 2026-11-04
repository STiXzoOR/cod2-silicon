# cgame/cg_draw_mp module reconciliation

Updated 2026-11-04.

**27/27 reconciled: 18 equal, 7 fixed, 2 intentional.** Every body was read against Mac 1.3, including the whole routed `CG_DrawTurretCrossHair` caller at 0x1cf59c. The register-attributed crosshair-position function was read directly because the extraction tool could not find its native definition. Constant/call triage was used to find candidates, not to certify equality.

`CG_Draw2D` had omitted the Mac caller's damage flash and directional damage quads (0x1d12d1–0x1d15fd), plus stationary grenade warnings (0x1d22de–0x1d2a33). The native path now includes the inspected filters, scope gates, pulse, pointer pivot, icon geometry and materials. Its lagometer now draws separate columns, clamps snapshot bar height and colors delayed snapshots yellow. Stat monitor icons regain the 32×32 two-column layout at 0x1d190a. Vote text uses the reference 5/24 scale and voted flag 0x100000. Snapshot/FPS stacking, expired center-print clearing and the notification fade gate also match the caller.

The crosshair had discarded the overlay-opacity return and instead multiplied by the weapon position or ADS transition. It now uses the returned opacity at 0x1d0810/0x1d0a53 and restores the ADS pitch offset at 0x1d100d. The enemy-red arm was absent from both Mac and the checked Windows color helper at 0x4c8d50–0x4c8d9e; both use friendly green or white. Chat uses the Mac 0x3e555555 font constant, rather than 1/3. FPS palettes were zero-initialized; the native arrays now carry the inspected faded colors. The stale OpenGL/D3D statistics gate was removed; native `refexport.TrackStatistics` remains the backend boundary.

Timed menus now refresh low health on the correct side of the threshold and skip fading it in that branch (0x1ce263/0x1ce60d). Attack input no longer falls through to a second weapon notification. Camera shake phase arguments and phase randomization now preserve Mac double arithmetic before float storage (0x1cec24–0x1ced6d).

`CG_DrawFollow` now reads the next snapshot, as 0x1ced9a/0x1cedb5 do. History showed that its old current-snapshot reads came from the initial reconstruction; the sole production caller already requires and dereferences `nextSnap`, and `CG_DrawActiveFrame` rejects a null next snapshot. The existing follow-policy fixture was corrected in its two setup fields. The CoD2x name policy and radar remain intact, as do the existing typed sound, shellshock and material interfaces.

Two storage adaptations are intentional. Negative crosshair client indices retain the existing native rejection before indexing `clientinfo`. The shake temporary now starts at zero: Mac can copy indeterminate size/rumble bytes when duration or strength is rejected, and native does not import those bytes.

The arm64 Release client translation unit compiled with stock and CoD2x flags. The dedicated target linked, but excludes this client module. All eight existing HUD fixture groups passed with ASan/UBSan in both stock and CoD2x modes; seven camera and five active-frame groups also passed. The newly restored damage/grenade, compass-independent HUD, chat, crosshair, FPS and shake paths were compiled and reviewed, not exercised. No new tests, full CI or replays were run. All five inactive source configurations match the baseline, and this source contains no `__LINE__` expansions.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-view.noindex/`.
