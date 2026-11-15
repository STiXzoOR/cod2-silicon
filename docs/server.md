# CoD2 Silicon dedicated server

`cod2_macos_ded` is the native arm64 dedicated server. Built as shown below it
is the stock CoD2 1.3 server; built with the CoD2x flag it is a CoD2x 1.4.6.8
server (see [CoD2x servers](#cod2x-servers)). It uses no SDL,
OpenGL, renderer, window or audio device. A locked user session can run it.
See [WS26 validation](macos-port/reports/WS26-dedicated.md) for current gameplay
limits and the checks still required before release.

## From the launcher

Servers › Hosting in CoD2 Silicon runs this server for you. New Server asks for
a name, the game version (CoD2x or the original 1.3), who can join, the rules
and the maps; Start Server runs it, and the page then shows who is on it, the
address to share, and its log. Next Map and Kick go through the server's own
admin channel on this Mac. Quitting CoD2 Silicon asks first, then stops the
servers it started.

- The app carries both servers, `cod2_macos_ded` (CoD2x) and
  `cod2_macos_ded_stock` (original 1.3), inside its game app.
- Each server has its own folder, `hosting/servers/<id>` in CoD2 Silicon's
  Application Support folder: `main/server.cfg`, which the launcher writes on
  every start, the game's own logs, and one log per start,
  `logs/server-<time>-<id>.log`. The newest ten are kept, 4 MB each at most,
  and lines that hold either password are left out. Report a Problem can copy
  the last one for you to paste. The game folder is only read.
- **Who can join a CoD2x server.** A CoD2x server takes players on its own
  version or a newer one, never an older one. This app's CoD2x game and
  server run as CoD2x's latest stable release, 1.4.6.8, so players on that
  release for Windows and Linux can join, and so can CoD2 Silicon players.
  There is nothing to set. (0.3.0 ran as CoD2x's unreleased source instead,
  which turned those players away; a 0.3.1 player cannot join a server still
  hosted from 0.3.0.) A server on the original 1.3 takes any 1.3 player. This
  applies to competitive zPAM servers too: zPAM 4 needs CoD2x.
- **The game's own local list.** In the game, the list of servers on your
  network only asks a few ports: 28960 to 28963 on the original 1.3, and
  28960 to 28991 on CoD2x. Hosting keeps to those when it picks a port. On
  any other port the server works, but friends join it by its address.
- "My Network" is `dedicated 1`: nothing is announced. "The Internet" is
  `dedicated 2`: the server lists itself on the public master servers. See
  [Network and configuration](#network-and-configuration) before choosing it.
- Export for Another Machine writes `server.cfg`, start commands for Linux and
  Windows and a read-me into a folder. Passwords are left out unless you turn
  them on. Game files and zPAM's files are never copied.

### Competitive rules: zPAM

zPAM is the match mod competitive CoD2 servers run, by the author of CoD2x. It
is not part of CoD2 Silicon and is not bundled. Choosing Competitive · zPAM
offers to download it from the
[zPAM project's releases](https://github.com/eyza-cod2/zpam3/releases): zPAM 4
for a CoD2x server, zPAM 3.36 for the original 1.3. The download is checked
against the size and SHA-256 its release publishes, and only its `.iwd` archives
and its `server.cfg` are kept, in `hosting/zpam`. A match server's `server.cfg`
is zPAM's own, with your name, passwords, match mode and maps put on its lines.

Players' games fetch zPAM and its maps from `http://cod2x.me/zpam`, zPAM's
default. A CoD2x server also asks CoD2x's update service about newer versions;
see [CoD2x servers](#cod2x-servers).

### Wi-Fi: players who freeze and jump

If players on your server stand still for a moment and then jump forward,
about twice a second, the usual cause on a Mac is its own Wi-Fi. AirDrop,
Handoff, Sidecar and AirPlay share the Wi-Fi radio with your network, and
macOS takes the radio off your network's channel for a moment to listen for
them. Every packet in that moment waits, however strong the signal, and an
ordinary once-a-second ping does not show it. The
[OpenIW2](https://github.com/skowronmateusz/OpenIW2) project measured it on
one Mac: pings to the router every 20 ms peaked at 96 ms with those features
on and at 14 ms with them off. It has not been measured for CoD2 Silicon.

The Hosting page says when this Mac is on Wi-Fi. In order of effect:

1. Run the server on a network cable.
2. Turn off AirDrop and Handoff while you host (System Settings › General ›
   AirDrop & Handoff).
3. Set your router's 5 GHz network to channel 149, the channel those features
   use, so the radio has nowhere to switch to.

The same pauses reach a player who is on Wi-Fi on their own Mac, on any
server.

### Letting players in from the internet

People on your network join with the address the Hosting page shows. For
anyone else, your router has to pass the server's UDP port (28960 unless you
changed it) to this Mac:

1. In your router's settings, add a port forward (sometimes "virtual server")
   for UDP 28960 to this Mac's address on your network, the one the Hosting
   page shows under "On your network".
2. When macOS asks whether CoD2 Silicon's server may accept incoming
   connections, allow it.
3. Give players your public address and the port. CoD2 Silicon can't see from
   inside your network whether the forward works; ask a friend to try, or use
   "The Internet" so the server shows in the public list.

Keep the Mac awake while the server runs. The admin password travels
unencrypted, as it always has in this game: use one you use nowhere else.

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
firewall and restrict who can reach rcon.

**What was tested against hostile packets.** For 0.3.1 the code that reads
what a stranger can send a listed server was fuzzed under the address and
undefined-behaviour sanitizers, 30 minutes for each of five targets on each
game version, with no crash:

- connectionless packets (`getstatus`, `getinfo`, `rcon` and the rest of
  that dispatcher);
- the challenge request, the connect request and the key server's reply;
- the packet channel and its fragment reassembly;
- a connected player's messages: client commands, userinfo, the download
  commands and both move packets;
- the message reader (delta decoding) and the command tokeniser.

That is evidence, not a guarantee, and it does not cover the game scripts, map
and mod files, or HTTP downloads. The engine is from 2005 and has had no
security audit ([SECURITY.md](../SECURITY.md)). A server for friends on a
trusted network or a VPN remains the safer choice; if you list one publicly,
keep `sv_allowDownload 0`, set a strong rcon password or none, and run it
under an account that holds nothing you care about.

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

## CoD2x servers

Add `-DCOD2_FEATURE_CFLAGS=-DCOD2_CODX=1` to the configure line, in its own
build directory, and the same target is a CoD2x 1.4.6.8 server: it is listed
under protocol 120 and has CoD2x's bans and hardware IDs, wallbang settings,
zPAM selection, the match service, and web requests and WebSocket connections
for scripts. It keeps its settings in `main/config_mp_server.cfg` and its log
in `main/console_mp_server.log`.

A CoD2x server talks to the CoD2x project's servers, as CoD2x's own server
does:

- A public server (`dedicated 2`) reports to `cod2master.activision.com` and
  `master.cod2x.me` (`sv_master1` to `sv_master3`).
- With `sv_update` on, the default, it asks `master.cod2x.me` whether a newer
  CoD2x or zPAM exists, when a map starts and every 30 to 35 minutes, over
  HTTPS and then UDP. A newer CoD2x is only reported in the console; nothing
  is installed. To that service a Mac server names itself as CoD2x's Linux
  server, the only kind it knows.
- With `sv_updatezpam` on, the default, it moves to the newest zPAM in its
  `main` folder, and with web downloads set up it fetches newer ones from the
  addresses the update service names and checks them before use.

Set `sv_update 0` to keep the server from contacting the update service. These
features were tested against stand-ins on a Mac, not against the CoD2x
project's servers, and the server has not hosted a public match.

A separate `scripts/server/package-server.sh [binary] [output-directory]` makes
a local server tarball and SHA-256 sidecar. It includes the native binary,
service templates/helper, config, guide and license, with no game data or shaders.
It rejects non-arm64 binaries and direct non-system library dependencies.
Signing/notarization and release publication remain release-operator tasks.
