#include "RED4ext/Api/ApiVersion.hpp"
#include "RED4ext/Api/v1/Runtime.hpp"
#include <Config/ProjectTemplate.hpp>
#include <Context/Context.hpp>

#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>
#include <System/NeuroSystem.hpp>

using namespace Red;

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(v1::PluginHandle aHandle, v1::EMainReason aReason, const v1::Sdk* aSdk)
{
    if (aReason == v1::EMainReason::Load)
    {
        Context::PluginHandle = aHandle;
        Context::PluginSdk = aSdk;
        Context::PluginLogger = aSdk->logger;

        // Package whole mod into RED4ext plugin folder
        aSdk->scripts->Add(aHandle, L"Scripts");

        v1::GameState state{};

        state.OnEnter = nullptr;
        state.OnUpdate = &mod::NeuroSystem::OnGameStateUpdate;
        state.OnExit = nullptr;

        aSdk->gameStates->Add(aHandle, EGameStateType::Running, &state);

        TypeInfoRegistrar::RegisterDiscovered();
        CRTTISystem::Get()->AddPostRegisterCallback([]() { Context::m_rttiReady = true; });
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(v1::PluginInfo* aInfo)
{
    aInfo->name = build::ProjectName;
    aInfo->author = build::AuthorName;

    constexpr auto ModVersion = build::GetModVersion();

    aInfo->version =
        RED4EXT_V1_SEMVER(static_cast<std::uint8_t>(ModVersion.major()), static_cast<std::uint8_t>(ModVersion.minor()),
                          static_cast<std::uint8_t>(ModVersion.patch())); // Set your version here.
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_INDEPENDENT;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
