#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_PORTAL_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_PORTAL_H

#include <components/settings/settingvalue.hpp>

namespace Settings
{
    struct PortalCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<bool> mDebugGeometry{ mIndex, "Portal", "debug geometry" };
    };
}

#endif
