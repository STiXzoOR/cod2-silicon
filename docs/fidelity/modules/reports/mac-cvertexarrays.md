# Mac/DirectX 9/CVertexArrays reconciliation

Updated 2026-11-05.

**46/48 reconciled: 15 equal, 31 fixed; 2 unchecked.** Mac 1.3 i386 is the authority. The legacy OFF bodies are unchanged.

The native VAO base destructor was empty. Mac 0x1127e8 installs the VAO vtable, destroys eight texcoord members in reverse order through their virtual complete destructors, and leaves the other four members with base vtables. Restoring this also repairs the D1/D0 wrappers at 0x112886/0x112924. Neither the original nor the restored destructor releases the shared GL binding.

The full texcoord guard overload at 0x113386 was missing. It now writes enable, type, stride, pointer, and size while leaving validation unchanged. The short enable-only guard remains separate. The base reset/force logic, pointer setters, guard setters, and both VAO constructor identities at 0x112498/0x112640 were read against the original instructions and native byte-sized flag layouts.

Twenty-two attributed array callbacks and destructors live in COpenGL.c, as do the five attributed GL state setters. Their restored implementations depend on the pending GL commit cd687d6. Its actual source was compared with the coalesced Mac bodies at 0x2ee818–0x2eeab1 and the state setters at 0x1fea4–0x1ffdf, then compiled with the real Apple client command. No shared COpenGL source or header was edited by this unit.

The COpenGL constructor rows at 0x22a0e/0x22b6e remain unchecked. The typed native singleton replaces the original public C++ layout, and the current draw path loads world/view/projection matrices directly from device/backend state. Native per-unit texture matrices are also present. Neither path establishes the contract of the original constructor's instance matrix at offset 0x78, so this report does not claim constructor equivalence or add unused fields to the singleton.

Triage and explicit-range constant checks ran. The three owned Apple arm64 renderer objects and the actual pending COpenGL provider object compile; the dedicated target builds. Five OFF configurations match with actual line-macro expansion. No tests were added or changed, no full CI or replays ran, and the renderer changes were not exercised. Integration and runtime validation remain with the parent worker.

Private evidence: `/Users/stix/Library/Application Support/CoD2-native-decomp/recon-mac-arrays.noindex/`.
