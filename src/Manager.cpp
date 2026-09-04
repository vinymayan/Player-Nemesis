#include "Manager.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <limits>
#include <ranges>
#include <utility>

#include "Settings.h"

namespace {
    constexpr std::uint32_t kNemesisRecord = 'NEMS';
    constexpr std::uint32_t kNemesisKilledRecord = 'NKIL';
    constexpr std::uint32_t kRecordVersion = 1;
    constexpr std::uint32_t kNemesisKilledRecordVersion = 1;
    constexpr std::string_view kFactionEditorID = "PlayerNemesisFaction";
    constexpr std::string_view kNameMutationKey = "player-nemesis/name";
    constexpr std::uint32_t kMaximumStringLength = 4096;

    template <class T>
    bool WriteValue(SKSE::SerializationInterface* a_serialization, const T& a_value) {
        return a_serialization->WriteRecordData(std::addressof(a_value), sizeof(T));
    }

    template <class T>
    bool ReadValue(SKSE::SerializationInterface* a_serialization, T& a_value) {
        return a_serialization->ReadRecordData(std::addressof(a_value), sizeof(T)) == sizeof(T);
    }

    bool WriteString(SKSE::SerializationInterface* a_serialization, std::string_view a_value) {
        const auto size = static_cast<std::uint32_t>(a_value.size());
        return WriteValue(a_serialization, size) &&
               (size == 0 || a_serialization->WriteRecordData(a_value.data(), size));
    }

    bool ReadString(SKSE::SerializationInterface* a_serialization, std::string& a_value) {
        std::uint32_t size = 0;
        if (!ReadValue(a_serialization, size) || size > kMaximumStringLength) return false;
        a_value.resize(size);
        return size == 0 || a_serialization->ReadRecordData(a_value.data(), size) == size;
    }

    bool IncludesSignature(std::string_view a_signatures, std::string_view a_wanted) {
        std::size_t begin = 0;
        while (begin <= a_signatures.size()) {
            const auto end = a_signatures.find(',', begin);
            auto token =
                a_signatures.substr(begin, end == std::string_view::npos ? a_signatures.size() - begin : end - begin);
            while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
            while (!token.empty() && token.back() == ' ') token.remove_suffix(1);
            if (token == a_wanted) return true;
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
        return false;
    }

    RE::Actor* LookupLoadedActor(RE::FormID a_formID) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
        return actor && actor->Is3DLoaded() ? actor : nullptr;
    }

    RE::Actor* ResolveResponsibleActor(RE::Actor* a_actor) {
        auto* current = a_actor;
        for (std::uint32_t depth = 0; current && depth < 8; ++depth) {
            auto commander = current->GetCommandingActor();
            auto* commanderActor = commander.get();
            if (!commanderActor || commanderActor == current) {
                break;
            }

            current = commanderActor;
        }
        return current;
    }

    void RunSelfChecks() {
#ifndef NDEBUG
        assert(kMaximumStringLength > 0);
#endif
    }
}

namespace FormUtil {
    const RE::TESFile* GetMasterFile(RE::TESForm* a_form) {
        if (!a_form) return nullptr;
        const auto formID = a_form->GetFormID();
        const auto modIndex = static_cast<std::uint8_t>(formID >> 24);
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return nullptr;
        if (modIndex == 0xFE) {
            return dataHandler->LookupLoadedLightModByIndex(static_cast<std::uint16_t>((formID >> 12) & 0xFFF));
        }
        return dataHandler->LookupLoadedModByIndex(modIndex);
    }

    std::string NormalizeFormID(RE::TESForm* a_form) {
        if (!a_form) return {};
        const auto formID = a_form->GetFormID();
        const auto modIndex = static_cast<std::uint8_t>(formID >> 24);
        if (modIndex == 0xFF) {
            try {
                if (auto editorID = clib_util::editorID::get_editorID(a_form); !editorID.empty()) return editorID;
            } catch (...) {
            }
            return std::format("Dynamic|{:08X}", formID);
        }
        const auto* file = GetMasterFile(a_form);
        if (!file) return std::format("{:08X}", formID);
        const auto localID = modIndex == 0xFE ? formID & 0xFFF : formID & 0x00FFFFFF;
        return std::format("{}|{:X}", file->GetFilename(), localID);
    }

