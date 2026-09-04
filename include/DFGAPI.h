#pragma once

#include <Windows.h>

#include <cstdint>

#include "RE/Skyrim.h"

namespace DFG {
    inline constexpr std::uint32_t InterfaceVersion = 1;
    enum class Operation : std::uint32_t { Create = 1, Update, Delete, Lookup };
    enum class Status : std::uint32_t {
        Success = 0,
        NotReady,
        InvalidArgument,
        InvalidJson,
        MissingEditorId,
        InvalidEditorId,
        MissingPackageName,
        InvalidPackageName,
        MissingFormKind,
        UnsupportedFormKind,
        EditorIdAlreadyExists,
        EditorIdReserved,
        EditorIdNotFound,
        EditorIdMismatch,
        FormKindMismatch,
        ProtectedField,
        DPFUnavailable,
        DPFCreateFailed,
        ConfigureFailed,
        PersistenceFailed,
        DPFReleaseFailed,
        InternalError,
        BatchPartialSuccess,
        BatchFailed
    };
    struct CreateFormRequest {
        std::uint32_t structSize{sizeof(CreateFormRequest)};
        const char* requester{};
        const char* packageName{};
        const char* formJson{};
    };
    struct UpdateFormRequest;
    struct DeleteFormRequest;
    struct CreateFormsRequest;
    struct UpdateFormsRequest;
    struct DeleteFormsRequest;
    struct LookupFormsRequest;
    struct BatchOperationResult;
    struct BatchLookupResult;
    struct LookupFormRequest {
        std::uint32_t structSize{sizeof(LookupFormRequest)};
        const char* requester{};
        const char* editorId{};
    };
    struct FormOperationResult {
        std::uint32_t structSize{sizeof(FormOperationResult)};
        Operation operation{Operation::Create};
        Status status{Status::InternalError};
        RE::TESForm* form{};
        RE::FormID formID{};
        std::uint32_t pluginNumber{};
        std::uint32_t localId{};
        std::uint8_t recoveredExistingSlot{};
        char editorId[128]{};
        char packageName[128]{};
        char pluginName[64]{};
        char error[256]{};
    };
    struct FormLookupResult {
        std::uint32_t structSize{sizeof(FormLookupResult)};
        Status status{Status::InternalError};
        std::uint8_t exists{};
        RE::TESForm* form{};
        RE::FormID formID{};
        std::uint32_t pluginNumber{};
        std::uint32_t localId{};
        char editorId[128]{};
        char packageName[128]{};
        char pluginName[64]{};
        char formKind[64]{};
        char sourceSignature[16]{};
        const char* formJson{};
        std::uint32_t formJsonLength{};
        char error[256]{};
    };
    using FormOperationCallback = void (*)(const FormOperationResult*, void*);
    using BatchOperationCallback = void (*)(const BatchOperationResult*, void*);
    using FormLookupCallback = void (*)(const FormLookupResult*, void*);
    using BatchLookupCallback = void (*)(const BatchLookupResult*, void*);

    class IDynamicFormsGenerator {
    public:
        virtual ~IDynamicFormsGenerator() = default;
        virtual std::uint32_t GetVersion() const noexcept = 0;
        virtual bool IsReady() const noexcept = 0;
        virtual bool QueueCreateForm(const CreateFormRequest*, FormOperationCallback, void*) noexcept = 0;
        virtual bool QueueUpdateForm(const UpdateFormRequest*, FormOperationCallback, void*) noexcept = 0;
        virtual bool QueueDeleteForm(const DeleteFormRequest*, FormOperationCallback, void*) noexcept = 0;
        virtual bool QueueCreateForms(const CreateFormsRequest*, BatchOperationCallback, void*) noexcept = 0;
        virtual bool QueueUpdateForms(const UpdateFormsRequest*, BatchOperationCallback, void*) noexcept = 0;
        virtual bool QueueDeleteForms(const DeleteFormsRequest*, BatchOperationCallback, void*) noexcept = 0;
        virtual bool QueueLookupForm(const LookupFormRequest*, FormLookupCallback, void*) noexcept = 0;
        virtual bool QueueLookupForms(const LookupFormsRequest*, BatchLookupCallback, void*) noexcept = 0;
    };

    inline IDynamicFormsGenerator* GetAPI() noexcept {
        const auto module = GetModuleHandleA("DynamicFormsGenerator.dll");
        const auto getter = module ? GetProcAddress(module, "GetDynamicFormsGeneratorAPI") : nullptr;
        if (!getter) return nullptr;
        auto* api = static_cast<IDynamicFormsGenerator*>(reinterpret_cast<void* (*)()>(getter)());
        return api && api->GetVersion() == InterfaceVersion ? api : nullptr;
    }
}
