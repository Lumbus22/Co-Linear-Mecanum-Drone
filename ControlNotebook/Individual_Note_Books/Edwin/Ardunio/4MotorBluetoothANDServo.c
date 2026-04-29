#include <Bluepad32.h>
#include <ESP32Servo.h>

// ----- Motor setup -----
const int motorPins[4] = {15, 2, 22, 23};
Servo motor1, motor2, motor3, motor4;

const int MIN_PULSE = 1020;   // full reverse
const int MID_PULSE = 1492;   // neutral
const int MAX_PULSE = 2000;   // full forward

// ----- Servo setup -----
const int SERVO_PIN = 19;
Servo armServo;

const int SERVO_UP = 170;     // adjust as needed
const int SERVO_DOWN = 10;    // adjust as needed

// ----- Bluepad32 setup -----
ControllerPtr controllers[BP32_MAX_GAMEPADS] = {nullptr};

// Map joystick range (-511..512) to bidirectional PWM
int mapJoystickToPulseBidirectional(int joyVal) {
  if (abs(joyVal) < 40) return MID_PULSE;
  int pulse = map(joyVal, -512, 512, MIN_PULSE, MAX_PULSE);
  return constrain(pulse, MIN_PULSE, MAX_PULSE);
}

// ----- Controller callbacks -----
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (!controllers[i]) {
      controllers[i] = ctl;
      Serial.printf("Controller connected at index %d: %s\n",
                    i, ctl->getModelName().c_str());
      break;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == ctl) {
      Serial.printf("Controller disconnected at index %d\n", i);
      controllers[i] = nullptr;
      break;
    }
  }
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("ESP32 Xbox Controller → Motors + Servo Control");

  // Attach motors
  motor1.attach(motorPins[0], MIN_PULSE, MAX_PULSE);
  motor2.attach(motorPins[1], MIN_PULSE, MAX_PULSE);
  motor3.attach(motorPins[2], MIN_PULSE, MAX_PULSE);
  motor4.attach(motorPins[3], MIN_PULSE, MAX_PULSE);

  // Initialize motors to neutral
  motor1.writeMicroseconds(MID_PULSE);
  motor2.writeMicroseconds(MID_PULSE);
  motor3.writeMicroseconds(MID_PULSE);
  motor4.writeMicroseconds(MID_PULSE);

  // Attach servo
  armServo.attach(SERVO_PIN);
  armServo.write(SERVO_DOWN);  // start down

  // Initialize Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  Serial.println("Put Xbox controller in pairing mode...");
}

// ----- Motor + Servo update logic -----
void updateMotorsFromController(ControllerPtr ctl) {
  if (!ctl->isConnected()) return;

  int leftY = ctl->axisY();
  int rightY = ctl->axisRY();

  int leftPulse = mapJoystickToPulseBidirectional(-leftY);
  int rightPulse = mapJoystickToPulseBidirectional(-rightY);

  motor1.writeMicroseconds(leftPulse);
  motor2.writeMicroseconds(leftPulse);
  motor3.writeMicroseconds(rightPulse);
  motor4.writeMicroseconds(rightPulse);

  // ----- Button Control for Servo -----
  uint16_t buttons = ctl->buttons();

  if (buttons & BUTTON_Y) {
    armServo.write(SERVO_UP);
    Serial.println("Servo UP");
  }

  if (buttons & BUTTON_A) {
    armServo.write(SERVO_DOWN);
    Serial.println("Servo DOWN");
  }

  Serial.printf("L: %4d → %4d  R: %4d → %4d\n",
                leftY, leftPulse, rightY, rightPulse);
}

// ----- Main loop -----
void loop() {
  if (BP32.update()) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
      ControllerPtr ctl = controllers[i];
      if (ctl && ctl->isConnected()) {
        updateMotorsFromController(ctl);
      }
    }
  }
  delay(50);
}
