#pragma once
/**
 * ui_network_guard.h — Centralised SDIO crash-defence pre-download guard.
 *
 * sdioPreWait() consolidates all SDIO cooldown boilerplate that previously
 * lived duplicated across ui_album_art.cpp, lyrics.cpp, sonos_controller.cpp,
 * and ui_clock_screen.cpp.
 *
 * Call OUTSIDE the network mutex, BEFORE acquiring it.
 * Inside-mutex re-checks remain inline in each caller (one-shot, correct).
 *
 * Always enforces:
 *   1. SDIO_GENERAL_COOLDOWN_MS  — min gap since last network op (any protocol)
 *
 * Optional checks (select via flags):
 *   SDIO_WAIT_TRACK_CHANGE   — track-change settle (SDIO_TRACK_CHANGE_SETTLE_MS, currently 0)
 *   SDIO_WAIT_STORM_GATE     — HTTP-500 storm while-loop (SDIO_STORM_COOLDOWN_MS)
 *   SDIO_WAIT_QUEUE_POLL     — updateQueue() residue (SDIO_QUEUE_POLL_COOLDOWN_MS)
 *   SDIO_WAIT_HTTPS_COOLDOWN — TLS teardown residue after last HTTPS session
 *                              (SDIO_HTTPS_COOLDOWN_MS = 3s).
 *                              Use only when the caller itself does HTTPS (lyrics, HTTPS art,
 *                              clock weather). Do NOT use for plain HTTP callers (sendSOAP,
 *                              local Sonos art) — it created 3s SDIO idle → pkt_rxbuff :928.
 *
 * Returns: false if abort_flag1 or abort_flag2 became true during any wait.
 *          true  if all cooldowns passed without abort.
 *
 * Callers with no abort flags can ignore the return value —
 * with nullptr flags it always returns true.
 */

#include <stdint.h>

// Flags for optional check groups
#define SDIO_WAIT_TRACK_CHANGE   0x01u   // Gate on last_track_change_ms
#define SDIO_WAIT_STORM_GATE     0x02u   // Gate on last_transient_500_ms (while-loop)
#define SDIO_WAIT_QUEUE_POLL     0x04u   // Gate on last_queue_fetch_time
#define SDIO_WAIT_HTTPS_COOLDOWN 0x08u   // Gate on last_https_end_ms (3s TLS teardown wait)

// Art task: all four guards. Storm gate waits for HLS transition to clear — Sonos
// WiFi broadcast traffic during transitions + simultaneous large HTTP download
// exhausts C6 pkt_rxbuff. WiFi.setSleep(false) makes the 3s wait safe (no power-save).
#define SDIO_WAIT_ART  (SDIO_WAIT_TRACK_CHANGE | SDIO_WAIT_STORM_GATE | \
                        SDIO_WAIT_QUEUE_POLL | SDIO_WAIT_HTTPS_COOLDOWN)

bool sdioPreWait(const char*    tag,
                 uint32_t       flags       = 0,
                 volatile bool* abort_flag1 = nullptr,
                 volatile bool* abort_flag2 = nullptr);
