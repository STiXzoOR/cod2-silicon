#!/usr/bin/env python3
"""Build a permission-gated index; never rewrite configs or invent signatures."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import urlsplit

REQUIRED = {"player", "era", "role", "feel", "story", "styleTags", "credit", "sourceURL", "permission", "platform"}
OPTIONAL = {"team", "favoriteMap"}


def public_url(value):
    if not isinstance(value, str):
        return False
    parts = urlsplit(value)
    return (parts.scheme == "https" and bool(parts.hostname) and parts.username is None and parts.password is None
            and not parts.query and not parts.fragment and "\\" not in parts.path
            and not any(piece in (".", "..") for piece in parts.path.split("/"))
            and not re.search(r"%(?:2e|2f|5c|25)", parts.path, re.I))


def text(value, maximum=256):
    return (isinstance(value, str) and bool(value.strip()) and len(value.encode()) <= maximum
            and not any(ord(c) < 32 and c != "\n" or ord(c) == 127 for c in value))


def metadata_valid(row):
    if not isinstance(row, dict) or not REQUIRED <= row.keys() <= REQUIRED | OPTIONAL:
        return False
    if not all(text(row[key], 8192 if key == "story" else 256) for key in (REQUIRED - {"styleTags", "permission", "platform", "sourceURL"}) | (row.keys() & OPTIONAL)):
        return False
    tags = row["styleTags"]
    permission = row["permission"]
    if not (isinstance(tags, list) and len(tags) <= 16 and all(text(tag, 64) for tag in tags) and len(set(tags)) == len(tags)
            and row["platform"] in ("native", "windows") and public_url(row["sourceURL"])
            and isinstance(permission, dict) and permission.keys() == {"grantedBy", "grantedAt", "evidenceURL", "scope"}
            and text(permission["grantedBy"]) and public_url(permission["evidenceURL"])
            and permission["scope"] == "public-redistribution"):
        return False
    try:
        date = datetime.date.fromisoformat(permission["grantedAt"])
        return date.isoformat() == permission["grantedAt"]
    except (ValueError, TypeError):
        return False


def json_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=False) + "\n").encode()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON field")
            result[key] = value
        return result
    return json.loads(path.read_bytes(), object_pairs_hook=unique)


def generate(root, base_url, revision, audit_binary):
    if not public_url(base_url) or not base_url.endswith("/") or revision < 1:
        raise ValueError("Supply a real HTTPS publication base URL and positive revision")
    entries_root = root / "entries"
    if entries_root.is_symlink() or not entries_root.is_dir():
        raise ValueError("entries must be a regular directory")
    entries, reviews = [], {}
    for directory in sorted(entries_root.iterdir()):
        if directory.name.startswith("."):
            continue
        slug = directory.name
        if directory.is_symlink() or not directory.is_dir() or not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", slug):
            raise ValueError("Unsafe entry directory")
        config, metadata = directory / "config.cfg", directory / "entry.json"
        if any(path.is_symlink() or not path.is_file() for path in (config, metadata)):
            raise ValueError(f"{slug}: regular config.cfg and entry.json required")
        if metadata.stat().st_size > 32768 or not 1 <= config.stat().st_size <= 65535:
            raise ValueError(f"{slug}: file size limit")
        row = read_json(metadata)
        if not metadata_valid(row):
            raise ValueError(f"{slug}: permission, credit or metadata invalid")
        before = config.read_bytes()
        audited = subprocess.run([str(audit_binary), str(config), str(audit_binary.parent / "ConfigCatalog.json")], capture_output=True)
        if audited.returncode:
            # Swift diagnostics may include private paths, but never echo them
            # or untrusted source bytes into generated publication logs.
            raise ValueError(f"{slug}: production grammar audit failed")
        result = json.loads(audited.stdout)
        if set(result) != {"stock", "cod2x"} or any(result[mode]["blocked"] for mode in result):
            raise ValueError(f"{slug}: forbidden config content; cannot redistribute")
        if before != config.read_bytes():
            raise ValueError(f"{slug}: config changed during audit")
        row.update(id=slug, configURL=base_url + "entries/" + slug + "/config.cfg",
                   sha256=hashlib.sha256(before).hexdigest(), byteCount=len(before))
        entries.append(row)
        reviews[slug] = result
    index = {"schemaVersion": 1, "revision": revision, "entries": entries}
    if len(entries) > 2048 or len(json_bytes(index)) > 1048576:
        raise ValueError("Index size limit")
    def cell(value):
        return value.replace("|", "\\|").replace("\n", " ").replace("\r", " ").replace("<", "&lt;").replace(">", "&gt;").replace("[", "&#91;").replace("]", "&#93;")
    readme = "# Config index\n\n"
    if not entries:
        readme += "No permissioned entries are available. Nothing has been published or signed.\n"
    else:
        readme += "Player configs remain unchanged. Import them through safety review before use.\n\n| Player | Era | Style | Download |\n| --- | --- | --- | --- |\n"
        for row in entries:
            readme += f"| {cell(row['player'])} | {cell(row['era'])} | {cell(', '.join(row['styleTags']))} | [config]({row['configURL']}) |\n"
    return {"index.json": json_bytes(index), "review.json": json_bytes(reviews), "INDEX.md": readme.encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--revision", type=int, required=True)
    parser.add_argument("--audit-binary", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        outputs = generate(args.root, args.base_url, args.revision, args.audit_binary)
        if args.check:
            if any(not (args.root / name).is_file() or (args.root / name).read_bytes() != content for name, content in outputs.items()):
                raise ValueError("Generated directory artifacts are missing or stale")
        else:
            for name, content in outputs.items():
                path = args.root / name
                if path.is_symlink():
                    raise ValueError("Unsafe output path")
                path.write_bytes(content)
        print(f"Checked {len(json.loads(outputs['index.json'])['entries'])} permissioned entries; no publication performed.")
    except (ValueError, OSError, KeyError, TypeError, json.JSONDecodeError):
        print("Directory generation rejected invalid, unsafe or stale inputs.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
