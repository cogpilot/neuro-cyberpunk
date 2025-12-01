#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/StringView.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/EInputAction.hpp>
#include <RED4ext/Scripting/Natives/Generated/EInputKey.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/vis/ListChoiceHubData.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/mappins/IMappin.hpp>

#include <Socket/Socket.hpp>
#include <Util/Time.hpp>

#include <neurosdk.h>
#include <tsl/hopscotch_set.h>

namespace Impl
{
class NeuroQuickhackDataDto;
class NeuroPhoneMessageDto;
}

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

    /**
     * \brief Checks if this response is to a forced action.
     */
    bool IsResponseToForcedAction() const;

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

    // Should the message be silent to Neuro
    bool m_silent{false};

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

    // The current state of the game (for instance, for the quickhack action - the quickhacks Neuro can do, their RAM
    // costs, the player RAM)
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
#pragma region Connection
    // If initial connection failed/we lost connection, retry every 5 seconds
    static constexpr util::Timestamp::Seconds RetryTimeSeconds{5};

    // Delay between synthetic inputs
    static constexpr auto DelayBetweenInputs = 0.1f;

    static constexpr auto MaxConnectionAttemptsForFirstConnection = 5;

    static constexpr auto MaxKeepTrackOfForcedActions = 5.f;

    // Lock to access Neuro socket
    Red::SharedSpinLock m_socketLock{};

    // Neuro API socket
    std::unique_ptr<neuro::NeuroSocket> m_neuroSocket{};

    // Is this not the first connection?
    bool m_failedToConnectBefore{};

    // Have we had a successful connection ever?
    bool m_hasHadSuccessfulConnection{};

    // Tries for first connection
    int m_firstConnectionAttempts{};

    // If we cannot establish a link to Neuro, disable further connection attempts
    bool m_disableConnection{};

    // Last reconnection retry time if we don't have a socket
    util::Timestamp m_lastRetryTime{};
#pragma region Messages
    // Lock for message queue
    Red::SharedSpinLock m_messageLock{};

    // Message queue for action responses ETC
    Red::DynArray<Red::Handle<NeuroMessage>> m_messageQueue{};

    // Have we sent a forced action message?
    // If so, drop all forced action messages until Neuro responds to the first one
    bool m_hasSentForcedActionMessage{};
    float m_forcedActionMessageTimer{};
#pragma endregion

#pragma region Inputs
    Red::SharedSpinLock m_inputLock{};

    // Input seems to be registered once per frame + sometimes you need delay
    Red::DynArray<Red::EInputKey> m_injectedKeyQueue{};

    float m_inputTimer{};
    std::uint32_t m_injectedKeyQueueIndex{};
#pragma endregion
#pragma endregion

#pragma region SceneHandling
    Red::SharedSpinLock m_choicehubLock{};
    tsl::hopscotch_set<int> m_encounteredChoiceHubIDs{};
#pragma endregion

#pragma region QuickhackHandling
    Red::EntityID m_quickhackActionTargetId{};
#pragma endregion

#pragma region SMSMessageHandling
    Red::SharedSpinLock m_smsLock{};
    Red::WeakHandle<Red::IScriptable> m_actionMessengerDialogViewController{};
#pragma endregion

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
     * \param aFrameInfo The frame time information.
     * \param aJobQueue The job queue provided to the game system for work.
     */
    void Tick(Red::FrameInfo& aFrameInfo, Red::JobQueue& aJobQueue);

    /**
     * \brief Tick function registered by update registrar for draining user input.
     * \param aFrameInfo The previous frame's information (frametime and whatnot)
     */
    void DrainInputQueue(Red::FrameInfo& aFrameInfo);

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

    /**
     * \brief Send context info to Neuro, but Neuro will not visibly react to this context. Useful for spammy stuff.
     * This function is also exposed to RTTI.
     *
     * \param aContextInfo The specified context.
     */
    void SendContextSilent(const Red::CString& aContextInfo);
#pragma endregion

#pragma region Util
    /**
     * \brief Checks if we're in pregame via system requests handler.
     * \return Whether or not we are in the main menu.
     */
    bool IsPreGame();

    /**
     * \brief Returns a pointer to the game system instance. Game systems are kept from the game's start until the end,
     * so this is safe.
     */
    static NeuroSystem* GetInstance();
#pragma endregion

#pragma region Debug
    /**
     * \brief Inject a synthetic keypress into the input manager.
     * Note: this is mostly for debugging purposes and for testing input injection for scene choice nodes.
     *
     * \param aKey The key to inject a press of.
     */
    void InjectKeypress(Red::EInputKey aKey);

    /**
     * \brief Inject a synthetic keypress chain into the input manager.
     *
     * \param aKeys The keys to inject in sequence.
     */
    void InjectKeypressChain(const Red::DynArray<Red::EInputKey>& aKeys);

    /**
     * \brief Inject a synthetic input press into the input manager.
     *
     * \param aActionName The action name to inject.
     * \param aDurationSeconds How long the action should be held down.
     */
    void InjectActionPress(Red::CName aActionName, float aDurationSeconds);
#pragma endregion

#pragma region ScriptingUtils
    /**
     * \brief Track a mappin for Autodrive to work, as MappinSystem does not export a TrackMappin method for generic
     * mappins OOTB and I don't believe this mod should do Codeware work.
     * \param aMappin The mappin to track.
     */
    void TrackMappin(Red::Handle<Red::game::mappins::IMappin>& aMappin);

    /**
     * \brief Send Neuro scene list choice data upon its initial appearance. Deduplicated via choicehub IDs.
     * \param aRef The list choice hub data reference.
     */
    void OnSceneListChoiceDataProvided(Red::ScriptRef<Red::game::interactions::vis::ListChoiceHubData>& aRef);

    /**
     * \brief Check if the forced action cooldown is active. Used for quickhack handling due to special handling of
     * target ID. Provided to RTTI.
     * \return Whether or not the forced action cooldown is active.
     */
    bool HasForcedActionCooldown();

    /**
     * \brief Dispatch new quickhack info to Neuro.
     * \param aQuickhackInfo The available quickhack information gathered by script.
     */
    void OnQuickhackDataProvided(Red::Handle<Impl::NeuroQuickhackDataDto>& aQuickhackInfo);

    /**
     * \brief Dispatch new phone message info to Neuro.
     * \param aMessageInfo The available phone message information gathered by script.
     */
    void OnSMSMessageDataProvided(Red::Handle<Impl::NeuroPhoneMessageDto>& aMessageInfo);

    /**
     * \brief Reset the bad connection counter to allow retry for Neuro socket.
     */
    void ResetBadConnectionCounter();
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
} // namespace mod
