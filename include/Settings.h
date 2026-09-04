#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Settings {
    enum class ActivationTiming : std::uint8_t { PlayerDeath = 0, PlayerRespawn, Reencounter };
    struct Tier {
        int rank{1};
        std::string nameTemplate{"{name}"};
        bool continuePreviousName{false};
    };
    struct Config {
        bool enabled{true};
        bool allowMultipleNemeses{false};
        int maximumNemeses{0};
        int maximumRank{10};
        std::string excludedPerk;
        std::string excludedPerkEditorID;
        RE::FormID excludedPerkID{};
        ActivationTiming activationTiming{ActivationTiming::PlayerDeath};
        int namePriority{50};
        std::vector<Tier> tiers;
    };

    Config& Get();
    void Load();
    void Save();
    void ResolveForms();
    std::string_view GetNameTemplate(std::uint32_t a_rank);
    std::string BuildTierName(std::uint32_t a_rank, std::string_view a_originalName);
    void RegisterMenu();
}
