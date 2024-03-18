#include "../stdafx.h"

#include "cm_survey.hpp"

#include "../openttd.h"

#include "../safeguards.h"

namespace citymania {

void SurveyGameSession(nlohmann::json &survey) {
    for (auto& [key, value]: _game_session_stats.cm.hotkeys) {
        survey["cm_hotkeys"][key] = value;
    }
}

} // namespace citymania
