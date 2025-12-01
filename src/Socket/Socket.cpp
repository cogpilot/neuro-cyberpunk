#include <Context/Context.hpp>
#include <Socket/Socket.hpp>
#include <neurosdk.h>
#include <string>
#include <string_view>

using namespace Red;

namespace Capabilities
{
namespace QueryWaypoints
{
constexpr char Name[] = "query_waypoints";
constexpr char Desc[] =
    R"(Query the available map pins (fast travel waypoints, quest objectives, points of interest and so on). Returns a JSON list of {"id", "name", "type", "district", "tracked", "distance"}. Only fast travel waypoints will have a specified district.)";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryWaypoints

namespace QueryQuestContext
{
constexpr char Name[] = "query_quest_context";
constexpr char Desc[] =
    R"(Query information about the currently tracked quest. Returns a description of the tracked quest with its name, type, current objectives and district.)";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryQuestContext

namespace QueryQuests
{
constexpr char Name[] = "query_all_quests";
constexpr char Desc[] =
    R"(Query information about all active quests. Returns a list of quests with their names, types, current objectives and associated districts.)";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryQuests

namespace QueryInventory
{
constexpr char Name[] = "query_inventory";
constexpr char Desc[] =
    R"(Query information about the player inventory contents. Returns a description of the items the player currently has in their inventory, including their name, type, quality and cost.)";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryInventory

namespace QueryPlayerInfo
{
constexpr char Name[] = "query_player_info";
constexpr char Desc[] =
    R"(Query information about the player information. Returns a description of the current state of the player, including health, combat state and equipped items/cyberware.)";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryPlayerInfo

namespace QueryMoney
{
constexpr char Name[] = "query_money";
constexpr char Desc[] = R"(Query information about the player's money. Returns the amount of money the player has.)";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace QueryMoney

#pragma region Actions
namespace DriveToDestination
{
constexpr char Name[] = "drive_to_waypoint";
constexpr char Desc[] =
    R"(Toggle automatic driving to drive to a specific waypoint if the player is currently driving a vehicle. You can go to any point returned by query_waypoints or randomly select a fast travel waypoint in a district. )";
constexpr char JsonSchema[] =
    R"({ "type": "object", "properties": { "destType": { "description": "The type of the selected destination.", "enum": ["id", "district", "tracked"] }, "target": { "description": "The selected destination. If destType == 'district', has to be one of the districts within query_waypoints. If destType == 'id', has to be one of the IDs returned by query_waypoints. If destType == 'tracked', can be left empty.", "type": "string" } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace DriveToDestination

namespace SelectChoiceOption
{
constexpr char Name[] = "select_dialogue_choice";
constexpr char Desc[] =
    R"(Select a dialogue choice option.
Choice options are provided in a forced action context as a JSON object with the approximate schema of
{
     hubId: int,
     title: string,
     choices: [{
        id: int,
        canInteract: bool,
        isImportant: bool,
        text: string
     }],
     isTimed: bool,
     timeSeconds: float
}. 
Choices may be timed.
Choices may affect the story.)";
constexpr char JsonSchema[] =
    R"({ "type": "object", "properties": { "id": { "description": "The ID of the selected dialogue option from the provided options.", "type": "integer" }, "hubId": { "description": "The ID of the dialogue hub from the provided context.", "type": "integer" } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace SelectChoiceOption

namespace SelectSMSResponse
{
constexpr char Name[] = "select_sms_message_choice";
constexpr char Desc[] =
    R"(Select a SMS message option.
Choice options are provided in a forced action context.
Choices may affect the story.)";
constexpr char JsonSchema[] =
    R"({ "type": "object", "properties": { "id": { "description": "The ID of the selected SMS message option from the provided options.", "type": "integer" } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace SelectSMSResponse

namespace RunQuickhackOnTarget
{
constexpr char Name[] = "run_quickhack_on_target";
constexpr char Desc[] =
    R"(Choose a quickhack to hack a target with. Targets can be enemies or inanimate objects.
A list of quickhacks is provided in context, as well as information on the quickhack target. Quickhacks can only be performed once a forced action is provided.)";
constexpr char JsonSchema[] =
    R"({ "type": "object", "title": "SelectQuickhack", "properties": { "id": { "description": "The ID of the selected quickhack from the provided options.", "type": "integer"  } } })";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace RunQuickhackOnTarget

namespace SummonCar
{
constexpr char Name[] = "summon_car";
constexpr char Desc[] =
    "Summon the last used vehicle to your location. This might not be possible depending on quest state.";
constexpr char JsonSchema[] = "{}";

constexpr neurosdk_action Action = {.name = Name, .description = Desc, .json_schema = JsonSchema};
} // namespace SummonCar

#pragma endregion

neurosdk_action ActionsList[] = {QueryQuestContext::Action,    QueryQuests::Action,        QueryWaypoints::Action,
                                 QueryInventory::Action,       QueryPlayerInfo::Action,    QueryMoney::Action,
                                 DriveToDestination::Action,   SelectChoiceOption::Action, SelectSMSResponse::Action,
                                 RunQuickhackOnTarget::Action, SummonCar::Action};

constexpr auto ActionsCount = ARRAYSIZE(ActionsList);

} // namespace Capabilities

neuro::NeuroSocket::NeuroSocket()
    : m_context(nullptr)
    , m_callbackFunc(nullptr)
{
}

neuro::NeuroSocket::~NeuroSocket()
{
    if (IsAlive())
    {
        neurosdk_context_destroy(&m_context);
    }
}

bool neuro::NeuroSocket::RespondToAction(StringView aActionId, StringView aMsg)
{
    neurosdk_message_t msg{
        .kind = NeuroSDK_MessageKind_ActionResult,
        .value = {.action_result = {.id = aActionId.Data(), .success = true, .message = aMsg.Data()}}};

    return neurosdk_context_send(&m_context, &msg) == NeuroSDK_None;
}

bool neuro::NeuroSocket::SendContext(StringView aContext, bool aSilent)
{
    neurosdk_message_t msg{.kind = NeuroSDK_MessageKind_Context,
                           .value = {.context = {.message = aContext.Data(), .silent = aSilent}}};

    return neurosdk_context_send(&m_context, &msg) == NeuroSDK_None;
}

bool neuro::NeuroSocket::SendForcedAction(StringView aActionName, StringView aQuery, StringView aState)
{
    const char* tempActionNames[] = {aActionName.Data()};

    neurosdk_message_t msg{.kind = NeuroSDK_MessageKind_ActionsForce,
                           .value = {.actions_force = {.state = aState.Data(),
                                                       .query = aQuery.Data(),
                                                       .ephemeral_context = false,
                                                       .action_names = tempActionNames,
                                                       .action_names_len = 1}}};

    return neurosdk_context_send(&m_context, &msg) == NeuroSDK_None;
}

bool neuro::NeuroSocket::Tick()
{
    if (!IsAlive())
    {
        return false;
    }

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
            neurosdk_message_destroy(&messageQueue[i]);
        }
    }

    return true;
}

bool neuro::NeuroSocket::Initialize(NeuroActionProcessor aProcessor)
{
    // Note: this is a FPS and we're polling every frame (which we shouldn't do, but...)
    // We can't have poll timeout be more than ~10ms without rethinking architecture
    neurosdk_context_create_desc_t desc{.game_name = "Cyberpunk 2077",
                                        .poll_ms = PollRateMs,
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

    return SendContext(
        "You are playing Cyberpunk 2077, an action RPG. Commands will only give reasonable output once you are ingame.",
        true);
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
        // Ignore debug prints for now, create too much spam in console
        // Context::PluginLogger->DebugF(Context::PluginHandle, "[libneurosdk | Debug] %s", aMsg);
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
