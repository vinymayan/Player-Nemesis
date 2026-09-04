#include "Settings.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <map>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "Manager.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace ImGui = ImGuiMCP;

namespace {
    constexpr const char* kModDirectory = "Data/Viny Mods/Give me another fight";
    constexpr const char* kSettingsPath = "Data/Viny Mods/Give me another fight/Settings.json";
    constexpr const char* kLanguagePath = "Data/Viny Mods/Give me another fight/Language.json";
    Settings::Config config;
    std::unordered_map<std::string, std::string> language;

    void Clamp() {
        config.maximumNemeses = std::max(config.maximumNemeses, 0);
        config.maximumRank = std::clamp(config.maximumRank, 1, 127);
        config.namePriority = std::clamp(config.namePriority, -1000, 1000);
        if (std::to_underlying(config.activationTiming) > std::to_underlying(Settings::ActivationTiming::Reencounter)) {
            config.activationTiming = Settings::ActivationTiming::PlayerDeath;
        }
        if (config.tiers.size() < static_cast<std::size_t>(config.maximumRank)) {
            config.tiers.resize(static_cast<std::size_t>(config.maximumRank));
        }
        for (std::size_t index = 0; index < config.tiers.size(); ++index) {
            config.tiers[index].rank = static_cast<int>(index + 1);
            if (config.tiers[index].nameTemplate.empty()) config.tiers[index].nameTemplate = "{name}";
            if (index == 0) config.tiers[index].continuePreviousName = false;
        }
    }

    void FlattenLanguage(const rapidjson::Value& a_object, const std::string& a_prefix) {
        for (auto member = a_object.MemberBegin(); member != a_object.MemberEnd(); ++member) {
            const auto key = a_prefix.empty() ? member->name.GetString() : a_prefix + "." + member->name.GetString();
            if (member->value.IsString())
                language[key] = member->value.GetString();
            else if (member->value.IsObject())
                FlattenLanguage(member->value, key);
        }
    }

    const char* GetLoc(std::string_view a_key, const char* a_fallback) {
        const auto found = language.find(std::string(a_key));
        return found == language.end() ? a_fallback : found->second.c_str();
    }

    std::string ApplyNameTemplate(std::string_view a_template, std::string_view a_baseName) {
        std::string result(a_template);
        constexpr std::string_view token = "{name}";
        std::size_t position = 0;
        while ((position = result.find(token, position)) != std::string::npos) {
            result.replace(position, token.size(), a_baseName);
            position += a_baseName.size();
        }
        return result.empty() ? std::string(a_baseName) : result;
    }

    void LoadLanguage() {
        language.clear();
        std::ifstream file(kLanguagePath, std::ios::binary);
        if (!file) return;
        std::stringstream stream;
        stream << file.rdbuf();
        rapidjson::Document document;
        document.Parse(stream.str().c_str());
        if (!document.HasParseError() && document.IsObject()) FlattenLanguage(document, {});
    }