    RE::FormID FormIDFromString(const std::string& a_value) {
        if (a_value.empty()) return 0;
        if (auto* form = RE::TESForm::LookupByEditorID(a_value)) return form->GetFormID();
        const auto separator = a_value.find('|');
        if (separator != std::string::npos && !a_value.starts_with("Dynamic|")) {
            try {
                const auto localID = static_cast<RE::FormID>(std::stoul(a_value.substr(separator + 1), nullptr, 16));
                auto* dataHandler = RE::TESDataHandler::GetSingleton();
                return dataHandler ? dataHandler->LookupFormID(localID, a_value.substr(0, separator)) : 0;
            } catch (...) {
                return 0;
            }
        }
        try {
            const auto value = separator == std::string::npos ? a_value : a_value.substr(separator + 1);
            return static_cast<RE::FormID>(std::stoul(value, nullptr, 16));
        } catch (...) {
            return 0;
        }
    }
}

void InternalFormInfo::UpdateDisplayName() {
    const auto base = !name.empty() ? name : (!editorID.empty() ? editorID : "Unknown");
    cachedDisplayName = std::format("{} [{:08X}]", base, formID);
}

Manager* Manager::GetSingleton() {
    static Manager singleton;
    return &singleton;
}

void Manager::PopulateAllLists() {
    if (_listsPopulated) return;
    PopulateList<RE::BGSPerk>("Perk");
    _listsPopulated = true;
    Settings::ResolveForms();
}

void Manager::RefreshLists(std::string_view a_signatures) {
    if (a_signatures.empty() || IncludesSignature(a_signatures, "All") || IncludesSignature(a_signatures, "PERK")) {
        PopulateList<RE::BGSPerk>("Perk");
        _listsPopulated = true;
        Settings::ResolveForms();
    }
    if (a_signatures.empty() || IncludesSignature(a_signatures, "All") || IncludesSignature(a_signatures, "FACT")) {
        _factionFormID = 0;
        EnsureFaction();
    }
}

const std::vector<InternalFormInfo>& Manager::GetList(const std::string& a_typeName) {
    static const std::vector<InternalFormInfo> empty;
    const auto found = _dataStore.find(a_typeName);
    return found == _dataStore.end() ? empty : found->second;
}

std::string Manager::ToUTF8(std::string_view a_text) {
    if (a_text.empty()) return {};

    const int utf8Test = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a_text.data(),
                                              static_cast<int>(a_text.size()), nullptr, 0);
    if (utf8Test > 0) return std::string(a_text);

    const int wideLength = MultiByteToWideChar(CP_ACP, 0, a_text.data(), static_cast<int>(a_text.size()), nullptr, 0);
    if (wideLength <= 0) return std::string(a_text);

    std::wstring wideText(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, a_text.data(), static_cast<int>(a_text.size()), wideText.data(), wideLength);

    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), wideLength, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) return std::string(a_text);

    std::string utf8Text(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), wideLength, utf8Text.data(), utf8Length, nullptr, nullptr);
    return utf8Text;
}

template <typename T>
void Manager::PopulateList(const std::string& a_typeName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    auto& list = _dataStore[a_typeName];
    list.clear();
    const auto& forms = dataHandler->GetFormArray<T>();
    list.reserve(forms.size());

    for (auto* form : forms) {
        if (!form || form->IsDeleted() || form->IsIgnored()) continue;

        RE::FormID currentID = 0;
        std::string currentPlugin = "Unknown";
        try {
            currentID = form->GetFormID();
            if (auto* file = form->GetFile(0)) {
                currentPlugin = file->GetFilename();
            } else {
                currentPlugin = "Dynamic";
            }

            InternalFormInfo info;
            info.formID = currentID;
            info.formType = a_typeName;
            info.pluginName = ToUTF8(currentPlugin);
            info.editorID = ToUTF8(clib_util::editorID::get_editorID(form));

            std::string rawName;
            if (auto* fullName = form->template As<RE::TESFullName>()) rawName = fullName->fullName.c_str();
            info.name = ToUTF8(rawName);
            info.UpdateDisplayName();
            list.push_back(std::move(info));
        } catch (const std::exception& exception) {
            logger::error("[PopulateList] Error on item {:08X} of plugin '{}' (Type: {}): {}", currentID,
                          currentPlugin, a_typeName, exception.what());
        } catch (...) {
            logger::error("[PopulateList] Unknown error on item {:08X} of plugin '{}' (Type: {})", currentID,
                          currentPlugin, a_typeName);
        }
    }

    logger::info("Loaded {} {} forms", list.size(), a_typeName);
}

