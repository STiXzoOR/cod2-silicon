# CoD2 Silicon dedicated server

`cod2_macos_ded` is the native arm64, stock CoD2 1.3 server. It uses no SDL,
OpenGL, renderer, window or audio device. A locked user session can run it.
See [WS26 validation](macos-port/reports/WS26-dedicated.md) for current gameplay
limits and the checks still required before release.

## Build and data layout

Build on Apple Silicon with the project's existing development dependencies:

```sh
cmake -S . -B build-macos -DCOD2_X64=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos --target cod2_macos_ded --parallel 3
```

Use your own licensed PC game data, patched to 1.3. The repository and server
package contain no game data. Keep the data separate from writable server files:

```text
/Users/you/Games/CoD2/main/iw_00.iwd ... iw_15.iwd   owned data, read-only
~/Library/Application Support/CoD2 Silicon Server/
  main/server.cfg                                 private configuration
  main/games_mp.log                               game log
  logs/console.log                                launchd stdout/stderr
  user/                                          private preference home
```

A foreground example, with quoted path values retained for the engine parser:

```sh
build-macos/cod2_macos_ded \
  +set fs_basepath '"/Users/you/Games/CoD2"' \
  +set fs_homepath '"/Users/you/Library/Application Support/CoD2 Silicon Server"' \
  +set net_ip 127.0.0.1 +set net_port 28960 +set dedicated 1 \
  +exec server.cfg +map mp_toujane
```

Copy `scripts/server/server.cfg` into the writable `main/` first. Enter `quit`
for a normal exit. SIGTERM and SIGINT request the same shutdown on the engine
thread. Launch without a display service; the dedicated executable has no GPU
or sound initialization. Physical display removal has not been tested here.

## User launchd service

Keep the checkout/helper at a stable absolute path. Install as the account that
will own the server; the default is a user agent and requires that user's login
session to exist. Locking the screen is supported. Logging out ends that agent's
session. Installation writes definitions but does not start the server.

```sh
scripts/server/cod2-silicon-server install \
  --binary "$PWD/build-macos/cod2_macos_ded" \
  --data "$HOME/Games/CoD2" --bind 0.0.0.0 --port 28960
# Edit ~/Library/Application Support/CoD2 Silicon Server/main/server.cfg
scripts/server/cod2-silicon-server start
scripts/server/cod2-silicon-server status
scripts/server/cod2-silicon-server logs
scripts/server/cod2-silicon-server stop
```

The helper creates private files (`umask 077`), preserves an existing config,
and stores the service in `~/Library/LaunchAgents/`. `uninstall` stops its own
jobs and removes their definitions; it keeps configuration and logs. For several
servers, set distinct `COD2_SERVER_HOME` and `COD2_SERVER_LABEL`, with distinct
UDP ports. `COD2_SERVER_AGENT_DIR` is useful for testing definitions in isolation.
Use the same overrides for every helper command.

A nonzero exit or crash restarts after launchd's ten-second throttle. `quit`
exits successfully and stays stopped until `start` or the next login. `stop`
unloads the job. Resource limits are 2,048 soft / 4,096 hard open files and no
core dumps; these are not a memory quota. An hourly companion job rotates console
and game logs at 10 MiB, keeping five copies. Rotation copies then truncates the
existing inode, so open engine/launchd descriptors continue writing. A write
concurrent with copy/truncate can be lost, and logs can exceed the threshold
between hourly runs. `rotate` performs the same check immediately.

For a Mac cloud host or startup without user login, the separate
`scripts/server/io.github.stixzoor.cod2silicon.daemon.plist` is an administrator
edited template. Substitute every placeholder, create owned writable directories,
set `UserName` to a dedicated non-root account, retain literal quotes around
engine path values containing spaces, and install a root-owned,
non-writable-by-others plist in `/Library/LaunchDaemons/`. Bootstrap into `system`
and use that domain for status/bootout. The user helper deliberately operates
only in `gui/<uid>`. Arrange an equivalent log-rotation job for the daemon.
The daemon template has not been exercised on this host.

