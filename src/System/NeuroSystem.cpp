#include <Shared/Raw/Ink/InkSystem.hpp>
#include <Shared/Raw/LocalizationSystem/LocalizationSystem.hpp>
#include <Shared/Raw/MappinSystem/MappinSystem.hpp>
#include <Shared/Raw/Player/Player.hpp>

#include <RED4ext/Scripting/Natives/Generated/game/interactions/ChoiceCaptionBluelinePart.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/interactions/ChoiceCaptionStringPart.hpp>
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
class NeuroQuickhackDto : public IScriptable
{
public:
    int m_id{};

    CString m_quickhackName{};
    CString m_quickhackDesc{};
    CString m_quickhackCategory{};
    CString m_quickhackStatus{};

    int m_quickhackCost{};

    float m_cooldown{};
    float m_uploadTime{};

    bool m_willReveal{};
    bool m_canUse{};

    RTTI_IMPL_TYPEINFO(NeuroQuickhackDto);
    RTTI_IMPL_ALLOCATOR();
};

class NeuroQuickhackDataDto : public IScriptable
{
public:
    ent::EntityID m_targetEntityId{};

    CString m_targetName{};
    CString m_faction{};

    // Are we currently in combat?
    bool m_isHostile{};

    // Is this a boss enemy?
    bool m_isBoss{};

    // Is this an enemy at all or just object?
    bool m_isInanimate{};

    // How much RAM do we have?
    int m_ramAmount{};

    DynArray<Handle<NeuroQuickhackDto>> m_quickhacks{};

    RTTI_IMPL_TYPEINFO(NeuroQuickhackDataDto);
    RTTI_IMPL_ALLOCATOR();
};

class NeuroPhoneMessageDto : public IScriptable
{
public:
    // Simplify handling
    WeakHandle<IScriptable> m_messageController{};

    CString m_contact{};
    CString m_convoNameIfApplicable{};
    CString m_messageHistory{};
    DynArray<CString> m_responseOptions{};

    RTTI_IMPL_TYPEINFO(NeuroPhoneMessageDto);
    RTTI_IMPL_ALLOCATOR();
};

/**
 * \brief For interop with Glaze JSON serialization/deserialization, as I'm not sure on being able to make REDengine
 * types work with Glaze.
 */
namespace JSON
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

// NOTE: rework this for new idea
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
};

struct NeuroQuickhackJson
{
    int id{};

    bool canUse{};

    std::string name{};
    std::string desc{};
    std::string category{};
    std::string status{};

    int ramCost{};

    float cooldown{};
    float uploadTime{};

    bool willReveal{};
};

struct NeuroQuickhackDataJson
{
    std::string targetName{};
    std::string faction{};

    int ramAmount{};

    bool isInanimate{};
    bool isHostile{};
    bool isBoss{};

    std::vector<NeuroQuickhackJson> quickhacks{};

    static NeuroQuickhackDataJson FromGame(Handle<NeuroQuickhackDataDto>& aData)
    {
        NeuroQuickhackDataJson json{};

        json.targetName = aData->m_targetName.c_str();
        json.faction = aData->m_faction.c_str();
        json.ramAmount = aData->m_ramAmount;
        json.isInanimate = aData->m_isInanimate;
        json.isHostile = aData->m_isHostile;
        json.isBoss = aData->m_isBoss;

        for (const auto& quickhack : aData->m_quickhacks)
        {
            NeuroQuickhackJson quickhackJson{};

            quickhackJson.id = quickhack->m_id;
            quickhackJson.name = quickhack->m_quickhackName.c_str();
            quickhackJson.desc = quickhack->m_quickhackDesc.c_str();
            quickhackJson.category = quickhack->m_quickhackCategory.c_str();
            quickhackJson.status = quickhack->m_quickhackStatus.c_str();

            quickhackJson.ramCost = quickhack->m_quickhackCost;
            quickhackJson.cooldown = quickhack->m_cooldown;
            quickhackJson.uploadTime = quickhack->m_uploadTime;
            quickhackJson.willReveal = quickhack->m_willReveal;
            quickhackJson.canUse = quickhack->m_canUse;

            json.quickhacks.push_back(std::move(quickhackJson));
        }

        return json;
    }
};

struct NeuroQuickhackResponseJson
{
    int id{};
};

