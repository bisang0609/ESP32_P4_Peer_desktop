#include "ui_network_guard.h"
#include "ui_common.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>

static inline bool _aborted(volatile bool* f1, volatile bool* f2) {
    return (f1 && *f1) || (f2 && *f2);
}

bool sdioPreWait(const char* tag, uint32_t flags,
                 volatile bool* abort_flag1, volatile bool* abort_flag2)
{
    // ── 1. Track-change settle (currently disabled — SDIO_TRACK_CHANGE_SETTLE_MS=0) ──
    // Was 2000ms to drain UPnP NOTIFY bursts, but Sonos lib is pure SOAP-polling:
    // no UPnP subscriptions, no WiFiServer, no NOTIFY events arrive at all.
    // The 2000ms idle was causing C6 WiFi power-save → pkt_rxbuff overflow.
    // Kept as a while-loop (not one-shot) so the window stays coherent if
    // SDIO_TRACK_CHANGE_SETTLE_MS is re-enabled: last_track_change_ms is written
    // by requestAlbumArt() on Core 1 a few ms after requestLyrics() spawns this
    // task, so a while-loop correctly extends the wait if the timestamp is updated.
    if (flags & SDIO_WAIT_TRACK_CHANGE) {
        bool logged = false;
        while (!_aborted(abort_flag1, abort_flag2)) {
            if (last_track_change_ms == 0) break;
            unsigned long elapsed = millis() - last_track_change_ms;
            if (elapsed >= SDIO_TRACK_CHANGE_SETTLE_MS) break;
            unsigned long remaining = SDIO_TRACK_CHANGE_SETTLE_MS - elapsed;
            if (!logged) {
                Serial.printf("[%s] Track-change settle: waiting %lums\n", tag, remaining);
                logged = true;
            }
            vTaskDelay(pdMS_TO_TICKS(remaining < 50 ? remaining : 50));
        }
    }
    if (_aborted(abort_flag1, abort_flag2)) return false;

    // ── 2. HTTP-500 storm while-loop ──────────────────────────────────────────
    // Sonos returns 500 during HLS source transitions.
    // While-loop re-evaluates each 50ms so new 500s extend the window.
    if (flags & SDIO_WAIT_STORM_GATE) {
        unsigned long storm_start = millis();
        bool logged = false;
        bool storm_was_active = false;
        while (true) {
            if (_aborted(abort_flag1, abort_flag2)) return false;
            if (last_transient_500_ms == 0) break;
            if (millis() - last_transient_500_ms >= SDIO_STORM_COOLDOWN_MS) break;
            if (millis() - storm_start > SDIO_STORM_SAFETY_CAP_MS) {
                Serial.printf("[%s] 500-storm safety cap — proceeding\n", tag);
                storm_was_active = true;
                break;
            }
            if (!logged) {
                Serial.printf("[%s] Sonos 500 storm: waiting for HLS transition...\n", tag);
                logged = true;
            }
            storm_was_active = true;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (_aborted(abort_flag1, abort_flag2)) return false;

        // Post-storm settle (currently disabled — SDIO_POST_STORM_SETTLE_MS=0).
        // Was 4000ms to wait for a 2nd UPnP NOTIFY wave, but Sonos library uses pure
        // SOAP-polling (no UPnP subscriptions, no WiFiServer, no NOTIFY events).
        // That 4000ms idle caused C6 WiFi power-save → pkt_rxbuff overflow at download start.
        if (storm_was_active && SDIO_POST_STORM_SETTLE_MS > 0) {
            Serial.printf("[%s] Post-storm settle: waiting %ums\n", tag, SDIO_POST_STORM_SETTLE_MS);
            vTaskDelay(pdMS_TO_TICKS(SDIO_POST_STORM_SETTLE_MS));
            if (_aborted(abort_flag1, abort_flag2)) return false;
        }
    }

    // ── 3. General cooldown ───────────────────────────────────────────────────
    // Minimum gap between any two network operations (HTTP or HTTPS).
    // Even local HTTP generates SDIO traffic that can combine with SOAP/NOTIFY.
    if (last_network_end_ms > 0) {
        unsigned long elapsed = millis() - last_network_end_ms;
        if (elapsed < SDIO_GENERAL_COOLDOWN_MS) {
            vTaskDelay(pdMS_TO_TICKS(SDIO_GENERAL_COOLDOWN_MS - elapsed));
        }
    }
    if (_aborted(abort_flag1, abort_flag2)) return false;

    // ── 4. HTTPS residue cooldown (opt-in: SDIO_WAIT_HTTPS_COOLDOWN) ─────────────
    // mbedTLS DMA buffers (~71KB) take ~3s to fully release after TLS session
    // teardown. Only needed when the caller itself does HTTPS — applying it
    // unconditionally to plain HTTP callers (sendSOAP, local Sonos getaa) caused
    // 3s of SDIO idle → P4 SDIO DMA clock-gated → pkt_rxbuff :928 overflow.
    if ((flags & SDIO_WAIT_HTTPS_COOLDOWN) && last_https_end_ms > 0) {
        unsigned long elapsed = millis() - last_https_end_ms;
        if (elapsed < SDIO_HTTPS_COOLDOWN_MS) {
            Serial.printf("[%s] HTTPS cooldown: waiting %lums\n",
                          tag, SDIO_HTTPS_COOLDOWN_MS - elapsed);
            vTaskDelay(pdMS_TO_TICKS(SDIO_HTTPS_COOLDOWN_MS - elapsed));
        }
    }
    if (_aborted(abort_flag1, abort_flag2)) return false;

    // ── 5. Queue poll residue cooldown ────────────────────────────────────────
    // updateQueue() returns ~20KB Browse response. Combined with a concurrent
    // download it exhausts the SDIO RX buffer pool → pkt_rxbuff :928 crash.
    if ((flags & SDIO_WAIT_QUEUE_POLL) && last_queue_fetch_time > 0) {
        unsigned long elapsed = millis() - last_queue_fetch_time;
        if (elapsed < SDIO_QUEUE_POLL_COOLDOWN_MS) {
            Serial.printf("[%s] Queue poll cooldown: waiting %lums\n",
                          tag, SDIO_QUEUE_POLL_COOLDOWN_MS - elapsed);
            vTaskDelay(pdMS_TO_TICKS(SDIO_QUEUE_POLL_COOLDOWN_MS - elapsed));
        }
    }
    if (_aborted(abort_flag1, abort_flag2)) return false;

    // ── 6. Inter-download cooldown ────────────────────────────────────────────
    // Back-to-back large HTTP downloads exhaust pkt_rxbuff: previous download's
    // TCP FIN-ACK + new download's SYN-ACK arrive simultaneously → overflow.
    // last_art_download_end_ms is set by art task AND clockBgTask after large downloads.
    //
    // HTTPS callers (SDIO_WAIT_HTTPS_COOLDOWN set) need SDIO_HTTPS_COOLDOWN_MS (3s):
    // TLS handshake generates a burst of 2-4 incoming TCP segments (ServerHello +
    // Certificate). If art's HTTP TCP residue hasn't fully drained, this burst
    // overflows pkt_rxbuff → :928. 3s matches the system-wide TLS drain standard.
    // HTTP callers (art task) need only SDIO_INTER_DOWNLOAD_MS (1s): HTTP SYN-ACK
    // is a single segment; 1s is sufficient for prior FIN-ACK to drain.
    if (last_art_download_end_ms > 0) {
        unsigned long threshold = (flags & SDIO_WAIT_HTTPS_COOLDOWN)
                                  ? SDIO_HTTPS_COOLDOWN_MS
                                  : SDIO_INTER_DOWNLOAD_MS;
        unsigned long elapsed = millis() - last_art_download_end_ms;
        if (elapsed < threshold) {
            Serial.printf("[%s] Inter-download cooldown: waiting %lums\n",
                          tag, threshold - elapsed);
            vTaskDelay(pdMS_TO_TICKS(threshold - elapsed));
        }
    }
    if (_aborted(abort_flag1, abort_flag2)) return false;

    // ── 7. Queue poll re-check ────────────────────────────────────────────────
    // The polling task runs freely during the inter-download wait above and may
    // fire updateQueue() at any point during that sleep.  Check #5 ran before
    // the wait and cannot see a queue fetch that occurs during it.  Re-check
    // here so we don't exit sdioPreWait with fresh 20KB SOAP residue in
    // pkt_rxbuff that will combine with the art download burst → :928 crash.
    if ((flags & SDIO_WAIT_QUEUE_POLL) && last_queue_fetch_time > 0) {
        unsigned long elapsed = millis() - last_queue_fetch_time;
        if (elapsed < SDIO_QUEUE_POLL_COOLDOWN_MS) {
            Serial.printf("[%s] Queue poll re-check: waiting %lums\n",
                          tag, SDIO_QUEUE_POLL_COOLDOWN_MS - elapsed);
            vTaskDelay(pdMS_TO_TICKS(SDIO_QUEUE_POLL_COOLDOWN_MS - elapsed));
        }
    }

    return !_aborted(abort_flag1, abort_flag2);
}
