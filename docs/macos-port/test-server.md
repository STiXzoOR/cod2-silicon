# CoD2x 1.4 test server

Use an **x86-64 Ubuntu 24.04 VM or cloud host with i386 runtime support**.
The reference server is the official 32-bit Linux `cod2_lnxded` 1.3 with
`LD_PRELOAD=libCoD2x.so` from CoD2x **1.4.6.8**. The native arm64 Mac cannot
run this pair. Do not substitute opencod2's reconstructed dedicated server:
CoD2x patches addresses in the original executable.

The commands below are instructions for the VM operator; they have not been
executed on the porting Mac. Keep the server installation and captures outside
this repository. Supply a clean, legally owned CoD2 1.3 `main/` directory from
the Windows installation described in [game-data.md](game-data.md).

## Install and start on the Ubuntu host

Save the following as `setup-cod2x-test.sh` on that host, then run it as your
normal user with `COD2_DATA=/absolute/path/to/owned/CoD2 bash setup-cod2x-test.sh`.
`COD2_DATA` names the parent of `main`, not `main` itself. The script installs
runtime packages on that host, creates a fresh server directory, downloads the
pinned release, writes `main/server.cfg`, and starts the server in the foreground.
It refuses to reuse an existing destination. No compiler is required.

```bash
#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -m) == x86_64 ]] || { echo 'Use an x86-64 Ubuntu host.' >&2; exit 1; }
[[ $EUID != 0 ]] || { echo 'Run as a normal user with sudo access.' >&2; exit 1; }
: "${COD2_DATA:?Set COD2_DATA to the owned CoD2 directory containing main/}"
COD2_SERVER=${COD2_SERVER:-"$HOME/cod2x-test-1.4.6.8"}
[[ -d "$COD2_DATA/main" ]] || { echo 'Missing main/ data directory.' >&2; exit 1; }
compgen -G "$COD2_DATA/main/iw_*.iwd" >/dev/null || {
  echo 'Missing stock IWD files.' >&2; exit 1;
}
[[ ! -e "$COD2_SERVER" ]] || { echo 'Choose a fresh COD2_SERVER directory.' >&2; exit 1; }

sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y software-properties-common
sudo add-apt-repository -y universe
sudo apt-get update
sudo apt-get install -y ca-certificates curl unzip rsync file tcpdump \
  libc6:i386 libstdc++5:i386 libstdc++6:i386 libgcc-s1:i386 zlib1g:i386

mkdir -p "$COD2_SERVER/main" "$COD2_SERVER/logs"
rsync -a "$COD2_DATA/main/" "$COD2_SERVER/main/"
cd "$COD2_SERVER"
curl --fail --location --output CoD2x_1.4.6.8_linux.zip \
  https://github.com/callofduty2x/CoD2x/releases/download/v1.4.6.8/CoD2x_1.4.6.8_linux.zip
unzip -j CoD2x_1.4.6.8_linux.zip '*libCoD2x.so' '*cod2_lnxded' -d .
chmod u+x cod2_lnxded libCoD2x.so
file cod2_lnxded libCoD2x.so
sha256sum CoD2x_1.4.6.8_linux.zip cod2_lnxded libCoD2x.so > logs/binaries.sha256
ldd ./cod2_lnxded ./libCoD2x.so | tee logs/dependencies.txt
if grep -q 'not found' logs/dependencies.txt; then
  echo 'Resolve the missing i386 libraries before starting.' >&2
  exit 1
fi

cat > main/server.cfg <<'CFG'
set sv_hostname "CoD2x native-port LAN test"
set sv_maxclients "8"
set sv_punkbuster "0"
set sv_pure "1"
set sv_allowDownload "0"
set sv_update "0"
set sv_cracked "0"
set sv_fps "20"
set g_gametype "dm"
set scr_dm_timelimit "10"
set scr_dm_scorelimit "0"
set logfile "2"
set g_log "games_mp.log"
set g_logSync "1"
set rcon_password ""
set sv_mapRotation "gametype dm map mp_carentan"
CFG

# dedicated 1 = LAN; dedicated 2 = public/master-server heartbeats.
# Disable the updater on the command line too, before configuration loading.
LD_PRELOAD="$COD2_SERVER/libCoD2x.so" ./cod2_lnxded \
  +set dedicated 1 +set net_ip 0.0.0.0 +set net_port 28960 \
  +set fs_basepath "$COD2_SERVER" +set fs_homepath "$COD2_SERVER" \
  +set sv_update 0 +exec server.cfg +map_rotate \
  2>&1 | tee "logs/server-$(date -u +%Y%m%dT%H%M%SZ).log"
```

