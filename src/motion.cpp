#include "motion.h"
#include "config.h"
#include <Arduino.h>

#ifdef BIKEGUARD_NO_MPU

bool motionInit() {
    Serial.println("[MOTION] Bench mode: MPU6050 disabled");
    return true;
}

bool motionDetected() {
    return false;
}

#else

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

static Adafruit_MPU6050 mpu;
static float lastMagnitude = 0.0f;
static unsigned long lastSampleTime = 0;
static bool lastMovementDetected = false;

bool motionInit() {
    Wire.begin(MPU_SDA, MPU_SCL);

    if (!mpu.begin()) {
        Serial.println("[MOTION] MPU6050 not found — check wiring");
        return false;
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // Seed the baseline magnitude
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    lastMagnitude = sqrtf(
        a.acceleration.x * a.acceleration.x +
        a.acceleration.y * a.acceleration.y +
        a.acceleration.z * a.acceleration.z
    );

    Serial.println("[MOTION] MPU6050 OK");
    return true;
}

bool motionDetected() {
    unsigned long now = millis();
    if (now - lastSampleTime < MOTION_SAMPLE_MS) {
        return lastMovementDetected;
    }
    lastSampleTime = now;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float magnitude = sqrtf(
        a.acceleration.x * a.acceleration.x +
        a.acceleration.y * a.acceleration.y +
        a.acceleration.z * a.acceleration.z
    );

    float delta = fabsf(magnitude - lastMagnitude);
    lastMagnitude = magnitude;
    lastMovementDetected = delta > MOTION_THRESHOLD;

    return lastMovementDetected;
}

#endif
