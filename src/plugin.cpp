#include "Events.h"
#include "Manager.h"
#include "Serialization.h"
#include "Settings.h"
#include "logger.h"

namespace {
    bool hasDFG = false;

    class DynamicFormsGeneratorListener final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        static DynamicFormsGeneratorListener* GetSingleton() {
            static DynamicFormsGeneratorListener singleton;
            return &singleton;
        }

        void Register() {
            if (auto* dispatcher = SKSE::GetModCallbackEventSource()) {
                dispatcher->AddEventSink(this);
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
                                              RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const std::string_view eventName = a_event->eventName.c_str();
            if (eventName == "DynamicFormsGeneratorLoaded") {
                Manager::GetSingleton()->PopulateAllLists();
                Manager::GetSingleton()->EnsureFaction();
            } else if (eventName == "DynamicFormsGeneratorUpdated") {
                Manager::GetSingleton()->RefreshLists(a_event->strArg.c_str());
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void OnMessage(SKSE::MessagingInterface::Message* a_message) {
    if (!a_message) {
        return;
    }

    if (a_message->type == SKSE::MessagingInterface::kPostLoad) {
        hasDFG = GetModuleHandleA("DynamicFormsGenerator.dll") != nullptr;
        if (hasDFG) {
            logger::info("DynamicFormsGenerator.dll found");
        }
        Settings::RegisterMenu();
    }

    if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
        if (!hasDFG) {
            logger::critical("DynamicFormsGenerator.dll is required. Player Nemesis will remain disabled.");
            return;
        }
        auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton();
        eventSource->AddEventSink(Sink::Actor3DLoadEventHandler::GetSingleton());
        eventSource->AddEventSink(Sink::NemesisDeathEventHandler::GetSingleton());
    }

    if (hasDFG && (a_message->type == SKSE::MessagingInterface::kNewGame ||
                   a_message->type == SKSE::MessagingInterface::kPostLoadGame)) {
        if (a_message->type == SKSE::MessagingInterface::kNewGame) {
            Manager::GetSingleton()->Revert();
        }
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::NpcCombatTracker::GetSingleton());

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            player->RemoveAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton());
            player->AddAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton());
        }
        Sink::NpcCombatTracker::RegisterSinksForExistingCombatants();
        Manager::GetSingleton()->OnGameReady();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(a_skse);

    DynamicFormsGeneratorListener::GetSingleton()->Register();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    if (auto* serialization = SKSE::GetSerializationInterface()) {
        serialization->SetUniqueID(Serialization::ID);
        serialization->SetSaveCallback(Serialization::Save);
        serialization->SetLoadCallback(Serialization::Load);
        serialization->SetRevertCallback(Serialization::Revert);
    }
    return true;
}
