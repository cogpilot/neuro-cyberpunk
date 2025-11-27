#include <Shared/Raw/Ink/InkSystem.hpp>
#include <Shared/Raw/LocalizationSystem/LocalizationSystem.hpp>
#include <Shared/Raw/MappinSystem/MappinSystem.hpp>
#include <Shared/Raw/Player/Player.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/interactions/ChoiceCaptionBluelinePart.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/ChoiceType.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/vis/BluelineDescription.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/vis/BluelinePart.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/vis/IVisualizerTimeProvider.hpp>

#include <System/NativeResponses.hpp>
#include <System/NeuroSystem.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glaze/glaze.hpp>

using namespace Red;

namespace Impl
{
struct DriveToWaypointJson
{
    std::string destType{};
    std::string target{};
};

struct NeuroChoiceLineJson
{
    int id{};

    bool canInteract{};
    bool isImportant{};

    std::string text{};
};

struct NeuroChoicehubDescJson
{
    int hubId{};

    std::string title{};

    std::vector<NeuroChoiceLineJson> choices{};

    bool isTimed{};
    float timeSeconds{};
};

struct NeuroPerformedChoiceJson
{
    int id{};
    int hubId{};
};

class NeuroQuickhackDto : public IScriptable
{
    CString m_quickhackName{};
    CString m_quickhackDesc{};
    CString m_quickhackCategory{};
    CString m_quickhackStatus{};

    int m_quickhackCost{};

    float m_cooldown{};
    float m_uploadTime{};

    bool m_willReveal{};

    RTTI_IMPL_TYPEINFO(NeuroQuickhackDto);
    RTTI_IMPL_ALLOCATOR();
};

class NeuroQuickhackDataDto : public IScriptable
{
    ent::EntityID m_targetEntityId{};

    CString m_targetName{};
    CString m_faction{};

    bool m_isHostile{};
    bool m_isBoss{};

    DynArray<Handle<NeuroQuickhackDto>> m_quickhacks{};

    RTTI_IMPL_TYPEINFO(NeuroQuickhackDataDto);
    RTTI_IMPL_ALLOCATOR();
};
} // namespace Impl

/**
 * \brief Helper for switch() statements with strings.
 */
#define CNAME_HASH(x) CName(x).hash

namespace Instance
{
// This is safe, as game system only gets destructed when exiting the game.
mod::NeuroSystem* Instance{};
} // namespace Instance

// Note: currently all of those ignore return values, I guess we could add some logging?
void mod::NeuroActionResponseMessage::DispatchNeuroMessage(neuro::NeuroSocket& aSocket)
{
    aSocket.RespondToAction(m_actionId, m_actionResponse);
}

bool mod::NeuroActionResponseMessage::IsResponseToForcedAction() const
{
    // Note: evil hardcode
    return m_actionName == "select_dialogue_choice" || m_actionName == "run_quickhack_on_target";
}

void mod::NeuroContextMessage::DispatchNeuroMessage(neuro::NeuroSocket& aSocket)
{
    aSocket.SendContext(m_message);
}

void mod::NeuroForcedActionMessage::DispatchNeuroMessage(neuro::NeuroSocket& aSocket)
{
    aSocket.SendForcedAction(m_actionName, m_query, m_state);
}

