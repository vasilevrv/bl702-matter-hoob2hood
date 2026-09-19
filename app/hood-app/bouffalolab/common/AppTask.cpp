#include "AppTask.h"

#include "HoodController.h"

#include <app/clusters/identify-server/identify-server.h>
#include <app/server/Server.h>
#include <lib/support/logging/CHIPLogging.h>
#include <plat.h>
#include <platform/bouffalolab/common/BflbConfig.h>

extern "C" {
#include <bl_gpio.h>
}

using namespace chip;
using namespace chip::DeviceLayer;

namespace {

void IdentifyStart(Identify *)
{
    ChipLogProgress(NotSpecified, "Identify requested");
}

void IdentifyStop(Identify *)
{
    ChipLogProgress(NotSpecified, "Identify finished");
}

Identify sFanIdentify(1, IdentifyStart, IdentifyStop, app::Clusters::Identify::IdentifyTypeEnum::kNone);
Identify sLightIdentify(2, IdentifyStart, IdentifyStop, app::Clusters::Identify::IdentifyTypeEnum::kLightOutput);

} // namespace

AppTask AppTask::sAppTask;
StackType_t AppTask::appStack[APP_TASK_STACK_SIZE / sizeof(StackType_t)];
StaticTask_t AppTask::appTaskStruct;

void StartAppTask()
{
    GetAppTask().sAppTaskHandle =
        xTaskCreateStatic(AppTask::AppTaskMain, "hood", MATTER_ARRAY_SIZE(AppTask::appStack), nullptr,
                          APP_TASK_PRIORITY, AppTask::appStack, &AppTask::appTaskStruct);
    if (GetAppTask().sAppTaskHandle == nullptr)
    {
        ChipLogError(NotSpecified, "Failed to create hood app task");
        appError(APP_ERROR_EVENT_QUEUE_FAILED);
    }
}

void AppTask::PostEvent(app_event_t event)
{
    if (xPortIsInsideInterrupt())
    {
        BaseType_t taskWoken = pdFALSE;
        xTaskNotifyFromISR(sAppTaskHandle, static_cast<uint32_t>(event), eSetBits, &taskWoken);
        portYIELD_FROM_ISR(taskWoken);
    }
    else
    {
        xTaskNotify(sAppTaskHandle, static_cast<uint32_t>(event), eSetBits);
    }
}

void AppTask::ResetWindowTimer(TimerHandle_t timer)
{
    (void) timer;
    GetAppTask().PostEvent(APP_EVENT_RESET_WINDOW_EXPIRE);
}

void AppTask::ApplyEvents(intptr_t eventsArg)
{
    const uint32_t events = static_cast<uint32_t>(eventsArg);

    // This callback runs on the CHIP event-loop task. Attribute accessors can
    // have a deep call chain, so keep them on its dedicated 8 KiB stack rather
    // than on the small application notification task.
    if ((events & (APP_EVENT_FAN_SETTING | APP_EVENT_COMMISSION_COMPLETE)) != 0U)
    {
        HoodController::ApplyFanSetting();
    }
    if ((events & APP_EVENT_FAN_MODE) != 0U && (events & APP_EVENT_FAN_SETTING) == 0U)
    {
        HoodController::ApplyFanMode();
    }
    if ((events & (APP_EVENT_LIGHT_SETTING | APP_EVENT_COMMISSION_COMPLETE)) != 0U)
    {
        HoodController::ApplyLightSetting();
    }
}

