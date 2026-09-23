#pragma once

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include <platform/CHIPDeviceLayer.h>

#define APP_ERROR_EVENT_QUEUE_FAILED CHIP_APPLICATION_ERROR(0x01)
#define APP_FACTORY_RESET_PIN 27U
#define APP_FACTORY_RESET_HOLD_MS 5000U
#define APP_BUTTON_MODE_WINDOW_MS 5000U
#define APP_BUTTON_POLL_MS 20U
#define APP_BUTTON_DEBOUNCE_MS 60U

class AppTask
{
public:
    friend AppTask & GetAppTask();

    enum app_event_t : uint32_t
    {
        APP_EVENT_NONE                = 0,
        APP_EVENT_FAN_SETTING         = 1U << 0,
        APP_EVENT_FAN_MODE            = 1U << 1,
        APP_EVENT_LIGHT_SETTING       = 1U << 2,
        APP_EVENT_COMMISSION_COMPLETE = 1U << 3,
    };

    void PostEvent(app_event_t event);

private:
    friend void StartAppTask();
    friend chip::DeviceLayer::PlatformManagerImpl;

    static void AppTaskMain(void * argument);
    static void ApplyEvents(intptr_t eventsArg);
    TaskHandle_t sAppTaskHandle = nullptr;

    static StackType_t appStack[APP_TASK_STACK_SIZE / sizeof(StackType_t)];
    static StaticTask_t appTaskStruct;
    static AppTask sAppTask;
};

inline AppTask & GetAppTask()
{
    return AppTask::sAppTask;
}

void StartAppTask();

// This check runs before the FreeRTOS scheduler and before LittleFS is
// initialized, so it can recover from a corrupted PSM partition.
bool FactoryResetButtonHeldAtBoot();

// The idle hook uses these heartbeats to feed the hardware watchdog only
// while both the application task and the Matter event loop are alive.
extern "C" bool app_watchdog_is_healthy(void);
