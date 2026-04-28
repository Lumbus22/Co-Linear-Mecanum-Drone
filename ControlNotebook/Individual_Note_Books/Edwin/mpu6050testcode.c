/*
  ESP32 + MPU6050 test with labeled Serial output
  - SDA = GPIO 21, SCL = GPIO 22
*/

#include <Wire.h>

constexpr uint8_t MPU_ADDR       = 0x68;  // or 0x69 if AD0 is HIGH
constexpr uint8_t REG_WHO_AM_I   = 0x75;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

int16_t be16(uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(21, 22, 400000); // SDA=21, SCL=22

  // Check WHO_AM_I
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_WHO_AM_I);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)1);
  uint8_t who = Wire.read();

  Serial.println("=== ESP32 + MPU6050 Test ===");
  Serial.print("WHO_AM_I register: 0x");
  Serial.println(who, HEX);

  // Wake device
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);
}

void loop() {
  uint8_t raw[14];
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)14);

  for (int i = 0; i < 14; i++) raw[i] = Wire.read();

  int16_t ax_raw = be16(&raw[0]);
  int16_t ay_raw = be16(&raw[2]);
  int16_t az_raw = be16(&raw[4]);
  int16_t temp_raw = be16(&raw[6]);
  int16_t gx_raw = be16(&raw[8]);
  int16_t gy_raw = be16(&raw[10]);
  int16_t gz_raw = be16(&raw[12]);

  // Scale factors
  float ax = ax_raw / 16384.0;
  float ay = ay_raw / 16384.0;
  float az = az_raw / 16384.0;
  float gx = gx_raw / 131.0;
  float gy = gy_raw / 131.0;
  float gz = gz_raw / 131.0;
  float tempC = (temp_raw / 340.0) + 36.53;

  // Print with labels
  Serial.print("Accel X [g]: "); Serial.print(ax, 3);
  Serial.print(" | Accel Y [g]: "); Serial.print(ay, 3);
  Serial.print(" | Accel Z [g]: "); Serial.print(az, 3);

  Serial.print(" || Gyro X [°/s]: "); Serial.print(gx, 2);
  Serial.print(" | Gyro Y [°/s]: "); Serial.print(gy, 2);
  Serial.print(" | Gyro Z [°/s]: "); Serial.print(gz, 2);

  Serial.print(" || Temp [°C]: "); Serial.println(tempC, 2);

  delay(200);
}