RE::TESFaction* Manager::GetFaction() {
    if (_factionFormID != 0) {
        if (auto* faction = RE::TESForm::LookupByID<RE::TESFaction>(_factionFormID)) return faction;
        _factionFormID = 0;
    }
    if (auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>(kFactionEditorID)) {
        _factionFormID = faction->GetFormID();
        return faction;
    }
    return nullptr;
}

void Manager::EnsureFaction() {
    if (GetFaction() || _factionRequestPending) return;
    auto* api = DFG::GetAPI();
    if (!api || !api->IsReady()) return;
    DFG::LookupFormRequest request;
    request.requester = "PlayerNemesis";
    request.editorId = kFactionEditorID.data();
    _factionRequestPending = api->QueueLookupForm(&request, OnDFGLookup, nullptr);
}

void Manager::OnDFGLookup(const DFG::FormLookupResult* a_result, void*) {
    auto* manager = GetSingleton();
    manager->_factionRequestPending = false;
    if (!a_result || a_result->status != DFG::Status::Success) {
        logger::warn("Could not look up PlayerNemesisFaction: {}", a_result ? a_result->error : "no result");
        return;
    }
    if (a_result->exists) {
        if (!a_result->form) {
            logger::warn("DFG found PlayerNemesisFaction but it is not resolved yet");
            return;
        }
        auto* faction = a_result->form->As<RE::TESFaction>();
        if (!faction) {
            logger::error("DFG editor ID PlayerNemesisFaction is not a faction");
            return;
        }
        manager->_factionFormID = faction->GetFormID();
        manager->ReconcileLoadedActors();
        return;
    }

    auto* api = DFG::GetAPI();
    if (!api || !api->IsReady()) return;
    constexpr auto json =
        R"({"formKind":"Faction","sourceSignature":"FACT","editorId":"PlayerNemesisFaction","fullName":"Player Nemesis"})";
    DFG::CreateFormRequest request;
    request.requester = "PlayerNemesis";
    request.packageName = "Player Nemesis";
    request.formJson = json;
    manager->_factionRequestPending = api->QueueCreateForm(&request, OnDFGCreate, nullptr);
}

void Manager::OnDFGCreate(const DFG::FormOperationResult* a_result, void*) {
    auto* manager = GetSingleton();
    manager->_factionRequestPending = false;
    if (a_result && a_result->status == DFG::Status::Success && a_result->form) {
        if (auto* faction = a_result->form->As<RE::TESFaction>()) {
            manager->_factionFormID = faction->GetFormID();
            manager->ReconcileLoadedActors();
            logger::info("PlayerNemesisFaction ready as {:08X}", manager->_factionFormID);
            return;
        }
    }
    if (a_result && a_result->status == DFG::Status::EditorIdAlreadyExists) {
        manager->EnsureFaction();
        return;
    }
    logger::error("Could not create PlayerNemesisFaction: {}", a_result ? a_result->error : "no result");
}

WhoEditThat::API::ClientHandle Manager::GetNameClient() {
    if (_nameClient != WhoEditThat::API::kInvalidClient) return _nameClient;
    auto* api = WhoEditThat::API::GetAPI();
    if (!api || !api->IsReady()) return WhoEditThat::API::kInvalidClient;
    WhoEditThat::API::ClientRegistration registration;
    registration.clientID = "PlayerNemesis";
    registration.displayName = "Player Nemesis";
    _nameClient = api->RegisterClient(&registration);
    return _nameClient;
}

void Manager::HandlePlayerAnimationEvent(std::string_view a_eventName) {
    if (a_eventName == "TrickDeathStarted") {
        _deathOpen = true;
        _killerRecorded = false;
        return;
    }
    if (a_eventName != "TrickDeathRespawn") return;
    _deathOpen = false;
    for (auto& [formID, record] : _nemeses) {
        if (record.pending == PendingActivation::Respawn) {
            record.pending = PendingActivation::Immediate;
            if (auto* actor = LookupLoadedActor(formID)) ApplyRecord(record, actor);
        } else if (record.pending == PendingActivation::Reencounter) {
            record.reencounterReady = true;
        }
    }
}

