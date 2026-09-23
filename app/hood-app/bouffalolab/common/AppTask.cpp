#include "AppTask.h"

#include "HoodController.h"

#include <app/clusters/identify-server/identify-server.h>
#include <app/server/Server.h>
#include <lib/support/logging/CHIPLogging.h>
#include <openthread/platform/settings.h>
#include <plat.h>
#include <platform/bouffalolab/common/BflbConfig.h>
#include <system/SystemClock.h>

extern "C" {
#include <bl_gpio.h>
#include <bl_sys.h>
#include <bl_timer.h>
}

extern "C" void otSysEventSignalPending(void);

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

volatile uint32_t sAppHeartbeatMs    = 0;
volatile uint32_t sMatterHeartbeatMs = 0;
volatile uint32_t sThreadHeartbeatMs = 0;

uint32_t MonotonicMilliseconds()
{
    return static_cast<uint32_t>(bl_timer_now_us64() / 1000U);
}

void MatterHeartbeatHandler(chip::System::Layer * systemLayer, void *)
{
    sMatterHeartbeatMs = MonotonicMilliseconds();
    if (systemLayer->StartTimer(chip::System::Clock::Seconds16(1), MatterHeartbeatHandler, nullptr) != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Matter watchdog heartbeat timer stopped");
    }
}

void StartMatterHeartbeat(intptr_t)
{
    MatterHeartbeatHandler(&SystemLayer(), nullptr);
}

bool PerformRawFactoryReset()
{
    ChipLogProgress(NotSpecified, "Erasing Matter and Thread storage");
    const CHIP_ERROR resetError = Internal::BflbConfig::FactoryResetRaw();
    if (resetError != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Raw factory reset failed: %" CHIP_ERROR_FORMAT, resetError.Format());
        return false;
    }

    ChipLogProgress(NotSpecified, "Raw factory reset complete; restarting");
    bl_sys_reset_por();
    return true;
}

} // namespace

AppTask AppTask::sAppTask;
StackType_t AppTask::appStack[APP_TASK_STACK_SIZE / sizeof(StackType_t)];
StaticTask_t AppTask::appTaskStruct;