struct NeuroSMSChoiceEntryJson
{
    int id{};
    std::string text{};
};

struct NeuroSMSJson
{
    std::string contact{};
    std::string convoNameIfApplicable{};
    std::string chatHistory{};

    std::vector<NeuroSMSChoiceEntryJson> choices{};

    static NeuroSMSJson FromGame(Handle<NeuroPhoneMessageDto>& aData)
    {
        NeuroSMSJson ret{};

        ret.contact = aData->m_contact.c_str();
        ret.convoNameIfApplicable = aData->m_convoNameIfApplicable.c_str();
        ret.chatHistory = aData->m_messageHistory.c_str();

        for (auto i = 0u; i < aData->m_responseOptions.size; i++)
        {
            NeuroSMSChoiceEntryJson choice{};

            choice.id = i;
            choice.text = aData->m_responseOptions[i].c_str();

            ret.choices.push_back(std::move(choice));
        }

        return ret;
    }
};

struct NeuroSMSResponseJson
{
    int id{};
};
} // namespace JSON
} // namespace Impl

/**
 * \brief Helper for switch() statements with strings.
 */
#define CNAME_HASH(x) CName(x).hash

mod::NeuroChoiceContext mod::NeuroChoiceContext::FromGameData(game::interactions::vis::ListChoiceData& aChoiceData,
                                                              CString& aHubTitle, int aId, bool aIsTimed,
                                                              float aChoiceTimer)
{
    static auto LocalizationSystem = shared::raw::Localization::LocalizationSystem::GetInstance();
    NeuroChoiceContext ctx{.m_id = aId,
                           .m_hubTitle = aHubTitle.c_str(),
                           .m_canChoose = true,
                           .m_important = false,
                           .m_isTimed = aIsTimed,
                           .m_timer = aChoiceTimer};

    std::vector<std::string> optionParts{};

    const auto flags = aChoiceData.type.properties;

    if ((flags & ((uint32_t)game::interactions::ChoiceType::Inactive |
                  (uint32_t)game::interactions::ChoiceType::CheckFailed)) != 0u)
    {
        // We can't interact with these
        ctx.m_canChoose = false;
    }

    if ((flags & ((uint32_t)game::interactions::ChoiceType::QuestImportant |
                  (uint32_t)game::interactions::ChoiceType::Glowline)) != 0u)
    {
        ctx.m_important = true;
    }

    std::vector<std::string> stringTags{};

    auto hasBluelineCondition = false;
    for (auto& captionPart : aChoiceData.captionParts.parts)
    {
        if (!hasBluelineCondition)
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

                    auto localizedName = LocalizationSystem->GetOnscreen(name.c_str());

                    optionParts.push_back(fmt::format("[{}]", localizedName.c_str()));
                    hasBluelineCondition = true;
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
                    auto localizedName = LocalizationSystem->GetOnscreen(name.c_str());

                    if (part->passed)
                    {
                        optionParts.push_back(fmt::format("[Passed {}: {}/{}]", localizedName.c_str(), lhs, rhs));
                    }
                    else
                    {
                        optionParts.push_back(fmt::format("[Failed {}: {}/{}]", localizedName.c_str(), lhs, rhs));
                    }
                    hasBluelineCondition = true;

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
                    hasBluelineCondition = true;
                    break;
                }
                default:
                    break;
                }
            }
        }

        if (const auto& asStringPart = Cast<game::interactions::ChoiceCaptionStringPart>(captionPart))
        {
            stringTags.push_back(LocalizationSystem->GetOnscreen(asStringPart->content).c_str());
        }
    }

    if (!stringTags.empty())
    {
        optionParts.push_back(fmt::format("[{}]", fmt::join(stringTags.rbegin(), stringTags.rend(), " ")));
    }

    optionParts.push_back(aChoiceData.localizedName.c_str());

    ctx.m_text = fmt::format("{}", fmt::join(optionParts, " "));

    return ctx;
}

