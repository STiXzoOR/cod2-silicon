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

## Integrated batch verification

Integrated source 6b334b3 passed both dedicated builds and every local CI step 02–13, including stock/CoD2x Release clients, platform sanitizer fixtures, function/callback ABI audits and existing source fixtures. All nine changed production source files differ only under linux-x86_64-port-on; the five inactive configurations remain unchanged. The existing owner-draw fixture correction applies across configurations and its runner extracts the required production helpers; no new test cases were added. No changed source contains line-number diagnostics or static assertions. These checks do not establish i386 binary-byte parity.

Parent inspection covered all six units; independent reviews covered protocol/snapshots, HUD, weapons/offhand and shellshock, including their relevant providers. Review corrected five HUD fade paths and a weapon diagnostic newline. Parent checks of the small effects foundation/parser changes included original arithmetic and loader storage boundaries. Nineteen effects identities or malformed-EOF boundaries remain unchecked rather than inferred from neighboring code.

Both 2,000-frame stock and CoD2x replays exactly match both 819d282 baselines and each other in players, state hashes and entities. All six logic fields agree with the original Linux recording; maximum origin drift remains 0.11792 and velocity drift 2.0. Dedicated replay does not exercise client HUD, camera, sound or weapon rendering; unreached corrections remain compiled and reviewed.

Catalog coverage passed without semantic catalog changes. Hosted ILP32 comparison remains unavailable because the unchanged reference fails to compile; no byte parity is claimed.
