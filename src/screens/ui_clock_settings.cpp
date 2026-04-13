/**
 * Clock Settings Screen
 * Configure clock/screensaver: trigger mode, inactivity timeout,
 * photo background + refresh interval, and timezone selection.
 * All settings persisted to NVS immediately on change.
 */

#include "ui_common.h"
#include "config.h"
#include "clock_screen.h"

// Forward declaration
lv_obj_t* createSettingsSidebar(lv_obj_t* screen, int activeIdx);

// ============================================================================
// Helper — reusable section header separator line
// ============================================================================
static void addSectionLabel(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_set_style_pad_top(lbl, 12, 0);
}

static void addDescLabel(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT2, 0);
}

// ============================================================================
// Helper — styled switch matching the General screen pattern
// ============================================================================
static lv_obj_t* addSwitch(lv_obj_t* parent, bool initial) {
    lv_obj_t* sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 50, 26);
    lv_obj_set_style_margin_top(sw, 8, 0);
    lv_obj_set_style_radius(sw, 13, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, COL_ACCENT,
        (lv_style_selector_t)((uint32_t)LV_PART_INDICATOR | (uint32_t)LV_STATE_CHECKED));
    lv_obj_set_style_radius(sw, 13, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(sw, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_radius(sw, 11, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);
    if (initial) lv_obj_add_state(sw, LV_STATE_CHECKED);
    return sw;
}

// ============================================================================
// createClockSettingsScreen
// ============================================================================
void createClockSettingsScreen() {
    scr_clock_settings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_clock_settings, lv_color_hex(0x121212), 0);

    // Sidebar — Clock is index 6
    lv_obj_t* content = createSettingsSidebar(scr_clock_settings, 4);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    // ── Title ────────────────────────────────────────────────────────────────
    lv_obj_t* lbl_title = lv_label_create(content);
    lv_label_set_text(lbl_title, "Clock");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_set_style_pad_bottom(lbl_title, 8, 0);

    // ── Activate mode ────────────────────────────────────────────────────────
    addSectionLabel(content, "Activate clock screen:");
    addDescLabel(content,
        "Choose when the clock/screensaver should appear");

    lv_obj_t* dd_mode = lv_dropdown_create(content);
    lv_dropdown_set_options(dd_mode,
        "Disabled\n"
        "After inactivity\n"
        "After inactivity  paused only\n"
        "After inactivity  nothing playing");
    lv_dropdown_set_selected(dd_mode, (uint16_t)clock_mode);
    lv_obj_set_width(dd_mode, lv_pct(100));
    lv_obj_set_style_bg_color(dd_mode, COL_CARD, 0);
    lv_obj_set_style_text_color(dd_mode, COL_TEXT, 0);
    lv_obj_set_style_text_font(dd_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(dd_mode, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_top(dd_mode, 6, 0);
    lv_obj_set_style_pad_bottom(dd_mode, 12, 0);
    {
        lv_obj_t* list = lv_dropdown_get_list(dd_mode);
        if (list) {
            lv_obj_set_style_bg_color(list, lv_color_hex(0x222222), 0);
            lv_obj_set_style_text_color(list, COL_TEXT, 0);
            lv_obj_set_style_text_font(list, &lv_font_montserrat_14, 0);
        }
    }
    lv_obj_add_event_cb(dd_mode, [](lv_event_t* e) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        clock_mode = (int)lv_dropdown_get_selected(dd);
        wifiPrefs.putInt(NVS_KEY_CLOCK_MODE, clock_mode);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Inactivity timeout ───────────────────────────────────────────────────
    addSectionLabel(content, "Inactivity timeout:");

    static lv_obj_t* lbl_timeout_val;
    lbl_timeout_val = lv_label_create(content);
    lv_label_set_text_fmt(lbl_timeout_val, "%d min", clock_timeout_min);
    lv_obj_set_style_text_color(lbl_timeout_val, COL_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_timeout_val, &lv_font_montserrat_14, 0);

    lv_obj_t* slider_timeout = lv_slider_create(content);
    lv_obj_set_width(slider_timeout, lv_pct(100));
    lv_obj_set_height(slider_timeout, 20);
    lv_slider_set_range(slider_timeout, 1, 60);
    lv_slider_set_value(slider_timeout, clock_timeout_min, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_timeout, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_timeout, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_timeout, COL_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_radius(slider_timeout, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(slider_timeout, 10, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(slider_timeout, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_top(slider_timeout, 4, 0);
    lv_obj_set_style_pad_bottom(slider_timeout, 12, 0);
    lv_obj_add_event_cb(slider_timeout, [](lv_event_t* e) {
        lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
        clock_timeout_min = lv_slider_get_value(slider);
        lv_label_set_text_fmt((lv_obj_t*)lv_event_get_user_data(e),
                              "%d min", clock_timeout_min);
        wifiPrefs.putInt(NVS_KEY_CLOCK_TIMEOUT, clock_timeout_min);
    }, LV_EVENT_VALUE_CHANGED, lbl_timeout_val);

    // ── Time format ──────────────────────────────────────────────────────────
    addSectionLabel(content, "Time format:");
    addDescLabel(content, "12-hour (AM/PM)");

    lv_obj_t* sw_12h = addSwitch(content, clock_12h);
    lv_obj_add_event_cb(sw_12h, [](lv_event_t* e) {
        lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
        clock_12h = lv_obj_has_state(sw, LV_STATE_CHECKED);
        wifiPrefs.putBool(NVS_KEY_CLOCK_12H, clock_12h);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Photo background ─────────────────────────────────────────────────────
    addSectionLabel(content, "Photo background:");
    addDescLabel(content, "Random photos from Flickr via loremflickr.com (requires WiFi)");

    lv_obj_t* sw_picsum = addSwitch(content, clock_picsum_enabled);
    lv_obj_add_event_cb(sw_picsum, [](lv_event_t* e) {
        lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
        clock_picsum_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        wifiPrefs.putBool(NVS_KEY_CLOCK_PICSUM, clock_picsum_enabled);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Photo theme keyword ───────────────────────────────────────────────────
    addSectionLabel(content, "Photo theme:");
    addDescLabel(content, "Category of photos to show");

    // Build newline-separated options string from the keyword list
    static char kw_opts[256];
    kw_opts[0] = '\0';
    for (int i = 0; i < CLOCK_BG_KW_COUNT; i++) {
        strncat(kw_opts, CLOCK_BG_KEYWORDS[i].label, sizeof(kw_opts) - strlen(kw_opts) - 2);
        if (i < CLOCK_BG_KW_COUNT - 1)
            strncat(kw_opts, "\n", sizeof(kw_opts) - strlen(kw_opts) - 1);
    }

    lv_obj_t* dd_kw = lv_dropdown_create(content);
    lv_dropdown_set_options(dd_kw, kw_opts);
    lv_dropdown_set_selected(dd_kw, (uint16_t)clock_bg_kw_idx);
    lv_obj_set_width(dd_kw, lv_pct(100));
    lv_obj_set_style_bg_color(dd_kw, COL_CARD, 0);
    lv_obj_set_style_text_color(dd_kw, COL_TEXT, 0);
    lv_obj_set_style_text_font(dd_kw, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(dd_kw, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_top(dd_kw, 6, 0);
    lv_obj_set_style_pad_bottom(dd_kw, 12, 0);
    {
        lv_obj_t* list = lv_dropdown_get_list(dd_kw);
        if (list) {
            lv_obj_set_style_bg_color(list, lv_color_hex(0x222222), 0);
            lv_obj_set_style_text_color(list, COL_TEXT, 0);
            lv_obj_set_style_text_font(list, &lv_font_montserrat_14, 0);
        }
    }
    lv_obj_add_event_cb(dd_kw, [](lv_event_t* e) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        clock_bg_kw_idx = (int)lv_dropdown_get_selected(dd);
        wifiPrefs.putInt(NVS_KEY_CLOCK_KW, clock_bg_kw_idx);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Photo refresh interval ───────────────────────────────────────────────
    addSectionLabel(content, "Photo refresh interval:");

    static lv_obj_t* lbl_refresh_val;
    lbl_refresh_val = lv_label_create(content);
    lv_label_set_text_fmt(lbl_refresh_val, "%d min", clock_refresh_min);
    lv_obj_set_style_text_color(lbl_refresh_val, COL_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_refresh_val, &lv_font_montserrat_14, 0);

    lv_obj_t* slider_refresh = lv_slider_create(content);
    lv_obj_set_width(slider_refresh, lv_pct(100));
    lv_obj_set_height(slider_refresh, 20);
    lv_slider_set_range(slider_refresh, 1, 60);
    lv_slider_set_value(slider_refresh, clock_refresh_min, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_refresh, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_refresh, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_refresh, COL_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_radius(slider_refresh, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(slider_refresh, 10, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(slider_refresh, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_top(slider_refresh, 4, 0);
    lv_obj_set_style_pad_bottom(slider_refresh, 12, 0);
    lv_obj_add_event_cb(slider_refresh, [](lv_event_t* e) {
        lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
        clock_refresh_min = lv_slider_get_value(slider);
        lv_label_set_text_fmt((lv_obj_t*)lv_event_get_user_data(e),
                              "%d min", clock_refresh_min);
        wifiPrefs.putInt(NVS_KEY_CLOCK_REFRESH, clock_refresh_min);
    }, LV_EVENT_VALUE_CHANGED, lbl_refresh_val);

    // ── Timezone ─────────────────────────────────────────────────────────────
    addSectionLabel(content, "Timezone:");
    addDescLabel(content, "Select your local timezone");

    // Build newline-separated options string for the dropdown
    // Each name is at most ~45 chars; 82 zones * 46 = ~3772 bytes total
    static char tz_opts[4096];
    tz_opts[0] = '\0';
    for (int i = 0; i < CLOCK_ZONES_COUNT; i++) {
        strncat(tz_opts, CLOCK_ZONES[i].name, sizeof(tz_opts) - strlen(tz_opts) - 2);
        if (i < CLOCK_ZONES_COUNT - 1) {
            strncat(tz_opts, "\n", sizeof(tz_opts) - strlen(tz_opts) - 1);
        }
    }

    lv_obj_t* dd_tz = lv_dropdown_create(content);
    lv_dropdown_set_options(dd_tz, tz_opts);
    lv_dropdown_set_selected(dd_tz, (uint16_t)clock_tz_idx);
    lv_obj_set_width(dd_tz, lv_pct(100));
    lv_obj_set_style_bg_color(dd_tz, COL_CARD, 0);
    lv_obj_set_style_text_color(dd_tz, COL_TEXT, 0);
    lv_obj_set_style_text_font(dd_tz, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(dd_tz, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_top(dd_tz, 6, 0);
    lv_obj_set_style_pad_bottom(dd_tz, 12, 0);
    // Make the dropdown list taller so more zones are visible without scrolling
    lv_dropdown_set_dir(dd_tz, LV_DIR_TOP);  // Open upward (settings at bottom)
    lv_obj_t* dd_list = lv_dropdown_get_list(dd_tz);
    if (dd_list) {
        lv_obj_set_height(dd_list, 260);
        lv_obj_set_style_bg_color(dd_list, lv_color_hex(0x222222), 0);
        lv_obj_set_style_text_color(dd_list, COL_TEXT, 0);
        lv_obj_set_style_text_font(dd_list, &lv_font_montserrat_14, 0);
    }
    lv_obj_add_event_cb(dd_tz, [](lv_event_t* e) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        clock_tz_idx = (int)lv_dropdown_get_selected(dd);
        wifiPrefs.putInt(NVS_KEY_CLOCK_TZ, clock_tz_idx);
        // Apply timezone immediately
        setenv("TZ", CLOCK_ZONES[clock_tz_idx].posix, 1);
        tzset();
        Serial.printf("[CLOCK] TZ set to %s (%s)\n",
                      CLOCK_ZONES[clock_tz_idx].name,
                      CLOCK_ZONES[clock_tz_idx].posix);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Weather widget ───────────────────────────────────────────────────────
    addSectionLabel(content, "Weather widget:");
    addDescLabel(content, "Show temperature, humidity, wind, and 6-hour forecast (Open-Meteo, no API key needed)");

    lv_obj_t* sw_weather = addSwitch(content, clock_weather_enabled);
    lv_obj_add_event_cb(sw_weather, [](lv_event_t* e) {
        lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
        clock_weather_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        wifiPrefs.putBool(NVS_KEY_CLOCK_WEATHER_EN, clock_weather_enabled);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Weather city ─────────────────────────────────────────────────────────
    addSectionLabel(content, "Weather location:");
    addDescLabel(content, "Auto-detect uses your public IP to find your location");

    // Build newline-separated city list for the dropdown
    // ~65 cities × ~25 chars avg (incl. "Region/City" prefix) = ~1625 bytes; 2048 is generous
    static char city_opts[2048];
    city_opts[0] = '\0';
    for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
        strncat(city_opts, CLOCK_CITIES[i].label, sizeof(city_opts) - strlen(city_opts) - 2);
        if (i < CLOCK_CITY_COUNT - 1)
            strncat(city_opts, "\n", sizeof(city_opts) - strlen(city_opts) - 1);
    }

    lv_obj_t* dd_city = lv_dropdown_create(content);
    lv_dropdown_set_options(dd_city, city_opts);
    lv_dropdown_set_selected(dd_city, (uint16_t)clock_weather_city_idx);
    lv_obj_set_width(dd_city, lv_pct(100));
    lv_obj_set_style_bg_color(dd_city, COL_CARD, 0);
    lv_obj_set_style_text_color(dd_city, COL_TEXT, 0);
    lv_obj_set_style_text_font(dd_city, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(dd_city, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_top(dd_city, 6, 0);
    lv_obj_set_style_pad_bottom(dd_city, 12, 0);
    lv_dropdown_set_dir(dd_city, LV_DIR_TOP);
    {
        lv_obj_t* list = lv_dropdown_get_list(dd_city);
        if (list) {
            lv_obj_set_height(list, 260);
            lv_obj_set_style_bg_color(list, lv_color_hex(0x222222), 0);
            lv_obj_set_style_text_color(list, COL_TEXT, 0);
            lv_obj_set_style_text_font(list, &lv_font_montserrat_14, 0);
        }
    }
    lv_obj_add_event_cb(dd_city, [](lv_event_t* e) {
        lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
        clock_weather_city_idx = (int)lv_dropdown_get_selected(dd);
        wifiPrefs.putInt(NVS_KEY_CLOCK_WEATHER_CITY, clock_weather_city_idx);
        clock_wx_valid = false;  // Trigger fresh fetch for new city
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Temperature unit ─────────────────────────────────────────────────────
    addSectionLabel(content, "Temperature unit:");
    addDescLabel(content, "Fahrenheit (°F)    default is Celsius");

    lv_obj_t* sw_fahr = addSwitch(content, clock_wx_fahrenheit);
    lv_obj_add_event_cb(sw_fahr, [](lv_event_t* e) {
        lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
        clock_wx_fahrenheit = lv_obj_has_state(sw, LV_STATE_CHECKED);
        wifiPrefs.putBool(NVS_KEY_CLOCK_WEATHER_FAHR, clock_wx_fahrenheit);
        clock_weather_needs_refetch = true;  // Re-fetch with correct temperature_unit
    }, LV_EVENT_VALUE_CHANGED, NULL);
}
