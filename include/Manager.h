#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ClibUtil/editorID.hpp"
#include "DFGAPI.h"
#include "EDFAPI.h"
#include "WhoEditThatAPI.h"

namespace FormUtil {
    const RE::TESFile* GetMasterFile(RE::TESForm* a_form);
    std::string NormalizeFormID(RE::TESForm* a_form);
    RE::FormID FormIDFromString(const std::string& a_value);
}

struct InternalFormInfo {
    RE::FormID formID{};
    std::string editorID;
    std::string name;
    std::string pluginName;
    std::string formType;
    std::string cachedDisplayName;

    void UpdateDisplayName();
};

enum class PendingActivation : std::uint8_t { None = 0, Immediate, Respawn, Reencounter };

struct NemesisRecord {
    RE::FormID actorFormID{};
    std::string actorKey;
    std::string originalName;
    std::uint32_t killCount{};
    std::uint8_t appliedRank{};
    bool active{};
    bool reencounterReady{};
    PendingActivation pending{PendingActivation::None};
};

class Manager {
public:
    static Manager* GetSingleton();

    void PopulateAllLists();
    void RefreshLists(std::string_view a_signatures);
    static std::string ToUTF8(std::string_view a_text);
    const std::vector<InternalFormInfo>& GetList(const std::string& a_typeName);

    void EnsureFaction();
    void HandlePlayerAnimationEvent(std::string_view a_eventName);
    void HandleKilledPlayer(RE::Actor* a_killer);
    void HandleActorDeath(RE::Actor* a_actor);
    void HandleActorLoaded(RE::Actor* a_actor);
    void HandleActorEncountered(RE::Actor* a_actor);
    void OnSettingsChanged();
    void OnGameReady();

    void Save(SKSE::SerializationInterface* a_serialization);
    bool LoadRecord(SKSE::SerializationInterface* a_serialization, std::uint32_t a_type, std::uint32_t a_version,
                    std::uint32_t a_length);
    void Revert();

private:
    Manager() = default;

    template <typename T>
    void PopulateList(const std::string& a_typeName);
    void ApplyRecord(NemesisRecord& a_record, RE::Actor* a_actor);
    void ApplyName(const NemesisRecord& a_record, RE::Actor* a_actor, std::uint32_t a_rank);
    void RemoveName(RE::Actor* a_actor);
    bool RequestEDFReevaluation(RE::Actor* a_actor);
    void ReconcileLoadedActors();
    RE::TESFaction* GetFaction();
    WhoEditThat::API::ClientHandle GetNameClient();
    static void OnDFGLookup(const DFG::FormLookupResult* a_result, void*);
    static void OnDFGCreate(const DFG::FormOperationResult* a_result, void*);
    static void OnEDFResult(const EDF::API::Result* a_result, void*);
    static void OnNameResult(const WhoEditThat::API::Result* a_result, void*);
    static void OnNameRemovalResult(const WhoEditThat::API::Result* a_result, void*);

    std::map<std::string, std::vector<InternalFormInfo>> _dataStore;
    std::map<RE::FormID, NemesisRecord> _nemeses;
    std::uint32_t _nemesesKilled{};
    RE::FormID _factionFormID{};
    WhoEditThat::API::ClientHandle _nameClient{};
    bool _listsPopulated{};
    bool _factionRequestPending{};
    bool _deathOpen{};
    bool _killerRecorded{};
};
