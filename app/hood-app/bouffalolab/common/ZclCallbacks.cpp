#include "AppTask.h"

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app::Clusters;

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & path, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    (void) type;
    (void) size;
    (void) value;

    if (path.mEndpointId == 1 && path.mClusterId == FanControl::Id)
    {
        if (path.mAttributeId == FanControl::Attributes::FanMode::Id)
        {
            GetAppTask().PostEvent(AppTask::APP_EVENT_FAN_MODE);
        }
        else if (path.mAttributeId == FanControl::Attributes::PercentSetting::Id ||
                 path.mAttributeId == FanControl::Attributes::SpeedSetting::Id)
        {
            GetAppTask().PostEvent(AppTask::APP_EVENT_FAN_SETTING);
        }
    }
    else if (path.mEndpointId == 2 && path.mClusterId == OnOff::Id &&
             path.mAttributeId == OnOff::Attributes::OnOff::Id)
    {
        GetAppTask().PostEvent(AppTask::APP_EVENT_LIGHT_SETTING);
    }
}
