#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define ADC_MAX 1023
#define ENV_MAX 4095
#define ENV_MIN 0
#define ATTACK_THRESHOLD 4000

#define MUX_CHANNELS 8
#define MUX_SELECT_1_PIN 11  // GP11 = PIN 15
#define MUX_SELECT_2_PIN 12  // GP12 = PIN 16
#define MUX_SELECT_3_PIN 13  // GP13 = PIN 17
#define TRIGGER_PIN_1 14     // GP15 = PIN 19
#define GATE_PIN_1 16        // GP16 = PIN 21
#define LED_PIN 25           // GP25 = LED

enum testMuxChannels { DELAY1, ATTACK1, DECAY1, SUSTAIN1, RELEASE1, MEOW1, MEOW2, MEOW3 };

Adafruit_MCP4725 dac1;
bool isGate1 = false,
  isTrigger1 = false,
  lastTrigger1 = false,
  envelopeIsActive1 = false,
  isDecayState1 = false,
  isRetrigger1 = false;
int muxValues1[MUX_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0 },
  targetValue1 = 0,
  muxCounter = 0,
  attackCV = 0,
  decayCV = 0,
  sustainCV = 0,
  releaseCV = 0,
  debugCount = 0;
float currentPole1 = 0.7,  // INITIALIZE POLE LOCATIONS
  currentEnvelopeValue1 = 0.0;
unsigned long delay1StartTime = 0;
double attackPole1 = 0.9,  // INITIALIZE ATACK, DECAY AND RELEASE POLES
  decayPole1 = 0.9,
  releasePole1 = 0.95;
double calcPole(int poleValue) {
  // return sqrt(0.999 * cos( double(ADC_MAX - poleValue) / 795.0));
  return sqrt(0.999 * cos(double(ADC_MAX - poleValue) / 4000.0));  // this seems to work better.
}
float delta(double targetPole, int targetValue1, float currentValue) {
  return (1.0 - targetPole) * targetValue1 + targetPole * currentValue;
}
void setMuxSelectPins() {
  digitalWrite(MUX_SELECT_1_PIN, bitRead(muxCounter, 0));
  digitalWrite(MUX_SELECT_2_PIN, bitRead(muxCounter, 1));
  digitalWrite(MUX_SELECT_3_PIN, bitRead(muxCounter, 2));
}
void setEnvPoles1() {
  switch (muxCounter) {
    case ATTACK1:
      attackPole1 = calcPole(muxValues1[ATTACK1]);
      break;
    case DECAY1:
      decayPole1 = calcPole(muxValues1[DECAY1]);
      break;
    case RELEASE1:
      releasePole1 = calcPole(muxValues1[RELEASE1]);
      break;
  }
}
void handleMuxes() {
  setMuxSelectPins();
  muxValues1[muxCounter] = ADC_MAX - analogRead(A0);
  setEnvPoles1();
  muxCounter++;
  if (muxCounter >= MUX_CHANNELS) muxCounter = 0;
}
void handleGatesAndTriggers() {
  isGate1 = digitalRead(GATE_PIN_1);
  isTrigger1 = !digitalRead(TRIGGER_PIN_1) && !lastTrigger1;
  isRetrigger1 = isGate1 && isTrigger1;
  if (!isRetrigger1 && !isGate1 && isTrigger1) isGate1 = true;
}
bool handleDelay1() {
  if (muxValues1[DELAY1] > 10) {
    if (delay1StartTime == 0) {
      delay1StartTime = millis();
      return true;
    } else if( millis() < delay1StartTime + (unsigned long) ( muxValues1[DELAY1] * 2 ) ) return true; // UP TO 2 SECONDS OF DELAY!
  }
  return false;
}
void updateEnvelope1() {
  if (isGate1) {  // IF GATE IS HAPPENING...
    if (!handleDelay1()) {  // IF WE'RE NOT DOING A DELAY...
      if (!envelopeIsActive1) { // IF envelopeIsActive IS FALSE, WE'VE DETECTED THE START OF A NEW ENVELOPE!
        isDecayState1 = false;
        targetValue1 = ENV_MAX;
        currentPole1 = attackPole1;
        envelopeIsActive1 = true;
      }
      if (isRetrigger1) {  // IF WE'RE RETRIGGERING...
        envelopeIsActive1 = true;
        isDecayState1 = false;
        targetValue1 = ENV_MAX;
        currentPole1 = attackPole1;
        // ELSE IF WE ARE AT THE END OF THE ATTACK PHASE... (AKA IF NOT DECAY AND AT ATTACK THRESHOLD YET AND TARGET VALUE IS ENV_MAX)
      } else if (!isDecayState1 && (currentEnvelopeValue1 > ATTACK_THRESHOLD) && (targetValue1 == ENV_MAX)) {
        isDecayState1 = true;
        targetValue1 = muxValues1[SUSTAIN1] * 4;
        currentPole1 = decayPole1;
      }
    }
  } else {  // ELSE THE GATE IS NOT ON, WE ARE IN THE RELEASE PHASE
    targetValue1 = ENV_MIN;
    currentPole1 = releasePole1;
    envelopeIsActive1 = false;
    isDecayState1 = true;
    delay1StartTime = 0;
  }
  currentEnvelopeValue1 = delta(currentPole1, targetValue1, currentEnvelopeValue1);
  dac1.setVoltage(round(currentEnvelopeValue1), false);
  lastTrigger1 = isTrigger1;
}
void setup() {
  Serial.begin(9600);
  analogReadResolution(10);  // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  dac1.begin(0x62);
  pinMode(GATE_PIN_1, INPUT_PULLDOWN);
  pinMode(TRIGGER_PIN_1, INPUT_PULLDOWN);
  pinMode(MUX_SELECT_1_PIN, OUTPUT);
  pinMode(MUX_SELECT_2_PIN, OUTPUT);
  pinMode(MUX_SELECT_3_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
}
void loop() {
  handleMuxes();
  handleGatesAndTriggers();
  updateEnvelope1();
  debugCount++;
  if( debugCount >= 200 ){    
    Serial.print( "0.0 1.0 " );
    Serial.print(currentEnvelopeValue1/ENV_MAX);
    Serial.print(" ");
    Serial.println( isGate1? "0.8" : "0.2" );
    debugCount = 0;
  }
}