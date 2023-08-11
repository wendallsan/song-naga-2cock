#define MUX_CHANNELS 8
#define MUX_SELECT_1_PIN 11 // GP11 = PIN 15
#define MUX_SELECT_2_PIN 12 // GP12 = PIN 16
#define MUX_SELECT_3_PIN 13 // GP13 = PIN 17

bool isReadPhase = true;
int muxValues1[MUX_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0 },
  muxCounter = 0;
void setMuxSelectPins() {
  digitalWrite(MUX_SELECT_1_PIN, bitRead(muxCounter, 0));
  digitalWrite(MUX_SELECT_2_PIN, bitRead(muxCounter, 1));
  digitalWrite(MUX_SELECT_3_PIN, bitRead(muxCounter, 2));
}
void readMux() {
  muxValues1[muxCounter] = analogRead(A0);
  muxCounter++;
  if (muxCounter >= MUX_CHANNELS) muxCounter = 0;
}
void setup() {
  Serial.begin(9600);
  analogReadResolution(10);  // SET ANALOG READ TO 10 BITS (VALUE RANGE 0 - 1023)
  pinMode(MUX_SELECT_1_PIN, OUTPUT);
  pinMode(MUX_SELECT_2_PIN, OUTPUT);
  pinMode(MUX_SELECT_3_PIN, OUTPUT);
}
int debugCounter = 0;
void loop() {
  if(isReadPhase) readMux();
  else setMuxSelectPins();
  isReadPhase = !isReadPhase;
  debugCounter++;
  if( debugCounter >= 500 ){
    for( int i = 0; i < MUX_CHANNELS; i++ ){
       if( i == MUX_CHANNELS - 1 ) Serial.println( muxValues1[i] );
       else{
        Serial.print( muxValues1[ i ] );
        Serial.print( ", " );       
      }
    }
    debugCounter = 0;
  }
  delay(1);
}