/**
 * YouTube Music Desktop (YTMD) Controller — Implementation
 *
 * Replaces SonosController. Keeps the same class/struct names so all
 * existing UI screen code compiles without modification.
 *
 * API server: 192.168.0.30:26538
 *
 * Endpoints:
 *   GET  /api/v1/song          — current song + progress (polled every 3 s)
 *   GET  /api/v1/queue         — playlist (polled on demand / every 30 s)
 *   POST /api/v1/previous
 *   POST /api/v1/toggle-play
 *   POST /api/v1/next
 *   PATCH /api/v1/queue        — body {"index": N}
 */

#include "ytmd_controller.h"
#include "ui_common.h"          // network_mutex, art_download_in_progress
#include "config.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// ─── Poll intervals ───────────────────────────────────────────────────────────
static constexpr uint32_t SONG_POLL_MS        = 3000;
static constexpr uint32_t QUEUE_POLL_MS       = 60000;
static constexpr uint32_t HTTP_TIMEOUT_MS     = 3500;
static constexpr uint32_t YTMD_POLL_STACK     = 8192;   // Polling task stack (PSRAM)
static constexpr uint8_t  ERROR_THRESHOLD     = 5;
// (JSON doc sizes removed — using ArduinoJson v7 JsonDocument with dynamic alloc)

// ─── Constructor / Destructor ─────────────────────────────────────────────────

SonosController::SonosController() {}

