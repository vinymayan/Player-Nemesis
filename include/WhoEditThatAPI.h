#pragma once

#include <Windows.h>

#include <cstdint>

namespace WhoEditThat::API {
    inline constexpr std::uint32_t kInterfaceVersion = 1;
    using ClientHandle = std::uint64_t;
    inline constexpr ClientHandle kInvalidClient = 0;
    enum class Operation : std::uint32_t {
        kUpsertActorValue = 1,
        kRemoveActorValue,
        kLookupActorValue,
        kUpsertDisplayName,
        kRemoveDisplayName,
        kListActorValues,
        kRemoveActorValuesByPrefix
    };
    enum class Status : std::uint32_t {
        kSuccess = 0,
        kNotReady,
        kLoadInProgress,
        kContextChanged,
        kCancelled,
        kInvalidArgument,
        kNotOwner,
        kNotFound,
        kActorUnavailable,
        kUnsupportedActorValue,
        kConflict,
        kPersistenceFailed,
        kInternalError
    };
    enum class NumericOperation : std::uint32_t { kFlat = 0, kPercent, kMultiply };
    enum class ModifierChannel : std::uint32_t { kPermanent = 0, kTemporary };
    struct ClientRegistration {
        std::uint32_t structSize{sizeof(ClientRegistration)};
        const char* clientID{};
        const char* displayName{};
    };
    struct ActorValueContributionRequest;
    struct ContributionScopeRequest;
    struct ActorValueListResult;
    struct ContributionRequest {
        std::uint32_t structSize{sizeof(ContributionRequest)};
        ClientHandle client{kInvalidClient};
        std::uint32_t actorFormID{};
        const char* mutationKey{};
    };
    struct DisplayNameRequest {
        std::uint32_t structSize{sizeof(DisplayNameRequest)};
        ClientHandle client{kInvalidClient};
        std::uint32_t actorFormID{};
        const char* mutationKey{};
        const char* displayName{};
        std::int32_t priority{};
    };
    struct Result {
        std::uint32_t structSize{sizeof(Result)};
        Operation operation{Operation::kLookupActorValue};
        Status status{Status::kInternalError};
        std::uint32_t actorFormID{};
        NumericOperation numericOperation{NumericOperation::kFlat};
        ModifierChannel channel{ModifierChannel::kPermanent};
        float previousDelta{};
        float appliedDelta{};
        std::uint32_t affectedCount{};
        char ownerID[96]{};
        char mutationKey[128]{};
        char actorValue[64]{};
        char message[256]{};
    };
    using Callback = void (*)(const Result*, void*);
    using ListCallback = void (*)(const ActorValueListResult*, void*);

    class IWhoEditThatAPI {
    public:
        virtual ~IWhoEditThatAPI() = default;
        virtual std::uint32_t GetVersion() const noexcept = 0;
        virtual bool IsReady() const noexcept = 0;
        virtual ClientHandle RegisterClient(const ClientRegistration*) noexcept = 0;
        virtual bool QueueUpsertActorValue(const ActorValueContributionRequest*, Callback, void*) noexcept = 0;
        virtual bool QueueRemoveActorValue(const ContributionRequest*, Callback, void*) noexcept = 0;
        virtual bool QueueLookupActorValue(const ContributionRequest*, Callback, void*) noexcept = 0;
        virtual bool QueueUpsertDisplayName(const DisplayNameRequest*, Callback, void*) noexcept = 0;
        virtual bool QueueRemoveDisplayName(const ContributionRequest*, Callback, void*) noexcept = 0;
        virtual bool QueueListActorValues(const ContributionScopeRequest*, ListCallback, void*) noexcept = 0;
        virtual bool QueueRemoveActorValuesByPrefix(const ContributionScopeRequest*, Callback, void*) noexcept = 0;
    };

    inline IWhoEditThatAPI* GetAPI() noexcept {
        const auto module = GetModuleHandleA("WhoEditThat.dll");
        const auto getter = module ? GetProcAddress(module, "GetWhoEditThatAPI") : nullptr;
        if (!getter) return nullptr;
        auto* api = static_cast<IWhoEditThatAPI*>(reinterpret_cast<void* (*)()>(getter)());
        return api && api->GetVersion() == kInterfaceVersion ? api : nullptr;
    }
}
