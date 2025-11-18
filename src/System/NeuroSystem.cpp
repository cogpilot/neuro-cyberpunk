#include <Shared/Raw/Ink/InkSystem.hpp>
#include <Shared/Raw/MappinSystem/MappinSystem.hpp>

#include <System/NativeResponses.hpp>
#include <System/NeuroSystem.hpp>

#include <glaze/glaze.hpp>

using namespace Red;

namespace Impl
{
struct DriveToWaypointJson
{
    std::string destType{};
    std::string target{};
};
}

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

                    if (std::from_chars(json.target.c_str(), json.target.c_str() + json.target.size(), mappinId.value).ec != std::errc())
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

void mod::NeuroSystem::Tick(JobQueue& aJobQueue)
{
    aJobQueue.Dispatch(
        [this](const JobGroup& aGroup)
        {
            std::unique_lock lock(m_socketLock);

            auto firstTick = !m_neuroSocket;

            if (!m_neuroSocket)
            {
                if (m_failedToConnectBefore && m_lastRetryTime.TimePassed() <= RetryTimeSeconds)
                {
                    return;
                }

                if (!InitializeConnection())
                {
                    m_lastRetryTime = {};
                    m_failedToConnectBefore = true;
                    return;
                }
            }

            // If it's the first tick after connection, inform Neuro about the game state and if she can make commands
            if (firstTick)
            {
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

            DynArray<Handle<NeuroMessage>> messages{};
            {
                // Minimize lock hold time
                std::unique_lock queueLock(m_messageLock);
                messages = std::move(m_messageQueue);
            }

            for (auto& i : messages)
            {
                i->DispatchNeuroMessage(*m_neuroSocket);
            }
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
    using namespace shared::raw::Ink;
    auto& inkSystem = *InkSystem::Get();
    SyntheticInputBuffer inputBuffer{};

    RawInputData pressData{};

    pressData.action = EInputAction::IACT_Press;
    pressData.key = aKey;

    inputBuffer.GetInputs().PushBack(pressData);
    inkSystem.InjectSyntheticInput(inputBuffer);
    
    inputBuffer.GetInputs().Clear();

    RawInputData releaseData{};

    releaseData.action = EInputAction::IACT_Release;
    releaseData.key = aKey;

    inputBuffer.GetInputs().PushBack(releaseData);
    inkSystem.InjectSyntheticInput(inputBuffer);
}

void mod::NeuroSystem::OnRegisterUpdates(UpdateRegistrar* aRegistrar)
{
    // Note: not sure how good an idea using PreRenderUpdate is, but it runs in pause menu, so...
    aRegistrar->RegisterUpdate(UpdateTickGroup::PreRenderUpdate, this, "NeuroSystem/Communicate",
                               [this](FrameInfo&, JobQueue& aJobQueue) { Tick(aJobQueue); });
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
}

void mod::NeuroSystem::OnInitialize(const Red::JobHandle& aJob)
{
    Instance::Instance = this;
}

RTTI_DEFINE_CLASS(mod::NeuroSystem, {
    RTTI_METHOD(SendContext);
    RTTI_METHOD(TrackMappin);
    RTTI_METHOD(InjectKeypress);
});

RTTI_DEFINE_CLASS(mod::NeuroMessage, { RTTI_ABSTRACT(); });
RTTI_DEFINE_CLASS(mod::NeuroActionResponseMessage, { RTTI_PARENT(mod::NeuroMessage); });
RTTI_DEFINE_CLASS(mod::NeuroContextMessage, { RTTI_PARENT(mod::NeuroMessage); });
RTTI_DEFINE_CLASS(mod::NeuroForcedActionMessage, { RTTI_PARENT(mod::NeuroMessage); });
