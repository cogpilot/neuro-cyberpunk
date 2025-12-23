#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/StringView.hpp>
#include <RedLib.hpp>

#include <RED4ext/Scripting/Natives/Generated/EInputAction.hpp>
#include <RED4ext/Scripting/Natives/Generated/EInputKey.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/vis/DialogChoiceHubs.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/vis/ListChoiceHubData.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/mappins/IMappin.hpp>
#include <RED4ext/Scripting/Natives/entEntityID.hpp>

#include <Socket/Socket.hpp>
#include <Util/Time.hpp>

#include <neurosdk.h>
#include <tsl/hopscotch_set.h>

#include <string>
#include <vector>

namespace Impl
{
class NeuroQuickhackDataDto;
class NeuroPhoneMessageDto;
} // namespace Impl

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

    // Whether or not the action is treated as successful. Used for dialogue choice options.
    bool m_success = true;

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

    // The priority of the action (low, medium, high, critical).
    Red::CString m_priority{"medium"};

    void DispatchNeuroMessage(neuro::NeuroSocket& aSocket) override;

    // I think ephemeral actions are kind of boring and Neuro should react to what she's being asked to do, so...

    RTTI_IMPL_TYPEINFO(NeuroForcedActionMessage);
    RTTI_IMPL_ALLOCATOR();
};

/**
 * \brief Wrapper for a choice option that Neuro has.
 */
struct NeuroChoiceContext
{
    int m_id{};
    std::string m_hubTitle{};
    std::string m_text{};
    bool m_canChoose{};
    bool m_important{};
    bool m_isTimed{};
    float m_timer{};

    static NeuroChoiceContext FromGameData(const Red::game::interactions::vis::ListChoiceData& aChoiceData, Red::CString& aHubTitle, int aId, bool aIsTimed, float aChoiceTimer);
};

/**
 * \brief Wrapper for updating Neuro's provided scene choice data.
 * Uses STL because we have to serialize this via Glaze.
 */
struct NeuroSceneDataContext
{
    std::vector<NeuroChoiceContext> m_choices{};

    /**
     * \brief Hash choice data to avoid sending Neuro multiple same contexts.
     */
    uint64_t HashChoices();
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

    static constexpr auto ChoicehubDelayBeforeForcedAction = 1.5f;

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

    // Is this the first tick we have access to Neuro? (for script callbacks)
    bool m_isFirstTick{};

    // Last reconnection retry time if we don't have a socket
    util::Timestamp m_lastRetryTime{};
#pragma endregion

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

#pragma region SceneHandling
    Red::SharedSpinLock m_choicehubLock{};

    // How long we should receive choice node updates before sending forced action to Neuro
    float m_choiceHubAvailableTime{};
    bool m_countdownToForcedChoiceSelectionStarted{};

    NeuroSceneDataContext m_choiceHubDataContext{};
    std::uint64_t m_lastChoiceHubDataHash{};
#pragma endregion

#pragma region QuickhackHandling
    Red::ent::EntityID m_quickhackActionTargetId{};
#pragma endregion

#pragma region SMSMessageHandling
    Red::SharedSpinLock m_smsLock{};
    Red::WeakHandle<Red::IScriptable> m_actionMessengerDialogViewController{};
#pragma endregion
   
#pragma region Fuzzer
    Red::SharedSpinLock m_fuzzerLock{};

    bool m_fuzzerActive{};
    int m_currentFuzzerFunction{};
    std::uint64_t m_fuzzerCalls{};
#pragma endregion

#pragma region Callbacks
    Red::SharedSpinLock m_callbackLock{};
    Red::DynArray<Red::WeakHandle<Red::IScriptable>> m_callbackList{};
    Red::DynArray<Red::WeakHandle<Red::IScriptable>> m_newCallbackList{};
#pragma endregion

#pragma region NeuroHandlers
    /**
     * \brief Game state update function for RED4ext. This will always run on game main thread.
     *
     * \param aApplication A pointer to the game application.
     * \return False to signify the state should keep running.
     */
    static bool OnGameStateUpdate(Red::CGameApplication* aApplication);

    /**
     * \brief Handler for Neuro actions.
     *
     * \param aAction The action description sent by Neuro.
     * \param aJobGroup The parent job group.
     */
    void DispatchNeuroAction(const neurosdk_message_action_t& aAction, const Red::JobGroup& aJobGroup);

    /**
     * \brief Reset m_neuroSocket and attempt to initialize it.
     * \return Whether or not initialization was successful.
     */
    bool InitializeConnection();

    /**
     * \brief Initialize connection to Neuro and reload it on failure.
     *
     * THIS MUST BE CALLED FROM GAME MAIN THREAD, OTHERWISE EVERYTHING BLOWS UP.
     */
    void TickStateUpdate();

    /**
     * \brief Tick function registered by the update registrar for communication updates.
     * \param aFrameInfo The frame time information.
     * \param aJobQueue The job queue provided to the game system for work.
     */
    void TickCommunication(Red::FrameInfo& aFrameInfo, Red::JobQueue& aJobQueue);

    /**
     * \brief Tick function registered by update registrar for draining user input.
     * \param aFrameInfo The previous frame's information (frametime and whatnot)
     */
    void TickInputQueue(Red::FrameInfo& aFrameInfo);

    /**
     * \brief Tick function registered by the update registrar to update scene information timers.
     * \param aFrameInfo The previous frame's information (frametime and whatnot)
     * \param aJobQueue The job queue provided by the update registrar.
     */
    void TickSceneInfo(Red::FrameInfo& aFrameInfo, Red::JobQueue& aJobQueue);

    /**
    * \brief Tick function registered by update registrar to spam functions to stress test responses for races/whatnot.
    * \param aQueue The associated frame job queue.
    */
    void TickFuzzer(Red::JobQueue& aQueue);

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

    /**
     * \brief Fire all registered script callbacks for Neuro connection state.
     * \param aState The current connection state.
     */
    void FireScriptCallbacks(bool aState);

    /**
     * \brief Fire all newly registered callbacks with Neuro connection state and clean the newly registered callback list.
     * \param aState The current connection state.
     */
    void FirePendingCallbacks(bool aState);
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
    * \brief Toggle the query action fuzzer. Available via RTTI as well.
    */
    void ToggleFuzzer();
#pragma endregion

#pragma region ScriptingUtils
    /**
     * \brief Track a mappin for Autodrive to work, as MappinSystem does not export a TrackMappin method for generic
     * mappins OOTB and I don't believe this mod should do Codeware work.
     * \param aMappin The mappin to track.
     */
    void TrackMappin(Red::Handle<Red::game::mappins::IMappin>& aMappin);

    /**
     * \brief Update current choicehub state and reset timer before sending current choicehub info to Neuro.
     * \param aRef The dialog data.
     */
    void OnSceneDialogChoiceHubsProvided(Red::game::interactions::vis::DialogChoiceHubs aRef);

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

    /**
    * \brief Check if Neuro connection is alive.
    */
    bool IsConnectionAlive();

    /**
    * \brief Register a callback for Neuro socket connection state. The function name is fixed as "OnNeuroSocketUpdate".
    * New callbacks will have this fired the next tick. The function may be called several times in one tick.
    * \param aContext The object the callback is owned by.
    */
    void RegisterAliveCallback(Red::WeakHandle<Red::IScriptable> aContext);

    /**
     * \brief Unregister a callback for Neuro socket connection state.
     * \param aContext The object the callback is owned by.
     */
    void UnregisterAliveCallback(Red::WeakHandle<Red::IScriptable> aContext);
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
