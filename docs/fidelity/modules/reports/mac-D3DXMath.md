# Mac Direct3D math reconciliation

Three entries: two fixed and one equal. All three original bodies were read from exact symbol entry points in the Mac i386 binary.

D3DXVec4Transform (0x23b66) loads the original vector and computes all four outputs before storing at 0x23c28–0x23c36. Native code wrote the first output before reading the remaining vector components, corrupting an in-place transform. The native path now calculates four temporary results before any output store. This also preserves the original read-before-write behavior when output overlaps matrix storage. Each dot product retains its left-associated four-term SSE order.

D3DXMatrixMultiply (0x239b8) already used a 16-float temporary before copying to output, preserving either input alias. Its native expression now spells the Mac third/fourth-term addition operand order, p3 + (p2 + (p0 + p1)), and the last column’s reversed multiply operands. The rounded dependency chain for ordinary finite values was already the same; this is an operand-order reconciliation, not evidence of a measured finite-value error. The actual original body, including the separately unrolled last column and all output stores, was inspected.

DXGetErrorDescription9A (0x23c3e) returns null without using its argument in the Mac binary; the existing native null return is equal. No speculative error table was added.

The active device model-view path calls D3DXMatrixMultiply. No direct native source caller of D3DXVec4Transform was found. D3DXMatrixInverse is attributed to a different module and was not changed or certified here. Legacy source remains unchanged under COD2_X64=OFF. Constant/call triage was used only as a supplement to instruction reading; no Mac float literals occur in these three bodies.

The actual arm64 client object compiles. The arm64 Release dedicated build also passes. All five legacy guard configurations are unchanged; only native port-on differs. Independent review is pending. No new tests or runtime probes were added; alias handling and arithmetic are compiled and reviewed only.
