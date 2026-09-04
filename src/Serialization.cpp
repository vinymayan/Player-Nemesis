#include "Serialization.h"

#include "Manager.h"

void Serialization::Save(SKSE::SerializationInterface* a_serialization) {
    Manager::GetSingleton()->Save(a_serialization);
}

void Serialization::Load(SKSE::SerializationInterface* a_serialization) {
    auto* manager = Manager::GetSingleton();
    manager->Revert();
    if (!a_serialization) return;
    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;
    while (a_serialization->GetNextRecordInfo(type, version, length)) {
        if (!manager->LoadRecord(a_serialization, type, version, length)) {
            logger::warn("Ignoring unknown Player Nemesis record {:08X}", type);
        }
    }
}

void Serialization::Revert(SKSE::SerializationInterface*) { Manager::GetSingleton()->Revert(); }
