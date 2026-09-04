#include "Events.h"

#include "DelayedDispatcher.h"
#include "Manager.h"

RE::BSEventNotifyControl Sink::NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event,
                                                              RE::BSTEventSource<RE::TESCombatEvent>*) {
    if (!a_event || !a_event->actor) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto actorPtr = a_event->actor.get();
    auto* actor = actorPtr ? actorPtr->As<RE::Actor>() : nullptr;
    if (!actor || actor->IsPlayerRef()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    switch (a_event->newState.get()) {
        case RE::ACTOR_COMBAT_STATE::kCombat:
            RegisterSink(actor);
            Manager::GetSingleton()->HandleActorEncountered(actor);
            break;
        case RE::ACTOR_COMBAT_STATE::kNone:
            UnregisterSink(actor);
            break;
        default:
            break;
    }

    return RE::BSEventNotifyControl::kContinue;
}

void Sink::NpcCombatTracker::RegisterSink(RE::Actor* a_actor) {
    if (!a_actor) {
        return;
    }

    std::unique_lock lock(_mutex);
    if (_trackedActors.insert(a_actor->GetFormID()).second) {
        a_actor->AddAnimationGraphEventSink(NpcCycleSink::GetSingleton());
        logger::debug("[NpcCombatTracker] Tracking actor {:08X}", a_actor->GetFormID());
    }
}

void Sink::NpcCombatTracker::UnregisterSink(RE::Actor* a_actor) {
    if (!a_actor || a_actor->IsPlayerRef()) {
        return;
    }

    std::unique_lock lock(_mutex);
    const auto wasTracked = _trackedActors.erase(a_actor->GetFormID()) > 0;
    a_actor->RemoveAnimationGraphEventSink(NpcCycleSink::GetSingleton());
    if (wasTracked) {
        logger::debug("[NpcCombatTracker] Stopped tracking actor {:08X}", a_actor->GetFormID());
    }
}

void Sink::NpcCombatTracker::RegisterSinksForExistingCombatants() {
    logger::info("[NpcCombatTracker] Checking actors already in combat");

    auto* processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        logger::warn("[NpcCombatTracker] ProcessLists is unavailable");
        return;
    }

    for (auto& actorHandle : processLists->highActorHandles) {
        auto actorPtr = actorHandle.get();
        auto* actor = actorPtr.get();
        if (actor && !actor->IsPlayerRef() && actor->IsInCombat()) {
            UnregisterSink(actor);
            RegisterSink(actor);
        }
    }
}

RE::BSEventNotifyControl Sink::NpcCycleSink::ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {
    if (!a_event || !a_event->holder) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* actor = a_event->holder->As<RE::Actor>();
    if (!actor) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const std::string_view eventName = a_event->tag;
    if (actor->IsPlayerRef()) {
        Manager::GetSingleton()->HandlePlayerAnimationEvent(eventName);
    } else if (eventName == "KilledPlayer") {
        Manager::GetSingleton()->HandleKilledPlayer(actor);
    }

    return RE::BSEventNotifyControl::kContinue;
}

void Sink::ScheduleSinkRegistration(RE::Actor* a_actor, int a_attempts) {
    if (!a_actor) {
        return;
    }
    if (a_attempts > 20) {
        logger::critical("[Actor3DLoadEventHandler] Giving up after {} attempts for actor {:08X}", a_attempts,
                         a_actor->GetFormID());
        return;
    }

    const auto actorHandle = a_actor->CreateRefHandle();
    Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(100), [actorHandle, a_attempts] {
        SKSE::GetTaskInterface()->AddTask([actorHandle, a_attempts] {
            auto actorPtr = actorHandle.get();
            if (!actorPtr) {
                return;
            }

            RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
            actorPtr->GetAnimationGraphManager(graphManager);
            if (!graphManager) {
                ScheduleSinkRegistration(actorPtr.get(), a_attempts + 1);
                return;
            }

            if (actorPtr->IsPlayerRef()) {
                actorPtr->RemoveAnimationGraphEventSink(NpcCycleSink::GetSingleton());
                actorPtr->AddAnimationGraphEventSink(NpcCycleSink::GetSingleton());
                logger::info("[Actor3DLoadEventHandler] Player sink reconnected");
            } else {
                NpcCombatTracker::UnregisterSink(actorPtr.get());
                NpcCombatTracker::RegisterSink(actorPtr.get());
                logger::debug("[Actor3DLoadEventHandler] NPC sink reconnected for {:08X}", actorPtr->GetFormID());
            }
        });
    });
}

RE::BSEventNotifyControl Sink::NemesisDeathEventHandler::ProcessEvent(
    const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) {
    if (!a_event || !a_event->dead || !a_event->actorDying) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* actor = a_event->actorDying->As<RE::Actor>();
    if (actor && !actor->IsPlayerRef()) {
        Manager::GetSingleton()->HandleActorDeath(actor);
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Sink::Actor3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event,
                                                                     RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (!a_event->loaded) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* form = RE::TESForm::LookupByID(a_event->formID);
    auto* actor = form ? form->As<RE::Actor>() : nullptr;
    if (actor) {
        Manager::GetSingleton()->HandleActorLoaded(actor);
        ScheduleSinkRegistration(actor);
    }

    return RE::BSEventNotifyControl::kContinue;
}