void Manager::HandleKilledPlayer(RE::Actor* a_killer) {
    const auto& settings = Settings::Get();
    if (!settings.enabled || !_deathOpen || _killerRecorded || !a_killer || a_killer->IsPlayerRef()) return;

    auto* nemesisActor = ResolveResponsibleActor(a_killer);
    if (!nemesisActor || nemesisActor->IsPlayerRef()) {
        _killerRecorded = true;
        logger::debug("KilledPlayer source {:08X} resolves to the player or no valid master; ignoring",
                      a_killer->GetFormID());
        return;
    }

    if (nemesisActor != a_killer) {
        logger::debug("KilledPlayer source {:08X} redirected to commanding actor {:08X}", a_killer->GetFormID(),
                      nemesisActor->GetFormID());
    }
    _killerRecorded = true;

    auto found = _nemeses.find(nemesisActor->GetFormID());
    if (found == _nemeses.end()) {
        if (!settings.allowMultipleNemeses && !_nemeses.empty()) {
            logger::info("Ignored additional nemesis candidate {:08X}", nemesisActor->GetFormID());
            return;
        }
        if (settings.allowMultipleNemeses && settings.maximumNemeses > 0 &&
            _nemeses.size() >= static_cast<std::size_t>(settings.maximumNemeses)) {
            logger::info("Ignored nemesis candidate {:08X}; simultaneous nemesis limit ({}) reached",
                         nemesisActor->GetFormID(), settings.maximumNemeses);
            return;
        }
        if (settings.excludedPerkID != 0) {
            auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(settings.excludedPerkID);
            if (perk && nemesisActor->HasPerk(perk)) {
                logger::info("Actor {:08X} is excluded from becoming a nemesis", nemesisActor->GetFormID());
                return;
            }
        }
        NemesisRecord record;
        record.actorFormID = nemesisActor->GetFormID();
        record.actorKey = FormUtil::NormalizeFormID(nemesisActor);
        const auto* name = nemesisActor->GetDisplayFullName();
        record.originalName = name && *name ? name : nemesisActor->GetName();
        found = _nemeses.emplace(record.actorFormID, std::move(record)).first;
    }

    auto& record = found->second;
    ++record.killCount;
    record.reencounterReady = false;
    switch (settings.activationTiming) {
        case Settings::ActivationTiming::PlayerDeath:
            record.pending = PendingActivation::Immediate;
            ApplyRecord(record, nemesisActor);
            break;
        case Settings::ActivationTiming::PlayerRespawn:
            record.pending = PendingActivation::Respawn;
            break;
        case Settings::ActivationTiming::Reencounter:
            record.pending = PendingActivation::Reencounter;
            break;
    }
    logger::info("Nemesis '{}' ({:08X}) now has {} player kill(s)", record.originalName, record.actorFormID,
                 record.killCount);
}

void Manager::HandleActorDeath(RE::Actor* a_actor) {
    if (!a_actor || a_actor->IsPlayerRef()) return;

    const auto found = _nemeses.find(a_actor->GetFormID());
    if (found == _nemeses.end()) return;

    if (auto* faction = GetFaction()) {
        if (a_actor->IsInFaction(faction) && a_actor->GetFactionRank(faction, false) != 0) {
            a_actor->AddToFaction(faction, 0);
        }
    } else {
        EnsureFaction();
    }

    RemoveName(a_actor);
    RequestEDFReevaluation(a_actor);

    if (_nemesesKilled != std::numeric_limits<std::uint32_t>::max()) {
        ++_nemesesKilled;
    }

    logger::info("Nemesis {:08X} defeated; faction rank reset to 0. Total defeated: {}", a_actor->GetFormID(),
                 _nemesesKilled);
    _nemeses.erase(found);
}

void Manager::HandleActorLoaded(RE::Actor* a_actor) {
    if (!a_actor) return;
    const auto found = _nemeses.find(a_actor->GetFormID());
    if (found != _nemeses.end() && a_actor->IsDead()) {
        HandleActorDeath(a_actor);
        return;
    }
    if (found == _nemeses.end()) return;
    auto& record = found->second;
    if (record.pending == PendingActivation::Immediate ||
        (record.pending == PendingActivation::Reencounter && record.reencounterReady) ||
        (record.active && record.pending == PendingActivation::None)) {
        if (record.pending == PendingActivation::Reencounter) record.pending = PendingActivation::Immediate;
        ApplyRecord(record, a_actor);
    }
}

void Manager::HandleActorEncountered(RE::Actor* a_actor) {
    if (!a_actor) return;
    const auto found = _nemeses.find(a_actor->GetFormID());
    if (found == _nemeses.end()) return;
    auto& record = found->second;
    if (record.pending == PendingActivation::Reencounter && record.reencounterReady) {
        record.pending = PendingActivation::Immediate;
        ApplyRecord(record, a_actor);
    } else if (record.active && record.pending == PendingActivation::None) {
        ApplyRecord(record, a_actor);
    }
}