    bool RenderInt(const char* a_label, int* a_value, int a_minimum, int a_maximum) {
        bool changed = false;
        ImGui::PushID(a_label);
        ImGui::SetNextItemWidth(200.0F);
        changed |= ImGui::SliderInt("##slider", a_value, a_minimum, a_maximum);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0F);
        changed |= ImGui::InputInt(a_label, a_value);
        *a_value = std::clamp(*a_value, a_minimum, a_maximum);
        ImGui::PopID();
        return changed;
    }

    bool RenderNonNegativeInt(const char* a_label, int* a_value) {
        ImGui::SetNextItemWidth(200.0F);
        const bool changed = ImGui::InputInt(a_label, a_value);
        *a_value = std::max(*a_value, 0);
        return changed;
    }

    bool DrawDropdown(const char* a_label, const std::string& a_category, RE::FormID& a_currentFormID,
                      float a_customWidth = -1.0F) {
        bool changed = false;
        const auto& fullList = Manager::GetSingleton()->GetList(a_category);
        if (fullList.empty()) return false;

        std::vector<const char*> comboItems;
        std::vector<int> mapToFull;
        comboItems.push_back(GetLoc("menu.opt_none", "None"));
        mapToFull.push_back(-1);

        int localSelection = 0;
        for (std::size_t index = 0; index < fullList.size(); ++index) {
            comboItems.push_back(fullList[index].cachedDisplayName.c_str());
            mapToFull.push_back(static_cast<int>(index));
            if (fullList[index].formID == a_currentFormID) {
                localSelection = static_cast<int>(index) + 1;
            }
        }

        ImGui::PushID(a_label);
        std::string displayLabel = a_label;
        const auto hashPosition = displayLabel.find("##");
        if (hashPosition != std::string::npos) displayLabel = displayLabel.substr(0, hashPosition);
        ImGui::Text("%s:", displayLabel.c_str());
        ImGui::SameLine();

        if (a_customWidth > 0.0F) ImGui::SetNextItemWidth(a_customWidth);
        const char* previewValue = comboItems[localSelection];
        if (ImGui::BeginCombo("##drop", previewValue)) {
            static std::map<std::string, std::string> searchBuffers;
            char searchBuffer[256] = "";
            if (searchBuffers.contains(a_label)) strcpy_s(searchBuffer, searchBuffers[a_label].c_str());

            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputText("##busca", searchBuffer, sizeof(searchBuffer))) {
                searchBuffers[a_label] = searchBuffer;
            }
            ImGui::Separator();

            std::string searchText = searchBuffer;
            std::transform(searchText.begin(), searchText.end(), searchText.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

            ImGui::BeginChild("##scroll", ImGuiMCP::ImVec2(0, 200), false);
            for (int index = 0; index < static_cast<int>(comboItems.size()); ++index) {
                std::string itemLower = comboItems[index];
                std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(),
                               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                if (searchText.empty() || itemLower.find(searchText) != std::string::npos) {
                    const bool selected = localSelection == index;
                    if (ImGui::Selectable(comboItems[index], selected)) {
                        localSelection = index;
                        const int originalIndex = mapToFull[localSelection];
                        a_currentFormID = originalIndex == -1 ? 0 : fullList[originalIndex].formID;
                        searchBuffers[a_label] = "";
                        changed = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndChild();
            ImGui::EndCombo();
        }

        ImGui::PopID();
        return changed;
    }

    void SyncExcludedPerkPersistence() {
        if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(config.excludedPerkID)) {
            config.excludedPerk = FormUtil::NormalizeFormID(perk);
            const char* editorID = perk->GetFormEditorID();
            config.excludedPerkEditorID = editorID && editorID[0] != '\0' ? editorID : "";
        } else {
            config.excludedPerk.clear();
            config.excludedPerkEditorID.clear();
            config.excludedPerkID = 0;
        }
    }

    bool RenderPerkDropdown() {
        const bool changed = DrawDropdown(GetLoc("menu.excluded_perk", "Perk that prevents becoming a nemesis"),
                                          "Perk", config.excludedPerkID, 300.0F);
        if (changed) SyncExcludedPerkPersistence();
        return changed;
    }

    bool RenderTierTemplate(Settings::Tier& a_tier) {
        bool changed = false;
        char buffer[512]{};
        strncpy_s(buffer, sizeof(buffer), a_tier.nameTemplate.c_str(), _TRUNCATE);
        const auto label = std::format("{} {}##tier{}", GetLoc("menu.tier", "Tier"), a_tier.rank, a_tier.rank);
        ImGui::SetNextItemWidth(520.0F);
        if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
            a_tier.nameTemplate = *buffer ? buffer : "{name}";
            changed = true;
        }

        if (a_tier.rank > 1) {
            ImGui::SameLine();
            const auto continueLabel = std::format(
                "{}##continueTier{}", GetLoc("menu.continue_previous_tier_name", "Continue previous tier name"),
                a_tier.rank);
            changed |= ImGui::Checkbox(continueLabel.c_str(), &a_tier.continuePreviousName);
        }
        return changed;
    }

    void RenderMenu() {
        bool changed = false;
        changed |= ImGui::Checkbox(GetLoc("menu.enabled", "Enable Player Nemesis"), &config.enabled);
        changed |= ImGui::Checkbox(GetLoc("menu.multiple", "Allow multiple nemeses"), &config.allowMultipleNemeses);
        if (config.allowMultipleNemeses) {
            changed |= RenderNonNegativeInt(GetLoc("menu.maximum_nemeses", "Maximum simultaneous nemeses"),
                                            &config.maximumNemeses);
            ImGui::TextWrapped("%s", GetLoc("menu.maximum_nemeses_disclaimer",
                                            "0 = unlimited. The limit applies only to current nemeses; "
                                            "defeated nemeses free a slot."));
        }
        changed |= RenderInt(GetLoc("menu.maximum_rank", "Maximum nemesis rank"), &config.maximumRank, 1, 127);
        changed |= RenderPerkDropdown();

        const char* timings[]{GetLoc("menu.timing_death", "When the player dies"),
                              GetLoc("menu.timing_respawn", "When the player respawns"),
                              GetLoc("menu.timing_reencounter", "When the player encounters the NPC again")};
        int timing = std::to_underlying(config.activationTiming);
        ImGui::SetNextItemWidth(420.0F);
        if (ImGui::Combo(GetLoc("menu.activation_timing", "Become a nemesis"), &timing, timings, 3)) {
            config.activationTiming = static_cast<Settings::ActivationTiming>(timing);
            changed = true;
        }
        // changed |= RenderInt(GetLoc("menu.name_priority", "Name priority"), &config.namePriority, -1000, 1000);

        Clamp();
        if (ImGui::CollapsingHeader(GetLoc("menu.name_tiers", "Nemesis names by tier"))) {
            ImGui::TextWrapped(
                "%s", GetLoc("menu.name_help",
                             "Use {name} as the name base. When 'Continue previous tier name' is enabled, {name} "
                             "means the final name from the previous tier; otherwise it means the original NPC name. "
                             "Omit {name} to replace the complete name."));
            for (int index = 0; index < config.maximumRank; ++index) {
                changed |= RenderTierTemplate(config.tiers[static_cast<std::size_t>(index)]);
            }
        }
        if (changed) {
            Settings::Save();
            Manager::GetSingleton()->OnSettingsChanged();
        }
    }
}

