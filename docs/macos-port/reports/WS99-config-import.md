# WS99 — Config Studio I1 import model

2026-10-09. Worktree `config-import-i1`, branch `codex/config-import-i1`, based
on `1a2c0da`. Model layer only. No UI, store writes, engine changes, build-list
changes, or changes to `Core.swift` / `Model.swift`. Nothing pushed, tagged,
released, or merged.

## Implementation and contracts for composition

- `ConfigImport.swift`: `ConfigImportInput` accepts selected file, selected
  folder plus explicit entry path, paste/drop bytes, and remote URL.
  `ConfigImportLoader.load(_:dialect:)` returns source provenance, F1 documents
  indexed by root-relative path, and explicit unresolved dependency receipts.
  Text bytes are preserved; paste/drop callers should use F1's
  `ConfigDocument.encode` for Latin-1 text instead of normalizing existing bytes.
- `ConfigImportSafety.swift`: `ConfigImportSafety.review(_:)` returns original
  logical commands with byte ranges, safe/needs-OK/blocked classifications,
  transitive effects, dependency names/definition locations, `importable`, and
  `selectedByDefault`. Every command splitter/tokenizer and effective argument
  join uses F1. The bind-length calculation measures F1's already parsed tokens.
- **F4 seam:** adapt `ConfigCatalog` to `ConfigImportCatalog.setting` and
  `.command`, respecting stock/CoD2x availability. Setting metadata supplies
  archive/protection flags and optional `inert`; command metadata supplies
  registered bindable status. `ConfigImportCatalogSnapshot` can decode the
  machine-local research JSON as a fixture. No research JSON or private config
  is bundled or committed. Known absent names from Appendix B and common
  foreign-name families are labelled “Has no effect in CoD2”, off by default;
  the `inert` field lets F4 extend this without mislabelling arbitrary helpers.
- **I2 apply contract:** use `importable` as well as classification. Unknown
  forwarded commands remain review material and are never importable, including
  when reached through a bind/variable. No raw `exec` reference is importable,
  even when resolved: apply the reviewed dependency commands, or materialize a
  fully reviewed launcher-owned chain and rewrite references explicitly. Route
  every confirmed write through F2, including locks, running-game ownership,
  expected disk bytes and one History transaction. Safe selection is a default,
  not permission to bypass those checks.
- Display `candidate.details` / `effect.details`: credential-bearing parents
  and children are redacted there. Original documents and token bytes remain
  available for byte-preserving review; do not log or render those as a substitute
  for redacted details. Nested effect ranges address decoded buffers identified
  by `location.context`, not original-file byte offsets.
- `WindowsProfiles` offers bounded discovery from mounted volumes or selected
  folders. Known Activision/Steam install paths include the user's specified
  `Program Files (x86)/Activision/Call of Duty 2/main/players` layout. Active
  profiles sort first; numbered/Raptr/`.cfg.bak` backups are explicitly optional.
  It reads selection metadata, not config contents, and skips symlink escapes.
  I2 can feed each candidate's URL through the ordinary file-import review.

## Decisions and bounds

Credential/CD-key/path assignments are blocked regardless of Import everything.
This includes password variants, login-bearing scripts, hardware identifiers,
and credentials embedded in URLs. The explicit “passwords never applied” rule
wins over §11's illustrative needs-OK mention of `rconpassword`. Ordinary rcon
commands, name/chat/connect/quit/write commands, download settings and
`unbindall` need OK. FOV, cheat-protected, read-only and write-protected settings
are non-importable. Both generated `unbindall` lines remain separate candidates.

Variable analysis follows every collected definition, including bare writes,
toggle alternatives and `setfromdvar` copies. It does not execute source text or
simulate server state, dvar coercion or source-order mutations. Immediate
exec/vstr recursion is separated from inspection of stored commands: legitimate
per-press `seta` pointer cycles stay intact, while actual recursive calls are
blocked. Conditional/state-dependent scripts can still be conservatively
rejected. Plus-action tails are flagged for replay on release; generated
key/time-only commands after trailing separators are also rejected. Their effect
preview uses an explicitly illustrative timestamp. B2 owns the full bind preview.

Default bounds: 65,535 bytes/file, 262,140 bytes/bundle, 64 attempted files,
8,192 collection commands, 16 stored-text collection levels, 32 expansion levels
and 4,096 recursively assessed commands/candidate. Reaching a classification bound makes the affected
candidate non-importable. Source-level aggregate/dependency limits throw.
Windows discovery scans at most 256 directory entries and 32 volumes/selected
roots, without a recursive volume or personal-directory search.

