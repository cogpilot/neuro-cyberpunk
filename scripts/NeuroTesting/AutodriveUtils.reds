@addField(VehicleObject)
public let m_neurodriveAiCommand: ref<AIVehicleCommand>;

@addMethod(VehicleObject)
public func KillNeurodrive() {
    if IsDefined(this.m_neurodriveAiCommand) {
        this.GetAIComponent().CancelCommand(this.m_neurodriveAiCommand);
        this.GetAIComponent().StopExecutingCommand(this.m_neurodriveAiCommand, true);
        this.m_neurodriveAiCommand = null;
    }
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

public class NeurodriveInputListener {
    protected cb func OnAction(action: ListenerAction, consumer: ListenerActionConsumer) -> Bool {
        if action.IsAction(n"TurnX") || action.IsAction(n"Accelerate") || action.IsAction(n"Decelerate") {
            let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject() as PlayerPuppet;

            if IsDefined(player) {
                let vehicle = player.GetMountedVehicle();

                if IsDefined(vehicle) {
                    vehicle.KillNeurodrive();
                }
            }
        }
    }
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