void AppTask::AppTaskMain(void * argument)
{
    (void) argument;
    uint32_t resetCount = 0;

    Internal::BflbConfig::ReadConfigValue(APP_RESET_COUNT_KEY, resetCount);
    resetCount++;
    Internal::BflbConfig::WriteConfigValue(APP_RESET_COUNT_KEY, resetCount);

    CHIP_ERROR error = PlatformMgr().StartEventLoopTask();
    if (error != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Platform event loop failed: %" CHIP_ERROR_FORMAT, error.Format());
        appError(error);
    }

    // PlatformInit resumes this task after the Matter server and attributes exist.
    vTaskSuspend(nullptr);

    if (resetCount >= APP_RESET_COUNT_LIMIT)
    {
        ChipLogProgress(NotSpecified, "Three quick resets: Matter factory reset");
        ConfigurationMgr().InitiateFactoryReset();
    }

    GetAppTask().mResetTimer =
        xTimerCreate("hoodReset", pdMS_TO_TICKS(APP_RESET_WINDOW_MS), pdFALSE, nullptr, ResetWindowTimer);
    if (GetAppTask().mResetTimer != nullptr)
    {
        xTimerStart(GetAppTask().mResetTimer, 0);
    }

    CHIP_ERROR scheduleError = PlatformMgr().ScheduleWork([](intptr_t) { HoodController::Init(); });
    if (scheduleError != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to schedule hood initialization: %" CHIP_ERROR_FORMAT, scheduleError.Format());
    }

    bl_gpio_enable_input(APP_FACTORY_RESET_PIN, 1, 0);
    ChipLogProgress(NotSpecified,
                    "D27 button ready: short press cycles modes, "
                    "hold 5 seconds for factory reset");

    bool buttonRawLow             = false;
    bool buttonStableLow          = false;
    bool longPressHandled         = false;
    TickType_t buttonRawChangedAt = xTaskGetTickCount();
    TickType_t buttonPressedAt    = 0;

    while (true)
    {
        uint32_t events = APP_EVENT_NONE;
        const BaseType_t notified =
            xTaskNotifyWait(0, UINT32_MAX, &events, pdMS_TO_TICKS(APP_BUTTON_POLL_MS));

        if (notified == pdTRUE && (events & APP_EVENT_RESET_WINDOW_EXPIRE) != 0U)
        {
            Internal::BflbConfig::WriteConfigValue(APP_RESET_COUNT_KEY, static_cast<uint32_t>(0));
        }
        if (notified == pdTRUE && (events & APP_EVENT_FACTORY_RESET) != 0U)
        {
            ConfigurationMgr().InitiateFactoryReset();
            vTaskSuspend(nullptr);
        }

        if (notified == pdTRUE &&
            (events & (APP_EVENT_FAN_SETTING | APP_EVENT_FAN_MODE | APP_EVENT_LIGHT_SETTING |
                       APP_EVENT_COMMISSION_COMPLETE)) != 0U)
        {
            // FanControl may update several coupled attributes synchronously.
            // Let that transaction finish, then read one coherent state.
            vTaskDelay(pdMS_TO_TICKS(10));
            scheduleError = PlatformMgr().ScheduleWork(ApplyEvents, static_cast<intptr_t>(events));
            if (scheduleError != CHIP_NO_ERROR)
            {
                ChipLogError(NotSpecified, "Failed to schedule hood state update: %" CHIP_ERROR_FORMAT,
                             scheduleError.Format());
            }
        }

        const TickType_t now = xTaskGetTickCount();
        const bool rawLow    = (bl_gpio_input_get_value(APP_FACTORY_RESET_PIN) == 0);

        if (rawLow != buttonRawLow)
        {
            buttonRawLow       = rawLow;
            buttonRawChangedAt = now;
        }

        if (buttonRawLow != buttonStableLow &&
            (now - buttonRawChangedAt) >= pdMS_TO_TICKS(APP_BUTTON_DEBOUNCE_MS))
        {
            buttonStableLow = buttonRawLow;

            if (buttonStableLow)
            {
                buttonPressedAt  = now;
                longPressHandled = false;
            }
            else if (!longPressHandled)
            {
                scheduleError = PlatformMgr().ScheduleWork([](intptr_t) { HoodController::CycleLocalMode(); });
                if (scheduleError != CHIP_NO_ERROR)
                {
                    ChipLogError(NotSpecified, "Failed to schedule D27 mode change: %" CHIP_ERROR_FORMAT,
                                 scheduleError.Format());
                }
            }
        }

        if (buttonStableLow && !longPressHandled &&
            (now - buttonPressedAt) >= pdMS_TO_TICKS(APP_FACTORY_RESET_HOLD_MS))
        {
            longPressHandled = true;
            ChipLogProgress(NotSpecified, "D27 held low: Matter factory reset");
            ConfigurationMgr().InitiateFactoryReset();
            vTaskSuspend(nullptr);
        }
    }
}