void StartAppTask()
{
    GetAppTask().sAppTaskHandle = xTaskCreateStatic(AppTask::AppTaskMain, "hood", MATTER_ARRAY_SIZE(AppTask::appStack), nullptr,
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

bool FactoryResetButtonHeldAtBoot()
{
    if (bl_gpio_input_get_value(APP_FACTORY_RESET_PIN) != 0)
    {
        return false;
    }

    const uint64_t pressedAt = bl_timer_now_us64();
    while (bl_gpio_input_get_value(APP_FACTORY_RESET_PIN) == 0)
    {
        if ((bl_timer_now_us64() - pressedAt) >= (static_cast<uint64_t>(APP_FACTORY_RESET_HOLD_MS) * 1000U))
        {
            return true;
        }
        bl_timer_delay_us(APP_BUTTON_POLL_MS * 1000U);
    }

    return false;
}

extern "C" bool app_watchdog_is_healthy(void)
{
    const uint32_t now = MonotonicMilliseconds();
    constexpr uint32_t kHeartbeatTimeoutMs = 10000U;
    constexpr uint32_t kThreadHeartbeatTimeoutMs = 30000U;

    return sAppHeartbeatMs != 0U && sMatterHeartbeatMs != 0U && sThreadHeartbeatMs != 0U &&
        (now - sAppHeartbeatMs) < kHeartbeatTimeoutMs && (now - sMatterHeartbeatMs) < kHeartbeatTimeoutMs &&
        (now - sThreadHeartbeatMs) < kThreadHeartbeatTimeoutMs;
}

extern "C" void ot_watchdog_heartbeat(void)
{
    sThreadHeartbeatMs = MonotonicMilliseconds();
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

    bl_gpio_enable_input(APP_FACTORY_RESET_PIN, 1, 0);
    sAppHeartbeatMs = MonotonicMilliseconds();

    CHIP_ERROR error = PlatformMgr().StartEventLoopTask();
    if (error != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Platform event loop failed: %" CHIP_ERROR_FORMAT, error.Format());
        appError(error);
    }

    // PlatformInit resumes this task after the Matter server and attributes
    // exist.
    vTaskSuspend(nullptr);

    CHIP_ERROR scheduleError = CHIP_NO_ERROR;
    scheduleError = PlatformMgr().ScheduleWork(StartMatterHeartbeat);
    if (scheduleError != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to start Matter watchdog heartbeat: %" CHIP_ERROR_FORMAT, scheduleError.Format());
    }

    scheduleError = PlatformMgr().ScheduleWork([](intptr_t) { HoodController::Init(); });
    if (scheduleError != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to schedule hood initialization: %" CHIP_ERROR_FORMAT, scheduleError.Format());
    }

    ChipLogProgress(NotSpecified,
                    "D27 button ready: first short press turns everything off, "
                    "additional presses within 5 seconds cycle modes, "
                    "hold 5 seconds for factory reset");

    bool buttonRawLow                   = false;
    bool buttonStableLow                = false;
    bool longPressHandled               = false;
    bool modeWindowActive               = false;
    HoodController::LocalMode localMode = HoodController::LocalMode::kOff;
    TickType_t buttonRawChangedAt       = xTaskGetTickCount();
    TickType_t buttonPressedAt          = 0;
    TickType_t lastModeChangeAt         = 0;
    TickType_t lastThreadProbeAt        = xTaskGetTickCount();

    while (true)
    {
        sAppHeartbeatMs = MonotonicMilliseconds();
        uint32_t events           = APP_EVENT_NONE;
        const BaseType_t notified = xTaskNotifyWait(0, UINT32_MAX, &events, pdMS_TO_TICKS(APP_BUTTON_POLL_MS));

        if (notified == pdTRUE &&
            (events & (APP_EVENT_FAN_SETTING | APP_EVENT_FAN_MODE | APP_EVENT_LIGHT_SETTING | APP_EVENT_COMMISSION_COMPLETE)) != 0U)
        {
            // FanControl may update several coupled attributes synchronously.
            // Let that transaction finish, then read one coherent state.
            vTaskDelay(pdMS_TO_TICKS(10));
            scheduleError = PlatformMgr().ScheduleWork(ApplyEvents, static_cast<intptr_t>(events));
            if (scheduleError != CHIP_NO_ERROR)
            {
                ChipLogError(NotSpecified, "Failed to schedule hood state update: %" CHIP_ERROR_FORMAT, scheduleError.Format());
            }
        }

        const TickType_t now = xTaskGetTickCount();
        const bool rawLow    = (bl_gpio_input_get_value(APP_FACTORY_RESET_PIN) == 0);

        if ((now - lastThreadProbeAt) >= pdMS_TO_TICKS(1000))
        {
            lastThreadProbeAt = now;
            otSysEventSignalPending();
        }

        if (modeWindowActive && (now - lastModeChangeAt) >= pdMS_TO_TICKS(APP_BUTTON_MODE_WINDOW_MS))
        {
            modeWindowActive = false;
        }

        if (rawLow != buttonRawLow)
        {
            buttonRawLow       = rawLow;
            buttonRawChangedAt = now;
        }

        if (buttonRawLow != buttonStableLow && (now - buttonRawChangedAt) >= pdMS_TO_TICKS(APP_BUTTON_DEBOUNCE_MS))
        {
            buttonStableLow = buttonRawLow;

            if (buttonStableLow)
            {
                buttonPressedAt  = now;
                longPressHandled = false;
            }
            else if (!longPressHandled)
            {
                uint8_t requestedMode = 0;
                if (modeWindowActive)
                {
                    requestedMode = static_cast<uint8_t>(localMode) + 1U;
                    if (requestedMode >= static_cast<uint8_t>(HoodController::LocalMode::kCount))
                    {
                        requestedMode = 0;
                    }
                }

                scheduleError = PlatformMgr().ScheduleWork(
                    [](intptr_t mode) { HoodController::SetLocalMode(static_cast<HoodController::LocalMode>(mode)); },
                    static_cast<intptr_t>(requestedMode));
                if (scheduleError != CHIP_NO_ERROR)
                {
                    ChipLogError(NotSpecified, "Failed to schedule D27 mode change: %" CHIP_ERROR_FORMAT, scheduleError.Format());
                }
                else
                {
                    localMode        = static_cast<HoodController::LocalMode>(requestedMode);
                    lastModeChangeAt = now;
                    modeWindowActive = true;
                }
            }
        }

        if (buttonStableLow && !longPressHandled && (now - buttonPressedAt) >= pdMS_TO_TICKS(APP_FACTORY_RESET_HOLD_MS))
        {
            longPressHandled = true;
            ChipLogProgress(NotSpecified, "D27 held low: Matter factory reset");
            (void) PerformRawFactoryReset();
        }
    }
}
