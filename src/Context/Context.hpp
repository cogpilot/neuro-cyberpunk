#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <Shared/Util/NamePoolRegistrar.hpp>

namespace Context
{
// Plugin identifier
inline Red::PluginHandle PluginHandle;

// Pointer to function table for logging ETC
inline const Red::Sdk* PluginSdk;

// Logger function table from plugin SDK
inline Red::Logger* PluginLogger;

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
                      std::vformat(aStr, std::make_format_args(aArgs...)));
}
} // namespace Context
