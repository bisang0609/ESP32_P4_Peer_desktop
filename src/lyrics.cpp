/**
 * Lyrics — stub (removed; YTMD does not require lyrics fetching)
 * All public functions are no-ops so existing call sites compile unchanged.
 */

#include "lyrics.h"
#include "ui_common.h"

int lyric_count = 0;
volatile bool lyrics_ready    = false;
volatile bool lyrics_fetching = false;
volatile bool lyrics_abort_requested = false;
int current_lyric_index = -1;

void initLyrics()                                                   {}
bool requestLyrics(const String&, const String&, int) { return false; }
void clearLyrics()                                                  {}
void createLyricsOverlay(lv_obj_t*)                                 {}
void updateLyricsDisplay(int)                                       {}
void setLyricsVisible(bool)                                         {}
void updateLyricsStatus()                                           {}
