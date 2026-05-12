#include <Bluepad32.h>
#include <ESP32Servo.h>

// ----- State Machine Definitions -----
enum VehicleState {
  STATE_ARMING,
  STATE_SEARCHING,
  STATE_DRIVING,
  STATE_FAILSAFE
};

VehicleState currentState = STATE_ARMING;
bool isControllerConnected = false; 

// ----- Hardware Setup -----
// Co-linear Chassis Layout (left to right):
// [M4\]--[M3/]--[M2\]--[M1/]
// outer-L  inner-L  inner-R  outer-R
const int motorPins[4] = {18, 21, 22, 23};
Servo motor1, motor2, motor3, motor4;

// PERFECTLY ALIGNED TO ESC SETTINGS
const int MIN_PULSE = 1006;   
const int MID_PULSE = 1502;   
const int MAX_PULSE = 2006;   

// --- SPEED LIMITER & DEADBANDS ---
const float SPEED_DIVIDER = 4.0;
const int JOY_DEADBAND = 40;      
const int ESC_DEADBAND = 15;      

const int LIMITED_MIN_PULSE = MID_PULSE - ((MID_PULSE - MIN_PULSE) / SPEED_DIVIDER); 
const int LIMITED_MAX_PULSE = MID_PULSE + ((MAX_PULSE - MID_PULSE) / SPEED_DIVIDER); 

// ----- Camera Tilt Setup -----
const int TILT_SERVO_PIN = 19;
Servo cameraTiltServo;

// Camera tracking variables
int currentTiltAngle = 90;          
const int TILT_MIN = 10;            
const int TILT_MAX = 170;           
const int TILT_CENTER = 90;         

unsigned long lastTiltUpdate = 0;
const int TILT_SPEED_DELAY = 15;    

const int LED_PIN = 2;        

// ----- Timers -----
unsigned long stateStartTime = 0;
unsigned long lastLedUpdate = 0;
bool ledState = false;

ControllerPtr myController = nullptr;

// ----- Helper Functions -----
void lockMotorsNeutral() {
  motor1.writeMicroseconds(MID_PULSE);
  motor2.writeMicroseconds(MID_PULSE);
  motor3.writeMicroseconds(MID_PULSE);
  motor4.writeMicroseconds(MID_PULSE);
}

int mapJoystickToPulseBidirectional(int joyVal) {
  if (abs(joyVal) < JOY_DEADBAND) {
    return MID_PULSE;
  }
  if (joyVal >= JOY_DEADBAND) {
    int pulse = map(joyVal, JOY_DEADBAND, 512, MID_PULSE + ESC_DEADBAND, LIMITED_MAX_PULSE);
    return constrain(pulse, MID_PULSE + ESC_DEADBAND, LIMITED_MAX_PULSE);
  } else {
    int pulse = map(joyVal, -512, -JOY_DEADBAND, LIMITED_MIN_PULSE, MID_PULSE - ESC_DEADBAND);
    return constrain(pulse, LIMITED_MIN_PULSE, MID_PULSE - ESC_DEADBAND);
  }
}

