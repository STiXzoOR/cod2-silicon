# cod2-configs repository tooling

This directory is the tooling of the public catalogue of player setups,
[STiXzoOR/cod2-configs](https://github.com/STiXzoOR/cod2-configs). `entries/` is empty here: the
entries live in that repository, which carries a copy of these files at its root.

[CONTRIBUTING.md](CONTRIBUTING.md) is the step-by-step for adding, correcting and removing a setup,
and for answering a removal request.

The catalogue collects configs that were published on the open web and credits the page each one
came from. It does not claim that a player approved the listing, and it removes or corrects an entry
when the player or the page asks, through an issue on that repository. Each `entries/<slug>/` holds
`config.cfg` and `entry.json` (see `entry.schema.json`):

- `config.cfg` is the published text after `clean` (built by `build-audit.sh` from `Clean.swift`):
  the launcher's own import review reads it for both game versions, and every line it blocks, and
  every line that says who the player is, what they typed, a secret, a path or a setting of another
  game, is left out. The rest keeps its order and bytes.
- `entry.json` says who it is credited to and where it was published: `sourceURL`, and `provenance`
  with the page's name, the day the copy was taken, how the name got on it (`self-published`,
  `listed` by a page under that name, or `uploaded` under that name) and a sentence on what was left
  out. `facts` carries the three numbers a card shows, as the config writes them. `featured` marks an
  entry for the first shelf: the launcher counts nothing about what players import, so there is no
  popularity ranking. No real names, nationalities or birthdays, even when a page shows them.

Schema validation can require a source record; it cannot prove that the page was right about whose
config it is. Say `listed` or `uploaded` unless the player posted it themselves.

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

Signing is a separate maintainer operation after content review.
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
