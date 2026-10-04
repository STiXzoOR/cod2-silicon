## Problem and resulting behavior

Describe the trigger, previous behavior and resulting behavior. Link the issue
if one exists. Include relevant tradeoffs and substantial AI assistance.

## Verification

Give exact commands and results. Separate synthetic checks from licensed-data
or live tests; record unavailable checks and known failures.

- [ ] Stock and CoD2x arm64 Release clients build.
- [ ] Relevant fixtures and the full merge gate pass; ABI mismatches stay at zero.
- [ ] `COD2_X64=OFF` object/binary comparison is byte-for-byte identical.
- [ ] Native changes use the appropriate architecture/platform/feature guards.
- [ ] No game data, binaries, shaders/cache, demos, keys, identifiers, credentials or dumps are included.
- [ ] Style matches `.clang-format`; shared-file changes stay focused.
- [ ] Every commit has a DCO sign-off (`git commit -s`).

Read [CONTRIBUTING.md](../CONTRIBUTING.md) for commands and data requirements.
