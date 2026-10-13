# WS59: required script loading failures

2026-10-07. Current-release fidelity follow-up to WS44 FA14, based on
`88a0b28`. Only native required-script checks change; no 0.3 feature is added.

The original Mac 1.3 `GScr_LoadScripts` checks the load and handle result for
`codescripts/delete`/`main`, `codescripts/struct`/`initstructs`, and
`codescripts/struct`/`createstruct`. Independent inspection of the owned
reference binary confirms the six result tests at `0x1a0364`, `0x1a0382`,
`0x1a039c`, `0x1a03ba`, `0x1a03d4`, and `0x1a03f2`; each failure invokes
`Com_Error` with code 1 before storing the corresponding handle or advancing
to gametype setup. The optional map script at `0x1a0446` and handle at
`0x1a0456` have no equivalent required-result checks.

The native implementation already has `GScr_LoadScriptFunction`, used for
required gametype scripts. The three mandatory built-in loads now use that
same checked helper under `COD2_X64`. Original inactive source remains intact.
The optional map script retains its existing behavior. Script compilation
lifecycle remains in `G_InitGame`; it is not duplicated here. CoD2x's separate
custom callback registration is outside this correction.

`tests/fixes13/script_load.c` executes the active production loader and checked
helper. Script services are synthetic: each load/handle boundary can fail,
while assertions check request order, error severity and script/label names,
no later gametype/class/field registration on required failure, and all final
handles on success. All six required failures were ignored by the baseline;
the same assertions pass after the correction in stock and CoD2x with ASan.
Optional load/handle failures still reach class/field setup as the reference
allows. This proves the loader's error contract, not a full VM or mod run.

The affected translation unit compiles with actual stock and CoD2x Release
commands. The one changed production file passes all ten inactive legacy
source configurations; this does not replace the unavailable Linux i386
artifact comparison. Private logs live beneath
`~/Library/Application Support/CoD2-native-protocol/validation.noindex/` with
`script-load-` prefixes. No disassembly, game data or generated source is
committed. Full composed runtime validation remains separate.
