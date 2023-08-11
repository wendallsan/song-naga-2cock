#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define ADC_MAX 1023
#define DAC_MAX 4095
#define ATTACK_THRESHOLD 4000
#define POLE_DIVISOR 795
#define MUX_CHANNELS 8

enum mainControlsMuxChannels {
  ADSR1_ATTACK,
  ADSR1_DECAY,
  ADSR1_SUSTAIN,
  ADSR1_RELEASE,
  ADSR2_ATTACK,
  ADSR2_DECAY,
  ADSR2_SUSTAIN,
  ADSR2_RELEASE
};


Adafruit_MCP4725 dac;

bool gateIsHigh = false,
     envelopeIsActive = false,
     isDecayState = false;
int muxValues[MUX_CHANNELS] = {0, 0, 0, 0, 0, 0, 0, 0},
  gatePin = 22,
  attackAnalogReadValue = ADC_MAX,  // MANUALLY SET THIS SINCE WE HAVE NO KNOB FOR IT CURRENTLY
  targetValue = 0,
  muxCounter = 0;
float currentPole = 0.7,       // INITIALIZE POLE LOCATION
  currentEnvelopeValue = 0.0;  // INITIALIZE THE ENVELOPE TO 0.0
double attackPole = 0.9,       // INITIALIZE ATACK, DECAY AND RELEASE POLES
  decayPole = 0.9,
       releasePole = 0.95;


int r0 = 0,  //value of select pin at the 4051 (s0)
  r1 = 0,    //value of select pin at the 4051 (s1)
  r2 = 0,    //value of select pin at the 4051 (s2)
  s0 = 2,
    s1 = 3,
    s2 = 4,
    count = 0;  //which y pin we are selecting





int getAttackAnalogReadValue() {
  // return analogRead(A1);
  return muxValues[ADSR1_ATTACK];
}
int getDecayAnalogReadValue() {
  return muxValues[ADSR1_DECAY];
}
int getSustainAnalogReadValue() {
  return muxValues[ADSR1_SUSTAIN];
}
int getReleaseAnalogReadValue() {
  return muxValues[ADSR1_RELEASE];
}
double calculatePole(int poleValue) {
  return sqrt(0.999 * cos((ADC_MAX - poleValue) / POLE_DIVISOR));
}
float differenceEquation(double targetPole, int targetValue, float currentValue) {
  return (1.0 - targetPole) * targetValue + targetPole * currentValue;
}
void handleMux() {
  r0 = bitRead(muxCount, 0);
  r1 = bitRead(muxCount, 1);
  r2 = bitRead(muxCount, 2);

  digitalWrite(s0, r0);
  digitalWrite(s1, r1);
  digitalWrite(s2, r2);
  muxValues[muxCount] = analogRead(A0);
  switch (muxCount) {
    case ADSR1_ATTACK:
      attackPole = calculatePole(getAttackAnalogReadValue());
      break;
    case ADSR1_DECAY:
      decayPole = calculatePole(getDecayAnalogReadValue());
      break; 
    // case ADSR1_SUSTAIN: 
      // NO ACTION IS NEEDED, AS WE READ STRAIGHT FROM THE STORED ARRAY VALUE FOR THIS.
    //  break;
    case ADSR1_RELEASE:
      releasePole = calculatePole(getReleaseAnalogReadValue());
      break;
  }
  muxCount++;
  if (muxCount >= MUX_CHANNELS) muxCount = 0;
}
void setup() {
  analogReadResolution(10);  // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  dac.begin(0x62);
  pinMode(gatePin, INPUT_PULLDOWN);  // SET UP THE GATE INPUT PIN
  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
}
void loop() {
  handleMux();
  gateIsHigh = digitalRead(gatePin);
  while (gateIsHigh) {
    if (!envelopeIsActive) {
      isDecayState = false;
      targetValue = DAC_MAX;
      currentPole = attackPole;
      envelopeIsActive = true;
    }
    if (!isDecayState && (currentEnvelopeValue > ATTACK_THRESHOLD) && (targetValue == DAC_MAX)) {
      isDecayState = true;
      targetValue = getSustainAnalogReadValue() * 4;
      currentPole = decayPole;
    }
    currentEnvelopeValue = differenceEquation(currentPole, targetValue, currentEnvelopeValue);
    dac.setVoltage(round(currentEnvelopeValue), false);
    gateIsHigh = digitalRead(gatePin);
  }
  if (envelopeIsActive) {
    targetValue = 0;
    currentPole = releasePole;
    envelopeIsActive = false;
  }
  currentEnvelopeValue = differenceEquation(releasePole, targetValue, currentEnvelopeValue);
  dac.setVoltage(round(currentEnvelopeValue), false);
  gateIsHigh = digitalRead(gatePin);
}