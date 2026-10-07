# WS55 — Level script order and initial registration publication

2026-10-07; base `644e5d4`.

Follow-up to WS54/FA04: Mac 1.3 executes game-type main, map main and the
startup callback in that order (`0x1ae0f5`, `0x1ae0fa`, `0x1ae0ff`). Native
`G_InitGame` had the first two reversed. These functions actually execute VM
threads, so their order can affect script-visible initialization, not merely
which script gets parsed first. Native startup now follows the reference.

The original also calls `SaveRegisteredWeapons` and `SaveRegisteredItems`
before returning from initialization (`0x1ae137`/`0x1ae13c`). Those calls are
restored after the existing end-of-initialization clear. The later conditional
frame calls remain: the reference also has frame-time calls at
`0x1af87c`/`0x1af886`. Thus the defect was missing initial publication, rather
than a need to remove frame publication. The real functions publish registered
names/item bits to configstrings and clear their pending flags.

The new `level_scripts` case in the existing fixes13 suite extracts the actual
startup and completion blocks, plus all three production script entry helpers
and both production registration publishers. A synthetic VM boundary records
thread handles and registers an authored weapon/item fixture. Before the fix,
the test independently reports both wrong callback order and missing initial
configstrings. Afterward, stock and CoD2x pass with AddressSanitizer, including
three thread releases, the weapon string, item bits, publication order and
cleared pending flags. The adjacent WS54 fog/objective regression still passes.
This executes production fragments, not the entire VM, map loader or live game.

Both actual Release game-main compilations pass. Ten legacy preprocessing
configurations have zero differences; source with native branches removed
is byte-identical to the baseline. Full composed live map/restart and
dedicated/listen checks remain integration work. Evidence is private in
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/` and
`/tmp/cod2-scripts-*.log`.
