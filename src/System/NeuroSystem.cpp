#include <Shared/Raw/Ink/InkSystem.hpp>
#include <System/NeuroSystem.hpp>

using namespace Red;

namespace Instance
{
// This is safe, as game system only gets destructed when exiting the game.
mod::NeuroSystem* Instance{};
} // namespace Instance

void mod::NeuroSystem::DispatchNeuroAction(const neurosdk_message_action_t& aAction, neuro::NeuroSocket& aSocket)
{
    // This method should be safe to be kept without locking, as the only time it's called is during Tick() anyway

    aSocket.RespondToAction(aAction.id, "This action has not been implemented yet.");
}

bool mod::NeuroSystem::InitializeConnection()
{
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

            if (!m_neuroSocket)
            {
                if ((!m_failedToConnectBefore || m_lastRetryTime.TimePassed() > RetryTimeSeconds) &&
                    !InitializeConnection())
                {
                    // Reset last retry time, tag "we failed to connect", exit
                    m_lastRetryTime = {};
                    m_failedToConnectBefore = true;
                    return;
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
        });
}

bool mod::NeuroSystem::IsPreGame()
{
    auto handler = shared::raw::Ink::InkSystem::Get()->m_requestsHandler.Lock();
    return shared::raw::Ink::SystemRequestsHandler::IsPreGame(handler);
}

void mod::NeuroSystem::OnRegisterUpdates(UpdateRegistrar* aRegistrar)
{
    // Note: not sure how good an idea using PreRenderUpdate is, but it runs in pause menu, so...
    aRegistrar->RegisterUpdate(UpdateTickGroup::PreRenderUpdate, this, "NeuroSystem/Communicate",
                               [this](FrameInfo&, JobQueue& aJobQueue) { Tick(aJobQueue); });
}

std::uint32_t mod::NeuroSystem::OnBeforeGameSave(const JobGroup& aJobGroup, void* aMetadataObject)
{
    std::unique_lock lock(m_socketLock);
    if (m_neuroSocket)
    {
        m_neuroSocket->SendContext("The game is being saved.");
    }

    return 0u;
}

void mod::NeuroSystem::OnGamePrepared()
{
    std::unique_lock lock(m_socketLock);
    if (!IsPreGame() && m_neuroSocket)
    {
        m_neuroSocket->SendContext(
            "The game world is being loaded. Either a save has been loaded or a new game was started.");
    }
}

void mod::NeuroSystem::OnWorldDetached(world::RuntimeScene* aScene)
{
    std::unique_lock lock(m_socketLock);
    if (!IsPreGame() && m_neuroSocket)
    {
        m_neuroSocket->SendContext(
            "The game world is being exited. Either a save is being loaded or the game is being exited to main menu.");
    }
}

void mod::NeuroSystem::OnInitialize(const Red::JobHandle& aJob)
{
    Instance::Instance = this;
}

RTTI_DEFINE_CLASS(mod::NeuroSystem, {});
