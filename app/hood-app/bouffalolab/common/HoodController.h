#pragma once

#include <stdint.h>

class HoodController
{
public:
    static void Init();
    static void ApplyFanSetting();
    static void ApplyFanMode();
    static void ApplyLightSetting();
    static void CycleLocalMode();

private:
    static void ApplyFanSpeed(uint8_t speed, bool updateMode);
};
