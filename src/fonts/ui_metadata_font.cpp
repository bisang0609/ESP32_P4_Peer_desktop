#include "ui_metadata_font.h"
#include <cstddef>
#include <cstdlib>

#if LV_USE_TINY_TTF
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#endif

static bool g_fonts_ready = false;
static bool g_runtime_fonts_ready = false;
static lv_font_t * g_font_kr_32 = nullptr;
static lv_font_t * g_font_kr_16 = nullptr;
static lv_font_t * g_font_kr_14 = nullptr;
static lv_font_t * g_font_kr_12 = nullptr;
static lv_font_t * g_font_jp_32 = nullptr;
static lv_font_t * g_font_jp_16 = nullptr;
static lv_font_t * g_font_jp_14 = nullptr;
static lv_font_t * g_font_jp_12 = nullptr;

#if LV_USE_TINY_TTF
static uint8_t * g_font_kr_blob = nullptr;
static size_t g_font_kr_blob_size = 0;
static uint8_t * g_font_jp_blob = nullptr;
static size_t g_font_jp_blob_size = 0;
#endif

static const char * const FONT_KR_PATHS[] = {
    "/fonts/NotoSansKR-Regular.ttf",
    "/fonts/NotoSansKR-Regular.otf",
    "/NotoSansKR-Regular.ttf",
    "/NotoSansKR-Regular.otf",
};

static const char * const FONT_JP_PATHS[] = {
    "/fonts/NotoSansJP-Regular.ttf",
    "/fonts/NotoSansJP-Regular.otf",
    "/NotoSansJP-Regular.ttf",
    "/NotoSansJP-Regular.otf",
};

#if LV_USE_TINY_TTF
static const char * find_existing_font_path(const char * const * paths, size_t count)
{
    for(size_t i = 0; i < count; ++i) {
        if(SPIFFS.exists(paths[i])) return paths[i];
    }
    return nullptr;
}

