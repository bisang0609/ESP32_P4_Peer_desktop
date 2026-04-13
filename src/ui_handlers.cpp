/**
 * UI Event Handlers and Utilities
 * All event callbacks, WiFi, brightness control, and UI update functions
 */

#include "ui_common.h"
#include "ui_icons.h"
#include <vector>
#include "config.h"
#include "lyrics.h"
#include "clock_screen.h"
#include <esp_task_wdt.h>

// ============================================================================
// Brightness Control
// ============================================================================
void setBrightness(int level) {
    brightness_level = constrain(level, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    display_set_brightness(brightness_level);
    wifiPrefs.putInt(NVS_KEY_BRIGHTNESS, brightness_level);
}

void resetScreenTimeout() {
    last_touch_time = millis();
    if (screen_dimmed) {
        // Instant wake-up - no animation
        display_set_brightness(brightness_level);
        screen_dimmed = false;
    }
}

// Brightness animation callback for smooth dimming
static void brightness_anim_cb(void* var, int32_t v) {
    display_set_brightness(v);
}

void checkAutoDim() {
    if (autodim_timeout == 0) return;  // Auto-dim disabled
    if (screen_dimmed) return;  // Already dimmed

    if ((millis() - last_touch_time) > (autodim_timeout * 1000)) {
        int dimmed = constrain(brightness_dimmed, 5, 100);

        // Smooth fade to dimmed brightness (1 second fade)
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, NULL);
        lv_anim_set_values(&anim, brightness_level, dimmed);
        lv_anim_set_duration(&anim, 1000);  // 1 second smooth fade
        lv_anim_set_exec_cb(&anim, brightness_anim_cb);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
        lv_anim_start(&anim);

        screen_dimmed = true;
    }
}

// ============================================================================
// Playback Event Handlers
// ============================================================================
void ev_play(lv_event_t* e) {
    SonosDevice* d = sonos.getCurrentDevice();
    if (d) d->isPlaying ? sonos.pause() : sonos.play();
}

void ev_prev(lv_event_t* e) {
    sonos.previous();
}

void ev_next(lv_event_t* e) {
    sonos.next();
}

void ev_shuffle(lv_event_t* e) {
    // btn_shuffle = dislike button; optimistic toggle
    SonosDevice* d = sonos.getCurrentDevice();
    if (d) {
        bool was_disliked = (d->likeStatus == "DISLIKE");
        d->likeStatus = was_disliked ? "INDIFFERENT" : "DISLIKE";
        lv_obj_t* ico = lv_obj_get_child((lv_obj_t*)lv_event_get_target(e), 0);
        if (ico) lv_obj_set_style_text_color(ico, was_disliked ? COL_TEXT2 : COL_HEART, 0);
    }
    sonos.dislike();
}

void ev_repeat(lv_event_t* e) {
    // btn_repeat = like button; optimistic toggle
    SonosDevice* d = sonos.getCurrentDevice();
    if (d) {
        bool was_liked = (d->likeStatus == "LIKE");
        d->likeStatus = was_liked ? "INDIFFERENT" : "LIKE";
        lv_obj_t* ico = lv_obj_get_child((lv_obj_t*)lv_event_get_target(e), 0);
        if (ico) lv_obj_set_style_text_color(ico, was_liked ? COL_TEXT2 : COL_ACCENT, 0);
    }
    sonos.like();
}

void ev_progress(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING) dragging_prog = true;
    else if (code == LV_EVENT_RELEASED) {
        SonosDevice* d = sonos.getCurrentDevice();
        if (d && d->durationSeconds > 0) sonos.seek((lv_slider_get_value(slider_progress) * d->durationSeconds) / 100);
        dragging_prog = false;
    }
}

void ev_vol_slider(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING) dragging_vol = true;
    else if (code == LV_EVENT_RELEASED) {
        sonos.setVolume(lv_slider_get_value(slider_vol));
        dragging_vol = false;
    }
}

void ev_mute(lv_event_t* e) {
    SonosDevice* d = sonos.getCurrentDevice();
    if (d) sonos.setMute(!d->isMuted);
}

void ev_queue_item(lv_event_t* e) {
    static uint32_t last_click_ms = 0;
    uint32_t now = millis();
    if (now - last_click_ms < 1500) return;  // debounce: ignore rapid repeat taps
    last_click_ms = now;
    int trackNum = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
    sonos.playQueueItem(trackNum);
    lv_screen_load(scr_main);
}

