# R1 — C++ type recovery and baseline execution

Status: bounded phase complete; full-game reconstruction remains unvalidated.
All binary-derived dumps and execution receipts remain in the private analysis
workspace under `reconstruction/phase-r1`. A0–A6 evidence is unchanged.

## Changes

The STABS parser now retains access, inheritance, overloaded/static/virtual
method and static-member metadata, including namespaced inline cross-references.
Successful outer definitions prevent context-free reparsing of their nested
spans; failed outer definitions still permit independent nested recovery.
Inheritance and virtual-storage types are explicitly `cpp_class`, which native
layout generation rejects until their ABI is independently established.
Unsupported grammar continues to fail closed. No original type dump is committed.

The owned MP corpus now produces 84,314 types, 26,218 names and zero reported
parser errors, including 1,286 C++ class nodes. A4 reported 2,002 errors; this
comparison includes elimination of redundant nested parse attempts, so it is
not a count of 2,002 independently repaired types. Parse success is not proof of
native layout or semantic equivalence. Older operator/member-pointer grammar
outside the measured corpus remains unsupported.

Only diagnostic comments changed in the committed BSS snapshot. Removing C
comments leaves identical declarations and initializers. No layout rejection
was weakened to obtain this result.

## Validation

- All 32 datagen tests and the full i386/arm64 generation round-trip pass,
  including original bytes, relocations, symbols, alignment and determinism.
- Strict snapshot comparison passes for all five committed generated files.
- Existing voice/original-instruction campaign: 57 checks pass. It retains
  modeled imports and codec tolerance gates; this is not bit-exact voice parity.
- Reverb (10 cases) and collision (5 cases) pass in stock and CODX fixtures.
  These use assembly/analytic expectations rather than executing the original.
- Two 100-frame native runs agree on existing simulation hashes. The display
  was locked: this establishes repeatability only, not visual/input validation.
- Fresh stock and CODX ARM64 application builds pass with the R2 math change.
  The host SDL deployment warning remains; macOS 26 compatibility is unproven.

Independent correctness, testing, maintainability and adversarial review found
no remaining correctness issue in the final bounded change. The initially
quadratic tag search was fixed and the final full corpus parse took 16.1 seconds.

Next work follows the validated-reconstruction plan: discovery/source closure,
required native layouts and original-process comparisons remain open.
