#include <Bluepad32.h>
#include <ESP32Servo.h>

// ----- Motor setup -----
const int motorPins[4] = {13, 12, 18, 19};
Servo motor1, motor2, motor3, motor4;

const int MIN_PULSE = 1000;
const int MAX_PULSE = 2000;

// ----- Bluepad32 setup -----
ControllerPtr controllers[BP32_MAX_GAMEPADS] = {nullptr};

// Map joystick range (-511..512) to PWM microseconds
int mapJoystickToPulse(int joyVal) {
  // Deadzone around center
  if (abs(joyVal) < 40) joyVal = 0;
  // Map joystick (-511 to 512) -> (MIN_PULSE to MAX_PULSE)
  int pulse = map(joyVal, -512, 512, MIN_PULSE, MAX_PULSE);
  return constrain(pulse, MIN_PULSE, MAX_PULSE);
}

// ----- Controller callbacks -----
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (!controllers[i]) {
      controllers[i] = ctl;
      Serial.printf("🎮 Controller connected at index %d: %s\n", i, ctl->getModelName().c_str());
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
  Serial.println("✅ ESP32 Xbox Controller → PWM Motor Control");

  // Attach motors
  motor1.attach(motorPins[0], MIN_PULSE, MAX_PULSE);
  motor2.attach(motorPins[1], MIN_PULSE, MAX_PULSE);
  motor3.attach(motorPins[2], MIN_PULSE, MAX_PULSE);
  motor4.attach(motorPins[3], MIN_PULSE, MAX_PULSE);

  // Initialize motors to off
  motor1.writeMicroseconds(MIN_PULSE);
  motor2.writeMicroseconds(MIN_PULSE);
  motor3.writeMicroseconds(MIN_PULSE);
  motor4.writeMicroseconds(MIN_PULSE);

  // Initialize Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);
  BP32.forgetBluetoothKeys(); // Optional: clear pairing memory

  Serial.println("Put your Xbox controller in pairing mode to connect...");
}

// ----- Motor update logic -----
void updateMotorsFromController(ControllerPtr ctl) {
  if (!ctl->isConnected()) return;

  int leftY = ctl->axisY();   // Left joystick vertical
  int rightY = ctl->axisRY(); // Right joystick vertical

  // Convert joystick to PWM
  int leftPulse = mapJoystickToPulse(-leftY);   // invert Y
  int rightPulse = mapJoystickToPulse(-rightY); // invert Y

  // Apply to motors (1&2 = left, 3&4 = right)
  motor1.writeMicroseconds(leftPulse);
  motor2.writeMicroseconds(leftPulse);
  motor3.writeMicroseconds(rightPulse);
  motor4.writeMicroseconds(rightPulse);

  // Print diagnostic info
  Serial.printf("Lstick: %4d → %4d  Rstick: %4d → %4d\n",
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

  delay(100); // Lower CPU use
}
