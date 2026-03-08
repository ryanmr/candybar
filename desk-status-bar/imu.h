#pragma once

// =============================================================
// IMU Reading — shared between critter and auto-dim
// =============================================================
void readIMU() {
  if (!qmiReady) return;
  if (qmi.getDataReady()) {
    IMUdata acc, gyro;
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
      lastAccX = acc.x;
      lastAccY = acc.y;
      lastAccZ = acc.z;
    }
    qmi.getGyroscope(gyro.x, gyro.y, gyro.z); // must read both to advance FIFO
  }
}
