#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_PORTAL_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_PORTAL_H

#include <components/settings/settingvalue.hpp>

namespace Settings
{
    struct PortalCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<bool> mDebugGeometry{ mIndex, "Portal", "debug geometry" };
        SettingValue<bool> mShowActors{ mIndex, "Portal", "show actors", false };

        // Semicolon-separated interior cell names whose ex_common_door_01 portals need
        // the quad offset along its outward axis (L-shaped entrance frame alignment).
        SettingValue<std::string> mForwardCells{ mIndex, "Portal", "portal forward cells" };

        // Units to shift the quad for cells in mForwardCells.
        // Positive = into entrance frame (inward); negative = toward approaching player.
        SettingValue<float> mForwardOffset{ mIndex, "Portal", "portal forward offset" };
    };
}

#endif
