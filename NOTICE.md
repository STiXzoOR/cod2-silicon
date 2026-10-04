# License and provenance notice

The [MIT license](LICENSE) covers CoD2 Silicon's own original work: this
project's changes after opencod2 commit `410342a` in the Git history. It does
not relicense the upstream code or third-party components.

The upstream [opencod2](https://github.com/opencod2/opencod2) code declares no
license and remains under its authors' rights. The history through `410342a`
identifies that base. Existing third-party copyright and license notices
continue to apply; see [CREDITS.md](CREDITS.md).

CoD2 Silicon independently reimplemented behavior after studying CoD2x,
cod2engine, CoD2rev_Server, KisakCOD and xtnded/cod2. These projects were
behavior and type references, read rather than copied. The resulting port
changes are original work. Format research also consulted the projects
listed in the credits; their code was not incorporated into the port.

No game assets, Activision binaries, original game shaders, extracted shader
payloads or decompiler dumps are included. Players supply their own licensed
game data. Original Mac shaders are extracted locally from the player's own
copy, never distributed with this project.

The `build/lp64_gen/` snapshot re-encodes the data blobs that upstream
opencod2 already commits, as typed C for 64-bit builds, in the same way as
upstream's own generated `build/*_gen` sources. It adds type layouts from the
original Mac 1.3 debug information and thirteen scalar values checked against
the Steam Mac executable. It contains no machine code from either binary.

“Call of Duty” is a trademark of Activision. This independent project is not
affiliated with, authorized by, sponsored by or endorsed by Activision,
Infinity Ward, Aspyr or the CoD2x project.
