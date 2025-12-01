module Neuro

@addField(VehicleObject)
public let m_neurodriveAiCommand: ref<AIVehicleCommand>;

@addMethod(VehicleObject)
public func KillNeurodrive(silent: Bool) -> Bool {
    if IsDefined(this.m_neurodriveAiCommand) {
        this.GetAIComponent().CancelCommand(this.m_neurodriveAiCommand);
        this.GetAIComponent().StopExecutingCommand(this.m_neurodriveAiCommand, true);
        this.m_neurodriveAiCommand = null;

        let warningMsg: SimpleScreenMessage;
        warningMsg.isShown = true;
        warningMsg.duration = 2;
        warningMsg.message = GetLocalizedTextByKey(n"Neuro-OnAutodriveKilled");
        warningMsg.type = SimpleMessageType.Relic;
        if !silent {
            GameInstance
                .GetBlackboardSystem(GetGameInstance())
                .Get(GetAllBlackboardDefs().UI_Notifications)
                .SetVariant(
                    GetAllBlackboardDefs().UI_Notifications.WarningMessage,
                    ToVariant(warningMsg),
                    true
                );
        }

        return true;
    }

    return false;
}

@addMethod(VehicleObject)
public func SetupNeurodrivePointToPoint(data: ref<DriveToPointAutonomousUpdate>) {
    let cmd = data.CreateCmd();

    let aiCommandEvent = new AICommandEvent();
    aiCommandEvent.command = cmd;

    this.QueueEvent(aiCommandEvent);
    this.GetAIComponent().SetDriveToPointAutonomousUpdate(data);

    this.m_neurodriveAiCommand = cmd;
}

@addMethod(VehicleObject)
public cb func OnNeurodriveAnnouncerEvent(evt: ref<NeurodriveAnnouncerEvent>) {
    let warningMsg: SimpleScreenMessage;
    warningMsg.isShown = true;
    warningMsg.duration = 2;
    warningMsg.message = GetLocalizedTextByKey(n"Neuro-OnAutodriveStart");
    warningMsg.type = SimpleMessageType.Relic;

    GameInstance
        .GetBlackboardSystem(GetGameInstance())
        .Get(GetAllBlackboardDefs().UI_Notifications)
        .SetVariant(
            GetAllBlackboardDefs().UI_Notifications.WarningMessage,
            ToVariant(warningMsg),
            true
        );
}

@wrapMethod(VehicleObject)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
    let mountChild: ref<GameObject> = GameInstance.FindEntityByID(this.GetGame(), evt.request.lowLevelMountingInfo.childId) as GameObject;

    if IsDefined(mountChild) && mountChild.IsPlayer() {
        if this.KillNeurodrive(true) {
            GameInstance.GetNeuroSystem().SendContext("Autodrive stopped due to exiting the vehicle.");
        }
    }
}

public class NeurodriveInputListener {
    protected cb func OnAction(action: ListenerAction, consumer: ListenerActionConsumer) -> Bool {
        if action.IsAction(n"TurnX") || action.IsAction(n"Accelerate") || action.IsAction(n"Decelerate") {
            let player = GameInstance
                .GetPlayerSystem(GetGameInstance())
                .GetLocalPlayerControlledGameObject() as PlayerPuppet;

            if IsDefined(player) {
                let vehicle = player.GetMountedVehicle();

                if IsDefined(vehicle) {
                    if vehicle.KillNeurodrive(false) {
                        GameInstance.GetNeuroSystem().SendContext("Autodrive stopped due to player input.");
                    }
                }
            }
        }
    }
}

public class NeurodriveAnnouncerEvent extends Event {
}

@addField(PlayerPuppet)
public let m_neurodriveListener: ref<NeurodriveInputListener>;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
    wrappedMethod();

    this.m_neurodriveListener = new NeurodriveInputListener();
    this.RegisterInputListener(this.m_neurodriveListener, n"TurnX");
    this.RegisterInputListener(this.m_neurodriveListener, n"Accelerate");
    this.RegisterInputListener(this.m_neurodriveListener, n"Decelerate");
}

@wrapMethod(PlayerPuppet)
protected cb func OnDetach() -> Bool {
    wrappedMethod();

    this.UnregisterInputListener(this.m_neurodriveListener, n"TurnX");
    this.UnregisterInputListener(this.m_neurodriveListener, n"Accelerate");
    this.UnregisterInputListener(this.m_neurodriveListener, n"Decelerate");
    this.m_neurodriveListener = null;
}