void Manager::ApplyRecord(NemesisRecord& a_record, RE::Actor* a_actor) {
    if (!Settings::Get().enabled || !a_actor || a_record.killCount == 0) return;
    auto* faction = GetFaction();
    if (!faction) {
        EnsureFaction();
        return;
    }
    const auto desiredRank = static_cast<std::uint8_t>(
        std::clamp<std::uint32_t>(a_record.killCount, 1, static_cast<std::uint32_t>(Settings::Get().maximumRank)));
    const auto previousRank = a_actor->IsInFaction(faction) ? a_actor->GetFactionRank(faction, false) : -1;
    if (previousRank != desiredRank) {
        a_actor->AddToFaction(faction, static_cast<std::int8_t>(desiredRank));
    }
    const auto edfAccepted =
        (previousRank == desiredRank && a_record.appliedRank == desiredRank) || RequestEDFReevaluation(a_actor);
    ApplyName(a_record, a_actor, desiredRank);
    if (edfAccepted) a_record.appliedRank = desiredRank;
    a_record.active = true;
    a_record.pending = PendingActivation::None;
    a_record.reencounterReady = false;
}

void Manager::ApplyName(const NemesisRecord& a_record, RE::Actor* a_actor, std::uint32_t a_rank) {
    auto* api = WhoEditThat::API::GetAPI();
    const auto client = GetNameClient();
    if (!api || client == WhoEditThat::API::kInvalidClient) return;
    const auto desiredName = Settings::BuildTierName(a_rank, a_record.originalName);
    WhoEditThat::API::DisplayNameRequest request;
    request.client = client;
    request.actorFormID = a_actor->GetFormID();
    request.mutationKey = kNameMutationKey.data();
    request.displayName = desiredName.c_str();
    request.priority = Settings::Get().namePriority;
    if (!api->QueueUpsertDisplayName(&request, OnNameResult, nullptr)) {
        logger::warn("WhoEditThat did not accept the name request for {:08X}", a_actor->GetFormID());
    }
}

void Manager::RemoveName(RE::Actor* a_actor) {
    auto* api = WhoEditThat::API::GetAPI();
    const auto client = GetNameClient();
    if (!a_actor || !api || client == WhoEditThat::API::kInvalidClient) return;

    WhoEditThat::API::ContributionRequest request;
    request.client = client;
    request.actorFormID = a_actor->GetFormID();
    request.mutationKey = kNameMutationKey.data();
    if (!api->QueueRemoveDisplayName(&request, OnNameRemovalResult, nullptr)) {
        logger::warn("WhoEditThat did not accept the name removal request for {:08X}", a_actor->GetFormID());
    }
}

void Manager::OnNameResult(const WhoEditThat::API::Result* a_result, void*) {
    if (!a_result || a_result->status != WhoEditThat::API::Status::kSuccess) {
        logger::warn("WhoEditThat name update failed: {}", a_result ? a_result->message : "no result");
    }
}

void Manager::OnNameRemovalResult(const WhoEditThat::API::Result* a_result, void*) {
    if (!a_result ||
        (a_result->status != WhoEditThat::API::Status::kSuccess &&
         a_result->status != WhoEditThat::API::Status::kNotFound)) {
        logger::warn("WhoEditThat name removal failed: {}", a_result ? a_result->message : "no result");
    }
}

bool Manager::RequestEDFReevaluation(RE::Actor* a_actor) {
    auto* api = EDF::API::GetAPI();
    if (!a_actor || !api || !api->IsReady()) return false;
    EDF::API::ActorRequest request;
    request.requester = "PlayerNemesis";
    request.actorFormID = a_actor->GetFormID();
    if (!api->QueueReevaluateActor(&request, OnEDFResult, nullptr)) {
        logger::warn("EDF did not accept reevaluation for {:08X}", a_actor->GetFormID());
        return false;
    }
    return true;
}

void Manager::OnEDFResult(const EDF::API::Result* a_result, void*) {
    if (!a_result || a_result->status != EDF::API::Status::kSuccess) {
        if (a_result) {
            auto* manager = GetSingleton();
            if (const auto found = manager->_nemeses.find(a_result->actorFormID); found != manager->_nemeses.end()) {
                found->second.appliedRank = 0;
            }
        }
        logger::warn("EDF reevaluation failed for {:08X}: {}", a_result ? a_result->actorFormID : 0,
                     a_result ? a_result->error : "no result");
    }
}

