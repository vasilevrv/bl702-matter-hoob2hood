#include "HoodController.h"

#include "HoodUart.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <lib/support/logging/CHIPLogging.h>

using chip::EndpointId;
using chip::Percent;
using chip::app::DataModel::Nullable;
namespace FanControl = chip::app::Clusters::FanControl;
namespace OnOff      = chip::app::Clusters::OnOff;
using chip::Protocols::InteractionModel::Status;

namespace {

constexpr EndpointId kFanEndpoint   = 1;
constexpr EndpointId kLightEndpoint = 2;
constexpr uint8_t kFanSpeedMax      = 4;

uint8_t sLastFanCommand  = 0xff;
int8_t sLastLightCommand = -1;

FanControl::FanModeEnum SpeedToMode(uint8_t speed)
{
    switch (speed)
    {
    case 0:
        return FanControl::FanModeEnum::kOff;
    case 1:
        return FanControl::FanModeEnum::kLow;
    case 2:
        return FanControl::FanModeEnum::kMedium;
    default:
        return FanControl::FanModeEnum::kHigh;
    }
}

void SendFan(uint8_t speed, bool force = false)
{
    if (!force && speed == sLastFanCommand)
    {
        return;
    }

    if (hood_uart_write(static_cast<uint8_t>('0' + speed)) == 0)
    {
        sLastFanCommand = speed;
        ChipLogProgress(NotSpecified, "Hood UART fan command: %u", static_cast<unsigned>(speed));
    }
    else
    {
        sLastFanCommand = 0xff;
        ChipLogError(NotSpecified, "Hood UART fan write failed");
    }
}

void SendLight(bool on, bool force = false)
{
    const int8_t state = on ? 1 : 0;
    if (!force && state == sLastLightCommand)
    {
        return;
    }

    if (hood_uart_write(on ? 'L' : 'l') == 0)
    {
        sLastLightCommand = state;
        ChipLogProgress(NotSpecified, "Hood UART light command: %s", on ? "on" : "off");
    }
    else
    {
        sLastLightCommand = -1;
        ChipLogError(NotSpecified, "Hood UART light write failed");
    }
}

void SendAllOff()
{
    if (hood_uart_write('X') == 0)
    {
        sLastFanCommand   = 0;
        sLastLightCommand = 0;
        ChipLogProgress(NotSpecified, "Hood UART command: all off");
    }
    else
    {
        sLastFanCommand   = 0xff;
        sLastLightCommand = -1;
        ChipLogError(NotSpecified, "Hood UART all-off write failed");
    }
}

} // namespace

void HoodController::Init()
{
    if (hood_uart_init() != 0)
    {
        ChipLogError(NotSpecified, "Hood UART1 init failed (D23/D25, 1200 8N1)");
        return;
    }

    ChipLogProgress(NotSpecified, "Hood UART1 ready: TX=D23 RX=D25, 1200 8N1");
    ApplyFanSetting();
    ApplyLightSetting();
}

void HoodController::ApplyFanSetting()
{
    Nullable<uint8_t> speedSetting;
    uint8_t speed = 0;

    if (FanControl::Attributes::SpeedSetting::Get(kFanEndpoint, speedSetting) == Status::Success && !speedSetting.IsNull())
    {
        speed = speedSetting.Value();
    }
    else
    {
        Nullable<Percent> percentSetting;
        if (FanControl::Attributes::PercentSetting::Get(kFanEndpoint, percentSetting) == Status::Success &&
            !percentSetting.IsNull())
        {
            speed = static_cast<uint8_t>((percentSetting.Value() * kFanSpeedMax + 99U) / 100U);
        }
    }

    if (speed > kFanSpeedMax)
    {
        speed = kFanSpeedMax;
    }
    ApplyFanSpeed(speed, true);
}

