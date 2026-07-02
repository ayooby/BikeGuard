#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api_os.h"
#include "api_debug.h"
#include "api_event.h"
#include "api_gps.h"
#include "api_hal_pm.h"
#include "api_sms.h"
#include "api_hal_gpio.h"
#include "api_hal_i2c.h"
#include "gps.h"
#include "gps_parse.h"

#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "Motion Test Task"

#define SECOND_TASK_STACK_SIZE  (2048 * 2)
#define SECOND_TASK_PRIORITY    1
#define SECOND_TASK_NAME        "Motion Trace Task"

#define I2C_ACC I2C2
#define MPU_ADDR 0x68
#define MPU_REG_PWR_MGMT_1 0x6B
#define MPU_REG_WHO_AM_I   0x75
#define MPU_REG_ACCEL_XOUT_H 0x3B

#define IGNITION_PIN GPIO_PIN25
#define ALERT_PHONE "+450000000"

#define SAMPLE_PERIOD_MS 1000
#define BASELINE_SAMPLES 3
#define MOTION_DELTA_THRESHOLD 1500
#define MOTION_CONSECUTIVE_COUNT 2
#define SMS_COOLDOWN_SECONDS 15
#define ARMING_GRACE_SECONDS 20
#define GPS_FIX_WAIT_SECONDS 30

static HANDLE mainTaskHandle = NULL;
static HANDLE secondTaskHandle = NULL;
static HANDLE thirdTaskHandle = NULL;
static bool systemReady = false;
static bool networkReady = false;
static bool smsReady = false;
static bool gpsOpen = false;
static uint32_t muteRemainingSec = 0;
static uint16_t armingGraceSec = 0;

static uint32_t AbsDiffI16(int16_t a, int16_t b)
{
    int32_t d = (int32_t)a - (int32_t)b;
    return (uint32_t)((d < 0) ? -d : d);
}

static void SetLowPowerProfile(void)
{
    if (gpsOpen) {
        PM_SetSysMinFreq(PM_SYS_FREQ_78M);
    } else {
        PM_SetSysMinFreq(PM_SYS_FREQ_13M);
    }
}

static bool GpsOpenForAlarm(void)
{
    if (gpsOpen) {
        return true;
    }

    GPS_Init();
    if (!GPS_Open(NULL)) {
        Trace(1, "[GPS] open failed");
        return false;
    }

    gpsOpen = true;
    Trace(1, "[GPS] open ok");
    if (!GPS_SetOutputInterval(1000)) {
        Trace(1, "[GPS] set output interval failed");
    }

    SetLowPowerProfile();
    return true;
}

static void GpsCloseIfOpen(void)
{
    if (!gpsOpen) {
        return;
    }

    GPS_Close();
    gpsOpen = false;
    Trace(1, "[GPS] closed");
    SetLowPowerProfile();
}

static bool GpsBuildGoogleLink(char* mapLink, size_t mapLinkLen)
{
    GPS_Info_t* gpsInfo = Gps_GetInfo();
    if (gpsInfo == NULL || !gpsInfo->rmc.valid || gpsInfo->rmc.latitude.scale <= 0 || gpsInfo->rmc.longitude.scale <= 0 ||
        gpsInfo->rmc.latitude.value == 0 || gpsInfo->rmc.longitude.value == 0) {
        mapLink[0] = '\0';
        return false;
    }

    int latDeg = (int)(gpsInfo->rmc.latitude.value / gpsInfo->rmc.latitude.scale / 100);
    int lonDeg = (int)(gpsInfo->rmc.longitude.value / gpsInfo->rmc.longitude.scale / 100);
    double lat = latDeg +
        (double)(gpsInfo->rmc.latitude.value - latDeg * gpsInfo->rmc.latitude.scale * 100) / gpsInfo->rmc.latitude.scale / 60.0;
    double lon = lonDeg +
        (double)(gpsInfo->rmc.longitude.value - lonDeg * gpsInfo->rmc.longitude.scale * 100) / gpsInfo->rmc.longitude.scale / 60.0;

    snprintf(mapLink, mapLinkLen, " https://maps.google.com/?q=%.6f,%.6f", lat, lon);
    return true;
}

static void SendSmsText(const char* number, const char* text)
{
    if (number == NULL || text == NULL || text[0] == '\0') {
        return;
    }

    SMS_SendMessage(number, (uint8_t*)text, (uint8_t)strlen(text), SIM0);
}

