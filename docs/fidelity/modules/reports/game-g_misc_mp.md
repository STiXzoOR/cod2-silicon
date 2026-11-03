# game/g_misc_mp module reconciliation

Updated 2026-11-03; baseline `af7d4c6`.

**28/28 reconciled: 21 equal, six fixed, one intentional omission.** All Mac exports and retained Linux helper boundaries were compared. Corrections restore a turret-use angle threshold and five fatal diagnostic prefixes; motion, ownership, firing and animation behavior otherwise match within the stated domains.

## Corrections

The turret-use test now rounds the acos result to float, multiplies by the original double degree constant, then rounds back to float. A finite normalized-dot case demonstrates why this matters: the original threshold is 0.8103586435317993 degrees while float-only multiplication produces 0.8103587031364441, rejecting a player whom the original accepts. The original clamp selection and final greater-than rejection also preserve unordered comparison behavior. Separate synthetic nonfinite tests cover that arithmetic edge; they are not evidence of ordinary nonfinite gameplay geometry.

Five diagnostic sites regain their original leading 0x15 byte: turret pool exhaustion, unknown weapon, missing flash tag, and both empty animation-child errors. Successful spawn and firing behavior remains unchanged.

The unreferenced Linux use_corona helper remains intentionally omitted. Mac SP_corona frees the entity immediately; its sole use_corona symbol marker is an undefined zero-address exception-frame name, not a callable body.

## Complete-body decisions
### Fire_Lead (equal)

Complete inline dispatch0x1bd57e–1bd70d plus rocket0x1be02d/worldfallback0x1be0c3 compared: NONE pointer remaps to WORLD, populate parms, get weapon, type0 bullet with spread/level.time otherwise rocket spread0, event0xaf with chosen attacker number. Native helper-call boundary unchanged; corrected nested diagnostic is recorded on Turret_FillWeaponParms and full Mac export. Firing fixture covers bullets, rockets, fallback and event arguments; Linux third unused argument is absent from Mac inline semantics.

### G_ClientStopUsingTurret (equal)

All296 Mac bytes0x1bc070–1bc197 compared: owner lookup, sound reset, prevStance -1 skip or2/1/default events142/141/140 then-1, TeleportPlayer(userOrigin,currentAngles), ps eFlags clear0x300/viewlock0/entity1023, owner active0/otherEntity0, turret active0/owner1023/flags clear0x800. Native side-effect order and typed field accesses match. Stance/control fixtures cover all event paths and retained unrelated state. Both-mode nine-group fixture passes (18 outcomes total); see use.md for service-seam and finite-domain limits.

### G_FreeTurret (equal)

All90 Mac bytes0x1bc198–1bc1f1 compared. Resolve owner; call stop iff owner.client exists; then active0, current pTurretInfo inuse0 and pointerNULL. Native rereads current info after subordinate call asMac. Owned/unowned fixtures cover complete branches. Both-mode nine-group fixture passes (18 outcomes total); see use.md for service-seam and finite-domain limits.

### G_InitTurrets (equal)

Complete26-byte Mac0x1bbcc8 clears inuse at32 slots with68-byte stride and leaves remaining fields untouched. Native typed32slot loop matches. Fixture seeds allbytes and verifies only inuse fields clear.

### G_IsTurretUsable (fixed)

All396 Mac bytes0x1bc42a–1bc5b5 compared, including inline turret_behind. Active ormissinginfo rejects before geometry; behind result then grenadeTimeLeft nonzero and groundEntityNum1023 reject. Native angle computation corrected to roundedfloat Q_acos -> double multiply -> float, and rejection predicate matches Mac JA. Finite boundary regression failsbaseline inbothmodes; remaining eligibility controls pass. Both-mode nine-group fixture passes (18 outcomes total); see use.md for service-seam and finite-domain limits.

### G_PlayerTurretPositionAndBlend (fixed)

Complete inline Mac positioning region in turret_think_client reviewed: animation guards0x1bcf6b–1bcfb2, body0x1bd052–1bdb99 and branch islands through0x1be084. Two no-children errors now include0x15 literal0x2e1638. Tree selection, quaternion conversion, ordered dot/height/yaw blending, goal-time arithmetic, both vertical/horizontal interpolation branches, transform/trace correction and state/link order match. Three error groups and five control groups pass both stock/CoD2x client/dedicated; baseline both no-children branches fail. Finite initialized animation domain and fatal Com_Error contract assumed.

### G_SpawnTurret (fixed)

Complete1361-byte Mac0x1bc84a checked including all out-of-line branches and literals. Firstfree32slot search,68-byte logical reset,weapon/class/precache errors,stance/sound setup,optional spawn-key fallbacks/clamps,healthdefault100,damage/playerSpread,flags/contentstype,bounds,origin/angles,handler13,nextthink+50,trajectory3 andlink match. Restored missing0x15 prefix on exhausted-pool and invalid-weapon errors at literals0x2e14ac/0x2e14e0. Both baseline modes fail those exacttext cases; all ten focused spawn/teleport/helper groups pass current modes. Clock validity follows bounded server lifecycle.

### SP_corona (equal)