Network retrieval is injectable and separate from classification. The default
fetcher streams through an ephemeral URLSession without cookies, cached data or
stored URL credentials, enforces size limits while reading, and checks redirect
origin/root before following it. HTTPS and HTTP on exactly localhost/127.0.0.1
are supported. Ordinary download query parameters are supported and are not
inherited by exec dependencies; recognized credential parameters are refused.
HTML (including BOM-prefixed HTML), binary/archive inputs, traversal and
out-of-root redirects are rejected. GitHub blob/raw URLs use owner/repo/ref as
the dependency root (a single-component ref); ambiguous slash-containing refs
need additional root metadata. Gist user/id and raw links import the selected
raw response and pin siblings to its returned revision folder. No repository
archive extraction or web-page scraping is implemented.

## Verification

The focused runner compiles Swift 6 with complete strict concurrency, warnings
as errors, macOS 13 target and `-j 2`. Compiler and test execution run under
`taskpolicy -b nice -n 19`. All fixture trees live under the runner's trapped
scratch root so failing assertions also clean up. No tests fetch the network or
inspect the real Windows volume.

```sh
sh tests/launcher/config_import.sh
sh tests/launcher/config_import.sh --corpus \
  /Users/stix/Projects/cod2-native-wt/_agents/research/corpus/windows-players \
  /Users/stix/Projects/cod2-native-wt/_agents/research/dvars

taskpolicy -b nice -n 19 sh tests/launcher/config_document.sh \
  --grammar /Users/stix/Projects/cod2-native-wt/_agents/research/grammar/fixtures \
  --corpus /Users/stix/Projects/cod2-native-wt/_agents/research/corpus/windows-players

taskpolicy -b nice -n 19 sh tests/launcher/run.sh
git diff --check 1a2c0da HEAD
git diff --name-only 1a2c0da HEAD -- src build CMakeLists.txt \
  launcher/Core.swift launcher/Model.swift scripts/build-launcher.sh
```

- Focused synthetic safety/dependency/source/Windows discovery checks pass.
  Red runs were observed for missing classifications, stored-script credential
  wrappers, true-vs-per-press cycles, bind boundary handling, catalog inert
  metadata, URL credentials, BOM HTML, encoded traversal, URL query handling
  generated event commands and protection-label precedence. Receipts are in the evidence directory.
- F1 passes its document regression, 75 independent grammar fixture checks and
  both-dialect byte round-trip of all 34 corpus files (33 cfg + one txt),
  **581,727 bytes**, with no content printed/copied into git.
- The corpus review runs each file independently; it does not fetch unresolved
  exec files or execute commands. Both dialects give the same fixture-catalog
  result:

| Dialect | Safe | Needs OK | Blocked | Importable | Effects | Cycle candidates | Limit candidates |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Stock | 6,836 | 10,439 | 2,235 | 16,730 | 116,318 | 0 | 24 |
| CoD2x | 6,836 | 10,439 | 2,235 | 16,730 | 116,318 | 0 | 24 |

These are static classification counts, including review-only comments, not a
claim that every config is importable or has exact runtime semantics. The 24
limit candidates remain blocked, rather than weakening the safety bounds.

**Broader launcher suite is red:** core, real local UDP exchange, artwork,
fonts, shared styling and macOS 13 view typecheck pass. The harness rendered
26 launcher captures and reported 200 shapes / zero findings. The first Python
snapshot dimension assertion fails: `home-dark.png` is **2824×1764**, while the
accepted dimensions are 1440×900 or 2880×1800. One assertion stops the suite;
remaining Python image/fallback checks were not reached. No snapshot gate was
changed. These captures use original fallback art, not game assets.

## Preservation, evidence and handoff

The restricted-path diff command above prints nothing: zero engine files or
forbidden integration files changed. There is no touched engine file requiring
an `unifdef -UCOD2_X64` / `cmp` proof. No compiled i386/native-OFF equivalence or
full composed engine gate is claimed by this model-only unit. The supplied wave
baseline remains visible: legacy-ABI 10 differences (generated native data ×5,
excluded by CMake OFF; test-only server control ×5), inactive gates 436 intentional
stock/native/synthetic-defined-zero differences, and unavailable Linux x86
multilib comparison. Those gates were not rerun or weakened here.
`COD2X_HOST_REVISION=0` is unchanged.

Evidence is under
`~/Library/Application Support/CoD2-native-current-readiness/WS99.noindex/`:
`import-corpus.log`, focused/red receipts, `document-regression.log`,
`launcher-regression.log`, and launcher captures under
`launcher-regression.noindex/snapshots/`. Temporary compiler outputs and failed
synthetic fixture trees were cleaned up. No QA `.app` was created or registered;
no game process, bench lease, licensed data write or real app-data write occurred.

Code commits, in order: `601bb58`, `4d2a634`, `1131e3d`, `af50679`; this
report follows as a separate DCO commit.

No I1 blocker. Root integration must add the three new launcher files to its
build list, connect F4's adapter and I2/F2 application flow, and retain the broader
launcher snapshot failure above. All unit changes and this report are committed
with DCO; nothing is left uncommitted.
