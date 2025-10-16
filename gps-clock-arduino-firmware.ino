// ATmega328PB: has Serial and Serial1
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);           // or 38400/115200 if you reconfigure MAX-M10S
  //GPS_PPS_begin(Serial1, 3);     // PPS on PD3 (Arduino D3)
}

void loop() {
  uint8_t h,m,s; bool ok;
  if (GPS_PPS_poll(Serial1, h,m,s, ok)) {
    if (ok) {
      Serial.print("UTC "); 
      if (h<10) Serial.print('0'); Serial.print(h); Serial.print(':');
      if (m<10) Serial.print('0'); Serial.print(m); Serial.print(':');
      if (s<10) Serial.print('0'); Serial.println(s);
    }
  }
}