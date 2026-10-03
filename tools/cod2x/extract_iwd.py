#!/usr/bin/env python3
"""Extract the data IWD from an owned CoD2x Windows DLL or release ZIP.

Only the IWD is written. DLLs are inspected as data and are never loaded.
The reference release embeds the ZIP with objcopy (CMakeLists.txt:34-69);
scanning ZIP central directories also covers a resource-embedded release.
"""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import sys
import zipfile

IWD_NAME = "iw_CoD2x_01.iwd"
MAX_INPUT = 256 * 1024 * 1024
MAX_IWD = 64 * 1024 * 1024


def validate_iwd(payload):
    if len(payload) > MAX_IWD:
        raise ValueError("IWD exceeds the extraction size limit")
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        names = {name.lower().replace("\\", "/") for name in archive.namelist()}
        if not {"ui_mp/main.menu", "materials/radar_player_arrow"} <= names:
            raise ValueError("Archive is not the CoD2x client IWD")
        if sum(info.file_size for info in archive.infolist()) > MAX_IWD:
            raise ValueError("IWD expands beyond the extraction size limit")
        if archive.testzip() is not None:
            raise ValueError("IWD CRC validation failed")
    return payload


def embedded_zip(data):
    candidates = []
    position = 0
    while True:
        position = data.find(b"PK\x05\x06", position)
        if position < 0:
            break
        end = position + 22
        if end <= len(data):
            _, disk, central_disk, disk_count, count, size, offset, comment = struct.unpack_from("<4s4H2IH", data, position)
            start = position - size - offset
            if disk == central_disk == 0 and disk_count == count and 0 <= start < position and end + comment <= len(data):
                payload = data[start:end + comment]
                if payload.startswith(b"PK\x03\x04"):
                    try:
                        candidates.append(validate_iwd(payload))
                    except (ValueError, zipfile.BadZipFile, RuntimeError):
                        pass
        position += 4
    if len(candidates) != 1:
        raise ValueError(f"Expected one embedded CoD2x IWD, found {len(candidates)}")
    return candidates[0]


def extract_payload(data):
    if len(data) > MAX_INPUT:
        raise ValueError("Release exceeds the extraction size limit")
    if data.startswith(b"MZ"):
        if len(data) < 64:
            raise ValueError("Truncated Windows DLL")
        offset = struct.unpack_from("<I", data, 60)[0]
        if data[offset:offset + 4] != b"PE\0\0":
            raise ValueError("Invalid Windows PE header")
        return embedded_zip(data)
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            found = []
            for info in archive.infolist():
                name = info.filename.replace("\\", "/").rsplit("/", 1)[-1].lower()
                if name not in (IWD_NAME.lower(), "mss32.dll"):
                    continue
                if info.file_size > MAX_INPUT:
                    raise ValueError("Release member exceeds the extraction size limit")
                content = archive.read(info)
                found.append(validate_iwd(content) if name == IWD_NAME.lower() else extract_payload(content))
            # A release may include the same archive separately and in its DLL.
            found = list(dict.fromkeys(found))
            if len(found) != 1:
                raise ValueError(f"Expected one CoD2x IWD in release, found {len(found)}")
            return found[0]
    except zipfile.BadZipFile as exc:
        raise ValueError("Source must be an owned CoD2x mss32.dll or release ZIP") from exc


def install_payload(payload, game, force=False):
    validate_iwd(payload)
    directory = Path(game).expanduser().resolve() / "main"
    directory.mkdir(parents=True, exist_ok=True)
    output = directory / IWD_NAME
    if output.exists() and output.read_bytes() == payload:
        return output
    with output.open("wb" if force else "xb") as stream:
        stream.write(payload)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Your CoD2x Windows mss32.dll or release ZIP")
    parser.add_argument("game", type=Path, help="Your game install or writable fs_homepath")
    parser.add_argument("--force", action="store_true", help="Replace an existing different IWD")
    args = parser.parse_args()
    try:
        source = args.source.expanduser()
        if source.stat().st_size > MAX_INPUT:
            raise ValueError("Release exceeds the extraction size limit")
        payload = extract_payload(source.read_bytes())
        output = install_payload(payload, args.game, args.force)
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as exc:
        parser.exit(1, f"Extraction failed: {exc}\n")
    print(f"Extracted {output} ({len(payload)} bytes, SHA-256 {hashlib.sha256(payload).hexdigest()})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
