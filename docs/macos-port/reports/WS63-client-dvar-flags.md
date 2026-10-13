# WS63: client registration flags

2026-10-07. Bounded WS44 FA05 correction based on `380bf2b`.

Twenty client registrations omitted bit `0x1000`. Independent inspection of
the owned Mac 1.3 `CL_InitOnceForAllClients` recovered each registration's
literal name from the Mach-O section and its immediate flags argument from
the actual i386 instructions. All twenty sites pass `0x1000`:

| Dvar | Mac registration call |
| --- | --- |
| `cl_noprint` | `0x148619` |
| `cl_timeout` | `0x14866d` |
| `cl_connectTimeout` | `0x148696` |
| `cl_shownet` | `0x1486c7` |
| `cl_shownuments` | `0x1486e8` |
| `cl_showServerCommands` | `0x148709` |
| `cl_showSend` | `0x14872a` |
| `cl_showTimeDelta` | `0x14874b` |
| `cl_freezeDemo` | `0x14876c` |
| `activeAction` | `0x14878d` |
| `cl_avidemo` | `0x1487be` |
| `cl_forceavidemo` | `0x1487df` |
| `cl_anglespeedkey` | `0x14886b` |
| `cl_showmouserate` | `0x148974` |
| `cl_talking` | `0x1489d7` |
| `cl_serverStatusResendTime` | `0x148a2c` |
| `cl_bypassMouseInput` | `0x148a4d` |
| `fx_cull` | `0x148d1d` |
| `fx_sort` | `0x148d3e` |
| `nextdemo` | `0x148e45` |

Native registrations now pass that bit. Contiguous native/legacy branches keep
the original inactive text unchanged; defaults, domains, types and CoD2x
competitive policy are unchanged. The two client FX registrations specifically
pass `0x1000`; a final merged registry can also contain renderer bits from
another registration site. This change does not replace that combined state
with a hardcoded final flags word.

The real native `Dvar_RegisterVariant_impl` uses this bit when transferring a
previously registered name to the engine registration. The extended fidelity
fixture therefore checks more than flags: preexisting nonexternal bool, int,
float and string variables retain their values while their canonical names
come from the client registration. Before the fix, all twenty flag checks and
all four name-transition checks fail (24 assertions); afterward both stock and
CoD2x pass all six fidelity fixture groups with ASan/UBSan. The registration
extractor supplies the same `FLT_MAX` union value used by the production
function for `cl_anglespeedkey`.

Actual stock and CoD2x Release compilation of `cl_main_mp.c` passes. The single
changed production file passes all ten legacy source configurations, and
`unifdef -UCOD2_X64` produces byte-identical complete source against the base
with omitted, enabled and explicitly undefined CoD2x macros. These source checks do
not establish unavailable Linux i386 binary equality. Full composed build and
runtime acceptance remain separate.

Private fixtures, actual compile databases, objects and logs are under
`~/Library/Application Support/CoD2-native-ws63/validation.noindex/`. Reference
facts are recorded in the parent's private `owner-register-facts.json`; no
disassembly, game data or generated reference material is committed.
