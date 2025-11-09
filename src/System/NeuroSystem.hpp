#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RedLib.hpp>

#include <Socket/Socket.hpp>
#include <Util/Time.hpp>

#include <neurosdk.h>

namespace mod
{
/**
 * \brief Bridge between Neuro's backend and game. Keeps websocket alive and serves as interface for scripting API.
 */
class NeuroSystem : public Red::IGameSystem
{    
public:
    // If initial connection failed/we lost connection, retry every 5 seconds
    static constexpr util::Timestamp::Seconds RetryTimeSeconds{5};

    Red::SharedSpinLock m_socketLock{};

    // Neuro API socket
    std::unique_ptr<neuro::NeuroSocket> m_neuroSocket{};

    // Is this not the first connection?
    bool m_failedToConnectBefore{};

    // Last reconnection retry time if we don't have a socket
    util::Timestamp m_lastRetryTime{};

#pragma region NeuroHandlers
    /**
     * \brief Handler for Neuro actions.
     * 
     * \param aAction The action description sent by Neuro.
     * \param aSocket The socket object.
     */
    static void DispatchNeuroAction(const neurosdk_message_action_t& aAction, neuro::NeuroSocket& aSocket);

    /**
     * \brief Reset m_neuroSocket and attempt to initialize it.
     * \return Whether or not initialization was successful.
     */
    bool InitializeConnection();

    /**
     * \brief Tick function registered by the update registrar.
     * \return The job queue provided to the game system for work.
     */
    void Tick(Red::JobQueue& aJobQueue);
#pragma endregion

#pragma region Util
    /**
     * \brief Checks if we're in pregame via system requests handler.
     * \return Whether or not we are in the main menu.
     */
    bool IsPreGame();
#pragma endregion

#pragma region Overrides
    void OnRegisterUpdates(Red::UpdateRegistrar* aRegistrar) override;
    std::uint32_t OnBeforeGameSave(const Red::JobGroup& aJobGroup, void* aMetadataObject) override;

    void OnGamePrepared() override;
    void OnWorldDetached(Red::world::RuntimeScene* aScene) override;
    void OnInitialize(const Red::JobHandle& aJob) override;
#pragma endregion

    RTTI_IMPL_TYPEINFO(NeuroSystem);
    RTTI_IMPL_ALLOCATOR();
};
}
