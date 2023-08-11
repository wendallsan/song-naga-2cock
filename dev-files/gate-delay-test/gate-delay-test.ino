#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define MUX_CHANNELS 8
#define MUX_SELECT_1_PIN 11 // GP11 = PIN 15
#define MUX_SELECT_2_PIN 12 // GP12 = PIN 16
#define MUX_SELECT_3_PIN 13 // GP13 = PIN 17
#define GATE_PIN_1 16 // GP17 = PIN 22
#define LED_PIN 25 // GP25 = LED
enum testMuxChannels { DELAY2, ATTACK1, DECAY1, SUSTAIN1, RELEASE1, DELAY2_CV_ADJUST, ATTACK1_CV_ADJUST, DECAY1_CV_ADJUST };
bool isGate1 = false,
     envelopeIsActive1 = false,
     isDelayState2 = false;
int muxValues1[MUX_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0 },
  muxCounter = 0;
unsigned long delay2StartTime = 0;
void setMuxSelectPins() {
  digitalWrite(MUX_SELECT_1_PIN, bitRead(muxCounter, 0));
  digitalWrite(MUX_SELECT_2_PIN, bitRead(muxCounter, 1));
  digitalWrite(MUX_SELECT_3_PIN, bitRead(muxCounter, 2));
}
void handleMuxes() {
  setMuxSelectPins();
  muxValues1[muxCounter] = analogRead(A0);
  muxCounter++;
  if (muxCounter >= MUX_CHANNELS) muxCounter = 0;
}
void handleEnvelope1() {
  if( isGate1 ){
    
    
    if( muxValues1[ DELAY2 ] > 10 ){
      if( delay2StartTime == 0 ) {
        delay2StartTime = millis();
        isDelayState2 = true;
        return;
      } else if( millis() < delay2StartTime + (unsigned long) muxValues1[ DELAY2 ] ) {
        return;
      }
      else { // ELSE THE DELAY STATE IS OVER
        isDelayState2 = false;
        envelopeIsActive1 = true;
      }
    } else {
      envelopeIsActive1 = true;
      isDelayState2 = false;
    }

    
  } else {
    envelopeIsActive1 = false;
    isDelayState2 = false;
    delay2StartTime = 0;    
  }
}
void setup() {
  Serial.begin(9600);
  analogReadResolution(10);  // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  pinMode(GATE_PIN_1, INPUT_PULLDOWN);
  pinMode(MUX_SELECT_1_PIN, OUTPUT);
  pinMode(MUX_SELECT_2_PIN, OUTPUT);
  pinMode(MUX_SELECT_3_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
}
int debugCounter = 0;
void loop() {
  handleMuxes();
  isGate1 = digitalRead(GATE_PIN_1);
  handleEnvelope1();
  digitalWrite(LED_PIN, envelopeIsActive1);
  debugCounter++;
  if( debugCounter >= 250 ){
    Serial.print( "delayState: " );
    Serial.print( isDelayState2? "X" : "O" );
    Serial.print( ", envelopeState: " );
    Serial.println( envelopeIsActive1? "X" : "O" );
    debugCounter = 0;
  }
  delay(1);
}