static void HandleRemoteMuteCommand(const char* header, const char* content)
{
    unsigned int minutes = 0;
    char ack[96];

    if (header == NULL || content == NULL) {
        return;
    }

    if (strstr(header, ALERT_PHONE) == NULL) {
        Trace(1, "[SMS] command ignored: sender not owner");
        return;
    }

    if (strncmp(content, "MUTE", 4) != 0) {
        return;
    }

    if (sscanf(content, "MUTE %u", &minutes) != 1) {
        Trace(1, "[SMS] invalid mute command: %s", content);
        SendSmsText(ALERT_PHONE, "MUTE ERROR");
        return;
    }

    muteRemainingSec = minutes * 60U;
    if (minutes == 0U) {
        Trace(1, "[SMS] mute cleared");
        snprintf(ack, sizeof(ack), "MUTE OFF");
    } else {
        Trace(1, "[SMS] mute set for %u min", minutes);
        snprintf(ack, sizeof(ack), "MUTE ON %u MIN", minutes);
    }

    SendSmsText(ALERT_PHONE, ack);
}

static bool IgnitionInit(void)
{
    GPIO_config_t config;
    memset(&config, 0, sizeof(config));
    config.pin = IGNITION_PIN;
    config.mode = GPIO_MODE_INPUT;
    config.defaultLevel = GPIO_LEVEL_LOW;
    config.intConfig.debounce = 0;
    config.intConfig.type = GPIO_INT_TYPE_LOW_LEVEL;
    config.intConfig.callback = NULL;

    if (!GPIO_Init(config)) {
        Trace(1, "[IGNITION] GPIO_Init failed");
        return false;
    }

    Trace(1, "[IGNITION] init ok on pin %d", IGNITION_PIN);
    return true;
}

static bool IgnitionOn(void)
{
    GPIO_LEVEL level = GPIO_LEVEL_LOW;
    if (!GPIO_Get(IGNITION_PIN, &level)) {
        Trace(1, "[IGNITION] GPIO_Get failed");
        return false;
    }
    return level == GPIO_LEVEL_HIGH;
}

static bool SmsInit(void)
{
    SMS_Parameter_t smsParam;

    if (!SMS_SetFormat(SMS_FORMAT_TEXT, SIM0)) {
        Trace(1, "[SMS] SMS_SetFormat failed");
        return false;
    }

    memset(&smsParam, 0, sizeof(smsParam));
    smsParam.fo = 17;
    smsParam.vp = 167;
    smsParam.pid = 0;
    smsParam.dcs = 0;

    if (!SMS_SetParameter(&smsParam, SIM0)) {
        Trace(1, "[SMS] SMS_SetParameter failed");
        return false;
    }

    if (!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD)) {
        Trace(1, "[SMS] SMS_SetNewMessageStorage failed");
        return false;
    }

    Trace(1, "[SMS] init complete");
    return true;
}

static void TryInitSmsWhenReady(void)
{
    if (smsReady) {
        return;
    }

    if (systemReady && networkReady) {
        smsReady = SmsInit();
        if (!smsReady) {
            Trace(1, "[SMS] init pending retry");
        }
    }
}

static bool SendAlertSms(int16_t ax, int16_t ay, int16_t az, uint32_t delta)
{
    char msg[160];
    char mapLink[96];
    uint8_t len;
    bool ok;

    if (!smsReady) {
        Trace(1, "[SMS] skip send: SMS not ready yet");
        return false;
    }

    GpsBuildGoogleLink(mapLink, sizeof(mapLink));

    snprintf(msg, sizeof(msg), "ALERT motion ax=%d ay=%d az=%d d=%u%s", ax, ay, az, delta, mapLink);
    len = (uint8_t)strlen(msg);
    ok = SMS_SendMessage(ALERT_PHONE, (uint8_t*)msg, len, SIM0);
    Trace(1, "[SMS] send to %s => %d, msg=%s", ALERT_PHONE, ok, msg);
    return ok;
}

