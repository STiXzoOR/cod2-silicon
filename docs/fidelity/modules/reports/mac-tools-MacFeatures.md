# Mac/Tools/MacFeatures reconciliation

Updated 2026-11-05. **5/5 reconciled: 5 intentional.**

Mac 0x8852 queries a 32-bit CPU frequency, reduces to integer MHz, divides by 1000 and falls back to 800 MHz. Live native `Sys_GetCPUGHz` uses the typed MacSystem provider: `hw.cpufrequency_max`, with Apple Silicon IOKit cluster-frequency data as fallback. Mac 0x88d4 queries 32-bit physical memory, shifts20 and falls back to 128 MiB. Live native `Sys_SystemMemoryMB` uses 64-bit `hw.memsize`, retaining memory above 4 GiB and returning 0 on query failure. These are deliberate modern hardware providers, not last-bit arithmetic equality.

Mac 0x8780 caches Gestalt system version; the native 0x1040 shim has no caller, while deployment/launcher enforce supported OS startup. Mac 0x87ca queries selector/attribute bits; the only source caller asks for x86 bit25 as the compatibility SSE capability. Native compiler-owned code does not depend on Gestalt. Mac 0x8802 caches the PowerPC AltiVec bit; native false selects scalar `CColorConverter` paths on arm64. The three unused native AltiVec arguments have explicit null callers.

Public project `UInt32` (unsigned long) and Boolean signatures were retained. Parent is aligning the new SDK-facing mac_configure declarations to those project signatures in its own lifecycle integration; no shared declarations were changed here.

## Verification and limits

Exact-symbol Mac disassembly was read by hand; triage and address-ranged constant checks were used as leads, not equality proofs. Native arm64 Release `cod2_macos_ded` builds at background priority with two jobs. No new tests, game launches, full CI or replays were run. Parent owns integrated client/runtime validation. Private evidence: `~/Library/Application Support/CoD2-native-decomp/recon-mac-files-tools.noindex/`.
