#include <Servo.h>

// Servo objects for the valves
Servo oxServo;
Servo fuelServo;

// servo handling 
int servoVal;
float servoPOS;
int increment = 0;
int max = 90;

// hardware pins
const int ledPin = 12;
const int hallEffect = 8;

// Emergency abort flag
bool emgAbort = false;

// Generic Cycle struct
// handles time open/close operations
struct valveCycle {
  bool active = false;
  unsigned long startTime = 0;
  unsigned long cycleTime = 0;
};

// Ignition Sequence struct
// handles countdown, timing and the LED "GO" light
struct ignitionSequence {
  bool active = false;
  unsigned long startTime = 0;
  unsigned long countdownMS = 0;
};

// instances for structs 
ignitionSequence sequence;
valveCycle oxCycle;
valveCycle fuelCycle;

// check if the abort flag has been set to true
bool checkAbort() {
  if (emgAbort) return(true);
  else return(false);
}

// Get all data and send over the serial line
void readAndSendData() {
  unsigned long lastSend = 0;

  if(millis() - lastSend >= 50) {
    // Read servo positions ||| This method is temp until we get feedback servos for exact POS
    int oxValue = oxServo.read();
    int fuelValue = fuelServo.read();

    // send data over Serial ||| this will just be the values and no string for better parsing on the python side
    Serial.println(String(oxValue) + " " + String(fuelValue));
    lastSend = millis();
  }
}

void incrementOpen() {

  if (increment == 0) {
    oxServo.write(0);
    fuelServo.write(0);
    increment++;
  }
  else if (increment == 10) {
    oxServo.write(0);
    fuelServo.write(0);
    increment = 0;
  }
  else {
    oxServo.write(oxServo.read() + 10);
    fuelServo.write(fuelServo.read() + 10);
    increment++;
  }
}

// Read data from the Serial port when available 
String readMessage() {
  String message = "";
  // make sure we arent interfering with soemthing else
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') break; // End of string
    // set every message to lowercase for easy handling
    message += char(tolower(c));
    delay(2);
  }

  return message;
}

// Starts the valve cycle ||| open valve for cycleTime
void startValveCycle(valveCycle &cycle, Servo &servo) {
  if (sequence.active) return;
  
  cycle.active = true;
  cycle.startTime = millis();
  servo.write(0); // set servo POS to 0

}

// Starts the ignition sequence using CountdownMS
void startIgnitionSequence() {
  if(oxCycle.active || fuelCycle.active) return;

  sequence.active = true;
  sequence.startTime = millis();
}

// Runs the state maching for valve cycle
void updateValveCycle(valveCycle &cycle, Servo &servo) {
  // make sure the cycle is active 
  if (!cycle.active) return;

  // make sure we havent aborted before doing anything
  if (checkAbort()) {
    servo.write(0);
    !cycle.active;
    return;
  }

  unsigned long elapsed = millis() - cycle.startTime;

  if (elapsed < cycle.cycleTime) {
    servo.write(max);   // Open
  }
  else if (elapsed >= cycle.cycleTime) {
    servo.write(0);     // Close
    cycle.active = false;  // Done
  }
}

// Run the full ignition sequence 
void ignitionSequenceRun() {
  if (!sequence.active) return;

  if (checkAbort()) {
    // set everything to 0 and disable the sequence
    oxServo.write(0);
    fuelServo.write(0);
    digitalWrite(ledPin, LOW);
    !sequence.active;
    return;
  }

  unsigned long elapsed = millis() - sequence.startTime;

  // 5 second before countdown ends enable LED for sparkbox crew
  if (elapsed >= (sequence.countdownMS - 5000) && elapsed < sequence.countdownMS) {
    digitalWrite(ledPin, HIGH);
  }
  // when countdown finishes open valves
  if (elapsed >= sequence.countdownMS && elapsed < sequence.countdownMS + 4000) {
    oxServo.write(max);
    fuelServo.write(max);
  }
  // wait 4 seconds and close valves and disable sequence 
  if (elapsed >= sequence.countdownMS + 4000) {
    oxServo.write(0);
    fuelServo.write(0);
    digitalWrite(ledPin, LOW);
    sequence.active = false;
  }
}

void setup() {
  oxServo.attach(9);
  fuelServo.attach(10);
  Serial.begin(115200);

  oxServo.write(0);
  fuelServo.write(0);

  pinMode(ledPin, OUTPUT);
}

void loop() {
  // read message every loop
  String command = readMessage();

  readAndSendData();

  // command checking 
  if (command.startsWith("oxservo")) {
    // parse int for precise movement 
    servoVal = command.substring(8).toInt();
    oxServo.write(servoVal);
  }
  if (command.startsWith("fuelservo")) {
    servoVal = command.substring(10).toInt();
    fuelServo.write(servoVal);
  }
  if (command.startsWith("cycleoxvalve")) {
    oxCycle.cycleTime = command.substring(12).toInt() * 1000;
    startValveCycle(oxCycle, oxServo);
  }
  if (command.startsWith("cyclefuelvalve")) {
    fuelCycle.cycleTime = command.substring(14).toInt() * 1000;
    startValveCycle(fuelCycle, fuelServo);
  }
  if (command.startsWith("ignseq")) {
    sequence.countdownMS = command.substring(6).toInt() * 1000;
    startIgnitionSequence();
  }
  if (command == "incrementopen") incrementOpen();
  if (command == "abort") emgAbort = true;

  updateValveCycle(oxCycle, oxServo);
  updateValveCycle(fuelCycle, fuelServo);
  ignitionSequenceRun();
}
