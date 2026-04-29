#include <ESP32Servo.h>
// Commands M1 1000 to stop
// Commands M2 1200 to move a bit

// Motor pins
const int motorPins[4] = {13, 12, 18, 19};

// Create servo objects
Servo motor1;
Servo motor2;
Servo motor3;
Servo motor4;

// Pulse range for ESCs (microseconds)
const int MIN_PULSE = 1000; // Minimum throttle
const int MAX_PULSE = 2000; // Maximum throttle

// Store current speeds
int motorSpeeds[4] = {1000, 1000, 1000, 1000};

void setup() {
  Serial.begin(115200);
  delay(2000); // Allow time for ESCs to initialize

  // Attach motors
  motor1.attach(motorPins[0], MIN_PULSE, MAX_PULSE);
  motor2.attach(motorPins[1], MIN_PULSE, MAX_PULSE);
  motor3.attach(motorPins[2], MIN_PULSE, MAX_PULSE);
  motor4.attach(motorPins[3], MIN_PULSE, MAX_PULSE);

  Serial.println("✅ ESP32 4-Motor ESC Control Ready!");
  Serial.println("Commands: M1 1500, M2 1200, M3 1800, M4 1300, ALL 1600");
  Serial.println("Range: 1000 (off) → 2000 (full throttle)");

  // Initialize all motors to minimum throttle
  setAllMotors(1000);
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    handleCommand(input);
  }
}

void handleCommand(String cmd) {
  cmd.toUpperCase();

  if (cmd.startsWith("M")) {
    int motorNum = cmd.substring(1, 2).toInt();
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex > 0 && motorNum >= 1 && motorNum <= 4) {
      int value = cmd.substring(spaceIndex + 1).toInt();
      value = constrain(value, MIN_PULSE, MAX_PULSE);
      setMotor(motorNum, value);
    }
  } 
  else if (cmd.startsWith("ALL")) {
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex > 0) {
      int value = cmd.substring(spaceIndex + 1).toInt();
      value = constrain(value, MIN_PULSE, MAX_PULSE);
      setAllMotors(value);
    }
  }
  else {
    Serial.println("Invalid command. Use: M1 1500 or ALL 1200");
  }
}

void setMotor(int motorNum, int pulse) {
  motorSpeeds[motorNum - 1] = pulse;
  switch (motorNum) {
    case 1: motor1.writeMicroseconds(pulse); break;
    case 2: motor2.writeMicroseconds(pulse); break;
    case 3: motor3.writeMicroseconds(pulse); break;
    case 4: motor4.writeMicroseconds(pulse); break;
  }
  Serial.printf("Motor %d set to %d µs\n", motorNum, pulse);
}

void setAllMotors(int pulse) {
  for (int i = 1; i <= 4; i++) {
    setMotor(i, pulse);
  }
  Serial.printf("All motors set to %d µs\n", pulse);
}
