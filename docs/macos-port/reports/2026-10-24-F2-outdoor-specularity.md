# F2 outdoor lookup and specularity

Compared R_RegisterOutdoorImage, R_GenerateOutdoorImage and
Image_BuildSpecularityMap against the original Mac 1.3 instructions.
The outdoor module already matches: surface flag filtering, bounds expansion,
512/512/256 sample scales, lookup matrix, downward traces, ceiling-to-floor
interpolation, byte clamping, image upload and temporary memory release.
Its existing native fixes remain unchanged. Original addresses are 0x108248
and 0x108460; the map dimensions at 0x32b960 are 512, 512, 256.

Image_BuildSpecularityMap (0x109f2c) has one corrected numeric constant.
The original log call at 0x109f84 receives the double represented by
0x3fb99999a0000000: the promoted float 0.1f, or 0.10000000149011612.
The reconstruction used double 0.1. For the valid r_cosinePowerMapShift
value 0.06788204610347748, row 114 / power 0 became byte 0 instead of 1.
The native branch now preserves the original float-to-double promotion.
No algorithm, dimension, range or platform interface changes.

The other instructions agree: float power/30 plus shift, double clamp and
cosine/log arithmetic, exponent 30 for column 31, float level times 1/255,
double power and scaling followed by float rounding and byte conversion.
The numerical constants were read at the instruction-referenced addresses.
No original code, assets or disassembly is stored in Git.

## Evidence

Before the production edit, tests/lp64/renderer/specularity.c compiled with
actual stock engine flags plus ASan/UBSan and failed at its independent
expected row-114 byte. It now passes with stock and CoD2x flags. Controls
check adjacent rows, black/white endpoints and the unchanged exponent-30
column. The existing outdoor_mapping fixture also passes both modes under
ASan/UBSan. The new fixture is registered in the existing renderer run.sh.
No duplicate full renderer suite was run. Exact invoked compiler arguments
are retained in validation-commands.json beside the logs. The standalone
registered command is the clang invocation in tests/lp64/renderer/run.sh with
name=specularity; add -DCOD2_CODX=1 for the CoD2x variant.

Both files pass unifdef -UCOD2_X64 comparison against 3eff16f; r_outdoor.c
is entirely unchanged. Private commands, red/green logs and bounded assembly
are under F2-outdoor-specularity.noindex in the private decomp evidence
folder. The original logarithm input is read directly from its immediate
argument words; expected output uses that value, independently of the
production literal. Current-platform libm differences from original Mac
libm and full original-renderer capture parity remain unverified. Integration
builds and live checks are recorded separately.
