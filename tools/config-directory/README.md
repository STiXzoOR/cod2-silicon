# cod2-configs repository preparation

This directory is the unpublished repository scaffold. It contains no players,
permission grants, publication URLs, signatures or production trust root.
`entries/` is intentionally empty. The launcher ships an unavailable state until
the maintainer establishes permissioned content and signed publication.

Each future `entries/<slug>/` contains `config.cfg` (the player's exact published
bytes) and `entry.json` conforming to `entry.schema.json`. Obtain public
redistribution permission and record who granted it, the date and an HTTPS link
to evidence. Include source credit and the original source URL. Do not use the
private research corpus. Review permission evidence yourself: schema validation
can require a record, but cannot prove that a grant is genuine or legally sufficient.

The generator computes signed download identity, length and SHA-256 from the
actual file. It invokes the production Swift ConfigDocument parser and I1 safety
classifier in both stock and CoD2x dialects with the F4 catalog. Blocked content
rejects publication. Risky and review-only command counts remain visible in
`review.json`. Signed configs still go through the launcher's normal safety
review; a signature never permits credentials, unknown forwarded commands or
raw exec imports.

To prepare this as a standalone repository, copy this directory as the repository
root. Supply a reviewed engine checkout containing ConfigDocument, ConfigImport,
ConfigImportSafety, ConfigCatalog and ConfigImportCatalogAdapter plus its catalog
resource. CI requires `MODEL_REPOSITORY`, `MODEL_REVISION` (a full immutable commit
hash), `PUBLICATION_BASE_URL` (the actual HTTPS directory root, ending in `/`),
and `INDEX_REVISION` as repository variables. They are deliberately unset here.
Pin the composed integration revision before enabling that CI; this local branch
has not been published. CI does not sign, deploy or publish anything.

```sh
# Use an output directory owned by your task, outside game data.
sh build-audit.sh /path/to/reviewed/cod2-silicon "$TASK_SCRATCH/audit"
python3 generate.py --root . --base-url "$PUBLICATION_BASE_URL" \
  --revision "$INDEX_REVISION" --audit-binary "$TASK_SCRATCH/audit/audit"
python3 generate.py --root . --base-url "$PUBLICATION_BASE_URL" \
  --revision "$INDEX_REVISION" --audit-binary "$TASK_SCRATCH/audit/audit" --check
```

Generated `index.json` conforms to `index.schema.json`; `INDEX.md` is the human
download index. `--check` compares all generated artifacts byte for byte and
refuses stale output. Config files are never rewritten. The checked-in empty
index is unsigned preparation only and cannot be loaded as a verified directory.

Signing is a separate maintainer operation after content and permission review.
The `sign` binary compiled above accepts **raw 32-byte Ed25519 key material** and
creates a detached **raw 64-byte signature over the exact index.json bytes**:

```sh
"$TASK_SCRATCH/audit/sign" sign index.json /private/path/to/signing-key index.sig
"$TASK_SCRATCH/audit/sign" verify index.json trust/public-key.ed25519 index.sig
```

Never put private keys in this repository. Establish key ownership, custody,
rotation, actual hosting and the launcher's pinned public key before publication.
Increment the positive index revision for each changed publication; the launcher
rejects a rollback or changed bytes at an already cached revision. This protects
only a retained verified cache, not a fresh install from an old correctly signed
index. No fixture key is a production trust root. CI refuses a present signature
without a separately reviewed `trust/public-key.ed25519` file.

Local verification from the engine checkout:

```sh
sh tools/config-directory/test.sh
```

Tests generate ephemeral keys and authored synthetic metadata/configs in trapped
scratch directories. They make no network requests and print no profile bytes or
key material. Swift uses version 6, complete concurrency checking, warnings as
errors, a macOS 13 host-architecture target, background priority and one compile
job. The launcher focused runners specifically target arm64.
