#include <Servo.h>

Servo oxServo;
Servo fuelServo;

int servoVal;
const int ledPin = 12;
bool emgAbort = false;

struct valveCycle {
  bool active = false;
  unsigned long startTime = 0;
  unsigned long cycleTime = 0;
};

struct ignitionSequence {
  bool active = false;
  unsigned long startTime = 0;
  unsigned long countdownMS = 0;
};

ignitionSequence sequence;
valveCycle oxCycle;
valveCycle fuelCycle;

bool checkAbort() {
  if (emgAbort) return(true);
  else return(false);
}

void readAndSendData() {
  float oxValue = oxServo.read();
  float fuelValue = fuelServo.read();

  Serial.println("Ox Servo: " + String(oxValue));
  Serial.println("Fuel Servo: " + String(fuelValue));
}

String readMessage() {
  String message = "";
  if (Serial.available()) {
    message = Serial.readString();
    message.toLowerCase();
    message.trim();
  }

  return message;
}

void startValveCycle(valveCycle &cycle, Servo &servo) {
  cycle.active = true;
  cycle.startTime = millis();
  servo.write(0);
}

void startIgnitionSequence() {
  sequence.active = true;
  sequence.startTime = millis();
}

void updateValveCycle(valveCycle &cycle, Servo &servo) {
  if (!cycle.active) return;

  if (checkAbort()) {
    servo.write(0);
    !cycle.active;
    return;
  }

  unsigned long elapsed = millis() - cycle.startTime;

  if (elapsed < cycle.cycleTime) {
    servo.write(180);   // Open
  }
  else if (elapsed >= cycle.cycleTime) {
    servo.write(0);     // Close
    cycle.active = false;  // Done
  }
}

void ignitionSequenceRun() {
  if (!sequence.active) return;

  if (checkAbort()) {
    oxServo.write(0);
    fuelServo.write(0);
    digitalWrite(ledPin, LOW);
    !sequence.active;
    return;
  }

  unsigned long elapsed = millis() - sequence.startTime;


  if (elapsed >= (sequence.countdownMS - 5000) && elapsed < sequence.countdownMS) {
    digitalWrite(ledPin, HIGH);
  }
  if (elapsed >= sequence.countdownMS && elapsed < sequence.countdownMS + 4000) {
    oxServo.write(180);
    fuelServo.write(180);
  }
  if (elapsed >= sequence.countdownMS + 4000) {
    oxServo.write(0);
    fuelServo.write(0);
    digitalWrite(ledPin, LOW);
    !sequence.active;
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
  String command = readMessage();

  readAndSendData();

  if (command.startsWith("oxservo")) {
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
  if (command == "abort") emgAbort = true;

  updateValveCycle(oxCycle, oxServo);
  updateValveCycle(fuelCycle, fuelServo);
  ignitionSequenceRun();
}