SonosController::~SonosController() {
    shutdown_ = true;
    if (pollTaskHandle_) {
        vTaskDelete(pollTaskHandle_);
        pollTaskHandle_ = nullptr;
    }
    if (uiUpdateQueue_) {
        vQueueDelete(uiUpdateQueue_);
        uiUpdateQueue_ = nullptr;
    }
    if (pollTaskStack_) {
        heap_caps_free(pollTaskStack_);
        pollTaskStack_ = nullptr;
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void SonosController::begin() {
    uiUpdateQueue_ = xQueueCreate(SONOS_UI_QUEUE_SIZE, sizeof(UIUpdate_t));
    state_ = SonosDevice{};   // zero-initialise / set defaults
    Serial.printf("[YTMD] Controller init — %s:%s\n", YTMD_HOST, YTMD_PORT);
}

void SonosController::startTasks() {
    if (pollTaskHandle_) return;  // already running

    // Allocate stack in PSRAM to preserve DMA SRAM for WiFi/SDIO
    pollTaskStack_ = (StackType_t*)heap_caps_malloc(
        YTMD_POLL_STACK, MALLOC_CAP_SPIRAM);
    if (!pollTaskStack_) {
        Serial.println("[YTMD] PSRAM stack alloc failed — using internal RAM");
        pollTaskStack_ = (StackType_t*)heap_caps_malloc(
            YTMD_POLL_STACK, MALLOC_CAP_8BIT);
    }
    if (!pollTaskStack_) {
        Serial.println("[YTMD] Stack alloc failed — cannot start poll task");
        return;
    }

    shutdown_ = false;
    pollTaskHandle_ = xTaskCreateStaticPinnedToCore(
        pollTaskFn, "YTMDPoll",
        YTMD_POLL_STACK / sizeof(StackType_t),
        this,
        SONOS_POLL_TASK_PRIORITY,
        pollTaskStack_,
        &pollTaskTCB_,
        1);

    if (pollTaskHandle_) {
        Serial.println("[YTMD] Poll task started");
    } else {
        Serial.println("[YTMD] Poll task create failed");
    }
}

void SonosController::suspendTasks() {
    shutdown_ = true;
    Serial.println("[YTMD] Tasks suspended");
}

void SonosController::resumeTasks() {
    shutdown_ = false;
    startTasks();
    Serial.println("[YTMD] Tasks resumed");
}

// ─── Playback commands ────────────────────────────────────────────────────────

void SonosController::play() {
    if (state_.isPlaying) return;
    sendCommand("/api/v1/toggle-play", "POST");
    state_.isPlaying = true;
    notifyUI(UPDATE_PLAYBACK_STATE);
}

void SonosController::pause() {
    if (!state_.isPlaying) return;
    sendCommand("/api/v1/toggle-play", "POST");
    state_.isPlaying = false;
    notifyUI(UPDATE_PLAYBACK_STATE);
}

void SonosController::next() {
    sendCommand("/api/v1/next", "POST");
}

void SonosController::previous() {
    sendCommand("/api/v1/previous", "POST");
}

void SonosController::playQueueItem(int index) {
    // index is 1-based from the UI; YTMD expects 0-based
    int zeroIdx = (index > 0) ? (index - 1) : 0;
    char body[24];
    snprintf(body, sizeof(body), "{\"index\":%d}", zeroIdx);
    sendCommand("/api/v1/queue", "PATCH", body);
}

void SonosController::requestQueueUpdate() {
    queueRefreshPending_ = true;
}

void SonosController::like() {
    sendCommand("/api/v1/like", "POST");
}

void SonosController::dislike() {
    sendCommand("/api/v1/dislike", "POST");
}

// ─── Polling task ─────────────────────────────────────────────────────────────

/*static*/ void SonosController::pollTaskFn(void* param) {
    static_cast<SonosController*>(param)->runPollLoop();
}

void SonosController::runPollLoop() {
    uint32_t lastSongPoll  = 0;
    uint32_t lastQueuePoll = 0;
    uint8_t  songFailures  = 0;

    // Don't trigger a heavy queue fetch during boot; we'll fetch on demand or periodic poll.
    queueRefreshPending_ = false;

    for (;;) {
        if (shutdown_) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        uint32_t now = millis();

        // ── Song poll (every SONG_POLL_MS) ──────────────────────────────────
        if (now - lastSongPoll >= SONG_POLL_MS) {
            lastSongPoll = now;

            if (WiFi.status() == WL_CONNECTED) {
                String json;
                bool ok = false;
                if (xSemaphoreTake(network_mutex, pdMS_TO_TICKS(3000))) {
                    ok = fetchJson("/api/v1/song", json);
                    if (!ok) ok = fetchJson("/api/v1/song-info", json);
                    xSemaphoreGive(network_mutex);
                }

                if (ok && json.length() > 0) {
                    parseSong(json);
                    songFailures = 0;
                    state_.connected = true;
                    // Fetch like state, volume, and next track on every song poll
                    if (xSemaphoreTake(network_mutex, pdMS_TO_TICKS(3000))) {
                        fetchLikeState();
                        fetchVolume();
                        fetchNextTrack();
                        xSemaphoreGive(network_mutex);
                    }
                } else {
                    if (songFailures < 255) songFailures++;
                    if (songFailures >= ERROR_THRESHOLD) {
                        state_.connected = false;
                        notifyUI(UPDATE_ERROR);
                    }
                }
            } else {
                state_.connected = false;
            }
        }

        // ── Queue poll (on demand or every QUEUE_POLL_MS) ───────────────────
        // Also picks up queue_fetch_requested set by ev_queue() in ui_handlers.cpp
        if (queue_fetch_requested) {
            queueRefreshPending_ = true;
            queue_fetch_requested = false;
        }
        bool queueDue = (now - lastQueuePoll >= QUEUE_POLL_MS);
        if ((queueRefreshPending_ || queueDue) && WiFi.status() == WL_CONNECTED) {
            queueRefreshPending_ = false;
            lastQueuePoll = now;

            bool ok = false;
            if (xSemaphoreTake(network_mutex, pdMS_TO_TICKS(3000))) {
                ok = fetchAndParseQueue();   // streams directly — no 974KB String buffer
                xSemaphoreGive(network_mutex);
            }
            (void)ok;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ─── REST helpers ─────────────────────────────────────────────────────────────

String SonosController::buildUrl(const char* path) {
    String url = "http://";
    url += YTMD_HOST;
    url += ':';
    url += YTMD_PORT;
    url += path;
    return url;
}

bool SonosController::fetchJson(const char* path, String& out) {
    WiFiClient client;
    HTTPClient http;
    String url = buildUrl(path);

    if (!http.begin(client, url)) return false;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.useHTTP10(true);   // prevents keep-alive; reduces SDIO pressure

    int code = http.GET();
    if (code == 200) {
        out = http.getString();
        http.end();
        return true;
    }
    http.end();
    return false;
}

bool SonosController::sendCommand(const char* path, const char* method, const char* body) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient client;
    HTTPClient http;
    String url = buildUrl(path);

    if (!http.begin(client, url)) return false;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.useHTTP10(true);

    int code = 0;
    if (body) {
        http.addHeader("Content-Type", "application/json");
        code = http.sendRequest(method, (uint8_t*)body, strlen(body));
    } else {
        code = http.sendRequest(method);
    }
    http.end();

    Serial.printf("[YTMD] CMD %s %s → %d\n", method, path, code);
    return (code >= 200 && code < 300);
}

// ─── JSON parsers ─────────────────────────────────────────────────────────────

/*static*/ uint32_t SonosController::parseSeconds(const char* s) {
    if (!s || !s[0]) return 0;
    double v = atof(s);
    return v > 0.0 ? (uint32_t)(v + 0.5) : 0;
}

// Extract text from a YouTube Music "runs" or "simpleText" JSON node.
// Writes into dst (null-terminated, max dstSize bytes).
static void extractText(JsonVariantConst node, char* dst, size_t dstSize) {
    if (!dst || dstSize == 0) return;
    dst[0] = '\0';

    const char* simple = node["simpleText"];
    if (simple && simple[0]) {
        strncpy(dst, simple, dstSize - 1);
        dst[dstSize - 1] = '\0';
        return;
    }

    JsonArrayConst runs = node["runs"].as<JsonArrayConst>();
    if (runs.isNull()) return;

    size_t used = 0;
    for (JsonVariantConst run : runs) {
        const char* text = run["text"];
        if (!text || !text[0]) continue;
        int written = snprintf(dst + used, dstSize - used, "%s", text);
        if (written < 0) break;
        used += (size_t)written;
        if (used >= dstSize - 1) break;
    }
    dst[dstSize - 1] = '\0';
}

// Pick the best thumbnail URL from a thumbnail node (handles multiple formats).
/*static*/ void SonosController::extractThumbnailUrl(const String& json,
                                                     char* dst, size_t dstSize) {
    // This is called with the full song JSON string; parse inline
    // to avoid nested JSON doc allocations.
    // Delegate to the parseSong thumbnail extraction below.
    (void)json; (void)dst; (void)dstSize;
}

void SonosController::parseSong(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[YTMD] Song JSON parse error: %s (len=%d)\n", err.c_str(), json.length());
        return;
    }

    const char* title  = doc["title"]  | "";
    const char* artist = doc["artist"] | "";
    bool isPaused = doc["isPaused"] | false;

    // Progress: YTMD may send seconds as number or string
    uint32_t totalSec   = 0;
    uint32_t elapsedSec = 0;
    JsonVariantConst durNode  = doc["songDuration"];
    JsonVariantConst elapsedNode = doc["elapsedSeconds"];

    if (!durNode.isNull()) {
        if (durNode.is<uint32_t>())     totalSec = durNode.as<uint32_t>();
        else if (durNode.is<int>())     totalSec = (uint32_t)max(0, durNode.as<int>());
        else if (durNode.is<float>())   totalSec = (uint32_t)(durNode.as<float>() + 0.5f);
        else if (durNode.is<const char*>()) totalSec = parseSeconds(durNode.as<const char*>());
    }
    if (!elapsedNode.isNull()) {
        if (elapsedNode.is<uint32_t>())   elapsedSec = elapsedNode.as<uint32_t>();
        else if (elapsedNode.is<int>())   elapsedSec = (uint32_t)max(0, elapsedNode.as<int>());
        else if (elapsedNode.is<float>()) elapsedSec = (uint32_t)(elapsedNode.as<float>() + 0.5f);
        else if (elapsedNode.is<const char*>()) elapsedSec = parseSeconds(elapsedNode.as<const char*>());
    }

    // Album art URL — try several common keys/structures
    char artUrl[384] = {0};
    {
        // 1. Direct string keys
        const char* cover = doc["cover"];
        if (cover && cover[0]) { strncpy(artUrl, cover, sizeof(artUrl)-1); }

        if (!artUrl[0]) {
            const char* imgSrc = doc["imageSrc"];
            if (imgSrc && imgSrc[0]) { strncpy(artUrl, imgSrc, sizeof(artUrl)-1); }
        }

        // 2. thumbnails[] array — prefer width >= 300
        if (!artUrl[0]) {
            JsonArrayConst thumbs = doc["thumbnails"].as<JsonArrayConst>();
            if (!thumbs.isNull()) {
                const char* best = nullptr;
                uint32_t bestW = 0;
                for (JsonVariantConst t : thumbs) {
                    const char* u = t["url"];
                    if (!u || !u[0]) continue;
                    uint32_t w = t["width"] | 0u;
                    if (w >= 300 && (best == nullptr || w < bestW)) {
                        best = u; bestW = w;
                    } else if (!best) {
                        best = u;
                    }
                }
                if (best) strncpy(artUrl, best, sizeof(artUrl)-1);
            }
        }

        // 3. Nested: doc["thumbnail"]["thumbnails"][0]["url"]
        if (!artUrl[0]) {
            JsonArrayConst thumbs = doc["thumbnail"]["thumbnails"].as<JsonArrayConst>();
            if (!thumbs.isNull()) {
                for (JsonVariantConst t : thumbs) {
                    const char* u = t["url"];
                    if (u && u[0]) { strncpy(artUrl, u, sizeof(artUrl)-1); break; }
                }
            }
        }

        // 4. doc["track"]["cover"] or doc["track"]["thumbnail"]["thumbnails"]
        if (!artUrl[0]) {
            JsonObjectConst track = doc["track"].as<JsonObjectConst>();
            if (!track.isNull()) {
                const char* cover2 = track["cover"];
                if (cover2 && cover2[0]) strncpy(artUrl, cover2, sizeof(artUrl)-1);
                if (!artUrl[0]) {
                    JsonArrayConst thumbs = track["thumbnail"]["thumbnails"].as<JsonArrayConst>();
                    if (!thumbs.isNull()) {
                        for (JsonVariantConst t : thumbs) {
                            const char* u = t["url"];
                            if (u && u[0]) { strncpy(artUrl, u, sizeof(artUrl)-1); break; }
                        }
                    }
                }
            }
        }

        // Google image size reduction: replace =w60-h60 with =w400-h400
        // (or =sNN with =s400 for square images)
        String artStr = artUrl;
        if (artStr.indexOf("googleusercontent.com") > 0 ||
            artStr.indexOf("ytimg.com") > 0 ||
            artStr.indexOf("ggpht.com") > 0) {
            // Replace small Google image size params
            int eqPos = artStr.lastIndexOf('=');
            if (eqPos > 0) {
                char suffix = artStr.charAt(eqPos + 1);
                if (suffix == 'w' || suffix == 's') {
                    // Cut off size param and append a larger size
                    artStr = artStr.substring(0, eqPos) + "=w400-h400";
                    strncpy(artUrl, artStr.c_str(), sizeof(artUrl)-1);
                }
            }
        }
    }

    // Build pseudo-URI for change detection (title + artist)
    String newURI = String(title) + "|" + String(artist);

    // Format time strings
    char relTimeBuf[16], durBuf[16];
    {
        uint32_t em = elapsedSec / 60, es = elapsedSec % 60;
        uint32_t dm = totalSec   / 60, ds = totalSec   % 60;
        snprintf(relTimeBuf, sizeof(relTimeBuf), "%u:%02u", em, es);
        snprintf(durBuf,     sizeof(durBuf),     "%u:%02u", dm, ds);
    }

    bool trackChanged  = (newURI     != state_.currentURI);
    bool stateChanged  = (isPaused   == state_.isPlaying);   // playing↔paused flip
    bool progressDirty = (elapsedSec != (uint32_t)state_.relTimeSeconds && !trackChanged);

    state_.currentTrack    = title;
    state_.currentArtist   = artist;
    state_.currentAlbum    = "";
    state_.isPlaying       = !isPaused;
    state_.relTimeSeconds  = (int)elapsedSec;
    state_.durationSeconds = (int)totalSec;
    state_.relTime         = relTimeBuf;
    state_.trackDuration   = durBuf;
    state_.albumArtURL     = artUrl;
    state_.currentURI      = newURI;
    state_.connected       = true;
    state_.lastUpdateTime  = millis();

    if (trackChanged) {
        notifyUI(UPDATE_TRACK_INFO);
        notifyUI(UPDATE_ALBUM_ART);
    } else if (stateChanged) {
        notifyUI(UPDATE_PLAYBACK_STATE);
    } else if (progressDirty) {
        notifyUI(UPDATE_PLAYBACK_STATE);
    }
}

void SonosController::seek(int seconds) {
    char body[32];
    snprintf(body, sizeof(body), "{\"seconds\":%d}", seconds);
    sendCommand("/api/v1/seek-to", "POST", body);
}

void SonosController::setVolume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    state_.volume = vol;
    char body[24];
    snprintf(body, sizeof(body), "{\"volume\":%d}", vol);
    sendCommand("/api/v1/volume", "POST", body);
}

void SonosController::setMute(bool muted) {
    if (state_.isMuted == muted) return;  // already in desired state
    state_.isMuted = muted;               // optimistic update
    sendCommand("/api/v1/toggle-mute", "POST");
}

bool SonosController::fetchVolume() {
    String json;
    if (!fetchJson("/api/v1/volume", json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    int  vol    = doc["state"]   | state_.volume;
    bool muted  = doc["isMuted"] | state_.isMuted;

    state_.volume  = vol;
    state_.isMuted = muted;
    return true;
}

bool SonosController::fetchLikeState() {
    String json;
    if (!fetchJson("/api/v1/like-state", json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    // {"state": "LIKE"|"DISLIKE"|"INDIFFERENT"|null}
    const char* st = doc["state"] | (const char*)nullptr;
    String newStatus = (st && st[0]) ? String(st) : "INDIFFERENT";

    if (newStatus != state_.likeStatus) {
        state_.likeStatus = newStatus;
        notifyUI(UPDATE_PLAYBACK_STATE);
    }
    return true;
}

bool SonosController::fetchNextTrack() {
    String json;
    if (!fetchJson("/api/v1/queue/next", json)) {
        // 204 = no next track (end of queue or unavailable)
        Serial.println("[YTMD:DBG] queue/next: 204 (no next track)");
        state_.nextTrackTitle     = "";
        state_.nextTrackArtist    = "";
        state_.nextTrackAvailable = true;
        return true;
    }

    Serial.printf("[YTMD:DBG] queue/next JSON: %.400s\n", json.c_str());

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    char titleBuf[96]  = {0};
    char artistBuf[96] = {0};

    extractText(doc["title"],           titleBuf,  sizeof(titleBuf));
    extractText(doc["shortBylineText"], artistBuf, sizeof(artistBuf));

    // plain string fallbacks
    if (!titleBuf[0])  { const char* v = doc["title"];  if (v) strncpy(titleBuf,  v, 95); }
    if (!artistBuf[0]) { const char* v = doc["artist"]; if (v) strncpy(artistBuf, v, 95); }

    Serial.printf("[YTMD:DBG] queue/next parsed: title='%s' artist='%s'\n", titleBuf, artistBuf);

    state_.nextTrackTitle     = titleBuf;
    state_.nextTrackArtist    = artistBuf;
    state_.nextTrackAvailable = true;
    return true;
}

bool SonosController::fetchAndParseQueue() {
    WiFiClient client;
    HTTPClient http;
    String url = buildUrl("/api/v1/queue");

    if (!http.begin(client, url)) return false;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.useHTTP10(true);

    int code = http.GET();
    if (code != 200) {
        http.end();
        Serial.printf("[YTMD] Queue HTTP %d\n", code);
        return false;
    }

    JsonDocument doc;
    WiFiClient& stream = http.getStream();
    DeserializationError err = deserializeJson(doc, stream,
        DeserializationOption::NestingLimit(YTMD_QUEUE_JSON_NESTING_LIMIT));
    http.end();

    if (err) {
        Serial.printf("[YTMD] Queue stream parse error: %s\n", err.c_str());
        return false;
    }

    // Re-use existing parseQueue logic via a small JSON string? No — call doc directly.
    // Inline the parse here to avoid a second large allocation.
    JsonArrayConst items = doc["items"].as<JsonArrayConst>();
    if (items.isNull()) items = doc.as<JsonArrayConst>();
    if (items.isNull()) return false;

    int count = 0, playingIdx = -1;
    for (JsonVariantConst item : items) {
        if (count >= QUEUE_ITEMS_MAX) break;

        // Try multiple renderer key names used by different YTMD versions
        JsonVariantConst renderer;
        if (!item["playlistPanelVideoRenderer"].isNull())
            renderer = item["playlistPanelVideoRenderer"].as<JsonVariantConst>();
        else if (!item["playlistPanelVideoWrapperRenderer"]["primaryRenderer"]["playlistPanelVideoRenderer"].isNull())
            renderer = item["playlistPanelVideoWrapperRenderer"]["primaryRenderer"]["playlistPanelVideoRenderer"].as<JsonVariantConst>();
        else if (!item["musicQueueVideoRenderer"].isNull())
            renderer = item["musicQueueVideoRenderer"].as<JsonVariantConst>();
        else if (!item["musicResponsiveListItemRenderer"].isNull())
            renderer = item["musicResponsiveListItemRenderer"].as<JsonVariantConst>();
        else
            renderer = item;  // flat format fallback

        char titleBuf[96]  = {0};
        char artistBuf[96] = {0};
        char durBuf[16]    = {0};

        extractText(renderer["title"],           titleBuf,  sizeof(titleBuf));
        extractText(renderer["shortBylineText"],  artistBuf, sizeof(artistBuf));
        extractText(renderer["lengthText"],       durBuf,    sizeof(durBuf));

        if (!titleBuf[0])  { const char* v = renderer["title"];  if (v) strncpy(titleBuf,  v, 95); }
        if (!artistBuf[0]) { const char* v = renderer["artist"]; if (v) strncpy(artistBuf, v, 95); }
        // artists[0].name fallback (YTMD flat format)
        if (!artistBuf[0]) {
            JsonArrayConst arts = renderer["artists"].as<JsonArrayConst>();
            if (!arts.isNull()) {
                const char* n = arts[0]["name"] | (const char*)nullptr;
                if (n && n[0]) strncpy(artistBuf, n, 95);
            }
        }
        if (!durBuf[0])    extractText(renderer["longBylineText"], artistBuf, sizeof(artistBuf));

        // DEBUG: log first 3 items to see structure
        if (count < 3) {
            String dbgItem; serializeJson(item, dbgItem);
            Serial.printf("[YTMD:DBG] queue[%d] item(400): %.400s\n", count, dbgItem.c_str());
            Serial.printf("[YTMD:DBG] queue[%d] => title='%s' artist='%s' dur='%s'\n",
                          count, titleBuf, artistBuf, durBuf);
        }

        if (!titleBuf[0])  strncpy(titleBuf,  "Unknown", 95);
        if (!artistBuf[0]) strncpy(artistBuf, "Unknown", 95);
        if (!durBuf[0])    strncpy(durBuf,    "--:--",   15);

        state_.queue[count].title       = titleBuf;
        state_.queue[count].artist      = artistBuf;
        state_.queue[count].duration    = durBuf;
        state_.queue[count].trackNumber = count + 1;
        state_.queue[count].albumArtURL = "";
        state_.queue[count].album       = "";

        if ((renderer["selected"] | false) || (renderer["isSelected"] | false)) playingIdx = count;
        count++;
    }

    state_.queueSize          = count;
    state_.totalTracks        = count;
    if (playingIdx >= 0) state_.currentTrackNumber = playingIdx + 1;

    Serial.printf("[YTMD] Queue: %d items, playing=%d (heap=%d)\n",
                  count, playingIdx, esp_get_free_heap_size());
    notifyUI(UPDATE_QUEUE);
    return true;
}

void SonosController::parseQueue(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[YTMD] Queue JSON parse error: %s (len=%d, heap=%d)\n",
                      err.c_str(), json.length(), esp_get_free_heap_size());
        return;
    }

    JsonArrayConst items = doc["items"].as<JsonArrayConst>();
    if (items.isNull()) {
        // Some versions return a flat array at root
        items = doc.as<JsonArrayConst>();
    }
    if (items.isNull()) return;

    int count = 0;
    int playingIdx = -1;

    for (JsonVariantConst item : items) {
        if (count >= QUEUE_ITEMS_MAX) break;

        // Unwrap renderer if needed
        JsonVariantConst renderer = item["playlistPanelVideoRenderer"].isNull()
            ? item["playlistPanelVideoWrapperRenderer"]["primaryRenderer"]["playlistPanelVideoRenderer"].as<JsonVariantConst>()
            : item["playlistPanelVideoRenderer"].as<JsonVariantConst>();
        if (renderer.isNull()) renderer = item;  // flat format fallback

        char titleBuf[96]  = {0};
        char artistBuf[96] = {0};
        char durBuf[16]    = {0};

        extractText(renderer["title"],         titleBuf,  sizeof(titleBuf));
        extractText(renderer["shortBylineText"], artistBuf, sizeof(artistBuf));
        extractText(renderer["lengthText"],    durBuf,    sizeof(durBuf));

        // Flat-format fallbacks
        if (!titleBuf[0])  { const char* v = renderer["title"];  if (v) strncpy(titleBuf,  v, 95); }
        if (!artistBuf[0]) { const char* v = renderer["artist"]; if (v) strncpy(artistBuf, v, 95); }
        if (!durBuf[0]) {
            extractText(renderer["longBylineText"], artistBuf, sizeof(artistBuf));
        }
        if (!titleBuf[0])  strncpy(titleBuf,  "Unknown", 95);
        if (!artistBuf[0]) strncpy(artistBuf, "Unknown", 95);
        if (!durBuf[0])    strncpy(durBuf,    "--:--", 15);

        state_.queue[count].title       = titleBuf;
        state_.queue[count].artist      = artistBuf;
        state_.queue[count].duration    = durBuf;
        state_.queue[count].trackNumber = count + 1;  // 1-based
        state_.queue[count].albumArtURL = "";
        state_.queue[count].album       = "";

        bool selected = renderer["selected"] | false;
        if (selected) playingIdx = count;

        count++;
    }

    state_.queueSize          = count;
    state_.totalTracks        = count;
    if (playingIdx >= 0)
        state_.currentTrackNumber = playingIdx + 1;  // 1-based

    Serial.printf("[YTMD] Queue: %d items, playing=%d\n", count, playingIdx);
    notifyUI(UPDATE_QUEUE);
}

// ─── UI notification ──────────────────────────────────────────────────────────

void SonosController::notifyUI(UIUpdateType_e type) {
    if (!uiUpdateQueue_) return;
    UIUpdate_t upd;
    upd.type = type;
    upd.message[0] = '\0';
    xQueueSend(uiUpdateQueue_, &upd, 0);
}

// ─── HTML / XML helpers (kept for lyrics.cpp compatibility) ──────────────────

String SonosController::decodeHTML(String text) {
    text.replace("&amp;",  "&");
    text.replace("&lt;",   "<");
    text.replace("&gt;",   ">");
    text.replace("&quot;", "\"");
    text.replace("&#39;",  "'");
    text.replace("&apos;", "'");
    text.replace("&nbsp;", " ");
    return text;
}

String SonosController::extractXML(const String& xml, const char* tag) {
    String open  = "<"; open  += tag; open  += ">";
    String close = "</"; close += tag; close += ">";
    int start = xml.indexOf(open);
    if (start < 0) {
        // Try with attributes: <tag attr="...">
        open = "<"; open += tag; open += " ";
        start = xml.indexOf(open);
        if (start < 0) return "";
        start = xml.indexOf('>', start);
        if (start < 0) return "";
        start++;
    } else {
        start += open.length();
    }
    int end = xml.indexOf(close, start);
    if (end < 0) return "";
    return xml.substring(start, end);
}

String SonosController::extractXMLRange(const String& xml, const char* tag,
                                        int rangeStart, int rangeEnd) {
    String open  = "<"; open  += tag; open  += ">";
    String close = "</"; close += tag; close += ">";
    int pos = 0;
    int found = 0;
    while (true) {
        int start = xml.indexOf(open, pos);
        if (start < 0) return "";
        int end = xml.indexOf(close, start + open.length());
        if (end < 0) return "";
        if (found >= rangeStart && found <= rangeEnd) {
            return xml.substring(start + open.length(), end);
        }
        found++;
        pos = end + close.length();
        if (found > rangeEnd) return "";
    }
}