Complete9-byte Mac0x1bbcbe tailcalls G_FreeEntity; native matches. No live corona toggle handler registered.

### SP_info_notnull (equal)

Complete29-byte Mac0x1bbc8a calls G_SetOrigin(self,self+0x138 currentOrigin); native typed field matches.

### SP_info_null (equal)

Complete9-byte Mac0x1bbc80 tailcalls G_FreeEntity with unchanged argument. Native same.

### SP_light (equal)

Complete9-byte Mac0x1bbca8 tailcalls G_FreeEntity; native identical argument/effect.

### SP_misc_model (equal)

Complete9-byte Mac0x1bbcb2 tailcalls G_FreeEntity; native matches.

### SP_turret (equal)

Complete77-byte Mac0x1bcd9c asks weaponinfo with emptydefault, fatal missingkey literal0x2e15c0 already includes0x15, then passes obtainedpointer to G_SpawnTurret. Native matches. Missingkey diagnostic fixture passes baseline/current.

### TeleportPlayer (equal)

Complete210-byte Mac0x1bbf9e preserves linked byte before unlink, copies origin sequentially then adds1 to Z, toggles eFlags bit2, sets supplied view angle, converts playerstate with snap1/handler1, copies currentOrigin and relinks only if originally linked. Native pointer reloads/call and copy order match. Both link-state controls cover intermediate effects and final origin.

### Turret_FillWeaponParms (fixed)

Complete inline Mac region0x1bd594–1bd6a3 and fatal island0x1be089–1be0be compared. Restores0x15 tag_flash error prefix from literal0x2e1668. Tag lookup, player origin/direction, gunForward copy, normalize distance and muzzleTrace multiply/add order match. Missing-tag baseline fails exact diagnostic; final both-mode client/dedicated passes. Bullet/rocket controls verify resulting parms.

### turret_ReturnToDefaultPos (equal)

Complete instantiated Mac preparation at0x1bbd7d–0x1bbd83 chooses dropPitch and zero yaw before the full angle update. Native unmanned call matches. Only native call passes0; retained manned zero-pitch branch and return forwarding match complete Linux helper. No standalone Mac symbol is claimed.

### turret_UpdateSound (equal)

Complete Mac inline blocks 0x1bbd41–0x1bbd79 and0x1bbec8–0x1bbef2 match: clear loopSound, return when delay<=0, load unsigned byte fireSnd, subtract50, stop iff new delay<=0 and byte stopSnd nonzero, clear loopSound then G_PlaySoundAlias. Retail STABS confirms delay offset40 and sound bytes64/66. Sound callback can replace pTurretInfo; native caller reloads it through ReturnToDefaultPos just as Mac reloads EDI after the call.

### turret_UpdateTargetAngles (equal)

Equal on the Mac instantiated unmanned path: complete inline region 0x1bbd8a–0x1bbec6 in turret_think. Save old pitch, add residual pitch, start both speeds at200, raise vertical speed to360 iff flags0x200 and0x100, multiply each by exact binary float0x3d4ccccd, AngleSubtract and signed clamp, save target pitch, apply pitchCap under0x200/0x400 and clear0x100 on reaching threshold, second pitch clamp, store pitch plus residual. Operation order and flag writes match. No native caller supplies bManned1 or observes completion result; the retained manned branch and completion bookkeeping match the complete Linux helper, without inventing a standalone Mac body.

### turret_behind (fixed)

Whole Linux helper maps to Mac inline0x1bc454–1bc587 inG_IsTurretUsable. Arc halfspan, center yaw, normalized forward and horizontal turret-minus-owner direction, dot andclamp compared. Mac storesQ_acosfloat at0x1bc56f, CVTSS2SD/MULSD(double57.29577951308232 from0x340c20)/CVTSD2SS at0x1bc572–57f; native priorfloatliteral rounded upward atfinite arc boundary. Restored pipeline and final !(angle>yawSpan). Native subtraction/select clamp now also preserves Mac unordered result -1, using originalDot in both comparisons. A separate nonfinite arithmetic edge case fails baseline Q_acos input assertion and passes current bothmodes; it also checks unordered halfspan acceptance. This does not claim nonfinite geometry arises in normal gameplay. Both-mode nine-group fixture passes (18 outcomes total); see use.md for service-seam and finite-domain limits.

### turret_clientaim (equal)

Complete inline0x1bce40–1bcf6a: set full viewlock/entity, subtract player/turret pitch and yaw, clamp each to turret arcs, zero roll, consume flag0x800 and xor teleport bit2. All native fields/flags match. Finite ordered-angle/arc domain (no arbitrary NaN equality claim); clamp/toggle/in-range fixture passes all four native variants.

### turret_controller (equal)

All166 Mac bytes0x1bbef8–0x1bbf9d compared: construct pitch/yaw/zero angles, get server DObj once, pass same obj/partBits/local buffer to tag_aim then tag_aim_animated, replace pitch with angles2[2] and yaw0, pass tag_flash. Retail STABS maps tags158/160/140 (16-bit) exactly. No null-DObj guard exists in either body; callee contract retained.

### turret_shoot_internal (equal)

