#include <Wire.h>

// ======== CONFIGURE ME ========
static const bool ACTIVE_HIGH = true;     // false for common-anode (active-low)
static const uint8_t I2C_ADDR[3] = { 0x20, 0x21, 0x22 }; // your CAT9555 addresses
// Which bits (0..6) on each port correspond to segments a..g (in that order)
static const uint8_t SEG_BIT_FOR[7] = { 0,1,2,3,4,5,6 }; // identity: a->b0, b->b1, ... g->b6
// ==============================

// 7-seg digit encodings for segments a..g (bit0..bit6) active=1
//           gfedcba (but stored as bit0=a ... bit6=g)
static const uint8_t DIGIT_7SEG[10] = {
  0x3F, // 0 = abcdef
  0x06, // 1 = bc
  0x5B, // 2 = abdeg
  0x4F, // 3 = abcdg
  0x66, // 4 = bcfg
  0x6D, // 5 = acdfg
  0x7D, // 6 = acdefg
  0x07, // 7 = abc
  0x7F, // 8 = abcdefg
  0x6F  // 9 = abcdfg
};

// CAT9555 register addresses
enum : uint8_t {
  REG_INPUT_0  = 0x00,
  REG_INPUT_1  = 0x01,
  REG_OUTPUT_0 = 0x02,
  REG_OUTPUT_1 = 0x03,
  REG_POL_0    = 0x04,
  REG_POL_1    = 0x05,
  REG_CFG_0    = 0x06,
  REG_CFG_1    = 0x07
};

// --- helpers ---
static uint8_t remapSegments(uint8_t segMask07) {
  // Take bits (a..g on bit0..6) and place each onto the wired bit positions.
  uint8_t out = 0;
  for (uint8_t i = 0; i < 7; ++i) {
    if (segMask07 & (1u << i)) {
      out |= (1u << SEG_BIT_FOR[i]);
    }
  }
  return out;
}

static void cat9555_write2(uint8_t addr, uint8_t reg0, uint8_t val0, uint8_t reg1, uint8_t val1) {
  Wire.beginTransmission(addr);
  Wire.write(reg0);
  Wire.write(val0);
  Wire.endTransmission();

  Wire.beginTransmission(addr);
  Wire.write(reg1);
  Wire.write(val1);
  Wire.endTransmission();
}

static void cat9555_init(uint8_t addr) {
  // All pins OUTPUT (0) on both ports
  cat9555_write2(addr, REG_CFG_0, 0x00, REG_CFG_1, 0x00);
  // No polarity inversion (we’ll handle inversion in software)
  cat9555_write2(addr, REG_POL_0, 0x00, REG_POL_1, 0x00);
  // Start OFF
  uint8_t off = ACTIVE_HIGH ? 0x00 : 0xFF;
  cat9555_write2(addr, REG_OUTPUT_0, off, REG_OUTPUT_1, off);
}

// --- public init: call once in setup() after Wire.begin() ---
void SevenSeg_begin() {
  for (uint8_t i = 0; i < 3; ++i) cat9555_init(I2C_ADDR[i]);
}

// Encodes a single 0..9 digit into a 7-bit mask with mapping + polarity
static uint8_t encodeDigitByte(uint8_t d) {
  if (d > 9) d = 0;
  uint8_t seg = DIGIT_7SEG[d];           // bit0..6 = a..g, active-high
  seg = remapSegments(seg);              // rearrange to wired bit positions
  seg &= 0x7F;                           // only 7 bits used
  if (!ACTIVE_HIGH) seg = (~seg) & 0x7F; // invert for active-low hardware
  // place into 8-bit port value: bit7 = unused (keep OFF)
  uint8_t portVal = seg | (ACTIVE_HIGH ? 0x00 : 0x80); // for active-low, OFF = 1 on unused bit
  return portVal;
}

// --- main API: write HH:MM:SS to the 6 digits ---
// Digit order (left→right):
//   chip0: Port0 = H tens, Port1 = H ones
//   chip1: Port0 = M tens, Port1 = M ones
//   chip2: Port0 = S tens, Port1 = S ones
void SevenSeg_displayTime(uint8_t hour, uint8_t minute, uint8_t second) {
  // clamp/normalize defensively
  if (hour >= 24)   hour   %= 24;
  if (minute >= 60) minute %= 60;
  if (second >= 60) second %= 60;

  uint8_t d[6];
  d[0] = hour   / 10;
  d[1] = hour   % 10;
  d[2] = minute / 10;
  d[3] = minute % 10;
  d[4] = second / 10;
  d[5] = second % 10;

  // Encode per digit to port bytes
  uint8_t p0, p1;

  // Chip 0: HH
  p0 = encodeDigitByte(d[0]);
  p1 = encodeDigitByte(d[1]);
  cat9555_write2(I2C_ADDR[0], REG_OUTPUT_0, p0, REG_OUTPUT_1, p1);

  // Chip 1: MM
  p0 = encodeDigitByte(d[2]);
  p1 = encodeDigitByte(d[3]);
  cat9555_write2(I2C_ADDR[1], REG_OUTPUT_0, p0, REG_OUTPUT_1, p1);

  // Chip 2: SS
  p0 = encodeDigitByte(d[4]);
  p1 = encodeDigitByte(d[5]);
  cat9555_write2(I2C_ADDR[2], REG_OUTPUT_0, p0, REG_OUTPUT_1, p1);
}