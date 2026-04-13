/**
 * YouTube Music Desktop (YTMD) Controller
 * Replaces SonosController — keeps the same public API so existing UI code compiles unchanged.
 *
 * Target: 192.168.0.30:26538  (hardcoded; settings UI to be added later)
 *
 * REST endpoints used:
 *   GET  /api/v1/song          — current song / playback state (poll every 3 s)
 *   GET  /api/v1/queue         — queue / playlist              (poll on demand)
 *   POST /api/v1/previous      — previous track
 *   POST /api/v1/toggle-play   — play / pause
 *   POST /api/v1/next          — next track
 *   PATCH /api/v1/queue        — body {"index":N}  — play queue item at index N
 */

#ifndef YTMD_CONTROLLER_H
#define YTMD_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// ─── Queue capacity (kept for UI compatibility) ─────────────────────────────
#define MAX_SONOS_DEVICES   1   // Only one YTMD "device"
#define QUEUE_ITEMS_MAX     50  // Max queue items shown

// ─── UI update notifications (same enum as before, same consumers) ───────────
typedef enum {
    UPDATE_TRACK_INFO,
    UPDATE_PLAYBACK_STATE,
    UPDATE_VOLUME,       // unused with YTMD
    UPDATE_TRANSPORT,    // unused with YTMD
    UPDATE_QUEUE,
    UPDATE_ALBUM_ART,
    UPDATE_ERROR,
    UPDATE_GROUPS        // unused with YTMD
} UIUpdateType_e;

typedef struct {
    UIUpdateType_e type;
    char message[128];
} UIUpdate_t;

// ─── Queue item (same field names as original) ───────────────────────────────
struct QueueItem {
    String title;
    String artist;
    String album;
    String duration;
    int    trackNumber;  // 1-based position in the queue
    String albumArtURL;
};

// ─── Main playback state (same field names as original SonosDevice) ──────────
struct SonosDevice {
    // Identity
    String name      = "YouTube Music";
    String roomName  = "YouTube Music";
    String rinconID  = "";

    // Connection
    bool     connected      = false;
    uint32_t lastUpdateTime = 0;
    uint32_t errorCount     = 0;

    // Playback
    bool   isPlaying  = false;
    int    volume     = 50;   // not controllable via YTMD, kept for UI compatibility
    bool   isMuted    = false;
    bool   shuffleMode = false;
    String repeatMode = "NONE";

    // Track info
    String currentTrack;
    String currentArtist;
    String currentAlbum;
    String albumArtURL;
    String relTime         = "0:00";
    String trackDuration   = "0:00";
    int    relTimeSeconds  = 0;
    int    durationSeconds = 0;

    // Pseudo-URI used for change detection (title + artist composite)
    String currentURI;

    // Source type flags — always false for YTMD; kept so UI code compiles
    bool   isRadioStation  = false;
    bool   isLineIn        = false;
    bool   isTvAudio       = false;
    String radioStationName;
    String radioStationArtURL;
    String streamContent;

    // Like/Dislike status from YTMD ("LIKE", "DISLIKE", "INDIFFERENT")
    String likeStatus = "INDIFFERENT";

    // Next track info (from GET /api/v1/queue/next)
    String nextTrackTitle;
    String nextTrackArtist;
    bool   nextTrackAvailable = false;  // true once first poll completes

    // Queue
    int       currentTrackNumber = 0;  // 0-based index in queue
    int       totalTracks        = 0;
    QueueItem queue[QUEUE_ITEMS_MAX];
    int       queueSize          = 0;

    // Group info (YTMD = single standalone device)
    String groupCoordinatorUUID;
    bool   isGroupCoordinator = true;   // Shows in device list first pass
    int    groupMemberCount   = 1;

    // Compat field (not used)
    IPAddress ip;
};

// ─── SonosController wrapping YTMD REST client ───────────────────────────────
class SonosController {
public:
    SonosController();
    ~SonosController();

    // Lifecycle
    void begin();
    void startTasks();

    // Device access (YTMD has exactly one "device")
    int          getDeviceCount()       { return 1; }
    SonosDevice* getDevice(int index)   { return (index == 0) ? &state_ : nullptr; }
    SonosDevice* getCurrentDevice()     { return &state_; }
    void         selectDevice(int)      {}           // no-op
    bool         tryLoadCachedDevice()  { return true; }
    void         cacheSelectedDevice()  {}           // no-op
    String       getCachedDeviceIP()    { return YTMD_HOST; }
    void         cacheDeviceIP(String)  {}           // no-op

