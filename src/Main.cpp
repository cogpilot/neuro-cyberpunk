#include <Config/ProjectTemplate.hpp>
#include <Context/Context.hpp>

#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

using namespace Red;

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(PluginHandle aHandle, EMainReason aReason, const Sdk* aSdk)
{
    if (aReason == EMainReason::Load)
    {
        Context::PluginHandle = aHandle;
        Context::PluginSdk = aSdk;
        Context::PluginLogger = aSdk->logger;

        // Package whole mod into RED4ext plugin folder
        aSdk->scripts->Add(aHandle, L"Scripts");

        TypeInfoRegistrar::RegisterDiscovered();
        CRTTISystem::Get()->AddPostRegisterCallback([]() { Context::m_rttiReady = true; });
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(PluginInfo* aInfo)
{
    aInfo->name = build::ProjectName;
    aInfo->author = build::AuthorName;

    constexpr auto ModVersion = build::GetModVersion();

    aInfo->version =
        RED4EXT_SEMVER_EX(static_cast<std::uint8_t>(ModVersion.major()), static_cast<std::uint8_t>(ModVersion.minor()),
                          static_cast<std::uint8_t>(ModVersion.patch()), RED4EXT_V0_SEMVER_PRERELEASE_TYPE_NONE,
                          0); // Set your version here.
    aInfo->runtime = RED4EXT_RUNTIME_INDEPENDENT;
    aInfo->sdk = RED4EXT_SDK_LATEST;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_LATEST;
}
