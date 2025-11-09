#include <Context/Context.hpp>
#include <Socket/Socket.hpp>
#include <neurosdk.h>
#include <string>
#include <string_view>

namespace Capabilities
{
namespace SummonCar
{
char Name[] = "Summon car";
char Desc[] = "Summon the last used vehicle to your location. This might not be possible depending on quest state.";
char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace SummonCar

namespace QueryWaypoints
{
char Name[] = "Query waypoints";
char Desc[] =
    R"(Query the available fast travel waypoints. Returns a JSON of {"name": WaypointName, "district": WaypointDistrict}.)";
char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryWaypoints

namespace QueryQuestContext
{
char Name[] = "Query quest context";
char Desc[] =
    R"(Query information about the currently tracked quest. Returns a JSON of {"name": QuestName, "desc": QuestDescription, "district": QuestDistrict}.)";
char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryQuestContext

namespace DriveToDestination
{
char Name[] = "Drive to waypoint";
char Desc[] =
    R"(Toggle automatic driving to drive to a specific waypoint if the player is currently driving a vehicle. You can go to: 
- a player defined waypoint (if present), 
- a specific fast travel point, 
- a random point in a district.)";
char JsonSchema[] = R"({ "type": "object", "title": "DriveToDestination", "properties": { "destType": { "description": "The type of the selected destination.", "enum": ["player", "fasttravel", "district"] }, "target": { "description": "The selected destination. If destType == 'player', can be left empty. If destType == 'fasttravel', has to be the name of a fast travel point. If destType == 'district', has to be the name of a district.", "type": "string" } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace DriveToDestination

namespace SelectChoiceOption
{
char Name[] = "Select dialogue choice";
char Desc[] =
    R"(Select a dialogue choice option.
Choice options are provided in a forced action context as a JSON array of {"option": StringOfDialogue, "id": ChoiceNumber}. 
Choices may be timed.
If a choice can run out of time, a <timeout> option is provided as the last item.
Choices may affect the story.)";
char JsonSchema[] = R"({ "type": "object", "title": "SelectChoiceOption", "properties": { "id": { "description": "The ID of the selected dialogue option from the provided options.", "type": "integer" } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace SelectChoiceOption

namespace RunQuickhackOnTarget
{
char Name[] = "Run a quickhack on a target";
char Desc[] =
    R"(Choose a quickhack to hack a target with. Targets can be enemies or inanimate objects.
A list of quickhacks is provided in context as a JSON array of objects {"name": QuickhackName, "desc": QuickhackDescription, "id": QuickhackNumber}, 
as well as information on the quickhack target. Quickhacks can only be performed when the scan menu is open and the player is looking at the target.)";
char JsonSchema[] = R"({ "type": "object", "title": "SelectQuickhack", "properties": { "id": { "description": "The ID of the selected quickhack from the provided options.", "type": "integer"  } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace RunQuickhackOnTarget

neurosdk_action ActionsList[] = {SummonCar::Action,          QueryQuestContext::Action,  QueryWaypoints::Action,
                                 DriveToDestination::Action, SelectChoiceOption::Action, RunQuickhackOnTarget::Action};

constexpr auto ActionsCount = ARRAYSIZE(ActionsList);

} // namespace Capabilities

neuro::NeuroSocket::NeuroSocket()
    : m_context(nullptr)
{
}

neuro::NeuroSocket::~NeuroSocket()
{
    if (IsAlive())
    {
        neurosdk_context_destroy(&m_context);
    }
}

bool neuro::NeuroSocket::RespondToAction(std::string_view aActionId, std::string_view aMsg)
{
    neurosdk_message_t msg{.kind = NeuroSDK_MessageKind_ActionResult,
        .value = {.action_result = {.id = aActionId.data(), .success = true, .message = aMsg.data()}}};

    return neurosdk_context_send(&m_context, &msg) == NeuroSDK_None;
}

bool neuro::NeuroSocket::SendContext(std::string_view aContext, bool aSilent)
{
    neurosdk_message_t msg{.kind = NeuroSDK_MessageKind_Context,
                           .value = {.context = {.message = aContext.data(), .silent = aSilent}}};

    return neurosdk_context_send(&m_context, &msg) == NeuroSDK_None;
}

bool neuro::NeuroSocket::Tick()
{
    neurosdk_message_t* messageQueue{};
    int messageCount{};
    if (const auto status = neurosdk_context_poll(&m_context, &messageQueue, &messageCount); status != NeuroSDK_None)
    {
        return false;
    }

    for (auto i = 0; i < messageCount; i++)
    {
        if (messageQueue[i].kind == NeuroSDK_MessageKind_Action)
        {
            m_callbackFunc(messageQueue[i].value.action, *this);
        }
    }

    return true;
}

bool neuro::NeuroSocket::Initialize(NeuroActionProcessor aProcessor)
{
    // Note: this is a FPS and we're polling every frame (which we shouldn't do, but...)
    // We can't have poll timeout be more than ~10ms without rethinking architecture
    neurosdk_context_create_desc_t desc{.game_name = "Cyberpunk 2077",
                                        .poll_ms = POLL_RATE_MS,
                                        .flags = (neurosdk_context_create_flags_e)NEUROSDK_CONTEXT_CREATE_FLAGS_DEBUG,
                                        .callback_log = neuro::NeuroSocket::LogNeuro};

    if (const auto status = neurosdk_context_create(&m_context, &desc); status != NeuroSDK_None)
    {
        Context::PluginLogger->ErrorF(Context::PluginHandle, "Failed to create Neuro SDK context! Error %s",
                                      neurosdk_error_string(status));
        return false;
    }

    // Tell backend we started up
    neurosdk_message_t connectionMessage{.kind = NeuroSDK_MessageKind_Startup};

    if (const auto status = neurosdk_context_send(&m_context, &connectionMessage); status != NeuroSDK_None)
    {
        Context::PluginLogger->ErrorF(Context::PluginHandle, "Failed to inform backend of startup! Error %s",
                                      neurosdk_error_string(status));
        return false;
    }

    neurosdk_message_t capabilityMessage{.kind = NeuroSDK_MessageKind_ActionsRegister,
                                         .value = {.actions_register = {.actions = Capabilities::ActionsList,
                                                                        .actions_len = Capabilities::ActionsCount}}};

    if (const auto status = neurosdk_context_send(&m_context, &capabilityMessage); status != NeuroSDK_None)
    {
        Context::PluginLogger->ErrorF(Context::PluginHandle, "Failed to inform backend of capabilities! Error %s",
                                      neurosdk_error_string(status));
        return false;
    }

    m_callbackFunc = aProcessor;

    return SendContext("You are playing Cyberpunk 2077, an action RPG. Commands will only give reasonable output once you are ingame.", true);
}

bool neuro::NeuroSocket::IsAlive()
{
    return neurosdk_context_connected(&m_context);
}

void neuro::NeuroSocket::LogNeuro(neurosdk_severity_e aSeverity, char* aMsg, void* aUserData)
{
    switch (aSeverity)
    {
    case NeuroSDK_Severity_Debug:
        Context::PluginLogger->DebugF(Context::PluginHandle, "[libneurosdk | Debug] %s", aMsg);
        break;
    case NeuroSDK_Severity_Info:
        Context::PluginLogger->InfoF(Context::PluginHandle, "[libneurosdk | Info] %s", aMsg);
        break;
    case NeuroSDK_Severity_Warn:
        Context::PluginLogger->WarnF(Context::PluginHandle, "[libneurosdk | Warn] %s", aMsg);
        break;
    case NeuroSDK_Severity_Error:
        Context::PluginLogger->ErrorF(Context::PluginHandle, "[libneurosdk | Error] %s", aMsg);
        break;
    }

    if (aSeverity > NeuroSDK_Severity_Info)
    {
        // Just in case, spew in game console as well
        Context::Spew("[libneurosdk | WarnOrError] {}", aMsg);
    }
}