static void EventDispatch(API_Event_t* pEvent)
{
    switch (pEvent->id) {
        case API_EVENT_ID_SYSTEM_READY:
            Trace(1, "system initialize complete");
            systemReady = true;
            TryInitSmsWhenReady();
            break;
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(1, "network register success");
            networkReady = true;
            TryInitSmsWhenReady();
            break;
        case API_EVENT_ID_GPS_UART_RECEIVED:
            GPS_Update(pEvent->pParam1, pEvent->param1);
            break;
        case API_EVENT_ID_SMS_RECEIVED:
        {
            char* header = (char*)pEvent->pParam1;
            char* content = (char*)pEvent->pParam2;
            Trace(1, "[SMS] received header:%s", header ? header : "<null>");
            Trace(1, "[SMS] received content:%s", content ? content : "<null>");
            HandleRemoteMuteCommand(header, content);
            break;
        }
        case API_EVENT_ID_SMS_SENT:
            Trace(1, "[SMS] send success event");
            break;
        default:
            break;
    }
}

void SecondTask(void *pData)
{
    while(1)
    {
        Trace(1,"Hello GPRS ");
        OS_Sleep(3000);
    }
}

static void MotionTraceTask(void* pData)
{
    uint8_t whoAmI = 0;
    uint8_t pwrCfg = 0x00;
    uint8_t data[6] = {0};
    I2C_Config_t config;
    I2C_Error_t err;
    int16_t ax = 0, ay = 0, az = 0;
    int16_t baseAx = 0, baseAy = 0, baseAz = 0;
    int32_t sumAx = 0, sumAy = 0, sumAz = 0;
    uint8_t baselineCount = 0;
    uint8_t consecutiveMotion = 0;
    uint8_t smsCooldownSec = 0;
    bool baselineReady = false;
    bool guardArmed = false;
    bool ignitionOn = false;
    bool gpsFixAvailable = false;
    uint32_t delta = 0;
    uint16_t gpsWaitSec = 0;

    memset(&config, 0, sizeof(config));
    config.freq = I2C_FREQ_100K;

    Trace(1, "[MOTION] starting I2C init");
    if (!I2C_Init(I2C_ACC, config)) {
        Trace(1, "[MOTION] I2C init failed");
        return;
    }

    Trace(1, "[MOTION] I2C init ok");

    if (!IgnitionInit()) {
        Trace(1, "[IGNITION] disabled due to init failure");
    }

    SetLowPowerProfile();

    err = I2C_WriteMem(I2C_ACC, MPU_ADDR, MPU_REG_PWR_MGMT_1, 1, &pwrCfg, 1, I2C_DEFAULT_TIME_OUT);
    if (err == I2C_ERROR_NONE) {
        Trace(1, "[MOTION] wake ok");
    } else {
        Trace(1, "[MOTION] wake failed err=%d", err);
    }

    OS_Sleep(100);

    err = I2C_ReadMem(I2C_ACC, MPU_ADDR, MPU_REG_WHO_AM_I, 1, &whoAmI, 1, I2C_DEFAULT_TIME_OUT);
    if (err == I2C_ERROR_NONE) {
        Trace(1, "[MOTION] WHO_AM_I=0x%02x", whoAmI);
    } else {
        Trace(1, "[MOTION] WHO_AM_I read failed err=%d", err);
    }

    while (1) {
        err = I2C_ReadMem(I2C_ACC, MPU_ADDR, MPU_REG_ACCEL_XOUT_H, 1, data, 6, I2C_DEFAULT_TIME_OUT);
        if (err == I2C_ERROR_NONE) {
            ax = (int16_t)((data[0] << 8) | data[1]);
            ay = (int16_t)((data[2] << 8) | data[3]);
            az = (int16_t)((data[4] << 8) | data[5]);
            Trace(1, "[MOTION] raw=%02x %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4], data[5]);
            Trace(1, "[MOTION] ax=%d ay=%d az=%d", ax, ay, az);

            ignitionOn = IgnitionOn();
            if (ignitionOn && guardArmed) {
                guardArmed = false;
                consecutiveMotion = 0;
                Trace(1, "[GUARD] OFF (ignition ON)");
                armingGraceSec = 0;
            } else if (!ignitionOn && !guardArmed && armingGraceSec == 0) {
                armingGraceSec = ARMING_GRACE_SECONDS;
                consecutiveMotion = 0;
                Trace(1, "[GUARD] arming grace started: %u sec", armingGraceSec);
            }

            if (armingGraceSec > 0) {
                armingGraceSec--;
                if (armingGraceSec == 0 && !ignitionOn) {
                    guardArmed = true;
                    Trace(1, "[GUARD] ON (ignition OFF)");
                }
            }

            if (muteRemainingSec > 0) {
                muteRemainingSec--;
            }

            if (!guardArmed) {
                SetLowPowerProfile();
            }

            if (!baselineReady) {
                sumAx += ax;
                sumAy += ay;
                sumAz += az;
                baselineCount++;

                if (baselineCount >= BASELINE_SAMPLES) {
                    baseAx = (int16_t)(sumAx / BASELINE_SAMPLES);
                    baseAy = (int16_t)(sumAy / BASELINE_SAMPLES);
                    baseAz = (int16_t)(sumAz / BASELINE_SAMPLES);
                    baselineReady = true;
                    Trace(1, "[MOTION] baseline set ax=%d ay=%d az=%d", baseAx, baseAy, baseAz);
                } else {
                    Trace(1, "[MOTION] baseline collecting %d/%d", baselineCount, BASELINE_SAMPLES);
                }
            } else {
                delta = AbsDiffI16(ax, baseAx) + AbsDiffI16(ay, baseAy) + AbsDiffI16(az, baseAz);
                Trace(1, "[MOTION] delta=%u threshold=%d", delta, MOTION_DELTA_THRESHOLD);

                if (guardArmed && muteRemainingSec == 0 && delta >= MOTION_DELTA_THRESHOLD) {
                    consecutiveMotion++;
                    Trace(1, "[MOTION] candidate motion %d/%d", consecutiveMotion, MOTION_CONSECUTIVE_COUNT);
                } else {
                    consecutiveMotion = 0;
                }

                if (consecutiveMotion >= MOTION_CONSECUTIVE_COUNT) {
                    Trace(1, "[MOTION] MOTION DETECTED");

                    if (smsCooldownSec == 0) {
                        gpsFixAvailable = false;
                        if (GpsOpenForAlarm()) {
                            for (gpsWaitSec = 0; gpsWaitSec < GPS_FIX_WAIT_SECONDS; ++gpsWaitSec) {
                                GPS_Info_t* gpsInfo = Gps_GetInfo();
                                if (gpsInfo != NULL && gpsInfo->rmc.valid) {
                                    gpsFixAvailable = true;
                                    Trace(1, "[GPS] fix valid");
                                    break;
                                }
                                Trace(1, "[GPS] waiting fix %u/%u", (unsigned)(gpsWaitSec + 1), GPS_FIX_WAIT_SECONDS);
                                OS_Sleep(1000);
                            }
                        }

                        if (SendAlertSms(ax, ay, az, delta)) {
                            smsCooldownSec = SMS_COOLDOWN_SECONDS;
                            Trace(1, "[SMS] cooldown started: %d sec", SMS_COOLDOWN_SECONDS);
                        }

                        Trace(1, "[GPS] fix used: %d", gpsFixAvailable);
                        GpsCloseIfOpen();
                    } else {
                        Trace(1, "[SMS] skip send, cooldown left: %d sec", smsCooldownSec);
                    }

                    consecutiveMotion = 0;
                }

                baseAx = (int16_t)(((int32_t)baseAx * 7 + ax) / 8);
                baseAy = (int16_t)(((int32_t)baseAy * 7 + ay) / 8);
                baseAz = (int16_t)(((int32_t)baseAz * 7 + az) / 8);
            }
        } else {
            Trace(1, "[MOTION] accel read failed err=%d", err);
        }

        if (smsCooldownSec > 0) {
            smsCooldownSec--;
        }

        SetLowPowerProfile();

        OS_Sleep(SAMPLE_PERIOD_MS);
    }
}

static void MainTask(void* pData)
{
    API_Event_t* event = NULL;

    thirdTaskHandle = OS_CreateTask(SecondTask,
        NULL, NULL, SECOND_TASK_STACK_SIZE, SECOND_TASK_PRIORITY, 0, 0, SECOND_TASK_NAME);

    secondTaskHandle = OS_CreateTask(MotionTraceTask,
        NULL, NULL, SECOND_TASK_STACK_SIZE, SECOND_TASK_PRIORITY, 0, 0, SECOND_TASK_NAME);

    while (1) {
        if (OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER)) {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

void motion_test_Main(void)
{
    Trace(1, "[BOOT] motion_test_Main entry reached");
    mainTaskHandle = OS_CreateTask(MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}
