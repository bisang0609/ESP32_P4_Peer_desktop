/**
 * UI Main Screen
 * Main player screen with album art, playback controls, and volume
 */

#include "ui_common.h"
#include "lyrics.h"
#include "ui_icons.h"
#include "ui_metadata_font.h"

// ==================== MAIN SCREEN - CLEAN SIMPLE DESIGN ====================
void createMainScreen() {
    scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(0x111111), 0);  // dark fallback before first art loads
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);

    // Blurred art background — fullscreen, must be first child (lowest z-order)
    img_blur_bg = lv_img_create(scr_main);
    lv_obj_set_size(img_blur_bg, 800, 480);
    lv_obj_set_pos(img_blur_bg, 0, 0);
    lv_obj_clear_flag(img_blur_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(img_blur_bg, LV_OBJ_FLAG_HIDDEN);  // hidden until first art loads

    // LEFT: Album Art Area — 450px wide so img has equal 30px margin left/top/bottom
    // (ART_SIZE=420, panel height=480 → top/bottom=(480-420)/2=30px, left=30px inset)
    panel_art = lv_obj_create(scr_main);
    lv_obj_set_size(panel_art, 450, 480);
    lv_obj_set_pos(panel_art, 0, 0);
    lv_obj_set_style_bg_color(panel_art, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(panel_art, LV_OPA_TRANSP, 0);  // fully transparent: blur bg is the background
    lv_obj_set_style_radius(panel_art, 0, 0);
    lv_obj_set_style_border_width(panel_art, 0, 0);
    lv_obj_set_style_pad_all(panel_art, 0, 0);
    lv_obj_clear_flag(panel_art, LV_OBJ_FLAG_SCROLLABLE);

    // Album art image — 30px from left edge, vertically centered → equal margins all sides
    img_album = lv_img_create(panel_art);
    lv_obj_set_size(img_album, ART_SIZE, ART_SIZE);
    lv_obj_align(img_album, LV_ALIGN_LEFT_MID, 30, 0);
    lv_obj_set_style_radius(img_album, 24, 0);
    lv_obj_set_style_clip_corner(img_album, true, 0);
    lv_obj_set_style_shadow_width(img_album, 40, 0);    // 40px equal spread: floating artwork look (Apple Music style)
    lv_obj_set_style_shadow_color(img_album, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(img_album, LV_OPA_70, 0);
    lv_obj_set_style_shadow_offset_x(img_album, 0, 0);
    lv_obj_set_style_shadow_offset_y(img_album, 0, 0);  // no direction = floating, not resting

    // Placeholder when no art — centered on the art image area
    art_placeholder = lv_label_create(panel_art);
    lv_label_set_text(art_placeholder, MDI_MUSIC_NOTE);
    lv_obj_set_style_text_font(art_placeholder, &lv_font_mdi_32, 0);
    lv_obj_set_style_text_color(art_placeholder, COL_TEXT2, 0);
    lv_obj_align(art_placeholder, LV_ALIGN_CENTER, 15, 0);  // +15px: center of art at x=240, center of panel at x=225


    // Line-in mode: waveform hero icon (hidden until x-rincon-stream: detected)
    // setLineInMode(true) shows these and hides img_album + art_placeholder
    lbl_linein_icon = lv_label_create(panel_art);
    lv_label_set_text(lbl_linein_icon, MDI_WAVEFORM);
    lv_obj_set_style_text_font(lbl_linein_icon, &lv_font_mdi_80, 0);
    lv_obj_set_style_text_color(lbl_linein_icon, COL_ACCENT, 0);
    lv_obj_align(lbl_linein_icon, LV_ALIGN_CENTER, 15, -20);  // +15px art-centre offset, -20px to leave room below
    lv_obj_add_flag(lbl_linein_icon, LV_OBJ_FLAG_HIDDEN);

    lbl_linein_subtitle = lv_label_create(panel_art);
    lv_label_set_text(lbl_linein_subtitle, "LIVE AUDIO");
    lv_obj_set_style_text_font(lbl_linein_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_linein_subtitle, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_letter_space(lbl_linein_subtitle, 3, 0);  // spaced-out caps for modern look
    lv_obj_align(lbl_linein_subtitle, LV_ALIGN_CENTER, 15, 58); // below icon (+80px icon height / 2 + gap)
    lv_obj_add_flag(lbl_linein_subtitle, LV_OBJ_FLAG_HIDDEN);

    // TV audio mode: television hero icon (hidden until x-sonos-htastream: detected)
    lbl_tv_icon = lv_label_create(panel_art);
    lv_label_set_text(lbl_tv_icon, MDI_TELEVISION);
    lv_obj_set_style_text_font(lbl_tv_icon, &lv_font_mdi_80, 0);
    lv_obj_set_style_text_color(lbl_tv_icon, COL_ACCENT, 0);
    lv_obj_align(lbl_tv_icon, LV_ALIGN_CENTER, 15, -20);
    lv_obj_add_flag(lbl_tv_icon, LV_OBJ_FLAG_HIDDEN);

    lbl_tv_subtitle = lv_label_create(panel_art);
    lv_label_set_text(lbl_tv_subtitle, "TV AUDIO");
    lv_obj_set_style_text_font(lbl_tv_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_tv_subtitle, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_letter_space(lbl_tv_subtitle, 3, 0);
    lv_obj_align(lbl_tv_subtitle, LV_ALIGN_CENTER, 15, 58);
    lv_obj_add_flag(lbl_tv_subtitle, LV_OBJ_FLAG_HIDDEN);

    // RIGHT: Control Panel (350px) — narrowed by 30px to accommodate art left margin
    panel_right = lv_obj_create(scr_main);
    lv_obj_set_size(panel_right, 350, 480);
    lv_obj_set_pos(panel_right, 450, 0);
    lv_obj_set_style_bg_color(panel_right, COL_BG, 0);
    lv_obj_set_style_bg_opa(panel_right, LV_OPA_TRANSP, 0);  // fully transparent: blur bg is the background
    lv_obj_set_style_radius(panel_right, 0, 0);
    lv_obj_set_style_border_width(panel_right, 0, 0);
    lv_obj_set_style_pad_all(panel_right, 0, 0);
    lv_obj_clear_flag(panel_right, LV_OBJ_FLAG_SCROLLABLE);

    // ===== TOP ROW: Back | Now Playing - Device | WiFi Queue Settings =====
    // Setup smooth scale transition for all buttons (110% on press)
    static lv_style_transition_dsc_t trans_btn;
    static lv_style_prop_t trans_props[] = {LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(&trans_btn, trans_props, lv_anim_path_ease_out, 150, 0, NULL);

    // Back button - scale effect
    lv_obj_t* btn_back = lv_btn_create(panel_right);
    lv_obj_set_size(btn_back, 40, 40);
    lv_obj_set_pos(btn_back, 10, 15);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_back, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn_back, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_back, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_back, &trans_btn, 0);
    lv_obj_add_event_cb(btn_back, ev_devices, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_back = lv_label_create(btn_back);
    lv_label_set_text(ico_back, MDI_ARROW_LEFT);
    lv_obj_set_style_text_font(ico_back, &lv_font_mdi_24, 0);
    lv_obj_set_style_text_color(ico_back, COL_TEXT, 0);
    lv_obj_center(ico_back);

    // "Now Playing - Device" label - positioned after back button
    lbl_device_name = lv_label_create(panel_right);
    lv_label_set_text(lbl_device_name, "Now Playing");
    lv_obj_set_style_text_color(lbl_device_name, COL_TEXT2, 0);
    lv_obj_set_style_text_font(lbl_device_name, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_device_name, 55, 25);

    // Settings button
    lv_obj_t* btn_settings = lv_btn_create(panel_right);
    lv_obj_set_size(btn_settings, 38, 38);
    lv_obj_set_pos(btn_settings, 305, 18);
    lv_obj_set_style_bg_opa(btn_settings, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_settings, 0, 0);
    lv_obj_add_event_cb(btn_settings, ev_settings, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_set = lv_label_create(btn_settings);
    lv_label_set_text(ico_set, MDI_COG);
    lv_obj_set_style_text_font(ico_set, &lv_font_mdi_24, 0);
    lv_obj_set_style_text_color(ico_set, COL_TEXT, 0);
    lv_obj_center(ico_set);

    // ===== TRACK INFO =====
    // Title (white, large) — FIRST: modern players put title above artist
    lbl_title = lv_label_create(panel_right);
    lv_obj_set_pos(lbl_title, 15, 88);
    lv_obj_set_width(lbl_title, 265);
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lbl_title, "Not Playing");
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_set_style_text_font(lbl_title, &ui_font_title_chain, 0);

    // Artist (gray, smaller) — below title
    lbl_artist = lv_label_create(panel_right);
    lv_obj_set_pos(lbl_artist, 15, 132);
    lv_obj_set_size(lbl_artist, 265, 20);  // Fixed height prevents text wrapping over elements below (issue #63)
    lv_label_set_long_mode(lbl_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);  // Scroll like title — shows full name
    lv_label_set_text(lbl_artist, "");
    lv_obj_set_style_text_color(lbl_artist, COL_TEXT2, 0);
    lv_obj_set_style_text_font(lbl_artist, &ui_font_artist_chain, 0);

    // Queue/Playlist button — aligned with artist row
    btn_queue = lv_btn_create(panel_right);
    lv_obj_set_size(btn_queue, 48, 48);
    lv_obj_set_pos(btn_queue, 295, 122);
    lv_obj_set_style_bg_opa(btn_queue, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_queue, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_queue, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn_queue, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_queue, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_queue, &trans_btn, 0);
    lv_obj_set_ext_click_area(btn_queue, 8);  // Add 8px clickable area around button
    lv_obj_add_event_cb(btn_queue, ev_queue, LV_EVENT_CLICKED, NULL);  // Go to Queue/Playlist
    lv_obj_t* ico_fav = lv_label_create(btn_queue);
    lv_label_set_text(ico_fav, MDI_PLAYLIST);
    lv_obj_set_style_text_font(ico_fav, &lv_font_mdi_32, 0);
    lv_obj_set_style_text_color(ico_fav, COL_TEXT, 0);
    lv_obj_center(ico_fav);

    // Album name — below artist
    lbl_album = lv_label_create(panel_right);
    lv_obj_set_size(lbl_album, 265, 20);
    lv_obj_set_pos(lbl_album, 15, 154);
    lv_label_set_long_mode(lbl_album, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lbl_album, "");
    lv_obj_set_style_text_color(lbl_album, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl_album, &lv_font_montserrat_14, 0);

    // ===== PROGRESS BAR =====
    slider_progress = lv_slider_create(panel_right);
    lv_obj_set_pos(slider_progress, 15, 182);
    lv_obj_set_size(slider_progress, 320, 8);  // 8px: easier touch target, more visual weight
    lv_slider_set_range(slider_progress, 0, 100);
    lv_obj_set_style_bg_color(slider_progress, COL_BTN, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_progress, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_progress, COL_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_progress, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_progress, ev_progress, LV_EVENT_ALL, NULL);

    // Time elapsed
    lbl_time = lv_label_create(panel_right);
    lv_obj_set_pos(lbl_time, 15, 198);
    lv_label_set_text(lbl_time, "0:00");
    lv_obj_set_style_text_color(lbl_time, COL_TEXT2, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_14, 0);

    // Time remaining — shown as negative countdown (Apple Music / Spotify style)
    lbl_time_remaining = lv_label_create(panel_right);
    lv_obj_set_pos(lbl_time_remaining, 280, 198);
    lv_label_set_text(lbl_time_remaining, "-0:00");
    lv_obj_set_style_text_color(lbl_time_remaining, COL_TEXT2, 0);
    lv_obj_set_style_text_font(lbl_time_remaining, &lv_font_montserrat_14, 0);

    // ===== PLAYBACK CONTROLS - PERFECTLY CENTERED =====
    // Layout: [shuffle] [prev] [PLAY] [next] [repeat]
    // Center of 350px panel = 175

    int ctrl_y = 285;
    int center_x = 175;

    // PLAY button (center) - ambient-coloured circle with scale effect
    btn_play = lv_btn_create(panel_right);
    lv_obj_set_size(btn_play, 80, 80);
    lv_obj_set_pos(btn_play, center_x - 40, ctrl_y - 40);
    lv_obj_set_style_bg_color(btn_play, COL_TEXT, 0);
    lv_obj_set_style_radius(btn_play, 40, 0);
    lv_obj_set_style_shadow_width(btn_play, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_play, 280, LV_STATE_PRESSED);  // Scale to 110%
    lv_obj_set_style_transform_scale_y(btn_play, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_play, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_play, &trans_btn, 0);

    lv_obj_add_event_cb(btn_play, ev_play, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_play = lv_label_create(btn_play);
    lv_label_set_text(ico_play, MDI_PAUSE);
    lv_obj_set_style_text_font(ico_play, &lv_font_mdi_40, 0);
    lv_obj_set_style_text_color(ico_play, COL_BG, 0);
    lv_obj_center(ico_play);

    // PREV button (left of play) - scale effect
    btn_prev = lv_btn_create(panel_right);
    lv_obj_set_size(btn_prev, 50, 50);
    lv_obj_set_pos(btn_prev, center_x - 108, ctrl_y - 25);
    lv_obj_set_style_bg_opa(btn_prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn_prev, 25, 0);
    lv_obj_set_style_shadow_width(btn_prev, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_prev, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn_prev, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_prev, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_prev, &trans_btn, 0);
    lv_obj_add_event_cb(btn_prev, ev_prev, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_prev = lv_label_create(btn_prev);
    lv_label_set_text(ico_prev, MDI_SKIP_PREV);
    lv_obj_set_style_text_font(ico_prev, &lv_font_mdi_40, 0);
    lv_obj_set_style_text_color(ico_prev, COL_TEXT, 0);
    lv_obj_center(ico_prev);

    // NEXT button (right of play) - scale effect
    btn_next = lv_btn_create(panel_right);
    lv_obj_set_size(btn_next, 50, 50);
    lv_obj_set_pos(btn_next, center_x + 58, ctrl_y - 25);
    lv_obj_set_style_bg_opa(btn_next, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn_next, 25, 0);
    lv_obj_set_style_shadow_width(btn_next, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_next, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn_next, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_next, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_next, &trans_btn, 0);
    lv_obj_add_event_cb(btn_next, ev_next, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_next = lv_label_create(btn_next);
    lv_label_set_text(ico_next, MDI_SKIP_NEXT);
    lv_obj_set_style_text_font(ico_next, &lv_font_mdi_40, 0);
    lv_obj_set_style_text_color(ico_next, COL_TEXT, 0);
    lv_obj_center(ico_next);

    // DISLIKE button (far left — replaces shuffle)
    btn_shuffle = lv_btn_create(panel_right);
    lv_obj_set_size(btn_shuffle, 45, 45);
    lv_obj_set_pos(btn_shuffle, center_x - 168, ctrl_y - 22);
    lv_obj_set_style_bg_opa(btn_shuffle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn_shuffle, 22, 0);
    lv_obj_set_style_shadow_width(btn_shuffle, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_shuffle, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn_shuffle, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_shuffle, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_shuffle, &trans_btn, 0);
    lv_obj_add_event_cb(btn_shuffle, ev_shuffle, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_dislike = lv_label_create(btn_shuffle);
    lv_label_set_text(ico_dislike, FA_THUMB_DOWN);
    lv_obj_set_style_text_font(ico_dislike, &lv_font_fa_thumbs_32, 0);
    lv_obj_set_style_text_color(ico_dislike, COL_TEXT2, 0);
    lv_obj_center(ico_dislike);

    // LIKE button (far right — replaces repeat)
    btn_repeat = lv_btn_create(panel_right);
    lv_obj_set_size(btn_repeat, 45, 45);
    lv_obj_set_pos(btn_repeat, center_x + 123, ctrl_y - 22);
    lv_obj_set_style_bg_opa(btn_repeat, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn_repeat, 22, 0);
    lv_obj_set_style_shadow_width(btn_repeat, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_repeat, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn_repeat, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_repeat, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_repeat, &trans_btn, 0);
    lv_obj_add_event_cb(btn_repeat, ev_repeat, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_like = lv_label_create(btn_repeat);
    lv_label_set_text(ico_like, FA_THUMB_UP);
    lv_obj_set_style_text_font(ico_like, &lv_font_fa_thumbs_32, 0);
    lv_obj_set_style_text_color(ico_like, COL_TEXT2, 0);
    lv_obj_center(ico_like);

    // ===== VOLUME SLIDER =====
    int vol_y = 360;

    // Mute button (left) - scale effect
    btn_mute = lv_btn_create(panel_right);
    lv_obj_set_size(btn_mute, 40, 40);
    lv_obj_set_pos(btn_mute, 20, vol_y);
    lv_obj_set_style_bg_opa(btn_mute, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn_mute, 20, 0);
    lv_obj_set_style_transform_scale_y(btn_mute, 280, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_mute, 0, 0);
    lv_obj_set_style_transform_scale_x(btn_mute, 280, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_mute, &trans_btn, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn_mute, &trans_btn, 0);
    lv_obj_add_event_cb(btn_mute, ev_mute, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ico_mute = lv_label_create(btn_mute);
    lv_label_set_text(ico_mute, MDI_VOLUME_HIGH);
    lv_obj_set_style_text_font(ico_mute, &lv_font_mdi_32, 0);
    lv_obj_set_style_text_color(ico_mute, COL_TEXT2, 0);
    lv_obj_center(ico_mute);

    // Volume slider
    slider_vol = lv_slider_create(panel_right);
    lv_obj_set_size(slider_vol, 240, 6);
    lv_obj_set_pos(slider_vol, 65, vol_y + 17);
    lv_slider_set_range(slider_vol, 0, 100);
    lv_obj_set_style_bg_color(slider_vol, COL_BTN, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_vol, COL_TEXT2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_vol, COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_vol, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_vol, ev_vol_slider, LV_EVENT_ALL, NULL);

    // ===== PLAY NEXT SECTION (below volume) =====
    int next_y = 440;

    // Small album art for next track (hidden for now)
    img_next_album = lv_img_create(panel_right);
    lv_obj_set_pos(img_next_album, 15, next_y);
    lv_obj_set_size(img_next_album, 40, 40);
    lv_obj_set_style_radius(img_next_album, 4, 0);
    lv_obj_set_style_clip_corner(img_next_album, true, 0);
    lv_obj_add_flag(img_next_album, LV_OBJ_FLAG_HIDDEN); // Hide thumbnail for now

    // "Next:" label
    lbl_next_header = lv_label_create(panel_right);  // Use GLOBAL, not local!
    lv_obj_set_pos(lbl_next_header, 15, next_y);
    lv_label_set_text(lbl_next_header, "Next:");
    lv_obj_set_style_text_color(lbl_next_header, COL_TEXT2, 0);
    lv_obj_set_style_text_font(lbl_next_header, &lv_font_montserrat_12, 0);

    // Next track title - clickable to play next
    lbl_next_title = lv_label_create(panel_right);
    lv_obj_set_pos(lbl_next_title, 55, next_y);
    lv_label_set_text(lbl_next_title, "");
    lv_obj_set_style_text_color(lbl_next_title, COL_TEXT, 0);
    lv_obj_set_style_text_font(lbl_next_title, &ui_font_next_title_chain, 0);
    lv_obj_set_width(lbl_next_title, 275);
    lv_label_set_long_mode(lbl_next_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // Make it clickable to play next track
    lv_obj_add_flag(lbl_next_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_next_title, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            sonos.next();
        }
    }, LV_EVENT_ALL, NULL);

    // Next track artist - also clickable
    lbl_next_artist = lv_label_create(panel_right);
    lv_obj_set_pos(lbl_next_artist, 55, next_y + 18);
    lv_label_set_text(lbl_next_artist, "");
    lv_obj_set_style_text_color(lbl_next_artist, COL_TEXT2, 0);
    lv_obj_set_style_text_font(lbl_next_artist, &ui_font_next_artist_chain, 0);
    lv_obj_set_width(lbl_next_artist, 275);
    lv_label_set_long_mode(lbl_next_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // Make it clickable to play next track
    lv_obj_add_flag(lbl_next_artist, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_next_artist, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            sonos.next();
        }
    }, LV_EVENT_ALL, NULL);
}