void HoodController::ApplyFanMode()
{
    FanControl::FanModeEnum mode = FanControl::FanModeEnum::kOff;
    Nullable<uint8_t> speedSetting;
    uint8_t currentSpeed = 0;
    uint8_t requestedSpeed;

    if (FanControl::Attributes::FanMode::Get(kFanEndpoint, &mode) != Status::Success)
    {
        return;
    }
    if (FanControl::Attributes::SpeedSetting::Get(kFanEndpoint, speedSetting) == Status::Success && !speedSetting.IsNull())
    {
        currentSpeed = speedSetting.Value();
    }

    switch (mode)
    {
    case FanControl::FanModeEnum::kOff:
        requestedSpeed = 0;
        break;
    case FanControl::FanModeEnum::kLow:
        requestedSpeed = 1;
        break;
    case FanControl::FanModeEnum::kMedium:
        requestedSpeed = 2;
        break;
    case FanControl::FanModeEnum::kHigh:
    case FanControl::FanModeEnum::kOn:
        requestedSpeed = (currentSpeed == 4U) ? 4U : 3U;
        break;
    default:
        return;
    }

    Nullable<uint8_t> requested(requestedSpeed);
    if (currentSpeed != requestedSpeed)
    {
        FanControl::Attributes::SpeedSetting::Set(kFanEndpoint, requested);
    }
    ApplyFanSpeed(requestedSpeed, false);
}

void HoodController::ApplyFanSpeed(uint8_t speed, bool updateMode)
{
    const Percent percent  = static_cast<Percent>(speed * 25U);
    uint8_t speedCurrent   = 0;
    Percent percentCurrent = 0;

    if (FanControl::Attributes::SpeedCurrent::Get(kFanEndpoint, &speedCurrent) != Status::Success || speedCurrent != speed)
    {
        FanControl::Attributes::SpeedCurrent::Set(kFanEndpoint, speed);
    }
    if (FanControl::Attributes::PercentCurrent::Get(kFanEndpoint, &percentCurrent) != Status::Success || percentCurrent != percent)
    {
        FanControl::Attributes::PercentCurrent::Set(kFanEndpoint, percent);
    }

    if (updateMode)
    {
        FanControl::FanModeEnum currentMode       = FanControl::FanModeEnum::kOff;
        const FanControl::FanModeEnum desiredMode = SpeedToMode(speed);
        if (FanControl::Attributes::FanMode::Get(kFanEndpoint, &currentMode) != Status::Success || currentMode != desiredMode)
        {
            FanControl::Attributes::FanMode::Set(kFanEndpoint, desiredMode);
        }
    }

    SendFan(speed);
}

void HoodController::ApplyLightSetting()
{
    bool on = false;
    if (OnOff::Attributes::OnOff::Get(kLightEndpoint, &on) != Status::Success)
    {
        return;
    }

    SendLight(on);
}

void HoodController::SetLocalMode(LocalMode mode)
{
    const uint8_t modeValue = static_cast<uint8_t>(mode);
    if (modeValue >= static_cast<uint8_t>(LocalMode::kCount))
    {
        ChipLogError(NotSpecified, "D27 mode change: invalid mode %u", static_cast<unsigned>(modeValue));
        return;
    }

    const bool nextLight    = mode != LocalMode::kOff;
    const uint8_t nextSpeed = modeValue >= static_cast<uint8_t>(LocalMode::kLightAndSpeed1)
        ? static_cast<uint8_t>(modeValue - static_cast<uint8_t>(LocalMode::kLight))
        : 0U;

    const Nullable<uint8_t> requestedSpeed(nextSpeed);
    const Nullable<Percent> requestedPercent(static_cast<Percent>(nextSpeed * 25U));

    const Status speedStatus   = FanControl::Attributes::SpeedSetting::Set(kFanEndpoint, requestedSpeed);
    const Status percentStatus = FanControl::Attributes::PercentSetting::Set(kFanEndpoint, requestedPercent);
    const Status lightStatus   = OnOff::Attributes::OnOff::Set(kLightEndpoint, nextLight);

    if (speedStatus != Status::Success || percentStatus != Status::Success || lightStatus != Status::Success)
    {
        ChipLogError(NotSpecified, "D27 mode change: failed to update Matter attributes");
        return;
    }

    // The hood can also be changed by another controller, so a local button
    // action must transmit the complete requested state even when our cached
    // Matter state already has the same values.
    if (mode == LocalMode::kOff)
    {
        SendAllOff();
    }
    else
    {
        SendFan(nextSpeed, true);
        SendLight(true, true);
    }

    // Keep the reported Matter state coherent. Attribute callbacks may
    // schedule another pass; the UART cache suppresses those duplicate bytes.
    ApplyFanSetting();
    ApplyLightSetting();
    ChipLogProgress(NotSpecified, "D27 mode: light=%s fan=%u", nextLight ? "on" : "off", static_cast<unsigned>(nextSpeed));
}
