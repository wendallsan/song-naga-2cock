#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 dac;
bool gateIsHigh = false,
     envelopeIsActive = false,
     isDecayState = false;
int gatePin = 22,
  attackAnalogReadValue = 1,  // MANUALLY SET THIS SINCE WE HAVE NO KNOB FOR IT CURRENTLY
  targetValue = 0;
float currentPole = 0.7,  // CURRENT POLE LOCATION
  currentEnvelopeValue = 0.0;   // INITIALIZE THE ENVELOPE TO 0.0
double attackPole = 0.9, // INITIALIZE A D AND R POLES
       decayPole = 0.9,
       releasePole = 0.95;

void setup() {
  Serial.begin(9600);
  analogReadResolution(10); // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  dac.begin(0x62); // START THE DAC
  pinMode(gatePin, INPUT_PULLDOWN); // SET UP THE GATE INPUT PIN

  // HARD SET attackPole FOR NOW SINCE WE DON'T HAVE AN ADC AVAILABLE FOR IT.
  attackPole = sqrt(0.999 * cos((1023 - attackAnalogReadValue) / 795));
}


int geAttackAnalogReadValue(){
  // return analogRead(A1);
  return 1;
}
int getDecayAnalogReadValue(){
  return analogRead(A1);
}
int getSustainAnalogReadValue(){
  return analogRead(A0);
}
int getReleaseAnalogReadValue(){
  return analogRead(A2);
}

double calculatePole( int poleValue ){
  return sqrt(0.999 * cos((1023 - poleValue) / 795));
}
void loop() {  

  decayPole = calculatePole( getDecayAnalogReadValue());
  releasePole = calculatePole( getReleaseAnalogReadValue());
  gateIsHigh = digitalRead(gatePin);




  while ( gateIsHigh ) {
    if ( envelopeIsActive == false ) {  // if a note isn't active and we're triggered, then start one!
      isDecayState = false;
      targetValue = 4096;        // drive toward full value
      currentPole = attackPole;
      envelopeIsActive = true;  // set the envelopeIsActive flag
    }
    // if we've reached envelope >4000 with targetValue= 4096, we must be at the end of attack phase
    // so switch to decay...
    if ((isDecayState == false) && (currentEnvelopeValue > 4000) && (targetValue == 4096)) {
      isDecayState = true;           // set decay flag
      targetValue = getSustainAnalogReadValue() * 4; // MULT BY 4 BECAUSE INPUT IS 10 BIT BUT OUTPUT IS 12 BIT
      currentPole = decayPole;         // and set 'time constant' decayPole for decay phase
    }

    currentEnvelopeValue = (1.0 - currentPole) * targetValue + currentPole * currentEnvelopeValue;
    dac.setVoltage(round(currentEnvelopeValue), false);

    gateIsHigh = digitalRead(gatePin);                    // read the gate pin (remember we're in the while loop)
  }



  
  if (envelopeIsActive == true) {  // this is the start of the release phase
    targetValue = 0;                // drive towards zero
    currentPole = releasePole;           // set 'time comnstant' releasePole for release phase
    envelopeIsActive = false;      // turn off envelopeIsActive flag
  }
  currentEnvelopeValue = ((1.0 - releasePole) * targetValue + releasePole * currentEnvelopeValue);  // implement the difference equation again (outside the while loop)
  
  dac.setVoltage(round(currentEnvelopeValue), false);
  gateIsHigh = digitalRead(gatePin);
}
