#ifndef SS_GFX_PROFILE_H
#define SS_GFX_PROFILE_H

#include <stdint.h>

#ifndef SS_PROFILE_GFX
#define SS_PROFILE_GFX 0
#endif

typedef struct {
    uint32_t primitive_calls;
    uint32_t rect_calls;
    uint32_t hline_calls;
    uint32_t stipple_calls;
    uint32_t xor_rect_calls;
    uint32_t glyph_slow;
    uint32_t glyph_fast;
    uint32_t glyph_clip;
    uint32_t text_calls;
    uint32_t gvram_words_read;
    uint32_t gvram_words_written;
    uint32_t submitted_area;
    uint32_t clipped_area;
    uint32_t dma_attempts;
    uint32_t dma_ok;
    uint32_t dma_error;
    uint32_t dma_timeout;
    uint32_t dma_fallback_rows;
    uint32_t dma_error_status_samples;
    uint8_t dma_last_csr;
    uint8_t dma_last_cer;
    uint32_t dma_config_samples;
    uint8_t dma_dcr;
    uint8_t dma_ocr;
    uint8_t dma_scr;
    uint8_t dma_mfc;
    uint8_t dma_dfc;
    uint8_t dma_bfc;
    uint32_t render_all_calls;
    uint32_t render_region_calls;
    uint32_t full_background_fills;
    uint32_t zmap_rebuilds;
    uint32_t windows_considered;
    uint32_t windows_rendered;
    uint32_t windows_skipped_no_overlap;
    uint32_t windows_skipped_occluded;
    uint32_t dirty_marks;
    uint32_t dirty_area_submitted;
    uint32_t dirty_area_clipped;
    uint32_t drag_save_words;
    uint32_t drag_restore_words;
} SSGfxProfile;

void ss_gfx_profile_reset(void);
void ss_gfx_profile_snapshot(SSGfxProfile* out);

#if SS_PROFILE_GFX
extern SSGfxProfile ss_gfx_profile;

#define SS_PROFILE_PRIMITIVE_CALL()     do { ss_gfx_profile.primitive_calls++; } while (0)
#define SS_PROFILE_RECT_CALL()          do { ss_gfx_profile.rect_calls++; } while (0)
#define SS_PROFILE_HLINE_CALL()         do { ss_gfx_profile.hline_calls++; } while (0)
#define SS_PROFILE_STIPPLE_CALL()       do { ss_gfx_profile.stipple_calls++; } while (0)
#define SS_PROFILE_XOR_RECT_CALL()      do { ss_gfx_profile.xor_rect_calls++; } while (0)
#define SS_PROFILE_GLYPH_SLOW()         do { ss_gfx_profile.glyph_slow++; } while (0)
#define SS_PROFILE_GLYPH_FAST()         do { ss_gfx_profile.glyph_fast++; } while (0)
#define SS_PROFILE_GLYPH_CLIP()         do { ss_gfx_profile.glyph_clip++; } while (0)
#define SS_PROFILE_TEXT_CALL()          do { ss_gfx_profile.text_calls++; } while (0)
#define SS_PROFILE_GVRAM_READ(words)     do { ss_gfx_profile.gvram_words_read += (uint32_t)(words); } while (0)
#define SS_PROFILE_GVRAM_WRITE(words)    do { ss_gfx_profile.gvram_words_written += (uint32_t)(words); } while (0)
#define SS_PROFILE_SUBMITTED_AREA(area)  do { ss_gfx_profile.submitted_area += (uint32_t)(area); } while (0)
#define SS_PROFILE_CLIPPED_AREA(area)    do { ss_gfx_profile.clipped_area += (uint32_t)(area); } while (0)
#define SS_PROFILE_DMA_ATTEMPT()         do { ss_gfx_profile.dma_attempts++; } while (0)
#define SS_PROFILE_DMA_OK()              do { ss_gfx_profile.dma_ok++; } while (0)
#define SS_PROFILE_DMA_ERROR()           do { ss_gfx_profile.dma_error++; } while (0)
#define SS_PROFILE_DMA_TIMEOUT()         do { ss_gfx_profile.dma_timeout++; } while (0)
#define SS_PROFILE_DMA_FALLBACK_ROWS(n) do { ss_gfx_profile.dma_fallback_rows += (uint32_t)(n); } while (0)
#define SS_PROFILE_DMA_ERROR_STATUS(csr, cer) do { \
        ss_gfx_profile.dma_error_status_samples++; \
        ss_gfx_profile.dma_last_csr = (uint8_t)(csr); \
        ss_gfx_profile.dma_last_cer = (uint8_t)(cer); \
    } while (0)