Complete inline0x1bd550–1bd578 plus post-fire0x1bd712–1bd718: reload weapon firetime, store3*firetime sound delay, fire only for client owner, then set weapon-jitter viewlock2. Native source boundary matches; nested diagnostic correction attributed to its owner/export. Fixture covers client and nonclient invocation. Valid weapon timings avoid signed-overflow domain.

### turret_think (equal)

All533 Mac bytes0x1bbce2–0x1bbef6 compared: nextthink=level.time+50, optional G_GeneralLink for nonzero tagInfo, owner client test using typed native entity stride, immediate return for client owner, otherwise sound update, clear firing bit0x40, then return-to-drop-pitch update. Inline helpers reconciled separately; field/callback ordering matches. Initialized entity/owner/turret domain and normal server clock bounds assumed.

### turret_think_client (fixed)

All932 Mac instructions/4836bytes0x1bcdea–1be0cd compared, including seven inline helpers and sound tail. Owner index uses typed entity stride; active must exactly1 and sessionState0 else stop/return. Track then update sound matches. Full export includes three corrected0x15 diagnostic sites in positioning/weapon-parms helpers.32 focused outcomes pass ASan/UBSan across stock/CoD2x client/dedicated; twelve baseline error outcomes fail and20 controls pass.

### turret_think_init (equal)

All567 Mac bytes0x1bc1f2–1bc428 compared: handler14 andnextthink=time+50 before missingtag returns, aim thenbutt tags, baseworldmatrix, butt-minusaim displacement, 31 angles0..-90 in-3 increments, tag-relative transforms, trace mask0x811/callerentity/bulletpriority, firstfraction<1 storesdropPitch. Otherwise retainpriorpitch. Missingtag and hitat0/7/30/nohit fixtures cover loop endpoints. Finite geometry, validlevelclock and fatalcallback contracts assumed. Both-mode nine-group fixture passes (18 outcomes total); see use.md for service-seam and finite-domain limits.

### turret_track (equal)

Complete enclosing inline track sequence: cache turretinfo, aim/position, reload weapon, set viewlock1, clear firingbit0x40, subtract50, retain positive delay, otherwise clamp0 and fire only attackbit1; set weapon delay before shoot and firingbit after. Mac0x1bcfb8–1bcfed and0x1bd52d–1bd726 match. Fixture checks cooldown, ready shot, release/no-shot and viewlock transitions. Corrected subordinate positioning diagnostics recorded separately and on export.

### turret_use (equal)

All660 Mac bytes0x1bc5b6–1bc849 compared: active/owner/viewlock, flag0x800, userOrigin copy, reciprocalotherEntity, prone-beforeduck previousstance, stanceeflags, subtraction-based ordered pitch/yaw clamping, zero roll, halfarc viewranges and normalizedbases. Native matches allbranches and floatoperation order. Nine stance combinations plus lowpitch/highyaw limits verified. Both-mode nine-group fixture passes (18 outcomes total); see use.md for service-seam and finite-domain limits.

### use_corona (intentional)

Unreferenced Linux-only toggle helper omitted. Complete Linux body toggles link and resets active on relink; only its definition appears in source. Mac SP_corona frees immediately and no handler references corona. Mac has only undefined zero-address use_corona.eh nlist marker (type18,section0,value0), not an executable function identity. Native omission preserves authoritative spawn behavior; no live Mac toggle body claim.

## Verification and limits

Seventy focused outcomes pass with assertions and ASan/UBSan: 20 spawn/helper/teleport outcomes, 18 use/lifecycle/geometry outcomes, and 32 firing/animation outcomes. The firing cases cover stock and CoD2x client and dedicated targets; the other groups cover both modes. Baselines reproduce all five diagnostic-site failures plus the finite-angle and synthetic nonfinite regression. Controls cover all stance restoration branches, 31 drop-pitch trace samples, eligibility, allocation/fallback/clamping, complete firing arguments, ownership guards, animation blending and trace-adjusted positioning.

Spawn tests compile the complete production translation unit. Use and firing fixtures extract complete production bodies after actual-target preprocessing and supply external service mocks. These bounded tests establish behavior at service boundaries; they do not claim visual turret/player alignment or live turret interaction acceptance. Unused manned motion helper variants are reconciled against the full Linux helper; the instantiated unmanned Mac path is compared in its complete export. No standalone Mac identity is invented for an inline helper.

Both stock and CoD2x client/dedicated builds, scoped ABI audits, and both legacy checks across ten inactive configurations pass. OFF source tokens are preserved; no i386 binary identity is claimed.

Both final-binary 2,000-frame stock and CoD2x replays match the preceding native trace and each other exactly: state hashes, entity counts and all recorded player fields. Original-reference first differences remain velocity frame 102, origin 104, quantized state 116, pm_flags 652, viewangles 776 and pm_time 1803; entity counts, pm_type, health, weapon and weaponstate still match. This corpus does not prove live use-boundary or visual animation acceptance.

One fresh local review has no findings. Receipt: `/tmp/compound-engineering-501/ce-code-review/20261103-misc-e69a4879/review.json`. Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/g-misc-20261103.noindex/`. Original parity and whole-project reconciliation remain open.
