# Mac/Tools/i5stdlib reconciliation

Updated 2026-11-05.

4/4 assigned rows equal. Mac stricmp 0x258f8 and strcmpi 0x2590c tail-call strcasecmp; strnicmp 0x25902 tail-calls strncasecmp. Native uses the same libc calls and returns their results. The native length type widens with size_t.

Mac strupr 0x25954 walks through NUL, sign-extends each char into __toupper, stores the result byte and returns the original pointer. Native has that same signed-char loop on Apple. No claim is made that Windows locale conversion has identical non-ASCII behavior; these are the retained Mac libc helpers.

The engine itoa at 0x25994 belongs to this source but is routed to punkbuster/pbsv in the ledger; its correction is documented there. The source compiled in the arm64 Release dedicated build. No new test or runtime exercise ran.