#define SS_PROFILE_DMA_CONFIG(dcr, ocr, scr, mfc, dfc, bfc) do { \
        if (ss_gfx_profile.dma_config_samples == 0) { \
            ss_gfx_profile.dma_config_samples = 1; \
            ss_gfx_profile.dma_dcr = (uint8_t)(dcr); \
            ss_gfx_profile.dma_ocr = (uint8_t)(ocr); \
            ss_gfx_profile.dma_scr = (uint8_t)(scr); \
            ss_gfx_profile.dma_mfc = (uint8_t)(mfc); \
            ss_gfx_profile.dma_dfc = (uint8_t)(dfc); \
            ss_gfx_profile.dma_bfc = (uint8_t)(bfc); \
        } \
    } while (0)
#define SS_PROFILE_RENDER_ALL()         do { ss_gfx_profile.render_all_calls++; } while (0)
#define SS_PROFILE_RENDER_REGION()      do { ss_gfx_profile.render_region_calls++; } while (0)
#define SS_PROFILE_FULL_BG_FILL()       do { ss_gfx_profile.full_background_fills++; } while (0)
#define SS_PROFILE_ZMAP_REBUILD()        do { ss_gfx_profile.zmap_rebuilds++; } while (0)
#define SS_PROFILE_WINDOW_CONSIDERED()   do { ss_gfx_profile.windows_considered++; } while (0)
#define SS_PROFILE_WINDOW_RENDERED()     do { ss_gfx_profile.windows_rendered++; } while (0)
#define SS_PROFILE_WINDOW_SKIP_NO_OVERLAP() do { ss_gfx_profile.windows_skipped_no_overlap++; } while (0)
#define SS_PROFILE_WINDOW_SKIP_OCCLUDED() do { ss_gfx_profile.windows_skipped_occluded++; } while (0)
#define SS_PROFILE_DIRTY_MARK()          do { ss_gfx_profile.dirty_marks++; } while (0)
#define SS_PROFILE_DIRTY_AREA(area)      do { ss_gfx_profile.dirty_area_submitted += (uint32_t)(area); } while (0)
#define SS_PROFILE_DIRTY_CLIPPED_AREA(area) do { ss_gfx_profile.dirty_area_clipped += (uint32_t)(area); } while (0)
#define SS_PROFILE_DRAG_SAVE(words)      do { ss_gfx_profile.drag_save_words += (uint32_t)(words); } while (0)
#define SS_PROFILE_DRAG_RESTORE(words)   do { ss_gfx_profile.drag_restore_words += (uint32_t)(words); } while (0)
#else
#define SS_PROFILE_PRIMITIVE_CALL()      do { } while (0)
#define SS_PROFILE_RECT_CALL()           do { } while (0)
#define SS_PROFILE_HLINE_CALL()          do { } while (0)
#define SS_PROFILE_STIPPLE_CALL()        do { } while (0)
#define SS_PROFILE_XOR_RECT_CALL()       do { } while (0)
#define SS_PROFILE_GLYPH_SLOW()          do { } while (0)
#define SS_PROFILE_GLYPH_FAST()          do { } while (0)
#define SS_PROFILE_GLYPH_CLIP()          do { } while (0)
#define SS_PROFILE_TEXT_CALL()           do { } while (0)
#define SS_PROFILE_GVRAM_READ(words)     do { } while (0)
#define SS_PROFILE_GVRAM_WRITE(words)    do { } while (0)
#define SS_PROFILE_SUBMITTED_AREA(area)  do { } while (0)
#define SS_PROFILE_CLIPPED_AREA(area)    do { } while (0)
#define SS_PROFILE_DMA_ATTEMPT()         do { } while (0)
#define SS_PROFILE_DMA_OK()              do { } while (0)
#define SS_PROFILE_DMA_ERROR()           do { } while (0)
#define SS_PROFILE_DMA_TIMEOUT()         do { } while (0)
#define SS_PROFILE_DMA_FALLBACK_ROWS(n)  do { } while (0)
#define SS_PROFILE_DMA_ERROR_STATUS(csr, cer) do { } while (0)
#define SS_PROFILE_DMA_CONFIG(dcr, ocr, scr, mfc, dfc, bfc) do { } while (0)
#define SS_PROFILE_RENDER_ALL()          do { } while (0)
#define SS_PROFILE_RENDER_REGION()       do { } while (0)
#define SS_PROFILE_FULL_BG_FILL()        do { } while (0)
#define SS_PROFILE_ZMAP_REBUILD()        do { } while (0)
#define SS_PROFILE_WINDOW_CONSIDERED()   do { } while (0)
#define SS_PROFILE_WINDOW_RENDERED()     do { } while (0)
#define SS_PROFILE_WINDOW_SKIP_NO_OVERLAP() do { } while (0)
#define SS_PROFILE_WINDOW_SKIP_OCCLUDED() do { } while (0)
#define SS_PROFILE_DIRTY_MARK()          do { } while (0)
#define SS_PROFILE_DIRTY_AREA(area)      do { } while (0)
#define SS_PROFILE_DIRTY_CLIPPED_AREA(area) do { } while (0)
#define SS_PROFILE_DRAG_SAVE(words)      do { } while (0)
#define SS_PROFILE_DRAG_RESTORE(words)   do { } while (0)
#endif

#endif /* SS_GFX_PROFILE_H */
