#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/StringView.hpp>
#include <RedLib.hpp>

#include <Socket/Socket.hpp>
#include <Util/Time.hpp>

#include <neurosdk.h>

namespace mod
{
/**
 * \brief An abstract message to Neuro. Derives from Red::IScriptable to use REDengine data structures.
 */
class NeuroMessage : public Red::IScriptable
{
public:
    RTTI_IMPL_TYPEINFO(NeuroMessage);
    RTTI_IMPL_ALLOCATOR();

    /**
     * \brief Dispatches a fitting message to the Neuro socket.
     * \param aSocket The socket.
     */
    virtual void DispatchNeuroMessage(neuro::NeuroSocket& aSocket) = 0;
};

/**
 * \brief A response to an action taken by Neuro. 
 */
class NeuroActionResponseMessage : public NeuroMessage
{
public:
    // The name of the action - note: this is just for convenient bookkeeping!
    // We do not send this to Neuro - she knows what action she took.
    Red::CString m_actionName{};

    // The data of the action (whatever Neuro sends us)
    // As with m_actionName, we do not send this back.
    Red::CString m_actionData{};

    // The ID of the action.
    Red::CString m_actionId{};

    // The response sent to Neuro. Can be text or JSON.
    Red::CString m_actionResponse{};

    void DispatchNeuroMessage(neuro::NeuroSocket& aSocket) override;

    RTTI_IMPL_TYPEINFO(NeuroActionResponseMessage);
    RTTI_IMPL_ALLOCATOR();
};

/**
 * \brief An update to Neuro's context.
 */
class NeuroContextMessage : public NeuroMessage
{
public:
    // The context message.
    Red::CString m_message{};

    void DispatchNeuroMessage(neuro::NeuroSocket& aSocket) override;

    RTTI_IMPL_TYPEINFO(NeuroContextMessage);
    RTTI_IMPL_ALLOCATOR();
};

/**
 * \brief A forced action for Neuro to handle.
 */
class NeuroForcedActionMessage : public NeuroMessage
{
public:
    // The name of the action.
    Red::CString m_actionName{};

    // The current state of the game (for instance, for the quickhack action - the quickhacks Neuro can do, their RAM costs, the player RAM)
    Red::CString m_state{};

    // A short description of what Neuro should do.
    Red::CString m_query{};

    void DispatchNeuroMessage(neuro::NeuroSocket& aSocket) override;

    // I think ephemeral actions are kind of boring and Neuro should react to what she's being asked to do, so...

    RTTI_IMPL_TYPEINFO(NeuroForcedActionMessage);
    RTTI_IMPL_ALLOCATOR();
};

/**
 * \brief Bridge between Neuro's backend and game. Keeps websocket alive and serves as interface for scripting API.
 */
class NeuroSystem : public Red::IGameSystem
{    
public:
    // If initial connection failed/we lost connection, retry every 5 seconds
    static constexpr util::Timestamp::Seconds RetryTimeSeconds{5};

    // Lock to access Neuro socket
    Red::SharedSpinLock m_socketLock{};

    // Neuro API socket
    std::unique_ptr<neuro::NeuroSocket> m_neuroSocket{};

    // Is this not the first connection?
    bool m_failedToConnectBefore{};

    // Last reconnection retry time if we don't have a socket
    util::Timestamp m_lastRetryTime{};

    // Lock for message queue
    Red::SharedSpinLock m_messageLock{};

    // Message queue for action responses ETC
    Red::DynArray<Red::Handle<NeuroMessage>> m_messageQueue{};

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

    /**
     * \brief Add a message for Neuro to the message queue.
     * \param aMessage The message structure.
     */
    void AddMessage(const Red::Handle<NeuroMessage>& aMessage);

    /**
     * \brief Send context info to Neuro OUTSIDE OF THE MESSAGE QUEUE.
     * 
     * This is not the recommended way and it is better to use the message queue.
     * Use this when there are no other options (for instance, the frame callback is not running)
     * 
     * \param aContextInfo The specified context.
     */
    void SendContextDirect(Red::StringView aContextInfo);

    /**
     * \brief Send context info to Neuro.
     * Prefer using this over SendContextDirect. This function is also exposed to RTTI.
     *
     * \param aContextInfo The specified context.
     */
    void SendContext(const Red::CString& aContextInfo);
#pragma endregion

#pragma region Util
    /**
     * \brief Checks if we're in pregame via system requests handler.
     * \return Whether or not we are in the main menu.
     */
    bool IsPreGame();

    /**
     * \brief Returns a pointer to the game system instance. Game systems are kept from the game's start until the end, so this is safe.
     */
    static NeuroSystem* GetInstance();
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