// ----- Bluepad32 Callbacks -----
void onConnectedController(ControllerPtr ctl) {
  if (myController == nullptr) {
    myController = ctl;
    isControllerConnected = true;
    Serial.printf("Controller Connected: %s\n", ctl->getModelName().c_str());
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (myController == ctl) {
    myController = nullptr;
    isControllerConnected = false;
    Serial.println("Controller Disconnected!");
  }
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize Camera Servo
  cameraTiltServo.attach(TILT_SERVO_PIN);
  cameraTiltServo.write(currentTiltAngle); 

  motor1.attach(motorPins[0], MIN_PULSE, MAX_PULSE); // M1: RR
  motor2.attach(motorPins[1], MIN_PULSE, MAX_PULSE); // M2: FR
  motor3.attach(motorPins[2], MIN_PULSE, MAX_PULSE); // M3: RL
  motor4.attach(motorPins[3], MIN_PULSE, MAX_PULSE); // M4: FL

  lockMotorsNeutral();

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  currentState = STATE_ARMING;
  stateStartTime = millis();
  Serial.println("STATE: ARMING ESCs...");
}

// ----- Main Loop -----
void loop() {
  BP32.update(); 
  unsigned long currentMillis = millis();

  switch (currentState) {

    case STATE_ARMING:
      lockMotorsNeutral();
      if (currentMillis - lastLedUpdate >= 50) {
        lastLedUpdate = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
      }
      
      if (currentMillis - stateStartTime >= 3000) {
        digitalWrite(LED_PIN, LOW);
        currentState = STATE_SEARCHING;
        Serial.println("STATE: SEARCHING for Controller...");
      }
      break;

    case STATE_SEARCHING:
      lockMotorsNeutral();
      if (currentMillis - lastLedUpdate >= 500) {
        lastLedUpdate = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
      }

      if (isControllerConnected) {
        currentState = STATE_DRIVING;
        digitalWrite(LED_PIN, HIGH);
        Serial.println("STATE: DRIVING LIVE");
      }
      break;

    case STATE_DRIVING:
      if (!isControllerConnected) {
        currentState = STATE_FAILSAFE;
        break;
      }

      if (myController && myController->isConnected()) {
        
        // -----------------------------------------
        // 1. DRIVING CONTROLS (CO-LINEAR \/\/ MECANUM KINEMATICS)
        // -----------------------------------------
        int driveY  = -myController->axisY();  // Left Stick Y (Forward/Backward)
        int strafeX = -myController->axisX();   // Left Stick X (Strafe Left/Right)
        int turnX   = -myController->axisRX();  // Right Stick X (Turn Left/Right)

        // Co-linear \/\/ mixing matrix (equal strafe ratio)
        int mixM4 = 2*driveY + 2*strafeX - turnX; // Outer-Left  (\)
        int mixM3 = 2*driveY - 2*strafeX - turnX; // Inner-Left  (/)
        int mixM2 = 2*driveY + 2*strafeX + turnX; // Inner-Right (\)
        int mixM1 = 2*driveY - 2*strafeX + turnX; // Outer-Right (/)

        // Proportional normalization: preserve ratios if any channel exceeds range
        // Worst case magnitude: (2+2+1)*512 = 2560 (drive+strafe+turn)
        int maxMix = max(max(abs(mixM4), abs(mixM3)), max(abs(mixM2), abs(mixM1)));
        if (maxMix > 512) {
            mixM4 = (mixM4 * 512) / maxMix;
            mixM3 = (mixM3 * 512) / maxMix;
            mixM2 = (mixM2 * 512) / maxMix;
            mixM1 = (mixM1 * 512) / maxMix;
        }

        // Convert the mixed values to ESC pulses
        // M4 and M3 are physically inverted on the chassis, so negate their mix
        // M2 and M1 are inverted via ESC configuration, so negate their mix
        int pulseM4 = mapJoystickToPulseBidirectional(-mixM4);
        int pulseM3 = mapJoystickToPulseBidirectional(-mixM3);
        int pulseM2 = mapJoystickToPulseBidirectional(-mixM2);
        int pulseM1 = mapJoystickToPulseBidirectional(-mixM1);

        // Apply pulses to independent motors
        motor4.writeMicroseconds(pulseM4);
        motor3.writeMicroseconds(pulseM3);
        motor2.writeMicroseconds(pulseM2);
        motor1.writeMicroseconds(pulseM1);

        // -----------------------------------------
        // 2. CAMERA TILT CONTROLS (INVERTED)
        // -----------------------------------------
        if (currentMillis - lastTiltUpdate >= TILT_SPEED_DELAY) {
          bool tiltChanged = false;
          uint8_t dpad = myController->dpad();
          uint16_t buttons = myController->buttons();

          // INVERTED: Up D-pad now decreases the angle
          if (dpad & DPAD_UP) {
            currentTiltAngle--;
            tiltChanged = true;
          }
          // INVERTED: Down D-pad now increases the angle
          if (dpad & DPAD_DOWN) {
            currentTiltAngle++;
            tiltChanged = true;
          }
          
          if (buttons & BUTTON_X) {
            currentTiltAngle = TILT_CENTER;
            tiltChanged = true;
          }

          if (tiltChanged) {
            currentTiltAngle = constrain(currentTiltAngle, TILT_MIN, TILT_MAX);
            cameraTiltServo.write(currentTiltAngle);
          }
          
          lastTiltUpdate = currentMillis;
        }

        // FULL INDEPENDENT 4-MOTOR PWM OUTPUT
        Serial.printf("OL(M4): %4d | IL(M3): %4d | IR(M2): %4d | OR(M1): %4d\n",
                      pulseM4, pulseM3, pulseM2, pulseM1);
      }
      break;

    case STATE_FAILSAFE:
      Serial.println("STATE: FAILSAFE TRIGGERED! Stopping motors.");
      lockMotorsNeutral();
      currentState = STATE_SEARCHING;
      break;
  }

  delay(10); 
}