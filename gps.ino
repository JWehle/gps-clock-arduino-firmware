// ==== Minimal GPS hh:mm:ss + PPS (ATmega328PB) ===================
// Wire MAX-M10S TX -> your UART RX (e.g., Serial1). PPS -> PD3 (D3).
// API:
//   void  GPS_PPS_begin(Stream& gps, uint8_t ppsPin);
//   bool  GPS_PPS_poll(Stream& gps, uint8_t& hh, uint8_t& mm, uint8_t& ss, bool& valid);
//
// Call GPS_PPS_begin() in setup(), then call GPS_PPS_poll() rapidly in loop().
// It returns true exactly once per second (on PPS), giving PPS-aligned time.
// =================================================================

#include <Arduino.h>

static volatile uint32_t s_ppsCount = 0;
static void ppsISR() { s_ppsCount++; }

// last PPS processed
static uint32_t s_ppsHandled = 0;

// latest fix from NMEA (waiting to apply on next PPS)
static bool     s_haveFix = false;
static uint8_t  s_fix_h = 0, s_fix_m = 0, s_fix_s = 0;

// running time (UTC) we maintain
static bool     s_synced = false;
static uint8_t  s_h = 0, s_m = 0, s_s = 0;

// tiny line buffer for NMEA
static char     s_buf[96];
static uint8_t  s_len = 0;

// --- simple helpers ---
static void inc_one_second(uint8_t& h, uint8_t& m, uint8_t& s) {
  if (++s >= 60) { s = 0; if (++m >= 60) { m = 0; if (++h >= 24) h = 0; } }
}

// parse $..RMC lines: field1=hhmmss.sss, field2=status (A/V).
// returns true and fills h/m/s when status is 'A' and time present.
static bool parse_RMC_hms(const char* line, uint8_t& h, uint8_t& m, uint8_t& s) {
  if (line[0] != '$') return false;
  // must be RMC type
  if (!(line[3]=='R' && line[4]=='M' && line[5]=='C')) return false;

  // walk comma-separated fields
  int field = -1; // -1 until after type
  const char* p = line;
  while (*p && *p != '*') {
    // move to next field start
    const char* start = p;
    while (*p && *p != ',' && *p != '*') p++;
    int len = p - start;

    if (field == 0) {
      // time: hhmmss[.sss]
      if (len >= 6) {
        auto d2 = [&](const char* s){ return uint8_t((s[0]-'0')*10 + (s[1]-'0')); };
        h = d2(start + 0);
        m = d2(start + 2);
        s = d2(start + 4);
      } else return false;
    } else if (field == 1) {
      // status: 'A' valid, 'V' void
      if (len < 1 || start[0] != 'A') return false;
      // we already filled h/m/s
      return true;
    }

    if (*p == ',') { p++; field++; }
    else if (*p == '*') break;
    else break;
    if (field > 1) break; // we only need first two fields
  }
  return false;
}

// gobble bytes from GPS, capture lines, remember last valid hh:mm:ss
static void ingest(Stream& gps) {
  while (gps.available()) {
    char c = (char)gps.read();
    if (c == '\r') continue;
    if (c == '\n') {
      s_buf[s_len] = '\0';
      if (s_len >= 10) {
        uint8_t h,m,s;
        if (parse_RMC_hms(s_buf, h,m,s)) {
          s_fix_h = h; s_fix_m = m; s_fix_s = s;
          s_haveFix = true; // apply on next PPS
        }
      }
      s_len = 0;
    } else if (s_len < sizeof(s_buf)-1) {
      s_buf[s_len++] = c;
    } else {
      s_len = 0; // overflow; reset
    }
  }
}

// ---- Public API ----
void GPS_PPS_begin(Stream& gps, uint8_t ppsPin /*use 3 for PD3*/) {
  (void)gps;
  pinMode(ppsPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(ppsPin), ppsISR, RISING);
  s_ppsHandled = s_ppsCount;
  s_synced = false;
  s_haveFix = false;
}

// Call fast in loop(). When it returns true, (hh:mm:ss) is updated on the PPS edge.
// 'valid' becomes true after first sync; remains true once we’ve had at least one fix+pps.
bool GPS_PPS_poll(Stream& gps, uint8_t& hh, uint8_t& mm, uint8_t& ss, bool& valid) {
  ingest(gps);

  uint32_t ppsNow = s_ppsCount;
  if (ppsNow == s_ppsHandled) { valid = s_synced; return false; }

  // new PPS edge:
  s_ppsHandled = ppsNow;

  if (s_haveFix) {
    // snap to GPS time + 1 second at PPS
    s_h = s_fix_h; s_m = s_fix_m; s_s = s_fix_s;
    inc_one_second(s_h, s_m, s_s);
    s_haveFix = false;
    s_synced = true;
  } else if (s_synced) {
    // free-run: step one second
    inc_one_second(s_h, s_m, s_s);
  } else {
    // not synced yet and no fix this second → do nothing
    valid = false;
    return false;
  }

  hh = s_h; mm = s_m; ss = s_s; valid = s_synced;
  return true;
}