uint64_t mod::NeuroSceneDataContext::HashChoices()
{
    // Start with hash seed
    uint64_t hash = 0xCBF29CE484222325ull;

    for (auto& i : m_choices)
    {
        hash = FNV1a64((uint8_t*)&i.m_id, sizeof(int), hash);

        if (!i.m_hubTitle.empty())
        {
            hash = FNV1a64((uint8_t*)i.m_hubTitle.data(), i.m_hubTitle.size(), hash);
        }

        if (!i.m_text.empty())
        {
            hash = FNV1a64((uint8_t*)i.m_text.data(), i.m_text.size(), hash);
        }

        hash = FNV1a64((uint8_t*)&i.m_canChoose, sizeof(bool), hash);
        hash = FNV1a64((uint8_t*)&i.m_important, sizeof(bool), hash);
        hash = FNV1a64((uint8_t*)&i.m_isTimed, sizeof(bool), hash);
        hash = FNV1a64((uint8_t*)&i.m_timer, sizeof(float), hash);
    }

    return hash;
}

namespace Instance
{
// This is safe, as game system only gets destructed when exiting the game.
mod::NeuroSystem* Instance{};
} // namespace Instance

// Note: currently all of those ignore return values, I guess we could add some logging?
void mod::NeuroActionResponseMessage::DispatchNeuroMessage(neuro::NeuroSocket& aSocket)
{
    aSocket.RespondToAction(m_actionId, m_actionResponse, m_success);
}

bool mod::NeuroActionResponseMessage::IsResponseToForcedAction() const
{
    // Note: evil hardcode
    // Evil hardcode has claimed 1 (one) action to date
    return m_actionName == "select_dialogue_choice" || m_actionName == "run_quickhack_on_target" ||
           m_actionName == "select_sms_message_choice";
}

void mod::NeuroContextMessage::DispatchNeuroMessage(neuro::NeuroSocket& aSocket)
{
    aSocket.SendContext(m_message, m_silent);
}

void mod::NeuroForcedActionMessage::DispatchNeuroMessage(neuro::NeuroSocket& aSocket)
{
    aSocket.SendForcedAction(m_actionName, m_query, m_state);
}

