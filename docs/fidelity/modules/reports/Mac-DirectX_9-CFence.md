# Mac/DirectX 9/CFence reconciliation

Updated 2026-11-05. All 5 assigned entries were read against Mac 1.3: 3 equal, 2 fixed.

The constructor at 0x1f9c0 previously stored a zero ID. It now generates 256 fences one at a time when the unused pool is empty, takes the last ID, sets it and removes it from the unused pool. Existing IDs are reused in the same LIFO order. Shutdown at 0x1fa90 now deletes each unused GL fence and empties the vector while retaining its backing allocation, matching the original erase behavior. Static initialization and destruction wrappers were checked at their actual addresses 0x1faee, 0x1fb4e and 0x1fb5e; the repeated mangled static-helper name requires address selection rather than otool -p's first match.

The native record is GLuint ID, native start pointer, UINT32 byte size and UINT32 frame count, 24 bytes on arm64. The buffer worker independently checked this shape for its recovered fence queues. The public unused-ID vector remains three GLuint pointers. The native allocation uses defined malloc/realloc storage rather than the original STL allocator implementation; no external serialized layout is involved.

Arm64 Release dedicated target and the actual CFence renderer object compiled at low priority. All five OFF preprocessing configurations, including actual __LINE__, match the baseline. No fence waits or GPU synchronization were exercised; no new tests, full CI or replays were run. The recovered buffer-fence caller and GL Init/Shutdown lifecycle still require parent integration validation. Private evidence: ~/Library/Application Support/CoD2-native-decomp/recon-mac-gl.noindex/.