void Manager::ReconcileLoadedActors() {
    for (auto iterator = _nemeses.begin(); iterator != _nemeses.end();) {
        auto current = iterator++;
        auto& [formID, record] = *current;
        if (auto* actor = LookupLoadedActor(formID)) {
            if (actor->IsDead()) {
                HandleActorDeath(actor);
            } else if (record.active || record.pending == PendingActivation::Immediate) {
                ApplyRecord(record, actor);
            }
        }
    }
}

void Manager::OnSettingsChanged() {
    Settings::ResolveForms();
    ReconcileLoadedActors();
}

void Manager::OnGameReady() {
    RunSelfChecks();
    PopulateAllLists();
    EnsureFaction();
    ReconcileLoadedActors();
}

void Manager::Save(SKSE::SerializationInterface* a_serialization) {
    if (!a_serialization) return;

    if (a_serialization->OpenRecord(kNemesisRecord, kRecordVersion)) {
        const auto count = static_cast<std::uint32_t>(_nemeses.size());
        if (!WriteValue(a_serialization, count)) return;
        for (const auto& [formID, record] : _nemeses) {
            const auto active = static_cast<std::uint8_t>(record.active);
            const auto ready = static_cast<std::uint8_t>(record.reencounterReady);
            const auto pending = static_cast<std::uint8_t>(record.pending);
            if (!WriteValue(a_serialization, formID) || !WriteValue(a_serialization, record.killCount) ||
                !WriteValue(a_serialization, record.appliedRank) || !WriteValue(a_serialization, active) ||
                !WriteValue(a_serialization, ready) || !WriteValue(a_serialization, pending) ||
                !WriteString(a_serialization, record.actorKey) || !WriteString(a_serialization, record.originalName)) {
                logger::error("Failed to serialize nemesis {:08X}", formID);
                return;
            }
        }
    }

    if (!a_serialization->OpenRecord(kNemesisKilledRecord, kNemesisKilledRecordVersion) ||
        !WriteValue(a_serialization, _nemesesKilled)) {
        logger::error("Failed to serialize defeated nemesis count");
    }
}

bool Manager::LoadRecord(SKSE::SerializationInterface* a_serialization, std::uint32_t a_type, std::uint32_t a_version,
                         std::uint32_t) {
    if (a_type == kNemesisKilledRecord) {
        if (a_version != kNemesisKilledRecordVersion) {
            logger::warn("Unsupported defeated nemesis serialization version {}", a_version);
            return true;
        }
        if (!ReadValue(a_serialization, _nemesesKilled)) {
            logger::warn("Could not read defeated nemesis count from co-save");
        }
        return true;
    }

    if (a_type != kNemesisRecord) return false;
    if (a_version != kRecordVersion) {
        logger::warn("Unsupported nemesis serialization version {}", a_version);
        return true;
    }
    std::uint32_t count = 0;
    if (!ReadValue(a_serialization, count)) return true;
    for (std::uint32_t index = 0; index < count; ++index) {
        NemesisRecord record;
        std::uint8_t active = 0, ready = 0, pending = 0;
        if (!ReadValue(a_serialization, record.actorFormID) || !ReadValue(a_serialization, record.killCount) ||
            !ReadValue(a_serialization, record.appliedRank) || !ReadValue(a_serialization, active) ||
            !ReadValue(a_serialization, ready) || !ReadValue(a_serialization, pending) ||
            !ReadString(a_serialization, record.actorKey) || !ReadString(a_serialization, record.originalName))
            break;
        RE::FormID resolvedID = 0;
        if (!a_serialization->ResolveFormID(record.actorFormID, resolvedID)) {
            resolvedID = FormUtil::FormIDFromString(record.actorKey);
        }
        if (resolvedID == 0 || record.killCount == 0) continue;
        record.actorFormID = resolvedID;
        record.active = active != 0;
        record.reencounterReady = ready != 0;
        record.pending = pending <= static_cast<std::uint8_t>(PendingActivation::Reencounter)
                             ? static_cast<PendingActivation>(pending)
                             : PendingActivation::None;
        _nemeses[resolvedID] = std::move(record);
    }
    logger::info("Loaded {} nemesis record(s)", _nemeses.size());
    return true;
}

void Manager::Revert() {
    _nemeses.clear();
    _nemesesKilled = 0;
    _deathOpen = false;
    _killerRecorded = false;
}

