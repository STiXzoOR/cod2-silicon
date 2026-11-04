# cgame/offhandweapons_mp module reconciliation

Updated 2026-11-05.

**9/9 reconciled: 5 equal, 4 fixed.** Mac 1.3 i386 is the authority. The complete `CG_DrawOffHandAmmo` caller routed from `universal/com_vector` is included. Every function was read against the Mac instructions; the legacy bodies are unchanged.

## Corrections

The name, icon and ammo draw functions omitted the fourth color component. Mac stores the fade result into alpha at 0x1b4cf4, 0x1b4fec and 0x1b53bc. The native paths now assign `drawColor[3] = fade`, removing uninitialized stack values from those draws. Their weapon/class/availability gates, rounded fade durations, RGB selection, icon/text arguments and inclusive clip-ammo sums were also compared.

`CG_UseOffHand` selected the local viewmodel using `cg->snap`. Mac 0x1b4e0e reads `nextSnap`, at original `cg+0x24`. The native path now uses that snapshot before choosing viewmodel/world tag position or trajectory fallback.

## Verification and limits

The two flash dvars match Mac defaults, ranges and flags. The highlight's empty-ammo color and cosine pulse arithmetic match, and all three `offhandStrings` entries match the binary table at 0x34d100. The full-width sound-alias pointer repairs from acccfb7 remain in both prep/use functions; the 32-bit truncations were not restored.

The regular client object compiles without warnings with generated arm64 Release `cod2_macos` flags. The required low-priority two-job dedicated build also passed, but dedicated excludes cgame. Call/string triage and constant checks ran for every assigned row; exact-entry checks confirm their ledger addresses. All five legacy preprocessing configurations match the base, with real line expansion checked separately. `git diff --check` passes.

The four corrections are compiled and reviewed but not exercised in a running client. Grenade HUD fading and local/remote grenade sound origins remain for parent integration validation. No new tests, full CI, replays, pushes or shared-header edits were made. Nothing remains unchecked.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-cgame-weapons.noindex/`.