    // Playback commands (non-blocking)
    void play();
    void pause();
    void next();
    void previous();
    void seek(int seconds);
    void setShuffle(bool)           {}  // no-op
    void setRepeat(const char*)     {}  // no-op
    void like();
    void dislike();
    void playQueueItem(int index);      // 0-based index via PATCH /api/v1/queue
    void requestQueueUpdate();

    // Volume
    void setVolume(int vol);
    void volumeUp(int = 5)         {}
    void volumeDown(int = 5)       {}
    void setMute(bool muted);
    int  getVolume()               { return state_.volume; }
    bool getMute()                 { return state_.isMuted; }

    // Group management stubs
    void updateGroupInfo()         {}
    bool joinGroup(int, int)       { return false; }
    bool leaveGroup(int)           { return false; }
    int  getGroupMemberCount(int)  { return 1; }
    bool isDeviceInGroup(int, int) { return false; }

    // Content browsing stubs (Sonos-specific, unused with YTMD)
    String browseContent(const char*, int = 0, int = 100)     { return ""; }
    bool   playURI(const char*, const char* = "")             { return false; }
    bool   playPlaylist(const char*, const char* = "Playlist"){ return false; }
    bool   playContainer(const char*, const char* = "")       { return false; }
    String listMusicServices()                                 { return ""; }
    String getCurrentTrackInfo()                               { return ""; }
    bool   saveCurrentTrack(const char* = "Favorites")        { return false; }
    int    discoverDevices()                                   { return 0; }

    // XML / HTML helpers (used by lyrics.cpp)
    String extractXML(const String& xml, const char* tag);
    String extractXMLRange(const String& xml, const char* tag, int rangeStart, int rangeEnd);
    String decodeHTML(String text);

    // State queries (no-ops — polling task updates state_ directly)
    bool updateTrackInfo()         { return true; }
    bool updateMediaInfo()         { return true; }
    bool updatePlaybackState()     { return true; }
    bool updateVolume()            { return true; }
    bool updateQueue(int = 0)      { return true; }
    bool updateTransportSettings() { return true; }

    // FreeRTOS queue / task handles for UI update pipeline
    QueueHandle_t getUIUpdateQueue()   { return uiUpdateQueue_; }
    QueueHandle_t getCommandQueue()    { return nullptr; }
    TaskHandle_t  getNetworkTaskHandle() { return nullptr; }
    TaskHandle_t  getPollingTaskHandle() { return pollTaskHandle_; }

    // Error helpers
    void handleNetworkError(const char*)  {}
    void resetErrorCount()                { state_.errorCount = 0; }

    // Task management (for DMA recovery in main.cpp)
    void suspendTasks();
    void resumeTasks();

    // YTMD server address (hardcoded for now; settings UI to be added later)
    static constexpr const char* YTMD_HOST = "192.168.0.30";
    static constexpr const char* YTMD_PORT = "26538";

private:
    SonosDevice   state_;
    QueueHandle_t uiUpdateQueue_  = nullptr;
    TaskHandle_t  pollTaskHandle_ = nullptr;

    StaticTask_t  pollTaskTCB_;
    StackType_t*  pollTaskStack_  = nullptr;

    volatile bool shutdown_  = false;
    volatile bool queueRefreshPending_ = false;

    // FreeRTOS task
    static void pollTaskFn(void* param);
    void        runPollLoop();

    // REST helpers
    bool sendCommand(const char* path, const char* method, const char* body = nullptr);
    bool fetchJson(const char* path, String& out);

    // Parsers
    void parseSong(const String& json);
    void parseQueue(const String& json);
    bool fetchAndParseQueue();        // streams HTTP directly — avoids large String buffer
    bool fetchLikeState();            // GET /api/v1/like-state
    bool fetchNextTrack();            // GET /api/v1/queue/next
    bool fetchVolume();               // GET /api/v1/volume
    static uint32_t parseSeconds(const char* s);
    static void     extractThumbnailUrl(const String& json, char* dst, size_t dstSize);

    void notifyUI(UIUpdateType_e type);

    String buildUrl(const char* path);
};

// ─── Global instance (defined in ui_globals.cpp) ─────────────────────────────
extern SonosController sonos;

#endif // YTMD_CONTROLLER_H
