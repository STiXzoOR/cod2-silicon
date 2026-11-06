# Server tail preflight: parent replay incomplete

The server tail annotation remains unaccepted and unapplied to the accepted
generation. Two worker experiments measured its effects, but the independent
parent replay failed its baseline precondition before applying the annotation.
Generation 15 and native production behavior remain unchanged.

The source and fresh worker runs each applied one temporary CALL_RETURN override
to a disposable, read-only project session. Their complete semantic effect files
match. They observed one instruction's analysis-flow change and one reference's
type change, with original bytes and separate function storage preserved. Only
the connectionless caller's exported C changed. Its 177 outside-body diagnostic
sites disappeared in those sessions; the 25 cleanup sites remained. Each worker
run passed its verifier and 16 negative controls.

Those observations do not resolve the inferred argument signature. The caller's
first inferred parameter changes from int to undefined4, and the displayed
SVC_Info call still has no arguments. No service, ABI, native or whole-function
equivalence follows from the diagnostic reduction.

The parent independently checked both complete sealed inventories and their
consumed inputs. Its fresh baseline exported all 8,115 status records and 7,481
C files. Function ID 10af2a exceeded the first 30-second budget and succeeded on
the 90-second retry. The accepted baseline and both worker baselines completed
that function on the first attempt. The existing strict comparator excludes only
elapsed time; the changed attempt count, status and timeout diagnostic correctly
prevented annotation. No comparison rule or timeout budget was changed.

A separate parent diagnostic reproduced that exact rejection and examined the
complete baseline. All 7,481 actual C files match accepted Generation 15. Only
one status record differs after excluding elapsed times, and its other fields
match. Recorded durations for that function were 16.65 seconds in Generation 15,
23.96 in the source run and 16.91 in the worker replay. The parent recorded a
30.20-second timeout followed by a successful 25.49-second retry. The scheduling
or hardware cause of that variation was not established.

The headless runner continued later scripts after the failed annotation script.
The parent stopped the private JVM to avoid completing unmodified captures.
The nine original project files in both frozen source and disposable copy remain
unchanged; two disposable lock files remain preserved. The failed copy will not
be reused. All processes stopped and the decompiler singleton was released.

The failed replay, partial captures and diagnostic-script failures are sealed.
One diagnostic script initially assumed every status had an attempts field;
another assumed termination left no extra lock files. The corrected diagnostic
records both facts without changing the failed acceptance result. The prepared
full after-export audit was not executed because no parent after-export exists.

Private evidence identifiers:

- Source seal: `4996f2197fae0e73ec644463b81c74ce03b18b746871aaae97ce045d04fc8348`.
- Worker replay seal: `4e1496a03c25fa1234b9892ea46c19c2eb5e364bcec65954b14509e623b56e1a`.
- Worker semantic effects: `e52620627136967d3c27797a0707839ad8ab637b88b2e33a0b518c97ad74ed53`.
- Failed parent replay seal: `02b83161bfe5c6619f8470ed0c2777309562360b12890a280c32a7831b5b07c1`.
- Parent non-acceptance decision: `3f5eb12206a67bf47d6738dd86c085fd95b748cbfe6f01a9a183aa75826c36ef`.
- Parent audit seal: `7cca34bd960e5d0abe44bd4fc750c681b176e59a52df06571aa129908b1af946`.

The earlier bounded original tail-transfer proof remains accepted. This preflight
requires renewed independent verification before promotion. Native tests were
not run for this analysis-only unit. The accepted unknown denominator remains
10,384 bytes in 4,431 intervals, and full reconstruction remains incomplete.
