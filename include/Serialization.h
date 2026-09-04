#pragma once

#include <cstdint>

namespace Serialization {
    constexpr std::uint32_t ID = 'PNEM';
    void Save(SKSE::SerializationInterface* a_serialization);
    void Load(SKSE::SerializationInterface* a_serialization);
    void Revert(SKSE::SerializationInterface* a_serialization);
}