void mod::NeuroSystem::DispatchNeuroAction(const neurosdk_message_action_t& aAction, neuro::NeuroSocket&)
{
    // Copy these in sync call - we do not own them
    auto response = MakeHandle<NeuroActionResponseMessage>();

    response->m_actionName = aAction.name;
    response->m_actionId = aAction.id;
    response->m_actionData = aAction.data;

    // Check if this is a forced action response
    std::unique_lock lock(GetInstance()->m_messageLock);
    if (GetInstance()->m_hasSentForcedActionMessage && response->IsResponseToForcedAction())
    {
        // If it is, drop the flag so we can send new forced action messages
        GetInstance()->m_hasSentForcedActionMessage = false;
    }

    if (GetInstance()->IsPreGame())
    {
        // We cannot service Neuro's requests in pregame.
        response->m_actionResponse = "You are not yet ingame. Requests cannot be served";
        GetInstance()->AddMessage(response);
        return;
    }

    JobQueue().Dispatch(
        [response = std::move(response)]()
        {
            util::Timestamp startTime{};

            // Handle action
            switch (CNAME_HASH(response->m_actionName.c_str()))
            {
            case CNAME_HASH("query_money"):
            {
                if (!CallVirtual(GetInstance(), "OnQueryMoney", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_quest_context"):
            {
                if (!CallVirtual(GetInstance(), "OnQueryTrackedQuest", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_all_quests"):
            {
                if (!CallVirtual(GetInstance(), "OnQueryAllQuests", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_waypoints"):
            {
                response->m_actionResponse = NeuroResponses::CreateMappinQueryResponse();
                break;
            }
            case CNAME_HASH("summon_car"):
            {
                if (!CallVirtual(GetInstance(), "OnSummonVehicle", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("drive_to_waypoint"):
            {
                // Note: first complex action that needs parsing JSON
                Impl::DriveToWaypointJson json{};

                // operator bool overload for glz::error_ctx returns true on failure!
                if (glz::read_json(json, response->m_actionData.c_str()))
                {
                    response->m_actionResponse = "Failed to parse action data JSON.";
                    break;
                }

                if (json.destType == "id")
                {
                    NewMappinID mappinId{};

                    if (std::from_chars(json.target.c_str(), json.target.c_str() + json.target.size(), mappinId.value)
                            .ec != std::errc())
                    {
                        response->m_actionResponse = "Failed to parse mappin ID.";
                        break;
                    }

                    if (!CallVirtual(GetInstance(), "OnAutodriveToMappin", response->m_actionResponse, mappinId))
                    {
                        response->m_actionResponse = "Failed to call responder method.";
                    }
                }
                else if (json.destType == "district")
                {
                    CString districtName{json.target.c_str()};

                    if (!CallVirtual(GetInstance(), "OnAutodriveToDistrict", response->m_actionResponse, districtName))
                    {
                        response->m_actionResponse = "Failed to call responder method.";
                    }
                }
                else
                {
                    response->m_actionResponse = "Invalid destType provided.";
                    break;
                }

                break;
            }
            case CNAME_HASH("select_dialogue_choice"):
            {
                Impl::NeuroPerformedChoiceJson json{};
                // operator bool overload for glz::error_ctx returns true on failure!
                if (glz::read_json(json, response->m_actionData.c_str()))
                {
                    response->m_actionResponse = "Failed to parse action data JSON.";
                    break;
                }

                if (json.id < 0 || json.hubId < 0)
                {
                    response->m_actionResponse = "Invalid choice ID or hub ID provided.";
                    break;
                }

                if (!CallVirtual(GetInstance(), "OnSelectDialogueChoice", response->m_actionResponse, json.hubId,
                                 json.id))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            default:
            {
                response->m_actionResponse = "Sorry, this action is not implemented yet.";
                break;
            }
            }

            // Add response to message queue
            GetInstance()->AddMessage(response);

            Context::Spew("Action '{}' handled in {} ms", response->m_actionName.c_str(),
                          startTime.TimePassedMs().count());
        });
}

bool mod::NeuroSystem::InitializeConnection()
{
    // Drain message queue before reinitialization
    std::unique_lock queueLock(m_messageLock);
    m_messageQueue.Clear();

    m_neuroSocket.reset();
    m_neuroSocket = std::make_unique<neuro::NeuroSocket>();

    return m_neuroSocket->Initialize(DispatchNeuroAction);
}

void mod::NeuroSystem::Tick(FrameInfo& aFrameInfo, JobQueue& aJobQueue)
{
    const auto dt = aFrameInfo.deltaTime;

    aJobQueue.Dispatch(
        [this, dt](const JobGroup& aGroup)
        {
            std::unique_lock lock(m_socketLock);

            auto firstTick = !m_neuroSocket;

            if (!m_neuroSocket)
            {
                // Just in case: drain the message queue even if it cannot be served
                std::unique_lock queueLock(m_messageLock);
                m_messageQueue.Clear();

                if (m_disableConnection)
                {
                    return;
                }

                if (!m_hasHadSuccessfulConnection &&
                    m_firstConnectionAttempts >= MaxConnectionAttemptsForFirstConnection)
                {
                    Context::Spew("Maximum initial connection attempts to Neuro reached, not trying again.");
                    m_disableConnection = true;
                    return;
                }

                // TODO: if we fail too often, send popup to game saying Neuro is unreachable and don't try to connect again?
                if (m_failedToConnectBefore && m_lastRetryTime.TimePassed() <= RetryTimeSeconds)
                {
                    return;
                }

                if (!InitializeConnection())
                {
                    if (!m_hasHadSuccessfulConnection)
                    {
                        m_firstConnectionAttempts++;
                    }

                    m_lastRetryTime = {};
                    m_failedToConnectBefore = true;
                    return;
                }
            }

            // If it's the first tick after connection, inform Neuro about the game state and if she can make commands
            if (firstTick)
            {
                m_hasHadSuccessfulConnection = true;
                if (!IsPreGame())
                {
                    SendContext("The game is currently ingame. Commands can be executed.");

                    // For convenience, construct message and add to message queue on scripting side
                    // Note: this might lag a bit due to sync call
                    CallVirtual(this, "OnConnectedIngame");
                }
                else
                {
                    SendContext("The game is currently in the main menu. No commands can be executed.");
                }
            }

            if (!m_neuroSocket->Tick())
            {
                // Things are bad
                m_neuroSocket.reset();
                m_lastRetryTime = {};
                m_failedToConnectBefore = true;
                return;
            }

            // Drain message queue
            std::unique_lock queueLock(m_messageLock);

            if (m_hasSentForcedActionMessage)
            {
                m_forcedActionMessageTimer += dt;

                if (m_forcedActionMessageTimer >= MaxKeepTrackOfForcedActions)
                {
                    Context::Spew("Forcing reset of forced action message state after {} seconds.",
                                  MaxKeepTrackOfForcedActions);
                    m_hasSentForcedActionMessage = false;
                    m_forcedActionMessageTimer = 0.f;
                }
            }

            for (auto& i : m_messageQueue)
            {
                if (const auto& asForced = Cast<NeuroForcedActionMessage>(i))
                {
                    // If we have already sent a forced action message, drop this one
                    if (m_hasSentForcedActionMessage)
                    {
                        Context::Spew("Dropping forced action message '{}' as another one is pending response.",
                                      asForced->m_actionName.c_str());
                        continue;
                    }
                    m_hasSentForcedActionMessage = true;
                    m_forcedActionMessageTimer = 0.f;
                }

                i->DispatchNeuroMessage(*m_neuroSocket);
            }

            m_messageQueue.Clear();
        });
}

void mod::NeuroSystem::DrainInputQueue(FrameInfo& aInfo)
{
    std::unique_lock lock(m_inputLock);

    if (m_injectedKeyQueueIndex >= m_injectedKeyQueue.size)
    {
        if (m_injectedKeyQueue.size > 0u)
        {
            m_injectedKeyQueue.Clear();
            m_injectedKeyQueueIndex = 0u;
            m_inputTimer = 0.f;
        }

        return;
    }

    m_inputTimer += aInfo.deltaTime;

    // Ignore timer for first input
    if (m_inputTimer < DelayBetweenInputs && m_injectedKeyQueueIndex != 0u)
    {
        return;
    }

    m_inputTimer = 0.f;

    auto key = m_injectedKeyQueue[m_injectedKeyQueueIndex];

    auto player = shared::raw::PlayerManager::GetPlayerObject(GetGameSystem<game::PlayerManager>());

    if (player)
    {
        shared::raw::Ink::SyntheticInputBuffer buffer{};
        shared::raw::Ink::RawInputData data{};

        data.action = Red::EInputAction::IACT_Press;
        data.key = key;
        data.value = 1.f;
        data.unk18 = 1;

        buffer.GetInputs().PushBack(data);

        // Make it click action?
        data.action = Red::EInputAction::IACT_Release;

        buffer.GetInputs().PushBack(data);

        shared::raw::Player::ProcessInput(player, buffer);
    }

    m_injectedKeyQueueIndex++;
}

void mod::NeuroSystem::AddMessage(const Handle<NeuroMessage>& aMsg)
{
    std::unique_lock lock(m_messageLock);
    m_messageQueue.PushBack(aMsg);
}

void mod::NeuroSystem::SendContextDirect(StringView aContextInfo)
{
    std::unique_lock lock(m_socketLock);
    if (m_neuroSocket)
    {
        m_neuroSocket->SendContext(aContextInfo.Data());
    }
}

void mod::NeuroSystem::SendContext(const CString& aContextInfo)
{
    auto msg = MakeHandle<NeuroContextMessage>();

    msg->m_message = aContextInfo;

    AddMessage(msg);
}

bool mod::NeuroSystem::IsPreGame()
{
    auto handler = shared::raw::Ink::InkSystem::Get()->m_requestsHandler.Lock();
    return shared::raw::Ink::SystemRequestsHandler::IsPreGame(handler);
}

mod::NeuroSystem* mod::NeuroSystem::GetInstance()
{
    return Instance::Instance;
}

void mod::NeuroSystem::TrackMappin(Handle<game::mappins::IMappin>& aMappin)
{
    auto mappinSystem = GetGameSystem<game::mappins::MappinSystem>();
    auto mappinId = shared::raw::Mappin::MappinID(aMappin);

    shared::raw::MappinSystem::TrackMappinByID(mappinSystem, mappinId);
}

void mod::NeuroSystem::InjectKeypress(EInputKey aKey)
{
    std::unique_lock lock(m_inputLock);
    m_injectedKeyQueue.PushBack(aKey);
}

void mod::NeuroSystem::InjectKeypressChain(const Red::DynArray<Red::EInputKey>& aKeys)
{
    std::unique_lock lock(m_inputLock);
    for (auto key : aKeys)
    {
        m_injectedKeyQueue.PushBack(key);
    }
}

void mod::NeuroSystem::InjectActionPress(CName aActionName, float aDurationSeconds)
{
}

void mod::NeuroSystem::OnSceneListChoiceDataProvided(Red::ScriptRef<game::interactions::vis::ListChoiceHubData>& aRef)
{
    /**
     * Schema:
     * {
     *      hubId: int,
     *      title: string,
     *      choices: [{
     *          id: int,
     *          canInteract: bool,
     *          isImportant: bool,
     *          text: string
     *      }],
     *      isTimed: bool,
     *      timeSeconds: float
     * }
     *
     */
    static auto LocalizationSystem = shared::raw::Localization::LocalizationSystem::GetInstance();

    if (!aRef)
    {
        return;
    }

    std::unique_lock lock(m_choicehubLock);

    // Note: a new ID is generated when choicehub gets recreated
    if (m_encounteredChoiceHubIDs.contains(aRef->id))
    {
        return;
    }

    m_encounteredChoiceHubIDs.insert(aRef->id);

    // Note: ChoiceTypeWrapper.IsType is just bitfield

    Impl::NeuroChoicehubDescJson neuroContext{};

    neuroContext.title = aRef->title.c_str();
    neuroContext.hubId = aRef->id;

    for (auto it = 0u; it < aRef->choices.size; it++)
    {
        auto& entry = aRef->choices[it];

        Impl::NeuroChoiceLineJson neuroChoiceInfo{};

        std::vector<std::string> optionParts{};

        neuroChoiceInfo.id = it;
        neuroChoiceInfo.canInteract = true;

        const auto flags = entry.type.properties;

        if ((flags & ((uint32_t)game::interactions::ChoiceType::Inactive |
                      (uint32_t)game::interactions::ChoiceType::CheckFailed)) != 0u)
        {
            // We can't interact with these
            neuroChoiceInfo.canInteract = false;
        }

        if ((flags & ((uint32_t)game::interactions::ChoiceType::QuestImportant |
                      (uint32_t)game::interactions::ChoiceType::Glowline)) != 0u)
        {
            neuroChoiceInfo.isImportant = true;
        }

        for (auto& captionPart : entry.captionParts.parts)
        {
            if (auto& asBluelinePart = Cast<game::interactions::ChoiceCaptionBluelinePart>(captionPart))
            {
                if (asBluelinePart->blueline->parts.size == 0u)
                {
                    continue;
                }

                auto& part = asBluelinePart->blueline->parts[0];

                switch (part->GetType()->GetName())
                {
                case CNAME_HASH("LifePathBluelinePart"):
                {
                    if (!part->passed)
                    {
                        // Just ignore it
                        break;
                    }
                    auto& lifepathRecord = Red::GetProperty<Handle<TweakDBRecord>>(part, "record");

                    gamedataLocKeyWrapper displayName{};

                    if (!TweakDB::Get()->TryGetValue<gamedataLocKeyWrapper>({lifepathRecord->recordID, ".displayName"},
                                                                            displayName))
                    {
                        break;
                    }

                    auto name = fmt::format("LocKey#{}", displayName.primaryKey);

                    CString localizedName{};
                    StringView nameView = name.c_str();

                    LocalizationSystem->GetOnscreen(localizedName, nameView);

                    optionParts.push_back(fmt::format("[{}]", localizedName.c_str()));
                    break;
                }
                case CNAME_HASH("BuildBluelinePart"):
                {
                    auto lhs = Red::GetProperty<int>(part, "lhsValue");
                    auto rhs = Red::GetProperty<int>(part, "rhsValue");
                    auto& buildRecord = Red::GetProperty<Handle<TweakDBRecord>>(part, "record");

                    gamedataLocKeyWrapper displayName{};

                    if (!TweakDB::Get()->TryGetValue<gamedataLocKeyWrapper>({buildRecord->recordID, ".displayName"},
                                                                            displayName))
                    {
                        break;
                    }

                    // I know this is bad, but I can't be bothered to rev more localization
                    auto name = fmt::format("LocKey#{}", displayName.primaryKey);

                    CString localizedName{};
                    StringView nameView = name.c_str();

                    LocalizationSystem->GetOnscreen(localizedName, nameView);

                    if (part->passed)
                    {
                        optionParts.push_back(fmt::format("[Passed {}: {}/{}]", localizedName.c_str(), lhs, rhs));
                    }
                    else
                    {
                        optionParts.push_back(fmt::format("[Failed {}: {}/{}]", localizedName.c_str(), lhs, rhs));
                    }

                    break;
                }
                case CNAME_HASH("PaymentBluelinePart"):
                {
                    auto neededMoney = Red::GetProperty<int>(part, "paymentMoney");
                    auto playerMoney = Red::GetProperty<int>(part, "playerMoney");

                    if (part->passed)
                    {
                        optionParts.push_back(fmt::format("[Pay {} eddies]", neededMoney));
                    }
                    else
                    {
                        optionParts.push_back(fmt::format("[Not enough money! {} eddies needed, has {} eddies]",
                                                          neededMoney, playerMoney));
                    }
                    break;
                }
                default:
                    break;
                }
            }

            if (!optionParts.empty())
            {
                break;
            }
        }

        CString captionTags{};

        if (CallGlobal("GetCaptionTagsFromArray;script_ref<array<InteractionChoiceCaptionPart>>", captionTags,
                       entry.captionParts.parts))
        {
            if (captionTags.Length() > 0)
            {
                optionParts.push_back(fmt::format("[{}]", captionTags.c_str()));
            }
        }

        optionParts.push_back(entry.localizedName.c_str());

        neuroChoiceInfo.text = fmt::format("{}", fmt::join(optionParts, " "));

        neuroContext.choices.push_back(neuroChoiceInfo);
    }

    // Note: we ignore timed *choice options*, I don't think those are used
    // Timed choicehubs, however, very much are
    if (auto timeProvider = aRef->timeProvider.Lock())
    {
        constexpr auto VisualizerGetDuration =
            shared::util::RawVFunc<0x110, float (*)(game::interactions::vis::IVisualizerTimeProvider*)>();

        constexpr auto VisualizerGetProgress =
            shared::util::RawVFunc<0x108, float (*)(game::interactions::vis::IVisualizerTimeProvider*)>();

        neuroContext.isTimed = true;
        neuroContext.timeSeconds = VisualizerGetDuration(timeProvider) - VisualizerGetProgress(timeProvider);
    }

    auto msg = MakeHandle<NeuroForcedActionMessage>();

    msg->m_query = "You are presented with the following options. Respond with the hub ID and choice option ID if you "
                   "want to pick an option. If you do not want to pick an option, send a -1 to the hub ID. Some of the "
                   "options may not be active for various reasons";

    msg->m_actionName = "select_dialogue_choice";

    std::string json{};

    if (glz::write_json(neuroContext, json))
    {
        return;
    }

    msg->m_state = json.c_str();

    AddMessage(msg);
}

void mod::NeuroSystem::OnRegisterUpdates(UpdateRegistrar* aRegistrar)
{
    // Note: not sure how good an idea using PreRenderUpdate is, but it runs in pause menu, so...
    aRegistrar->RegisterUpdate(UpdateTickGroup::PreRenderUpdate, this, "NeuroSystem/Communicate",
                               [this](FrameInfo& aFrameInfo, JobQueue& aJobQueue) { Tick(aFrameInfo, aJobQueue); });

    aRegistrar->RegisterUpdate(UpdateTickGroup::PreBuckets, this, "NeuroSystem/DrainInput",
                               [this](FrameInfo& aFrameInfo, JobQueue&) { DrainInputQueue(aFrameInfo); });
}

std::uint32_t mod::NeuroSystem::OnBeforeGameSave(const JobGroup& aJobGroup, void* aMetadataObject)
{
    SendContext("The game is being saved.");

    return 0u;
}

void mod::NeuroSystem::OnGamePrepared()
{
    SendContextDirect("The game world is being loaded. Either a save has been loaded or a new game was started.");
}

void mod::NeuroSystem::OnWorldDetached(world::RuntimeScene* aScene)
{
    SendContextDirect(
        "The game world is being exited. Either a save is being loaded or the game is being exited to main menu.");

    std::unique_lock sceneLock(m_choicehubLock);
    m_encounteredChoiceHubIDs.clear();

    std::unique_lock inputLock(m_inputLock);
    m_injectedKeyQueue.Clear();
}

void mod::NeuroSystem::OnInitialize(const Red::JobHandle& aJob)
{
    Instance::Instance = this;
}

RTTI_DEFINE_CLASS(Impl::NeuroQuickhackDataDto, {
    RTTI_PROPERTY(m_targetEntityId);
    RTTI_PROPERTY(m_targetName);
    RTTI_PROPERTY(m_faction);
    RTTI_PROPERTY(m_isHostile);
    RTTI_PROPERTY(m_isBoss);
    RTTI_PROPERTY(m_quickhacks);
});

RTTI_DEFINE_CLASS(Impl::NeuroQuickhackDto, {
    RTTI_PROPERTY(m_quickhackName);
    RTTI_PROPERTY(m_quickhackDesc);
    RTTI_PROPERTY(m_quickhackCategory);
    RTTI_PROPERTY(m_quickhackStatus);
    RTTI_PROPERTY(m_quickhackCost);
    RTTI_PROPERTY(m_cooldown);
    RTTI_PROPERTY(m_uploadTime);
    RTTI_PROPERTY(m_willReveal);
});

RTTI_DEFINE_CLASS(mod::NeuroSystem, {
    RTTI_METHOD(SendContext);
    RTTI_METHOD(TrackMappin);
    RTTI_METHOD(InjectKeypress);
    RTTI_METHOD(InjectActionPress);
    RTTI_METHOD(InjectKeypressChain);
    RTTI_METHOD(OnSceneListChoiceDataProvided);
});

RTTI_DEFINE_CLASS(mod::NeuroMessage, { RTTI_ABSTRACT(); });
RTTI_DEFINE_CLASS(mod::NeuroActionResponseMessage, { RTTI_PARENT(mod::NeuroMessage); });
RTTI_DEFINE_CLASS(mod::NeuroContextMessage, { RTTI_PARENT(mod::NeuroMessage); });
RTTI_DEFINE_CLASS(mod::NeuroForcedActionMessage, { RTTI_PARENT(mod::NeuroMessage); });