Check the console for the CoD2x version banner and a loaded `mp_carentan` map.
In another terminal, `ss -lunp 'sport = :28960'` confirms the UDP listener.
Use the server console's `status` command to inspect connected clients and
`quit` to stop cleanly. To restart, run the final `LD_PRELOAD=...` command from
the server directory with `COD2_SERVER` set to its absolute path.

The upstream README supplies the official 1.3 executable in the release ZIP
and identifies the required preload library. Its build links OpenSSL and the
modern C++ runtime statically; `ldd` above checks the actual downloaded release
rather than assuming that recipe proves all binary dependencies. Ubuntu 24.04
provides [libstdc++5 for i386](https://packages.ubuntu.com/noble/libstdc%2B%2B5).
The release is pinned to the
[CoD2x 1.4.6.8 release](https://github.com/callofduty2x/CoD2x/releases/tag/v1.4.6.8);
recorded SHA-256 values identify the local test artifacts, not a separately
verified publisher signature.

## Ports and Mac connection

Allow **UDP 28960** inbound from the two test clients (and replies outbound).
For a cloud host, apply that restriction in the cloud security group as well
as the host firewall. Use a private LAN/VPN address when possible. For example,
if UFW is already configured, `sudo ufw allow from 192.168.1.0/24 to any port
28960 proto udp` permits that LAN; choose your actual subnet. Do not enable a
new firewall remotely without preserving SSH access. TCP 28960 is not needed
for gameplay. This setup disables downloads and remote console access.

Upstream uses UDP **20720** for automatic updates. It is unnecessary here:
`sv_update 0` keeps the chosen release stable. `dedicated 1` suppresses public
master-server heartbeats; it is not a firewall or access-control rule. Do not
try to clear `sv_master1` or `sv_master2`: CoD2x registers them read-only.

Once the native client can start, set the address to the Ubuntu host:

```bash
tools/macos/run.sh "$HOME/Games/CoD2-native-data" \
  +set name native-test +connect 192.168.1.50:28960
```

Or enter `/connect 192.168.1.50:28960` in its console. On a real Windows CoD2
1.3 installation, install the matching CoD2x Windows release as instructed in
its README (`mss32.dll` and `mss32_original.dll`), then issue the same connect
command. First establish that Windows joins, spawns, moves, fires, changes
team, survives a map restart, and completes a round. Repeat with the Mac.
Keep the same map, IWD inventory, server config, and server binary hashes.

CoD2x uses protocol **120**, while stock 1.3 uses **118**. In
`~/Projects/cod2-native-refs/CoD2x/src/shared/server.cpp`, `SV_DirectConnect`
requires a 32-character `cl_hwid2`. A rejected unmodified opencod2 client is an
expected blocker until WS4 implements the handshake; LAN mode does not remove
this check. A timeout, invalid-HWID response, IWD mismatch, and post-connect
crash are different failures and should be reported separately.

## Capture Windows and Mac sessions

Capture at the server so both traces use the same clock and network vantage
point. Start capture before connecting; stop it with Ctrl-C after the round.
Replace the addresses with those **seen by the server**, especially through
NAT/VPN. These commands run in separate sessions, with only the named client
connected:

```bash
sudo tcpdump -i any -nn -s 0 -U -w windows-cod2x.pcap \
  'udp port 28960 and host 192.168.1.60'
sudo tcpdump -i any -nn -s 0 -U -w mac-native.pcap \
  'udp port 28960 and host 192.168.1.61'
```

Read a trace using `tcpdump -nn -tttt -X -r windows-cod2x.pcap`, or open it in
Wireshark and use `udp.port == 28960`. Compare the ordering and content of the
connectionless exchanges (four `0xff` bytes, challenge/connect replies),
protocol and userinfo fields, rejection strings, then establishment of the
sequenced game channel and packet cadence. Correlate each phase with the
server log; CoD2x's `showPacketStrings` cvar can add network diagnostics when
needed. Expect challenges, timestamps, sequence numbers, addresses, player
identifiers, and user input to differ. Raw PCAP byte equality is not a parity
test, and compressed/encoded game traffic is not all readable text. Use the
[engine statehash workflow](parity.md) for deterministic simulation comparison.

PCAPs and verbose logs may contain userinfo, identifiers, chat and passwords.
Keep raw captures in private test storage, outside git; redact identifying
fields before sharing a diagnostic excerpt.

## Verification scope

This guide was checked against the reference CoD2x README, `src/shared/server.cpp`,
`src/linux/updater.cpp`, `src/shared/shared.h`, and `CMakeLists.txt`, plus
opencod2's server cvar registrations. No Ubuntu host, owned game data, Windows
client, or working native client was available in this workstream, so package
installation, release extraction, server boot and gameplay remain to be run.
