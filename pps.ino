// ==== Minimal GPS hh:mm:ss + PPS + LED Pulse ====================
// PPS input : PD3 (Arduino D3, INT1)
// Status LED: PD7 (Arduino D7) -> blinks ON for 50 ms at each PPS edge
// =================================================================

#include <Arduino.h>

static volatile uint32_t s_ppsCount = 0;
static volatile uint32_t s_lastPpsMs = 0;  // when last PPS happened (ms)

// ISR: increment PPS counter + LED ON
static void ppsISR() {
  s_ppsCount++;
  digitalWrite(7, HIGH);      // turn LED on
  s_lastPpsMs = millis();     // mark time
}

// last PPS processed
static uint32_t s_ppsHandled = 0;

// latest fix waiting to apply
static bool     s_haveFix = false;
static uint8_t  s_fix_h=0, s_fix_m=0, s_fix_s=0;

// running time (UTC)
static bool     s_synced=false;
static uint8_t  s_h=0, s_m=0, s_s=0;

// NMEA line buffer
static char     s_buf[96];
static uint8_t  s_len=0;

// helper: increment hh:mm:ss
static void inc_one_second(uint8_t& h, uint8_t& m, uint8_t& s) {
  if (++s >= 60) { s=0; if (++m>=60) { m=0; if (++h>=24) h=0; } }
}

// parse_RMC_hms and ingest() same as before…

// ---- Public API ----
void GPS_PPS_begin(Stream& gps, uint8_t ppsPin /*3 for PD3*/) {
  (void)gps;
  pinMode(ppsPin, INPUT);
  pinMode(7, OUTPUT);           // PD7 LED
  digitalWrite(7, LOW);

  attachInterrupt(digitalPinToInterrupt(ppsPin), ppsISR, RISING);

  s_ppsHandled = s_ppsCount;
  s_synced = false;
  s_haveFix = false;
}

// Call fast in loop(). When it returns true, (hh:mm:ss) updated on PPS edge.
// Also manages LED OFF after 50 ms.
bool GPS_PPS_poll(Stream& gps, uint8_t& hh, uint8_t& mm, uint8_t& ss, bool& valid) {
  ingest(gps);

  // handle LED pulse timing
  if (digitalRead(7) == HIGH && (millis() - s_lastPpsMs) >= 50) {
    digitalWrite(7, LOW);
  }

  uint32_t ppsNow = s_ppsCount;
  if (ppsNow == s_ppsHandled) { valid = s_synced; return false; }

  s_ppsHandled = ppsNow;

  if (s_haveFix) {
    s_h = s_fix_h; s_m = s_fix_m; s_s = s_fix_s;
    inc_one_second(s_h, s_m, s_s);
    s_haveFix = false;
    s_synced = true;
  } else if (s_synced) {
    inc_one_second(s_h, s_m, s_s);
  } else {
    valid = false;
    return false;
  }

  hh = s_h; mm = s_m; ss = s_s; valid = s_synced;
  return true;
}