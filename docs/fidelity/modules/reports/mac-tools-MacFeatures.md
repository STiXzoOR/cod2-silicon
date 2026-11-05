# Mac/Tools/MacFeatures reconciliation

Updated 2026-11-05. **5/5 reconciled: 1 fixed, 4 intentional.**

Mac 0x8852 queries a 32-bit CPU frequency, reduces to integer MHz and divides by 1000. Mac 0x88a2-0x88c3 falls back to 800 MHz when the query fails. The native wrapper previously returned 0 after all modern queries failed; it now restores `800.0f / 1000.0f` when the provider has no positive frequency. Positive results still come from typed MacSystem queries (`hw.cpufrequency_max`, then Apple Silicon IOKit cluster data). Live `Sys_CpuGHz` feeds `sys_info`; `Com_SetRecommended` multiplies it by 1.02 and compares positive CSV thresholds, so preserving zero could reject every CPU row. Compiled and reviewed, not exercised. Modern successful-query precision remains an intentional provider adaptation.

Mac 0x88d4 queries 32-bit physical memory, shifts20 and falls back to 128 MiB. Native `Sys_SystemMemoryMB` intentionally retains the 64-bit `hw.memsize` provider, including its 0 result when unavailable. The actual `Com_SetRecommended` consumer clamps values below 128 to 128; successful values of at least 128 receive an 8 MiB tolerance. Thus native query failure selects a conservative 128 MiB, whereas substituting Mac's 128 MiB wrapper fallback would become 136 MiB in that consumer. This audit keeps the existing conservative unavailable-memory behavior explicitly; it does not claim equality of the diagnostic value or every threshold outcome.

Mac 0x8780 caches Gestalt system version; the native 0x1040 shim has no caller, while deployment/launcher enforce supported OS startup. Mac 0x87ca queries selector/attribute bits; the only source caller asks for x86 bit25 as the compatibility SSE capability. Native compiler-owned code does not depend on Gestalt. Mac 0x8802 caches the PowerPC AltiVec bit; native false selects scalar `CColorConverter` paths on arm64. The three unused native AltiVec arguments have explicit null callers.

Public project `UInt32` (unsigned long) and Boolean signatures were retained. Parent is aligning the new SDK-facing mac_configure declarations to those project signatures in its own lifecycle integration; no shared declarations were changed here.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.
