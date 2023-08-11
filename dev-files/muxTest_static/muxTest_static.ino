#define MUX_SELECT_1_PIN 11  // GP11 = PIN 15
#define MUX_SELECT_2_PIN 12  // GP12 = PIN 16
#define MUX_SELECT_3_PIN 13  // GP13 = PIN 17

int muxCounter = 0; // SET THIS IN RANGE OF [0...7] TO SELECT WHICH KNOB TO READ VALUES FROM
void setup() {
  Serial.begin(9600);
  analogReadResolution(10);  // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  pinMode(MUX_SELECT_1_PIN, OUTPUT);
  pinMode(MUX_SELECT_2_PIN, OUTPUT);
  pinMode(MUX_SELECT_3_PIN, OUTPUT);
  digitalWrite(MUX_SELECT_1_PIN, bitRead(muxCounter, 0));
  digitalWrite(MUX_SELECT_2_PIN, bitRead(muxCounter, 1));
  digitalWrite(MUX_SELECT_3_PIN, bitRead(muxCounter, 2));
}
void loop() {
  Serial.println(analogRead(A0));
  delay(250);
}