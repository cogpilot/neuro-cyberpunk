#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <RedLib.hpp>
#include <Shared/Util/NamePoolRegistrar.hpp>

#include <fmt/format.h>

namespace Context
{
// Plugin identifier
inline Red::v1::PluginHandle PluginHandle;

// Pointer to function table for logging ETC
inline const Red::v1::Sdk* PluginSdk;

// Logger function table from plugin SDK
inline Red::v1::Logger* PluginLogger;

// Is game RTTI ready for use?
inline bool m_rttiReady{};

template<typename... Args>
inline void Spew(std::string_view aStr, Args... aArgs)
{
    if (!m_rttiReady)
    {
        return;
    }

    Red::Log::Channel(shared::util::NamePoolRegistrar<"Neuro-sama Interactions Debug">::Get(),
                      fmt::vformat(aStr, fmt::make_format_args(aArgs...)));
}
} // namespace Context
