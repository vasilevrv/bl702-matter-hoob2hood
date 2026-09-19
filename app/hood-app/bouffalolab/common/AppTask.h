#pragma once

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include <platform/CHIPDeviceLayer.h>

#define APP_ERROR_EVENT_QUEUE_FAILED CHIP_APPLICATION_ERROR(0x01)
#define APP_RESET_COUNT_KEY "hood_reset_cnt"
#define APP_RESET_COUNT_LIMIT 3U
#define APP_RESET_WINDOW_MS 5000U
#define APP_FACTORY_RESET_PIN 27U
#define APP_FACTORY_RESET_HOLD_MS 5000U
#define APP_FACTORY_RESET_POLL_MS 100U

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
        APP_EVENT_RESET_WINDOW_EXPIRE = 1U << 3,
        APP_EVENT_FACTORY_RESET       = 1U << 4,
        APP_EVENT_COMMISSION_COMPLETE = 1U << 5,
    };

    void PostEvent(app_event_t event);

private:
    friend void StartAppTask();
    friend chip::DeviceLayer::PlatformManagerImpl;

    static void AppTaskMain(void * argument);
    static void ApplyEvents(intptr_t eventsArg);
    static void ResetWindowTimer(TimerHandle_t timer);
    TaskHandle_t sAppTaskHandle = nullptr;
    TimerHandle_t mResetTimer   = nullptr;

    static StackType_t appStack[APP_TASK_STACK_SIZE / sizeof(StackType_t)];
    static StaticTask_t appTaskStruct;
    static AppTask sAppTask;
};

inline AppTask & GetAppTask()
{
    return AppTask::sAppTask;
}

void StartAppTask();