void mod::NeuroSystem::DispatchNeuroAction(const neurosdk_message_action_t& aAction, const JobGroup& aJobGroup)
{
    // Copy these in sync call - we do not own them
    auto response = MakeHandle<NeuroActionResponseMessage>();

    response->m_actionName = aAction.name;
    response->m_actionId = aAction.id;
    response->m_actionData = aAction.data;
    {
        // In scope to not deadlock on pregame
        // Check if this is a forced action response
        std::unique_lock lock(m_messageLock);
        if (m_hasSentForcedActionMessage && response->IsResponseToForcedAction())
        {
            // If it is, drop the flag so we can send new forced action messages
            m_hasSentForcedActionMessage = false;
        }
    }

    if (IsPreGame())
    {
        // We cannot service Neuro's requests in pregame.
        response->m_actionResponse = "You are not yet ingame. Requests cannot be served";
        AddMessage(response);
        return;
    }

    JobQueue(aJobGroup).Dispatch(
        [this, response = std::move(response)]()
        {
            util::Timestamp startTime{};

            // Handle action
            switch (CNAME_HASH(response->m_actionName.c_str()))
            {
            case CNAME_HASH("query_money"):
            {
                if (!CallVirtual(this, "OnQueryMoney", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_quest_context"):
            {
                if (!CallVirtual(this, "OnQueryTrackedQuest", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_all_quests"):
            {
                if (!CallVirtual(this, "OnQueryAllQuests", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_player_info"):
            {
                if (!CallVirtual(this, "OnQueryPlayerInfo", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("query_inventory"):
            {
                if (!CallVirtual(this, "OnQueryInventory", response->m_actionResponse))
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
                if (!CallVirtual(this, "OnSummonVehicle", response->m_actionResponse))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("drive_to_waypoint"):
            {
                // Note: first complex action that needs parsing JSON
                Impl::JSON::DriveToWaypointJson json{};

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

                    if (!CallVirtual(this, "OnAutodriveToMappin", response->m_actionResponse, mappinId))
                    {
                        response->m_actionResponse = "Failed to call responder method.";
                    }
                }
                else if (json.destType == "district")
                {
                    CString districtName{json.target.c_str()};

                    if (!CallVirtual(this, "OnAutodriveToDistrict", response->m_actionResponse, districtName))
                    {
                        response->m_actionResponse = "Failed to call responder method.";
                    }
                }
                else if (json.destType == "tracked")
                {
                    if (!CallVirtual(this, "OnAutodriveToTracked", response->m_actionResponse))
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
                Impl::JSON::NeuroPerformedChoiceJson json{};
                // Note: this action is special and should be tried until Neuro succeeds
                response->m_success = false;

                {
                    // Reset these
                    std::unique_lock lock(m_choicehubLock);
                    m_countdownToForcedChoiceSelectionStarted = false;
                    m_choiceHubAvailableTime = 0.f;
                }

                // operator bool overload for glz::error_ctx returns true on failure!
                if (glz::read_json(json, response->m_actionData.c_str()))
                {
                    response->m_actionResponse = "Failed to parse action data JSON.";
                    break;
                }

                if (!CallVirtual(this, "OnSelectDialogueChoice", response->m_actionResponse, json.id,
                                 response->m_success))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("run_quickhack_on_target"):
            {
                Impl::JSON::NeuroQuickhackResponseJson json{};
                // operator bool overload for glz::error_ctx returns true on failure!
                if (glz::read_json(json, response->m_actionData.c_str()))
                {
                    response->m_actionResponse = "Failed to parse action data JSON.";
                    break;
                }

                if (json.id < 0)
                {
                    response->m_actionResponse = "Invalid quickhack ID provided.";
                    break;
                }

                // Ugly code, but should avoid lock
                EntityID targetId = InterlockedExchange64((volatile long long*)(&m_quickhackActionTargetId.hash), 0LL);

                if (!targetId)
                {
                    response->m_actionResponse =
                        "No entity was specified as quickhack target. Did you skip the forced action?";
                    break;
                }

                if (!CallVirtual(this, "OnQuickhackTarget", response->m_actionResponse, targetId, json.id))
                {
                    response->m_actionResponse = "Failed to call responder method.";
                }
                break;
            }
            case CNAME_HASH("select_sms_message_choice"):
            {
                Impl::JSON::NeuroSMSResponseJson json{};
                // operator bool overload for glz::error_ctx returns true on failure!
                if (glz::read_json(json, response->m_actionData.c_str()))
                {
                    response->m_actionResponse = "Failed to parse action data JSON.";
                    break;
                }

                std::unique_lock lock(m_smsLock);

                if (auto locked = m_actionMessengerDialogViewController.Lock())
                {
                    if (!CallVirtual(locked, "MakeSyntheticMessageResponse", response->m_actionResponse, json.id))
                    {
                        response->m_actionResponse = "Failed to call responder method.";
                    }
                }
                else
                {
                    response->m_actionResponse = "Failed to obtain messenger dialog handle.";
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
            AddMessage(response);

            Context::Spew("Action '{}' handled in {} ms", response->m_actionName.c_str(),
                          startTime.TimePassedMs().count());
        });
}

bool mod::NeuroSystem::OnGameStateUpdate(CGameApplication* aApp)
{
    if (!GetInstance())
    {
        // Note: I don't think this should ever happen?
        // Game system initialization *should* happen earlier than running state
        return false;
    }

    GetInstance()->TickStateUpdate();
    return false;
}

bool mod::NeuroSystem::InitializeConnection()
{
    // Drain message queue before reinitialization
    std::unique_lock queueLock(m_messageLock);
    m_messageQueue.Clear();

    m_neuroSocket.reset();
    m_neuroSocket = std::make_unique<neuro::NeuroSocket>();

    return m_neuroSocket->Initialize();
}

void mod::NeuroSystem::TickStateUpdate()
{
    std::unique_lock lock(m_socketLock);

    auto firstTick = !m_neuroSocket;

    if (!m_neuroSocket)
    {
        if (m_disableConnection)
        {
            return;
        }

        if (!m_hasHadSuccessfulConnection && m_firstConnectionAttempts >= MaxConnectionAttemptsForFirstConnection)
        {
            // NOTE: maybe make popup in game?
            Context::Spew("Maximum initial connection attempts to Neuro reached, not trying again.");
            CallVirtual(this, "OnConnectionFailure");

            m_disableConnection = true;
            return;
        }

        // TODO: if we fail too often, send popup to game saying Neuro is unreachable and don't try to connect
        // again?
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
}

void mod::NeuroSystem::TickCommunication(FrameInfo& aFrameInfo, JobQueue& aJobQueue)
{
    const auto dt = aFrameInfo.deltaTime;

    aJobQueue.Dispatch(
        [this, dt](const JobGroup& aGroup)
        {
            std::unique_lock lock(m_socketLock);

            if (!m_neuroSocket)
            {
                // Note: In this case, message queue will be drained by the main thread
                return;
            }

            if (!m_neuroSocket->Tick([this, &aGroup](const neurosdk_message_action_t& aAction)
                                     { DispatchNeuroAction(aAction, aGroup); }))
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

void mod::NeuroSystem::TickInputQueue(FrameInfo& aInfo)
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

void mod::NeuroSystem::TickSceneInfo(FrameInfo& aInfo, JobQueue& aJobQueue)
{
    const auto dt = aInfo.deltaTime;

    // Not sure if this needs jobqueue
    aJobQueue.Dispatch(
        [this, dt]()
        {
            std::unique_lock lock(m_choicehubLock);

            if (!m_countdownToForcedChoiceSelectionStarted)
            {
                m_choiceHubAvailableTime = 0.f;
                return;
            }

            if (m_choiceHubDataContext.m_choices.empty())
            {
                m_countdownToForcedChoiceSelectionStarted = false;
                m_choiceHubAvailableTime = 0.f;
                return;
            }

            m_choiceHubAvailableTime -= dt;

            if (m_choiceHubAvailableTime > 0.f)
            {
                return;
            }

            // End the countdown
            m_choiceHubAvailableTime = 0.f;
            m_countdownToForcedChoiceSelectionStarted = false;

            std::string json{};

            if (glz::write_json(m_choiceHubDataContext.m_choices, json))
            {
                return;
            }

            auto msg = MakeHandle<NeuroForcedActionMessage>();

            msg->m_actionName = "select_dialogue_choice";
            msg->m_query = "You have to make a choice now. This action will repeat until the choice node gets "
                           "invalidated (for instance, the player walks away or a timer runs out) or you make a valid "
                           "choice. The choice options are provided via context.";
            msg->m_state = json.c_str();

            AddMessage(msg);
        });
}

void mod::NeuroSystem::TickFuzzer(JobQueue& aQueue)
{
    static constexpr std::array<const char*, 6u> NoParameterActionNames = {"OnQueryMoney",     "OnQueryTrackedQuest",
                                                                           "OnQueryAllQuests", "OnQueryPlayerInfo",
                                                                           "OnQueryInventory", "OnQueryWaypoints"};
    aQueue.Dispatch(
        [this]()
        {
            std::unique_lock lock(m_fuzzerLock);

            if (!m_fuzzerActive)
            {
                return;
            }

            if (IsPreGame())
            {
                return;
            }

            const CName currentActionName = NoParameterActionNames[m_currentFuzzerFunction];

            constexpr auto OutputDebugData = true;

            if constexpr (OutputDebugData)
            {
                auto str = fmt::format("[Fuzzer] Last action {}\n", NoParameterActionNames[m_currentFuzzerFunction]);

                OutputDebugStringA(str.c_str());
            }

            // Unused
            Red::CString returnValue{};

            if (currentActionName == CNAME_HASH("OnQueryWaypoints"))
            {
                returnValue = NeuroResponses::CreateMappinQueryResponse();
            }
            else if (!CallVirtual(this, currentActionName, returnValue))
            {
                Context::Spew("[Fuzzer] Failed to call func {}!", currentActionName.ToString());
            }

            m_currentFuzzerFunction = (m_currentFuzzerFunction + 1) % NoParameterActionNames.size();
        });
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

void mod::NeuroSystem::SendContextSilent(const CString& aContextInfo)
{
    auto msg = MakeHandle<NeuroContextMessage>();

    msg->m_message = aContextInfo;
    msg->m_silent = true;

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

void mod::NeuroSystem::ToggleFuzzer()
{
    std::unique_lock lock(m_fuzzerLock);
    m_fuzzerActive = !m_fuzzerActive;
}

void mod::NeuroSystem::OnSceneDialogChoiceHubsProvided(game::interactions::vis::DialogChoiceHubs aRef)
{
    auto neuroChoiceId = 0;

    static auto LocalizationSystem = shared::raw::Localization::LocalizationSystem::GetInstance();

    std::unique_lock lock(m_choicehubLock);
    m_choiceHubDataContext.m_choices.clear();

    if (aRef.choiceHubs.size == 0u)
    {
        // Empty hub...
        if (m_countdownToForcedChoiceSelectionStarted)
        {
            // Notify Neuro that she doesn't need to care anymore
            SendContext("Choices are no longer available!");
        }

        m_choiceHubAvailableTime = 0.f;
        m_countdownToForcedChoiceSelectionStarted = false;
        m_lastChoiceHubDataHash = 0u;
        return;
    }

    // Just in case - though in practice this should not be hit
    auto hasAtLeastOneAvailable = false;

    for (auto& hub : aRef.choiceHubs)
    {
        auto isTimed = false;
        auto timer = 0.f;
        auto title = LocalizationSystem->GetOnscreen(hub.title);

        if (auto timeProvider = hub.timeProvider.Lock())
        {
            constexpr auto VisualizerGetDuration =
                shared::util::RawVFunc<0x110, float (*)(game::interactions::vis::IVisualizerTimeProvider*)>();

            constexpr auto VisualizerGetProgress =
                shared::util::RawVFunc<0x108, float (*)(game::interactions::vis::IVisualizerTimeProvider*)>();

            isTimed = true;
            timer = VisualizerGetDuration(timeProvider) - VisualizerGetProgress(timeProvider);
        }

        for (auto& choice : hub.choices)
        {
            m_choiceHubDataContext.m_choices.push_back(
                std::move(NeuroChoiceContext::FromGameData(choice, title, neuroChoiceId, isTimed, timer)));

            if (m_choiceHubDataContext.m_choices.back().m_canChoose)
            {
                hasAtLeastOneAvailable = true;
            }

            neuroChoiceId++;
        }
    }

    auto hash = m_choiceHubDataContext.HashChoices();

    if (hash == m_lastChoiceHubDataHash)
    {
        // Duplicates might confuse Neuro
        return;
    }

    m_lastChoiceHubDataHash = hash;

    std::string json{};

    if (glz::write_json(m_choiceHubDataContext.m_choices, json))
    {
        return;
    }

    auto msg = fmt::format("You have new available choices. The current choices are as following: {}", json);

    SendContext(msg.c_str());

    if (hasAtLeastOneAvailable)
    {
        m_choiceHubAvailableTime = ChoicehubDelayBeforeForcedAction;
        m_countdownToForcedChoiceSelectionStarted = true;
    }
}

bool mod::NeuroSystem::HasForcedActionCooldown()
{
    // Uh, I don't think we can deadlock this?
    std::unique_lock lock(m_messageLock);
    return m_hasSentForcedActionMessage;
}

void mod::NeuroSystem::OnQuickhackDataProvided(Handle<Impl::NeuroQuickhackDataDto>& aQuickhackInfo)
{
    // Avoid pointless lock
    InterlockedExchange64((volatile long long*)&m_quickhackActionTargetId.hash, aQuickhackInfo->m_targetEntityId.hash);

    auto jsonData = Impl::JSON::NeuroQuickhackDataJson::FromGame(aQuickhackInfo);

    auto msg = MakeHandle<NeuroForcedActionMessage>();

    msg->m_query = "You can quickhack a target (enemy or object) using various hacks. The available hacks are "
                   "provided, but only the "
                   "hacks with canUse = true can be uploaded to the target. If you do not wish to quickhack the "
                   "target, pass a negative number as the quickhack ID.";

    std::string json{};

    if (glz::write_json(jsonData, json))
    {
        return;
    }

    msg->m_state = json.c_str();
    msg->m_actionName = "run_quickhack_on_target";

    AddMessage(msg);
}

void mod::NeuroSystem::OnSMSMessageDataProvided(Handle<Impl::NeuroPhoneMessageDto>& aMessageInfo)
{
    std::unique_lock lock(m_smsLock);

    m_actionMessengerDialogViewController = aMessageInfo->m_messageController;

    auto jsonData = Impl::JSON::NeuroSMSJson::FromGame(aMessageInfo);

    auto msg = MakeHandle<NeuroForcedActionMessage>();

    msg->m_query = "You have options to reply to a phone message. The choices are provided in the action context. If "
                   "you do not wish to reply to the message, pass -1 as the message ID.";

    std::string json{};

    if (glz::write_json(jsonData, json))
    {
        return;
    }

    msg->m_state = json.c_str();
    msg->m_actionName = "select_sms_message_choice";

    AddMessage(msg);
}

void mod::NeuroSystem::ResetBadConnectionCounter()
{
    std::unique_lock lock(m_socketLock);

    m_disableConnection = false;
    m_firstConnectionAttempts = 0;
}

void mod::NeuroSystem::OnRegisterUpdates(UpdateRegistrar* aRegistrar)
{
    // Note: not sure how good an idea using PreRenderUpdate is, but it runs in pause menu, so...
    // Note: It does not run in the pause menu, I believe we can do with one update registration
    // aRegistrar->RegisterUpdate(UpdateTickGroup::PreRenderUpdate, this, "NeuroSystem/Communicate",
    //                            [this](FrameInfo& aFrameInfo, JobQueue& aJobQueue) { Tick(aFrameInfo, aJobQueue); });

    aRegistrar->RegisterUpdate(UpdateBucketMask::Character, UpdateBucketStage::Entities_PostTick, this,
                               "NeuroSystem/Tick",
                               [this](UpdateBucketEnum aUpdateBucket, FrameInfo& aFrameInfo, JobQueue& aJobQueue)
                               {
                                   TickFuzzer(aJobQueue);
                                   TickInputQueue(aFrameInfo);
                                   TickSceneInfo(aFrameInfo, aJobQueue);
                                   TickCommunication(aFrameInfo, aJobQueue);
                               });
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
    m_countdownToForcedChoiceSelectionStarted = false;
    m_choiceHubAvailableTime = 0.f;

    m_lastChoiceHubDataHash = 0u;
    m_choiceHubDataContext.m_choices.clear();

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
    RTTI_PROPERTY(m_isInanimate);
    RTTI_PROPERTY(m_ramAmount);
    RTTI_PROPERTY(m_quickhacks);
});

RTTI_DEFINE_CLASS(Impl::NeuroQuickhackDto, {
    RTTI_PROPERTY(m_id);
    RTTI_PROPERTY(m_quickhackName);
    RTTI_PROPERTY(m_quickhackDesc);
    RTTI_PROPERTY(m_quickhackCategory);
    RTTI_PROPERTY(m_quickhackStatus);
    RTTI_PROPERTY(m_quickhackCost);
    RTTI_PROPERTY(m_cooldown);
    RTTI_PROPERTY(m_uploadTime);
    RTTI_PROPERTY(m_willReveal);
    RTTI_PROPERTY(m_canUse);
});

RTTI_DEFINE_CLASS(Impl::NeuroPhoneMessageDto, {
    RTTI_PROPERTY(m_messageController);
    RTTI_PROPERTY(m_contact);
    RTTI_PROPERTY(m_convoNameIfApplicable);
    RTTI_PROPERTY(m_messageHistory);
    RTTI_PROPERTY(m_responseOptions);
});

RTTI_DEFINE_CLASS(mod::NeuroSystem, {
    RTTI_METHOD(SendContext);
    RTTI_METHOD(SendContextSilent);
    RTTI_METHOD(TrackMappin);
    RTTI_METHOD(InjectKeypress);
    RTTI_METHOD(InjectKeypressChain);
    RTTI_METHOD(HasForcedActionCooldown);
    RTTI_METHOD(OnQuickhackDataProvided);
    RTTI_METHOD(OnSMSMessageDataProvided);
    RTTI_METHOD(ResetBadConnectionCounter);
    RTTI_METHOD(OnSceneDialogChoiceHubsProvided);
    RTTI_METHOD(ToggleFuzzer);
});

RTTI_DEFINE_CLASS(mod::NeuroMessage, { RTTI_ABSTRACT(); });
RTTI_DEFINE_CLASS(mod::NeuroActionResponseMessage, { RTTI_PARENT(mod::NeuroMessage); });
RTTI_DEFINE_CLASS(mod::NeuroContextMessage, { RTTI_PARENT(mod::NeuroMessage); });
RTTI_DEFINE_CLASS(mod::NeuroForcedActionMessage, { RTTI_PARENT(mod::NeuroMessage); });
