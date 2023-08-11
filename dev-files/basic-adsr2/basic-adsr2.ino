#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define ADC_MAX 1023
#define ENV_MAX 4095
#define ENV_MIN 0
#define ATTACK_THRESHOLD 4000
#define STATIC_ATTACK 10
#define STATIC_DECAY 10
#define STATIC_SUSTAIN 500
#define STATIC_RELEASE 10

#define GATE_PIN_1 16 // GP17 = PIN 22
#define LED_PIN 25 // GP25 = LED

Adafruit_MCP4725 dac1;
bool isGate1 = false,
     envelopeIsActive1 = false,
     isDecayState1 = false;
int targetValue1 = 0,
  attackCV = 0,
  decayCV = 0,
  sustainCV = 0,
  releaseCV = 0,
  calcCounter = 0;
float currentPole1 = 0.7,  // INITIALIZE POLE LOCATIONS
  currentEnvelopeValue1 = 0.0;
double attackPole1 = 0.9,  // INITIALIZE ATACK, DECAY AND RELEASE POLES
  decayPole1 = 0.9,
  releasePole1 = 0.95;
double calcPole(int poleValue) {
  return sqrt(0.999 * cos( ADC_MAX - poleValue / 795.0));
}

float delta(double targetPole, int targetValue1, float currentValue) {
  return (1.0 - targetPole) * targetValue1 + targetPole * currentValue;
  // return ( targetPole * targetValue1 ) + ( targetPole * currentValue);
}

void setEnvPoles1( int calcCounter ) {
  switch (calcCounter) {
    case 0:
      attackPole1 = calcPole(STATIC_ATTACK);
      break;
    case 1:
      decayPole1 = calcPole(STATIC_DECAY);
      break;
    case 2:
      releasePole1 = calcPole(STATIC_RELEASE);
      break;
  }
}

void updateEnvelope1() {
  if( isGate1 ){
    // IF GATE1 IS TRUE AND ENVELOPEISACTIVE1 IS FALSE, WE HAVE DETECTED THE START OF A NEW ENVELOPE!
    if (!envelopeIsActive1) { 
      isDecayState1 = false;
      targetValue1 = ENV_MAX;
      currentPole1 = attackPole1;
      envelopeIsActive1 = true;
    }
    if (!isDecayState1 && (currentEnvelopeValue1 > ATTACK_THRESHOLD) && (targetValue1 == ENV_MAX)) {
      isDecayState1 = true;
      targetValue1 = STATIC_SUSTAIN * 4;
      currentPole1 = decayPole1;
    }
  } else { // ELSE THE GATE IS NOT ON, WE ARE IN THE RELEASE PHASE
    targetValue1 = ENV_MIN;
    currentPole1 = releasePole1;
    envelopeIsActive1 = false;
    isDecayState1 = true;
  }
  currentEnvelopeValue1 = delta(currentPole1, targetValue1, currentEnvelopeValue1); // DO THE MATH
  dac1.setVoltage(round(currentEnvelopeValue1), false); // SET THE VOLTS
}
void setup() {
  Serial.begin(9600);
  analogReadResolution(10);  // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  dac1.begin(0x62);
  pinMode(GATE_PIN_1, INPUT_PULLDOWN);
  pinMode(LED_PIN, OUTPUT);
}
void loop() {
  isGate1 = digitalRead(GATE_PIN_1);

  setEnvPoles1( calcCounter );
  calcCounter++;
  if( calcCounter >= 2 ) calcCounter = 0;

  updateEnvelope1();
  
  digitalWrite( LED_PIN, isGate1 );
  Serial.print( "0.0 " );
  Serial.print( attackPole1 );
  Serial.print( " " );
  Serial.print( targetValue1 / ENV_MAX ) ;
  Serial.println( " 2.0" );
}