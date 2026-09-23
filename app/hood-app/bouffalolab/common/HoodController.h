#pragma once

#include <stdint.h>

class HoodController
{
public:
    enum class LocalMode : uint8_t
    {
        kOff = 0,
        kLight,
        kLightAndSpeed1,
        kLightAndSpeed2,
        kLightAndSpeed3,
        kCount,
    };

    static void Init();
    static void ApplyFanSetting();
    static void ApplyFanMode();
    static void ApplyLightSetting();
    static void SetLocalMode(LocalMode mode);

private:
    static void ApplyFanSpeed(uint8_t speed, bool updateMode);
};
