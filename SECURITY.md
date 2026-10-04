# Security policy

CoD2 Silicon is an experimental reconstruction of an engine from 2005.
Networking, file parsing, downloaded content and memory safety retain
engine-era risks. It has not had a comprehensive security audit and is not
a hardened engine. Use trusted game data, mods and servers; keep test servers
off untrusted networks unless you accept those risks.

## Report a vulnerability privately

Use [GitHub private vulnerability reporting](https://github.com/STiXzoOR/cod2-silicon/security/advisories/new)
from this repository's **Security → Advisories → Report a vulnerability**.
Do not open a public issue with an exploit or sensitive logs. The maintainer
must enable private reporting in the repository settings before publication.

Include the affected version/commit, macOS version, reproduction steps,
impact and a minimal non-proprietary example if possible. Redact CD keys,
key digests, HWIDs, Steam credentials, passwords and tokens. Never attach game
archives, Activision binaries or decompiler dumps. The maintainer will use
the private advisory to coordinate investigation and disclosure.

Security fixes target the current development branch and latest release;
there is no guaranteed response time or supported legacy-release window.
The i386 preservation requirement still applies; a legacy security change
requires an explicit maintainer decision and a clearly recorded exception.
