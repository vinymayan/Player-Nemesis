#pragma once

#include <set>
#include <shared_mutex>

namespace Sink {
    void ScheduleSinkRegistration(RE::Actor* a_actor, int a_attempts = 0);

    class NpcCycleSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static NpcCycleSink* GetSingleton() {
            static NpcCycleSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
    };

    class NpcCombatTracker final : public RE::BSTEventSink<RE::TESCombatEvent> {
    public:
        static NpcCombatTracker* GetSingleton() {
            static NpcCombatTracker singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event,
                                              RE::BSTEventSource<RE::TESCombatEvent>*) override;

        static void RegisterSink(RE::Actor* a_actor);
        static void UnregisterSink(RE::Actor* a_actor);
        static void RegisterSinksForExistingCombatants();

    private:
        inline static std::set<RE::FormID> _trackedActors;
        inline static std::shared_mutex _mutex;
    };


    class NemesisDeathEventHandler final : public RE::BSTEventSink<RE::TESDeathEvent> {
    public:
        static NemesisDeathEventHandler* GetSingleton() {
            static NemesisDeathEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event,
                                              RE::BSTEventSource<RE::TESDeathEvent>*) override;
    };

    class Actor3DLoadEventHandler final : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
    public:
        static Actor3DLoadEventHandler* GetSingleton() {
            static Actor3DLoadEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event,
                                              RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;
    };
}
