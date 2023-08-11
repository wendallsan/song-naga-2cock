#define TRIGGER_PIN_1 19
#define GATE_PIN_1 21

bool isGate1 = false,
     isTrigger1 = false,
     lastTrigger1 = false;

void handleGatesAndTriggers() {
  isGate1 = digitalRead(GATE_PIN_1);
  isTrigger1 = digitalRead(TRIGGER_PIN_1) && !lastTrigger1;
}

void setup() {
  Serial.begin(9600);
  Serial.println( "Serial out is running." );
  pinMode(GATE_PIN_1, INPUT_PULLDOWN);
  pinMode(TRIGGER_PIN_1, INPUT_PULLDOWN);
}
int debugCounter = 0;
void loop() {
  handleGatesAndTriggers();
  debugCounter++;
  if( debugCounter >= 500 ){
    Serial.println( isGate1? "yes" : "no" );
    debugCounter = 0;
  }
  delay(1);
}