// ============================================================================
// Navigation Event Handlers
// ============================================================================
void ev_devices(lv_event_t* e) {
    lv_screen_load(scr_devices);
}

void ev_queue(lv_event_t* e) {
    // Show cached data immediately, then request a fresh windowed fetch.
    // The polling task picks up queue_fetch_requested and calls updateQueue(startIndex)
    // on its next cycle (safe: no SOAP on UI thread).
    SonosDevice* d = sonos.getCurrentDevice();
    int start = 0;
    if (d && d->currentTrackNumber > 0) {
        start = d->currentTrackNumber - SONOS_QUEUE_BATCH_SIZE / 2;
        if (start < 0) start = 0;
        if (d->totalTracks > 0 && start + SONOS_QUEUE_BATCH_SIZE > d->totalTracks)
            start = d->totalTracks - SONOS_QUEUE_BATCH_SIZE;
        if (start < 0) start = 0;
    }
    queue_fetch_start_index = start;
    queue_fetch_requested   = true;
    refreshQueueList();
    lv_screen_load(scr_queue);
}

void ev_settings(lv_event_t* e) {
    lv_screen_load(scr_settings);
}

void ev_back_main(lv_event_t* e) {
    lv_screen_load(scr_main);
}

void ev_back_settings(lv_event_t* e) {
    lv_screen_load(scr_settings);
}

void ev_groups(lv_event_t* e) {
    sonos.updateGroupInfo();
    refreshGroupsList();
    lv_screen_load(scr_groups);
}