static bool load_font_blob_from_spiffs(const char * path, uint8_t ** out_blob, size_t * out_size)
{
    if(path == nullptr || out_blob == nullptr || out_size == nullptr) return false;
    if(*out_blob != nullptr && *out_size > 0) return true;

    File font_file = SPIFFS.open(path, FILE_READ);
    if(!font_file || font_file.isDirectory()) return false;

    const size_t file_size = static_cast<size_t>(font_file.size());
    if(file_size == 0) {
        font_file.close();
        return false;
    }

    uint8_t * blob = static_cast<uint8_t *>(heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if(blob == nullptr) blob = static_cast<uint8_t *>(malloc(file_size));
    if(blob == nullptr) {
        font_file.close();
        return false;
    }

    const size_t bytes_read = font_file.read(blob, file_size);
    font_file.close();
    if(bytes_read != file_size) {
        free(blob);
        return false;
    }

    *out_blob = blob;
    *out_size = file_size;
    return true;
}

static void destroy_tiny_ttf_font(lv_font_t ** font)
{
    if(font == nullptr || *font == nullptr) return;
    lv_tiny_ttf_destroy(*font);
    *font = nullptr;
}

static bool create_tiny_ttf_set(const uint8_t * blob, size_t blob_size,
                                lv_font_t ** out_32, lv_font_t ** out_16,
                                lv_font_t ** out_14, lv_font_t ** out_12)
{
    if(blob == nullptr || blob_size == 0 || out_32 == nullptr || out_16 == nullptr || out_14 == nullptr || out_12 == nullptr) {
        return false;
    }

    *out_32 = lv_tiny_ttf_create_data_ex(blob, blob_size, 32, LV_FONT_KERNING_NORMAL, 256);
    *out_16 = lv_tiny_ttf_create_data_ex(blob, blob_size, 16, LV_FONT_KERNING_NORMAL, 256);
    *out_14 = lv_tiny_ttf_create_data_ex(blob, blob_size, 14, LV_FONT_KERNING_NORMAL, 192);
    *out_12 = lv_tiny_ttf_create_data_ex(blob, blob_size, 12, LV_FONT_KERNING_NORMAL, 192);

    if(*out_32 && *out_16 && *out_14 && *out_12) return true;

    destroy_tiny_ttf_font(out_32);
    destroy_tiny_ttf_font(out_16);
    destroy_tiny_ttf_font(out_14);
    destroy_tiny_ttf_font(out_12);
    return false;
}
#endif

lv_font_t ui_font_title_chain;
lv_font_t ui_font_artist_chain;
lv_font_t ui_font_next_title_chain;
lv_font_t ui_font_next_artist_chain;

static void set_default_metadata_font_chain(void)
{
    ui_font_title_chain = lv_font_montserrat_32;
    ui_font_artist_chain = lv_font_montserrat_16;
    ui_font_next_title_chain = lv_font_montserrat_14;
    ui_font_next_artist_chain = lv_font_montserrat_12;
}

bool ui_metadata_fonts_init(void)
{
    set_default_metadata_font_chain();
    if(g_fonts_ready) return g_runtime_fonts_ready;

#if LV_USE_TINY_TTF
    const char * font_kr_path = find_existing_font_path(FONT_KR_PATHS, sizeof(FONT_KR_PATHS) / sizeof(FONT_KR_PATHS[0]));
    const char * font_jp_path = find_existing_font_path(FONT_JP_PATHS, sizeof(FONT_JP_PATHS) / sizeof(FONT_JP_PATHS[0]));

    if(font_kr_path) {
        if(load_font_blob_from_spiffs(font_kr_path, &g_font_kr_blob, &g_font_kr_blob_size)) {
            Serial.printf("[FONT] Loaded KR runtime font: %s (%u bytes)\n", font_kr_path, static_cast<unsigned>(g_font_kr_blob_size));
            if(g_font_kr_32 == nullptr) {
                create_tiny_ttf_set(g_font_kr_blob, g_font_kr_blob_size,
                                    &g_font_kr_32, &g_font_kr_16, &g_font_kr_14, &g_font_kr_12);
            }
        } else {
            Serial.printf("[FONT] Failed to read KR font from SPIFFS: %s\n", font_kr_path);
        }
    }

    if(font_jp_path) {
        if(load_font_blob_from_spiffs(font_jp_path, &g_font_jp_blob, &g_font_jp_blob_size)) {
            Serial.printf("[FONT] Loaded JP runtime font: %s (%u bytes)\n", font_jp_path, static_cast<unsigned>(g_font_jp_blob_size));
            if(g_font_jp_32 == nullptr) {
                create_tiny_ttf_set(g_font_jp_blob, g_font_jp_blob_size,
                                    &g_font_jp_32, &g_font_jp_16, &g_font_jp_14, &g_font_jp_12);
            }
        } else {
            Serial.printf("[FONT] Failed to read JP font from SPIFFS: %s\n", font_jp_path);
        }
    }

    if(g_font_kr_32 && g_font_kr_16 && g_font_kr_14 && g_font_kr_12) {
        if(g_font_jp_32 && g_font_jp_16 && g_font_jp_14 && g_font_jp_12) {
            g_font_kr_32->fallback = g_font_jp_32;
            g_font_kr_16->fallback = g_font_jp_16;
            g_font_kr_14->fallback = g_font_jp_14;
            g_font_kr_12->fallback = g_font_jp_12;
        }

        ui_font_title_chain.fallback = g_font_kr_32;
        ui_font_artist_chain.fallback = g_font_kr_16;
        ui_font_next_title_chain.fallback = g_font_kr_14;
        ui_font_next_artist_chain.fallback = g_font_kr_12;
        g_runtime_fonts_ready = true;
        g_fonts_ready = true;
        return true;
    }
#endif

    // Fallback when runtime TTF files are unavailable: keep chain functional for CJK ideographs.
#if LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
    ui_font_title_chain.fallback = &lv_font_source_han_sans_sc_16_cjk;
    ui_font_artist_chain.fallback = &lv_font_source_han_sans_sc_16_cjk;
#endif
#if LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
    ui_font_next_title_chain.fallback = &lv_font_source_han_sans_sc_14_cjk;
    ui_font_next_artist_chain.fallback = &lv_font_source_han_sans_sc_14_cjk;
#endif
    g_runtime_fonts_ready = false;
    g_fonts_ready = true;
    return false;
}
