#ifndef CM_SETTINGS_HPP
#define CM_SETTINGS_HPP

#include "cm_type.hpp"

#include <memory>
#include <vector>
#include <string>

#include "../cargo_type.h"

namespace citymania {

enum class ModKey : uint8 {
    NONE = 0,
    SHIFT = 1,
    CTRL = 2,
    ALT = 3,
    COMMAND = 4,
    END,
};

struct EconomySettings {
    bool cashback_for_extra_land_clear;
    bool cashback_for_bridges_and_tunnels;
    bool cashback_for_foundations;
};

struct LimitsSettings {
    uint16_t max_airports; ///< maximum number of airports per company, 0=unlimited
    bool disable_canals;
    uint16_t min_distance_between_docks; ///< docks can be build only x tiles apart another, 0=disable check
    uint8_t min_advertisement_action;  // minimum level of advertisement allowed
};

class CBRequirement {
public:
    CargoType cargo_type;
    uint32_t from;
    uint32_t amount;
    uint8_t decay;
    uint8_t index;
    std::string name;
    bool has_storage;

    static CBRequirement Parse(const char *name, const char *value, uint8_t index);

    CBRequirement(CargoType cargo_type, uint32_t from, uint32_t amount, uint8_t decay,
                  uint8_t index, std::string name)
        :cargo_type{cargo_type}, from{from}, amount{amount}, decay{decay},
            index{index}, name{name}, has_storage{decay < 100} {}
};

struct CBSettings {
    uint8_t requirements_type;  // 0 - regular 1 - income-based requirements (new cb only)
    std::vector<CBRequirement> requirements;
    uint16_t acceptance_range;  // How far can station be to count towards requiremnts
    uint8_t storage_size;  // cargo storage multiplier (x * monthly requirements)
    uint8_t town_protection_range;  // Claimed town protection range (square from centre), overlaped with tz0
    uint16_t claim_max_houses;  // Max amount of houses claimable town can have
    bool smooth_growth_rate;  // Calculate growth rate precisely instead of rounding to 50 houses and allow going below 70 ticks (default max)
    bool allow_negative_growth;  // Make town shrink (with the same speed as growth) if requirements aren't satisfied
};

struct Settings {
    CBSettings cb;
    EconomySettings economy;
    LimitsSettings limits;

    GameType game_type;  // GameType
    ControllerType controller_type;  // ControllerType
    uint8_t max_players_in_company;
    uint16_t destroyed_houses_per_month;  // max amount of houses a company can destroy per month
    uint16_t game_length_years;  // game length in years(0 = disabled)
    bool protect_funded_industries;
    uint16_t same_depot_sell_years;  // can only sell vehicles in the same place (20 tiles radius) for first x yearss of its lifetime (0 = disabled)
};

}; // namespace citymania

#endif