Settings::Config& Settings::Get() { return config; }

void Settings::Load() {
    config = Config{};
    std::ifstream file(kSettingsPath, std::ios::binary);
    if (file) {
        std::stringstream stream;
        stream << file.rdbuf();
        rapidjson::Document document;
        document.Parse(stream.str().c_str());
        if (!document.HasParseError() && document.IsObject()) {
            if (document.HasMember("enabled") && document["enabled"].IsBool())
                config.enabled = document["enabled"].GetBool();
            if (document.HasMember("allowMultipleNemeses") && document["allowMultipleNemeses"].IsBool())
                config.allowMultipleNemeses = document["allowMultipleNemeses"].GetBool();
            if (document.HasMember("maximumNemeses") && document["maximumNemeses"].IsInt())
                config.maximumNemeses = document["maximumNemeses"].GetInt();
            if (document.HasMember("maximumRank") && document["maximumRank"].IsInt())
                config.maximumRank = document["maximumRank"].GetInt();
            if (document.HasMember("excludedPerk") && document["excludedPerk"].IsString())
                config.excludedPerk = document["excludedPerk"].GetString();
            if (document.HasMember("excludedPerkEditorID") && document["excludedPerkEditorID"].IsString())
                config.excludedPerkEditorID = document["excludedPerkEditorID"].GetString();
            if (document.HasMember("namePriority") && document["namePriority"].IsInt())
                config.namePriority = document["namePriority"].GetInt();
            if (document.HasMember("activationTiming") && document["activationTiming"].IsString()) {
                const std::string_view timing = document["activationTiming"].GetString();
                if (timing == "PlayerRespawn")
                    config.activationTiming = ActivationTiming::PlayerRespawn;
                else if (timing == "Reencounter")
                    config.activationTiming = ActivationTiming::Reencounter;
            }
            if (document.HasMember("tiers") && document["tiers"].IsArray()) {
                for (const auto& value : document["tiers"].GetArray()) {
                    if (!value.IsObject() || !value.HasMember("rank") || !value["rank"].IsInt() ||
                        !value.HasMember("nameTemplate") || !value["nameTemplate"].IsString())
                        continue;
                    const auto rank = value["rank"].GetInt();
                    if (rank < 1 || rank > 127) continue;
                    if (config.tiers.size() < static_cast<std::size_t>(rank)) config.tiers.resize(rank);
                    const bool continuePreviousName =
                        value.HasMember("continuePreviousName") && value["continuePreviousName"].IsBool()
                            ? value["continuePreviousName"].GetBool()
                            : false;
                    config.tiers[rank - 1] = {rank, value["nameTemplate"].GetString(), continuePreviousName};
                }
            }
        } else {
            logger::error("Could not parse {}", kSettingsPath);
        }
    }
    Clamp();
    ResolveForms();
}