// ============================================================================
// Speaker Discovery Event Handler
// ============================================================================
void ev_discover(lv_event_t* e) {
    Serial.println("[SCAN] Scan button pressed");

    // Disable scan button during discovery
    if (btn_sonos_scan) {
        lv_obj_add_state(btn_sonos_scan, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn_sonos_scan, lv_color_hex(0x555555), LV_STATE_DISABLED);
    }

    // Show spinner
    if (spinner_scan) {
        Serial.println("[SCAN] Showing spinner");
        lv_obj_remove_flag(spinner_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(spinner_scan);  // Bring to front
    } else {
        Serial.println("[SCAN] ERROR: spinner_scan is NULL!");
    }

    lv_label_set_text(lbl_status, "Scanning for speakers...");
    lv_obj_set_style_text_color(lbl_status, COL_ACCENT, 0);
    lv_obj_clean(list_devices);
    lv_refr_now(NULL);  // Force immediate screen refresh

    int cnt = sonos.discoverDevices();

    // Hide spinner
    if (spinner_scan) {
        lv_obj_add_flag(spinner_scan, LV_OBJ_FLAG_HIDDEN);
    }

    // Re-enable scan button
    if (btn_sonos_scan) {
        lv_obj_clear_state(btn_sonos_scan, LV_STATE_DISABLED);
    }

    if (cnt == 0) {
        lv_label_set_text(lbl_status, MDI_ALERT " No Sonos devices found on network");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    if (cnt < 0) {
        lv_label_set_text(lbl_status, MDI_ALERT " Discovery failed - check network");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    lv_label_set_text_fmt(lbl_status, MDI_CHECK " Found %d Sonos device%s", cnt, cnt == 1 ? "" : "s");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x4ECB71), 0);
    refreshDeviceList();
}

// ============================================================================
// WiFi Event Handlers
// ============================================================================
void ev_wifi_scan(lv_event_t* e) {
    // Disable button and show loading state
    if (btn_wifi_scan) {
        lv_obj_add_state(btn_wifi_scan, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn_wifi_scan, lv_color_hex(0x555555), LV_STATE_DISABLED);
    }
    if (lbl_scan_text) {
        lv_label_set_text(lbl_scan_text, MDI_REFRESH "  Scanning...");
    }

    lv_label_set_text(lbl_wifi_status, "Scanning for networks...");
    lv_obj_set_style_text_color(lbl_wifi_status, COL_ACCENT, 0);
    lv_obj_clean(list_wifi);
    // Hide password strip if visible from a previous selection
    lv_obj_add_flag(pw_strip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    // Show spinner
    if (spinner_wifi_scan) {
        lv_obj_remove_flag(spinner_wifi_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(spinner_wifi_scan);
    }
    lv_timer_handler();  // Update UI immediately

    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    int n = WiFi.scanNetworks();
    wifiNetworkCount = min(n, 20);

    // Hide spinner, re-enable button
    if (spinner_wifi_scan) lv_obj_add_flag(spinner_wifi_scan, LV_OBJ_FLAG_HIDDEN);
    if (btn_wifi_scan)     lv_obj_clear_state(btn_wifi_scan, LV_STATE_DISABLED);
    if (lbl_scan_text)     lv_label_set_text(lbl_scan_text, MDI_REFRESH " Scan");

    if (n == 0) {
        lv_label_set_text(lbl_wifi_status, MDI_ALERT " No networks found");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    if (n < 0) {
        lv_label_set_text(lbl_wifi_status, MDI_ALERT " Scan failed - try again");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    lv_label_set_text_fmt(lbl_wifi_status, MDI_CHECK " Found %d network%s", n, n == 1 ? "" : "s");
    lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x4ECB71), 0);

    // Deduplicate: for mesh networks (same SSID, multiple APs) keep best RSSI only
    std::vector<int> unique_indices;
    for (int i = 0; i < wifiNetworkCount; i++) {
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        bool found = false;
        for (int& j : unique_indices) {
            if (WiFi.SSID(j) == ssid) {
                if (rssi > WiFi.RSSI(j)) j = i;  // keep stronger signal
                found = true;
                break;
            }
        }
        if (!found) unique_indices.push_back(i);
    }
    wifiNetworkCount = min((int)unique_indices.size(), 20);

    for (int ui = 0; ui < wifiNetworkCount; ui++) {
        int i = unique_indices[ui];
        wifiNetworks[ui] = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);

        // Icon color only: green=strong, accent=medium, red=weak
        lv_color_t icon_color;
        if      (rssi > -60) icon_color = lv_color_hex(0x4ECB71);
        else if (rssi > -75) icon_color = COL_ACCENT;
        else                 icon_color = lv_color_hex(0xFF6B6B);

        lv_obj_t* btn = lv_btn_create(list_wifi);
        lv_obj_set_size(btn, lv_pct(100), 50);
        lv_obj_set_user_data(btn, (void*)(intptr_t)ui);
        lv_obj_set_style_bg_color(btn, COL_CARD, 0);
        lv_obj_set_style_bg_color(btn, COL_BTN, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int idx = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
            selectedSSID = wifiNetworks[idx];
            // Show password strip + update SSID label
            lv_label_set_text(lbl_pw_ssid, selectedSSID.c_str());
            lv_obj_clear_flag(pw_strip, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(lbl_wifi_status, MDI_WIFI " %s", selectedSSID.c_str());
            lv_obj_set_style_text_color(lbl_wifi_status, COL_TEXT, 0);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, MDI_WIFI);
        lv_obj_set_style_text_font(icon, &lv_font_mdi_16, 0);
        lv_obj_set_style_text_color(icon, icon_color, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_t* ssid_lbl = lv_label_create(btn);
        lv_label_set_text(ssid_lbl, wifiNetworks[ui].c_str());
        lv_obj_set_style_text_color(ssid_lbl, COL_TEXT, 0);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_width(ssid_lbl, lv_pct(80));
        lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 36, 0);
    }
    WiFi.scanDelete();
}

void ev_wifi_connect(lv_event_t* e) {
    if (selectedSSID.length() == 0) {
        lv_label_set_text(lbl_wifi_status, MDI_ALERT " Please select a network first");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    const char* pwd = lv_textarea_get_text(ta_password);

    // Disable connect button during connection
    if (btn_wifi_connect) {
        lv_obj_add_state(btn_wifi_connect, LV_STATE_DISABLED);
    }

    lv_label_set_text_fmt(lbl_wifi_status, MDI_REFRESH " Connecting to %s...", selectedSSID.c_str());
    lv_obj_set_style_text_color(lbl_wifi_status, COL_ACCENT, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler();  // Update UI

    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    WiFi.begin(selectedSSID.c_str(), pwd);

    // Non-blocking connection with visual feedback (max 30 seconds ??mesh/Orbi can be slow).
    // MUST reset the hardware WDT each iteration: this function runs on mainAppTask which is
    // registered with the 30s WDT. The loop itself takes up to 30s ??WDT fires at loop end.
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 60) {
        esp_task_wdt_reset();  // Feed WDT ??loop runs up to 30s, WDT timeout = 30s
        vTaskDelay(pdMS_TO_TICKS(500));
        lv_timer_handler();  // Keep UI responsive
        lv_label_set_text_fmt(lbl_wifi_status, MDI_REFRESH " Connecting to %s%s",
            selectedSSID.c_str(),
            tries % 4 == 0 ? "..." : tries % 4 == 1 ? ".  " : tries % 4 == 2 ? ".. " : " ..");
    }
    esp_task_wdt_reset();  // Reset after loop exits (NVS write below can take ~100ms)

    // Re-enable button
    if (btn_wifi_connect) {
        lv_obj_clear_state(btn_wifi_connect, LV_STATE_DISABLED);
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Save credentials to NVS
        Serial.printf("[WIFI] Saving credentials to NVS: SSID='%s'\n", selectedSSID.c_str());
        wifiPrefs.putString("ssid", selectedSSID);
        wifiPrefs.putString("pass", pwd);

        // Verify write succeeded
        String verifySSID = wifiPrefs.getString("ssid", "");
        String verifyPass = wifiPrefs.getString("pass", "");

        if (verifySSID == selectedSSID && verifyPass == pwd) {
            Serial.println("[WIFI] Credentials successfully saved and verified in NVS");
        } else {
            Serial.println("[WIFI] WARNING: NVS verification failed! Credentials may not persist.");
        }

        String ip = WiFi.localIP().toString();
        lv_label_set_text_fmt(lbl_wifi_status,
            MDI_WIFI " Connected to %s  (%s)",
            selectedSSID.c_str(), ip.c_str());
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x4ECB71), 0);

        // Hide strip + keyboard, clear password field
        lv_obj_add_flag(pw_strip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(ta_password, "");
    } else {
        // Determine failure reason
        wl_status_t status = WiFi.status();
        const char* reason = "Unknown error";

        if (status == WL_CONNECT_FAILED) {
            reason = "Authentication failed - check password";
        } else if (status == WL_NO_SSID_AVAIL) {
            reason = "Network not found";
        } else if (status == WL_CONNECTION_LOST) {
            reason = "Connection lost";
        } else if (status == WL_DISCONNECTED) {
            reason = "Connection timeout ??check password and try again";
        }

        lv_label_set_text_fmt(lbl_wifi_status, MDI_ALERT " Failed: %s", reason);
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF6B6B), 0);
    }
}

// updateUI() Sub-Functions (static ??implementation details, not in any header)
// ============================================================================

// Handles disconnect/reconnect UI state. Returns true if device is connected
// and updateUI() should continue. Returns false ??caller must return immediately.
static bool updateConnectionState(SonosDevice* d) {
    static bool was_connected  = false;
    static bool ui_cleared     = false;
    static bool last_conn_state = false;

    if (d->connected != last_conn_state) {
        Serial.printf("[UI] Connection state changed: %s (errorCount=%d)\n",
                     d->connected ? "CONNECTED" : "DISCONNECTED", d->errorCount);
        last_conn_state = d->connected;
    }

    if (!d->connected) {
        if (was_connected || !ui_cleared) {
            lv_label_set_text(lbl_title, "Device Not Connected");
            lv_label_set_text(lbl_artist, "");
            lv_label_set_text(lbl_album, "");
            lv_label_set_text(lbl_time, "0:00");
            lv_label_set_text(lbl_time_remaining, "0:00");
            lv_slider_set_value(slider_progress, 0, LV_ANIM_OFF);

            lv_obj_add_flag(img_album, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(art_placeholder, LV_OBJ_FLAG_HIDDEN);

            lv_obj_add_flag(img_next_album, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_next_title, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_next_artist, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_next_header, LV_OBJ_FLAG_HIDDEN);

            if (panel_art)   lv_obj_set_style_bg_color(panel_art,   lv_color_hex(0x1a1a1a), 0);
            if (panel_right) lv_obj_set_style_bg_color(panel_right, COL_BG, 0);

            lv_obj_t* lbl = lv_obj_get_child(btn_play, 0);
            lv_label_set_text(lbl, MDI_PAUSE);
            lv_obj_set_style_text_font(lbl, &lv_font_mdi_40, 0);
            lv_obj_center(lbl);

            ui_title = "";
            ui_artist = "";
            was_connected = false;
            ui_cleared = true;
            Serial.println("[UI] Device disconnected - UI cleared");
        }
        return false;  // not connected
    }

    if (d->connected && !was_connected) {
        was_connected = true;
        ui_cleared = false;
        ui_title = "";
        ui_artist = "";
        Serial.println("[UI] Device reconnected - forcing UI refresh");
    }
    return true;  // connected
}

// Displays art or placeholder from the background art task.
// Must be called on the main LVGL thread. Takes art_mutex internally.
static void displayCompletedArt() {
    if (!xSemaphoreTake(art_mutex, 0)) return;

    if (art_ready) {
        // Build art_dsc here on the main thread ??same thread as lv_timer_handler() /
        // LVGL renderer ??so there is never concurrent read+write of the descriptor.
        // The background art task only writes art_buffer (pixels); we set the header here.
        memset(&art_dsc, 0, sizeof(art_dsc));
        art_dsc.header.w    = ART_SIZE;
        art_dsc.header.h    = ART_SIZE;
        art_dsc.header.cf   = LV_COLOR_FORMAT_RGB565;
        art_dsc.data_size   = ART_SIZE * ART_SIZE * 2;
        art_dsc.data        = (const uint8_t*)art_buffer;
        lv_img_set_src(img_album, &art_dsc);
        lv_obj_set_size(img_album, ART_SIZE, ART_SIZE);
        lv_obj_center(img_album);
        lv_obj_remove_flag(img_album, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(art_placeholder, LV_OBJ_FLAG_HIDDEN);
        art_ready = false;
        art_show_placeholder = false;
    } else if (art_show_placeholder) {
        lv_obj_add_flag(img_album, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(art_placeholder, LV_OBJ_FLAG_HIDDEN);
        art_show_placeholder = false;
    }
    if (blur_bg_ready && img_blur_bg && blur_bg_buf) {
        memset(&blur_bg_dsc, 0, sizeof(blur_bg_dsc));
        blur_bg_dsc.header.w  = 800;
        blur_bg_dsc.header.h  = 480;
        blur_bg_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        blur_bg_dsc.data_size = 800 * 480 * 2;
        blur_bg_dsc.data      = (const uint8_t*)blur_bg_buf;
        lv_img_set_src(img_blur_bg, &blur_bg_dsc);
        lv_obj_remove_flag(img_blur_bg, LV_OBJ_FLAG_HIDDEN);
        blur_bg_ready = false;
    }
    if (color_ready) {
        // Restore progress bar + button pressed accent colors from dominant art color.
        // Panel bg changes in setBackgroundColor are invisible (panels are transparent)
        // but slider_progress indicator/knob + button pressed highlights still update.
        setBackgroundColor(dominant_color);
        color_ready = false;
    }
    xSemaphoreGive(art_mutex);
}

// Updates the "Next Up" track labels from the cached queue.
static void updateNextTrackUI(SonosDevice* d) {
    static String last_next_title = "";

    // Use dedicated next-track fields fetched from /api/v1/queue/next
    if (!d->isRadioStation && !d->isLineIn && !d->isTvAudio
            && d->nextTrackAvailable) {
        if (d->nextTrackTitle.length() > 0) {
            if (d->nextTrackTitle != last_next_title) {
                lv_label_set_text(lbl_next_title,  d->nextTrackTitle.c_str());
                lv_label_set_text(lbl_next_artist, d->nextTrackArtist.c_str());
                lv_obj_clear_flag(lbl_next_header, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(lbl_next_title,  LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(lbl_next_artist, LV_OBJ_FLAG_HIDDEN);
                last_next_title = d->nextTrackTitle;
            }
        } else {
            // API confirmed no next track (204)
            if (last_next_title != "") {
                lv_obj_add_flag(lbl_next_header, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(lbl_next_title,  LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(lbl_next_artist, LV_OBJ_FLAG_HIDDEN);
                last_next_title = "";
            }
        }
        return;
    }

    // Fallback: search queue array (used before first nextTrack poll)
    if (!d->isRadioStation && !d->isLineIn && !d->isTvAudio && d->queueSize > 0 && d->currentTrackNumber > 0) {
        int nextIdx = -1;

        for (int i = 0; i < d->queueSize; i++) {
            if (d->queue[i].trackNumber == d->currentTrackNumber + 1) {
                nextIdx = i;
                break;
            }
        }

        if (nextIdx < 0 && (d->repeatMode == "ALL" || d->repeatMode == "ONE")) {
            for (int i = 0; i < d->queueSize; i++) {
                if (d->queue[i].trackNumber == 1) {
                    nextIdx = i;
                    break;
                }
            }
        }

        if (nextIdx >= 0 && d->queue[nextIdx].title.length() > 0) {
            String nextTitle = d->queue[nextIdx].title;
            if (nextTitle != last_next_title) {
                lv_label_set_text(lbl_next_title, d->queue[nextIdx].title.c_str());
                lv_label_set_text(lbl_next_artist, d->queue[nextIdx].artist.c_str());
                lv_obj_clear_flag(lbl_next_header, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(lbl_next_title, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(lbl_next_artist, LV_OBJ_FLAG_HIDDEN);
                last_next_title = nextTitle;
            }
        } else if (nextIdx < 0) {
            // Only hide if next track is truly unavailable (not just temporarily)
            if (last_next_title != "") {
                lv_obj_add_flag(lbl_next_header, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(lbl_next_title, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(lbl_next_artist, LV_OBJ_FLAG_HIDDEN);
                last_next_title = "";
            }
        }
    } else {
        if (last_next_title != "") {
            lv_obj_add_flag(lbl_next_header, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_next_title, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_next_artist, LV_OBJ_FLAG_HIDDEN);
            last_next_title = "";
        }
    }
}

// Requests album art when the track changes.
// YTMD version: simplified — no radio fallback, no Sonos URI pattern matching.
// Track identity is determined by d->currentURI (title+artist composite set by ytmd_controller).
static void updateAlbumArtRequest(SonosDevice* d) {
    static String last_track_uri = "";
    static bool   had_track      = false;

    bool uri_changed = (d->currentURI != last_track_uri);
    if (uri_changed) {
        last_track_uri = d->currentURI;

        if (d->currentURI.length() > 0) {
            Serial.printf("[ART] Track changed: %s\n", d->currentURI.c_str());
            art_abort_download = true;
            if (xSemaphoreTake(art_mutex, pdMS_TO_TICKS(50))) {
                last_art_url    = "";
                pending_art_url = "";
                art_ready       = false;
                xSemaphoreGive(art_mutex);
            }
            clearAlbumArtCache();
            if (img_album)       lv_obj_add_flag(img_album, LV_OBJ_FLAG_HIDDEN);
            if (art_placeholder) lv_obj_remove_flag(art_placeholder, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Clear art when nothing is playing
    bool has_track = (d->currentTrack.length() > 0);
    if (had_track && !has_track) {
        art_abort_download = true;
        clearAlbumArtCache();
        if (img_album)       lv_obj_add_flag(img_album, LV_OBJ_FLAG_HIDDEN);
        if (art_placeholder) lv_obj_remove_flag(art_placeholder, LV_OBJ_FLAG_HIDDEN);
        if (xSemaphoreTake(art_mutex, pdMS_TO_TICKS(50))) {
            last_art_url = ""; pending_art_url = ""; art_ready = false;
            xSemaphoreGive(art_mutex);
        }
    }
    had_track = has_track;

    // Request art when URL is available and changed
    static String last_requested_art_url = "";
    bool hasArt    = (d->albumArtURL.length() > 0);
    bool artChanged = uri_changed || (hasArt && d->albumArtURL != last_requested_art_url);

    if (!hasArt && uri_changed) {
        if (img_album)       lv_obj_add_flag(img_album, LV_OBJ_FLAG_HIDDEN);
        if (art_placeholder) lv_obj_remove_flag(art_placeholder, LV_OBJ_FLAG_HIDDEN);
        if (xSemaphoreTake(art_mutex, pdMS_TO_TICKS(50))) {
            last_art_url = ""; pending_art_url = ""; art_ready = false;
            xSemaphoreGive(art_mutex);
        }
    } else if (hasArt && artChanged) {
        pending_is_station_logo = false;
        requestAlbumArt(d->albumArtURL);
        last_requested_art_url = d->albumArtURL;
    }
}

// ============================================================================
// UI Update Function
// ============================================================================
void updateUI() {
    SonosDevice* d = sonos.getCurrentDevice();
    if (!d) return;

    if (!updateConnectionState(d)) return;

    // Device is connected - update UI normally

    // Title
    if (d->currentTrack != ui_title) {
        lv_label_set_text(lbl_title, d->currentTrack.length() > 0 ? d->currentTrack.c_str() : "Not Playing");
        ui_title = d->currentTrack;
    }

    // Artist
    if (d->currentArtist != ui_artist) {
        lv_label_set_text(lbl_artist, d->currentArtist.c_str());
        ui_artist = d->currentArtist;
    }

    // Album name (below album art)
    static String ui_album_name = "";
    if (d->currentAlbum != ui_album_name) {
        lv_label_set_text(lbl_album, d->currentAlbum.c_str());
        ui_album_name = d->currentAlbum;
    }

    // Device name in header
    static String ui_device_name = "";
    if (d->roomName != ui_device_name) {
        String np = "Now Playing - " + d->roomName;
        lv_label_set_text(lbl_device_name, np.c_str());
        ui_device_name = d->roomName;
    }

    // Time display
    String t = d->relTime;
    if (t.startsWith("0:")) t = t.substring(2);
    lv_label_set_text(lbl_time, t.c_str());

    // Remaining time as negative countdown: -M:SS (Apple Music / Spotify style)
    if (d->durationSeconds > 0) {
        int rem = d->durationSeconds - d->relTimeSeconds;
        if (rem < 0) rem = 0;
        int rm = rem / 60;
        int rs = rem % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "-%d:%02d", rm, rs);
        lv_label_set_text(lbl_time_remaining, buf);
    }

    // Progress slider
    if (!dragging_prog && d->durationSeconds > 0)
        lv_slider_set_value(slider_progress, (d->relTimeSeconds * 100) / d->durationSeconds, LV_ANIM_OFF);

    // Play/Pause button
    if (d->isPlaying != ui_playing) {
        lv_obj_t* lbl = lv_obj_get_child(btn_play, 0);
        lv_label_set_text(lbl, d->isPlaying ? MDI_PAUSE : MDI_PLAY);
        lv_obj_set_style_text_font(lbl, &lv_font_mdi_40, 0);
        lv_obj_center(lbl);  // MDI icons are optically centered ??no offset needed

        ui_playing = d->isPlaying;
    }

    // Volume slider update
    if (!dragging_vol && d->volume != ui_vol && slider_vol) {
        lv_slider_set_value(slider_vol, d->volume, LV_ANIM_OFF);
        ui_vol = d->volume;
    }

    // Mute button
    if (d->isMuted != ui_muted && btn_mute) {
        lv_obj_t* lbl = lv_obj_get_child(btn_mute, 0);
        lv_label_set_text(lbl, d->isMuted ? MDI_VOLUME_OFF : MDI_VOLUME_HIGH);
        ui_muted = d->isMuted;
    }

    // Like/Dislike icon colors — sync to YTMD server state on each poll
    {
        static String ui_like_status = "";
        if (d->likeStatus != ui_like_status) {
            ui_like_status = d->likeStatus;
            if (btn_repeat) {
                lv_obj_t* ico = lv_obj_get_child(btn_repeat, 0);
                if (ico) lv_obj_set_style_text_color(ico,
                    (d->likeStatus == "LIKE") ? COL_ACCENT : COL_TEXT2, 0);
            }
            if (btn_shuffle) {
                lv_obj_t* ico = lv_obj_get_child(btn_shuffle, 0);
                if (ico) lv_obj_set_style_text_color(ico,
                    (d->likeStatus == "DISLIKE") ? COL_HEART : COL_TEXT2, 0);
            }
        }
    }

    // Next track info
    updateNextTrackUI(d);

    // Album art - only request if URL changed to prevent download loops
    // NOTE: last_art_url is GLOBAL (extern in ui_common.h), don't shadow it!
    updateAlbumArtRequest(d);
    displayCompletedArt();

}

void processUpdates() {
    static uint32_t lastUpdate = 0;
    UIUpdate_t upd;
    bool need = false;
    bool queue_updated = false;
    while (xQueueReceive(sonos.getUIUpdateQueue(), &upd, 0)) {
        need = true;
        if (upd.type == UPDATE_QUEUE) queue_updated = true;
    }
    if (need && (millis() - lastUpdate > 200)) { updateUI(); lastUpdate = millis(); }
    else displayCompletedArt();  // Run even without Sonos events (e.g. art ready while polling suppressed)
    // Auto-refresh queue list if the queue screen is visible when new data arrives
    if (queue_updated && lv_screen_active() == scr_queue) refreshQueueList();
}