Apple documents the job model in [Creating launchd jobs](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingLaunchdJobs.html)
and the keys in [launchd.plist(5)](https://github.com/apple-oss-distributions/launchd/blob/main/man/launchd.plist.5).

## Network and configuration

Use `net_ip 127.0.0.1` for loopback tests or an interface address for a specific
LAN. `0.0.0.0` binds all IPv4 interfaces. The game/query/rcon socket uses UDP
28960 by default; each instance needs its own port. Check the address with
`lsof -nP -a -p <server-pid> -iUDP`. Clients connect with
`connect <server-address>:28960`. The listen address is a startup setting.

On macOS, allow incoming connections for the actual dedicated executable when
the application firewall prompts. In System Settings → Network → Firewall →
Options, add/allow the executable as needed; do not disable the whole firewall.
Apple describes the controls in [Allow incoming connections](https://support.apple.com/en-gb/guide/mac-help/mh34041/26/mac/26).
For routed LANs/cloud machines, allow the selected UDP port in the host/network
firewall and restrict who can reach rcon. Internet-facing packet-parser hardening
is still a separate workstream; start with a trusted LAN or VPN.

Recommended baseline: `sv_fps 20`, eight slots, `sv_maxRate 25000`, `sv_pure 1`,
`sv_allowDownload 0`, `sv_punkbuster 0`. The rate setting limits non-LAN clients'
bytes per second; zero means no server cap. Stock 1.3's LAN send path bypasses
this throttle, so loopback testing does not establish WAN bandwidth enforcement. Keep the stock IWD set identical on
clients and server. Pure validation compares protocol checksums of IWD contents,
not a SHA-256 digest of the ZIP file. The runtime accepts higher `sv_fps` values
(10–1000 is the registered range), but higher rates need live validation with
your player count and should not be assumed faster or stable.

Set `rcon_password` in the private config; an empty password disables rcon.
In a connected client's console, set the matching password, then use
`rcon status`, `rcon map_restart` or `rcon map_rotate`. The stock rcon protocol
carries the password over UDP without transport encryption. Keep it on a trusted
network/VPN and use a password distinct from your account credentials.

Public listing is off by default (`dedicated 1`). `install --public` selects
`dedicated 2`; `sv_master1` through `sv_master5` select endpoints, and empty
entries disable endpoints. The example keeps the historical master name in
slot 1, but availability of that public service is not claimed. Tests use only
a private loopback master receiver. Public mode sends heartbeats every 180 seconds,
status every 600 seconds, and a `flatline` heartbeat during shutdown. Restart
with a changed mode; setting masters alone does not enable public listing.

## Keep the Mac awake and update

The screen can sleep or lock; the computer must remain awake to serve packets.
For a temporary session, `caffeinate -i -s -w <server-pid>` prevents idle sleep
and, on AC power, system sleep until that PID exits. Re-run it after a server
restart. Inspect `pmset -g` before changing persistent settings. If an
administrator chooses `sudo pmset -c sleep 0` for an always-on AC host, record
the old value and restore it when the machine returns to normal use. These
commands are described by the installed `man caffeinate` and `man pmset`.
No power settings are changed by the helper. Prefer wired networking and monitor
host availability separately from the launchd process status.

Stop the service, build/install the new dedicated binary at the same path, then
start it and inspect the logs and `getstatus` before clients reconnect. Keep
configuration and licensed data out of binary updates. To change install paths,
stop and uninstall, then reinstall with the new paths; configuration is preserved.

CoD2x client builds fall back to stock 1.3/protocol 118 behavior here. CoD2x
server extensions, protocol 120 and server-side competitive features are outside
this server's scope. See the validation report for the tested client baseline.

A separate `scripts/server/package-server.sh [binary] [output-directory]` makes
a local server tarball and SHA-256 sidecar. It includes the native binary,
service templates/helper, config, guide and license, with no game data or shaders.
It rejects non-arm64 binaries and direct non-system library dependencies.
Signing/notarization and release publication remain release-operator tasks.
