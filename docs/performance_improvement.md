# Draw Pipeline Performance Opportunities

This note captures the next set of graphics hot spots uncovered while reviewing the dirty‐region renderer and DMA path. It builds on the existing hybrid scanline + DMA optimisations documented in `DRAW_IMPROVEMENT_ja.md`.

## Current Hot Path Overview
- `ss_damage_draw_regions` walks every queued dirty rectangle and traverses each layer in z order (`ssos/os/window/damage.c:214`).
- For every layer that overlaps the region, `ss_damage_calculate_layer_region_overlap` forwards the request to `ss_layer_draw_rect_layer_bounds`.
- `ss_layer_draw_rect_layer_bounds` rechecks the global 8×8 ownership map per scanline, merges visible runs, and for each run either copies via CPU or issues a DMA burst using the lightweight helpers in `ssos/os/kernel/dma.c:29-59`.

The hybrid scanline work already trimmed a large amount of redundant memory traffic, but three remaining costs dominate when windows stack tightly or animations touch the same scanlines repeatedly.

## Opportunity 1 — Lightweight Occlusion Pre‑check
- `ss_damage_optimize_for_occlusion` is disabled (`return;` at `ssos/os/window/damage.c:268`) because the earlier, heavier occlusion sweeps risked starving layer 1 updates.
- Without any occlusion trimming, `ss_layer_draw_rect_layer_bounds` still probes the 8×8 map for every scanline even when upper layers completely cover the rect.
- Reintroduce a *conservative* filter: before calling into the scanline loop, check whether any block inside the region belongs to the candidate layer. A straight scan over the existing map array (already aligned to 8×8 blocks) is enough.
- Benefit: skips the expensive per‑block loop and DMA setup for layers that are fully hidden, without re‑introducing the previous masking bugs.
- Action sketch:
  1. Add helper `ss_layer_region_visible(layer, region)` that scans the map using the dirty rect extents and returns early on the first visible block.
  2. Call the helper from `ss_damage_draw_regions` before `ss_damage_draw_layer_region`.
  3. Gate the helper behind a debug toggle initially and log counts via `g_damage_perf` to validate coverage.

## Opportunity 2 — Batch DMA Submissions
- Each merged span currently performs `dma_clear()`, `dma_init_x68k_16color()`, `dma_start()`, `dma_wait_completion()`, and a final `dma_clear()` (pre-change behaviour in `ssos/os/window/layer.c`).
- The hardware already supports array chaining via `xfr_inf`, but we always program index 0 only (`ssos/os/kernel/dma.c:42-48`).
- Collect all contiguous spans for a scanline (or for an entire dirty rect) into the descriptor table and kick a single DMA transfer:
  - Fill `xfr_inf[n]` entries with `{src, width}` pairs.
  - Set `dma->btc` to the span count and issue one `dma_start`.
  - Clear once when the batch completes.
- Expected wins: fewer register writes, reduced bus arbitration overhead, better overlap with CPU work in the main loop.
- Implementation guardrails:
  - Retain the CPU copy path for short spans or misaligned addresses.
  - Track DMA completion in `ss_damage_perf_update` so we can quantify fewer starts per frame.
  - Use the lightweight helpers in `ssos/os/window/layer.c` so each span reuses the preconfigured controller while still falling back to CPU copies for gaps.

## Opportunity 3 — Use the Staging Buffer
- `ss_damage_init` allocates a 768×512 offscreen buffer (`g_damage_buffer.buffer`) but no code currently writes into it.
- Compositing each dirty rect into that RAM buffer allows us to:
  - Traverse z order once, blending as needed, entirely in system RAM.
  - Perform a single, wide DMA burst into VRAM.
  - Potentially keep persistent UI layers cached and only patch animated regions.
- Migration plan:
  1. Add a code path that writes `(overlap_x, overlap_y, overlap_w, overlap_h)` into the staging buffer using the existing layer scanline routine but targeting `g_damage_buffer.buffer`.
  2. DMA the final rect from the staging buffer to VRAM with the batching strategy above.
  3. Benchmark memory bandwidth (RAM→RAM copy + RAM→VRAM DMA) against the current per-layer VRAM writes; bail out if motion is sparse and the extra RAM traffic outweighs the gains.

## Immediate Next Steps
1. Prototype the occlusion pre-check and log how often it culls spans; ensure no regressions by running the interactive demo and unit tests.
2. Extend `dma_init_x68k_16color` to accept a span count and describe the batching descriptor layout; add instrumentation to confirm reduced DMA invocations per frame.
3. Experiment with staging-buffer compositing on a gated build flag to validate whether the additional copy pays off for UI workloads (mouse drags, frequent label updates).

Keeping the instrumentation already in place (`SS_PERF_*` macros, `g_damage_perf`) will make it easy to compare frame time, draw time, and DMA counts before and after each change.