void Settings::Save() {
    Clamp();
    if (config.excludedPerkID != 0) {
        SyncExcludedPerkPersistence();
    } else {
        config.excludedPerk.clear();
        config.excludedPerkEditorID.clear();
    }
    std::error_code error;
    std::filesystem::create_directories(kModDirectory, error);
    if (error) {
        logger::error("Could not create settings directory: {}", error.message());
        return;
    }
    rapidjson::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("enabled", config.enabled, allocator);
    document.AddMember("allowMultipleNemeses", config.allowMultipleNemeses, allocator);
    document.AddMember("maximumNemeses", config.maximumNemeses, allocator);
    document.AddMember("maximumRank", config.maximumRank, allocator);
    document.AddMember("excludedPerk", rapidjson::Value(config.excludedPerk.c_str(), allocator), allocator);
    if (!config.excludedPerkEditorID.empty()) {
        document.AddMember("excludedPerkEditorID", rapidjson::Value(config.excludedPerkEditorID.c_str(), allocator),
                           allocator);
    }
    const char* timing = config.activationTiming == ActivationTiming::PlayerRespawn ? "PlayerRespawn"
                         : config.activationTiming == ActivationTiming::Reencounter ? "Reencounter"
                                                                                    : "PlayerDeath";
    document.AddMember("activationTiming", rapidjson::Value(timing, allocator), allocator);
    document.AddMember("namePriority", config.namePriority, allocator);
    rapidjson::Value tiers(rapidjson::kArrayType);
    for (const auto& tier : config.tiers) {
        rapidjson::Value value(rapidjson::kObjectType);
        value.AddMember("rank", tier.rank, allocator);
        value.AddMember("nameTemplate", rapidjson::Value(tier.nameTemplate.c_str(), allocator), allocator);
        value.AddMember("continuePreviousName", tier.continuePreviousName, allocator);
        tiers.PushBack(value, allocator);
    }
    document.AddMember("tiers", tiers, allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    std::ofstream output(kSettingsPath, std::ios::binary | std::ios::trunc);
    if (output) output << buffer.GetString();
}

void Settings::ResolveForms() {
    config.excludedPerkID = 0;
    if (!config.excludedPerkEditorID.empty()) {
        if (auto* form = RE::TESForm::LookupByEditorID(config.excludedPerkEditorID)) {
            if (form->As<RE::BGSPerk>()) {
                config.excludedPerkID = form->GetFormID();
                return;
            }
        }
    }

    const auto resolvedID = FormUtil::FormIDFromString(config.excludedPerk);
    if (resolvedID != 0) {
        if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(resolvedID)) {
            config.excludedPerkID = perk->GetFormID();
            const char* editorID = perk->GetFormEditorID();
            if (editorID && editorID[0] != '\0') config.excludedPerkEditorID = editorID;
        }
    }
}

std::string_view Settings::GetNameTemplate(std::uint32_t a_rank) {
    Clamp();
    const auto index = std::clamp<std::uint32_t>(a_rank, 1, config.maximumRank) - 1;
    return config.tiers[index].nameTemplate;
}

std::string Settings::BuildTierName(std::uint32_t a_rank, std::string_view a_originalName) {
    Clamp();
    const auto lastIndex = std::clamp<std::uint32_t>(a_rank, 1, config.maximumRank) - 1;
    std::string resolvedName(a_originalName);

    for (std::size_t index = 0; index <= lastIndex; ++index) {
        const auto& tier = config.tiers[index];
        const std::string_view baseName =
            index > 0 && tier.continuePreviousName ? std::string_view(resolvedName) : a_originalName;
        auto nextName = ApplyNameTemplate(tier.nameTemplate, baseName);
        resolvedName = std::move(nextName);
    }

    return resolvedName.empty() ? std::string(a_originalName) : resolvedName;
}

void Settings::RegisterMenu() {
    LoadLanguage();
    Load();
    if (!SKSEMenuFramework::IsInstalled()) return;
    SKSEMenuFramework::SetSection(GetLoc("menu.section", "Give me another fight"));
    SKSEMenuFramework::AddSectionItem(GetLoc("menu.settings", "Settings"), RenderMenu);
}
