# Defined native invalid-input boundaries

Updated 2026-11-05. Six previously unchecked Mac rows now have explicit native
contracts. These are intentional adaptations, not original corrupted-memory or
external-library output equality. Ordinary original control/arithmetic quirks
remain. No new tests, cases or assertions were added.

## FX token storage

Exact Mac GetToken0xab80c–0xaba9e was inspected along with its FX loader and
native GPGroup parser consumers. FX_ParseEffect0x5d25f–0x5d2b3 allocates
fileLength+1 and adds one NUL; Hunk_AllocateTempMemoryInternal0x38192 supplies no
readable suffix contract. The readUntilEOL loop0xab95f–0xab999 did not stop at
NUL, and the quoted loop0xaba0f–0xaba1f advanced its cursor beyond NUL. Native now
stops the line at NUL and leaves an unterminated quote's cursor on the allocated
terminator. A following token scan reaches its ordinary end-of-input path.
Whitespace/comments, quoted closing delimiter, maximum token handling and line
trimming retain their established behavior. Malformed EOF no longer reads
following allocation bytes; those bytes cannot supply a portable parity target.

## Dvar temporary values

Direct SetVec2 at0x54bba checks type4 at0x54bcf and supplies two local floats
at0x54c16–0x54c27 to a four-component setter. ByName0x550f6 does the same at
0x551d0–0x551f1. Native deliberately retains that type dispatch and supplied x/y,
but owns four initialized floats, with z/w zero. Both direct and inlined owners
use the same representation. This does not infer values for Mac saved registers
or uninitialized stack locals. Non-type4 formatting and INTERNAL setter source
remain unchanged; missing-name ByName registration still formats only x/y.

SetColorByName0x55796 passes four byte-derived integers to four %g conversions
on the missing-name branch0x557fb–0x55822. Native now explicitly passes doubles
for those same byte values, defining the registered string. The existing-name
conversion/call0x55896–0x558d9, byte truncation, lookup and0x4000 registration
flags remain unchanged. No normalization or ordinary color rule was changed.

## Absent voice-player name

UI_OwnerDraw0x1536b2–0x15401c's missing-talker path can use playerNames[-1] at
0x154012. That points into preceding assets, without an owned valid string.
Native retains the voice icon and existing text positioning/scaling call but
supplies empty text when list lookup yields a negative index. Valid list entries
still use their exact playerNames entry. This is an explicit empty-name boundary,
not a claim that original preceding bytes were empty.

## Download request owner

The entire native DL_BeginDownload and selected caller/completion graph were
reviewed against original0x1fbc42–0x1fc0a0. Native curl owns the request URL,
redirect/authentication/error processing and completion messages. The bounded
64-byte UI label independently copies literal text and masks ordinary authority
credentials. Original libwww HTParse details for unusual relative/malformed URLs
are part of the replaced library boundary; native does not claim its exact
normalization or the old URL-as-format reads. The original URL is still passed
to CURLOPT_URL. Existing MOTD classification and UI progress corrections remain.

Fresh independent source review found another actual native owner defect:
network success was published even if buffered data failed to flush at fclose.
DL_Teardown now closes exactly once and downgrades DONE to FAILED on that error
before DL_DownloadLoop returns. CL_WWWDownload observes failure before rename,
wwwdl done or disconnected-download reconnect. Cancellation still becomes
CANCELLED, ordinary error states stay FAILED, and replacement begins retain
ownership cleanup. The existing DL_DownloadLoop intentional verdict is retained
with an explicit evidence amendment. No libwww callback reconstruction is needed.

## Verification limits

All four changed translation units compile with actual stock and CoD2x arm64
Release client commands. The unchanged domain/promotion/lifetime fixture passes
both modes under ASan/UBSan. Five OFF configurations are identical; changes
appear only in the native-on configuration. None of these files uses __LINE__.
The malformed parser, wrong-type dvar, missing-name color, missing-talker draw
and failed final file flush paths are compiled and reviewed, not directly
exercised. No real download, rendering, full CI or replay was performed in this
unit; the parent runs the integrated gate. All other reconciled verdicts remain.

## Integrated batch 21 acceptance

The final integrated snapshot passed both dedicated builds, full local CI02–13, legacy checks, independent reviews and unchanged-baseline replays. See [batch 21 verification](integration-batch21.md) for the preserved initial fixture failure, repair, exact scope and unexercised limits.
