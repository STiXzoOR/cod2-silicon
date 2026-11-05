# Platform repair after batch 22

The user authorized pushing batch 22 candidate `1f8e4ea2` with its recorded platform failures, then fixing them. The ledger remains 354 unchecked, 4,754 equal, 1,180 fixed and 3,169 intentional. The original failed runs remain evidence; this follow-up does not turn those runs into passes.

## Metal fixture

The GPU submission timeline includes both producer work and asynchronous presentation. The existing frame fixture compared the shared total while checking producer-only tail-fence and skipped-scene behavior. A temporary 20 ms yield reproduced the exact failure: the shared total advanced from 5 to 6 while producer commits stayed at 5. The background presentation uses the same GPU timeline and legitimately commits during that interval.

The fixture now counts successful calls to the real producer commit, wrapping only the included frame implementation. Background presentation still uses the real queue unchanged. The four existing assertions continue to detect premature, extra, missing and rejected-scene producer submissions. No tests, cases or assertions were added or removed; all 70 assertion sites remain. With the same temporary yield, five runs observed the shared counter advance while the producer-only assertions passed. Temporary delays and diagnostic logging were removed before final verification.

## Software gamma

Both original sanitizer failures reached the final identity framebuffer blit after successful adjusted-gamma pixel checks and mode changes. ASan reported a 1,280-byte write inside Apple libGLImage/GLRendererFloat. Failure while reporting the free stack leaves allocation ownership and the underlying driver defect unproved. This is the recurrence documented in the [earlier gamma report](../../../macos-port/reports/2026-10-26-F2-gamma-ci-diagnostics.md).

The bounded workaround keeps using the already initialized gamma shader with an identity ramp on `Apple Software Renderer`. It avoids the failing transition back to the software identity blit. Initial identity presentation and other renderers retain their existing blit path. Full GL attributes are preserved whenever the shader is selected; the existing program, active texture, pixel-store and framebuffer restoration remains in place. This adds no shader resource or initialization failure path. The production change is confined to `COD2_X64`.

## Verification and limits

The existing default/software gamma cases each passed ten repetitions. The complete native platform stage passed with and without sanitizers after temporary diagnostics were removed. Source preprocessing and actual diagnostic line expansions match in all five inactive configurations. The full integration gate is recorded separately once it finishes; these focused results alone do not certify the whole project.

The parent performed the repair review after the 90% usage cutoff; no new independent worker was started. Batch 22's original reconciliation had independent review, but that review does not cover this follow-up. No new gameplay replay is required for these presentation/fixture changes; batch 22's existing stock and CoD2x replays match the accepted baselines. Neither those replays nor these fixtures certify complete native-client playability.
