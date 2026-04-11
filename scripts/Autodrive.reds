module Neuro

@addField(VehicleObject)
public let m_neurodriveAiCommand: ref<AIVehicleCommand>;

@addField(VehicleObject)
public let m_isNeurodriveUsingTrackedMappin: Bool;

@addMethod(VehicleObject)
public func KillNeurodrive(silent: Bool) -> Bool {
    if IsDefined(this.m_neurodriveAiCommand) {
        this.GetAIComponent().CancelCommand(this.m_neurodriveAiCommand);
        this.GetAIComponent().StopExecutingCommand(this.m_neurodriveAiCommand, true);
        this.m_neurodriveAiCommand = null;

        if !silent {
            SimpleScreenMessage.DisplayNeuroRelatedOnscreenMsg(n"Neuro-OnAutodriveKilled");
        }

        return true;
    }

    // Just in case
    GameInstance
        .GetJournalManager(GetGameInstance())
        .UnregisterScriptCallback(this, n"OnJournalTrackedUpdate");
    return false;
}

@addMethod(VehicleObject)
public cb func OnAICommandStateEvent(ev: ref<AICommandStateEvent>) {
    if !IsDefined(this.m_neurodriveAiCommand) {
        return;
    }

    if ev.command != this.m_neurodriveAiCommand {
        return;
    }

    let neuroSystem = GameInstance.GetNeuroSystem();

    switch ev.newState {
        case AICommandState.Success:
            this.KillNeurodrive(true);
            this.ForceBrakesUntilStoppedOrFor(5);
            SimpleScreenMessage.DisplayNeuroRelatedOnscreenMsg(n"Neuro-OnAutodriveArrived");
            neuroSystem.SendContext("Autodrive arrived to destination.");
            break;
        case AICommandState.Failure:
            // This happens more than expected - do not kill autodrive!
            // this.KillNeurodrive(true);
            SimpleScreenMessage.DisplayNeuroRelatedOnscreenMsg(n"Neuro-OnAutodriveFail");
            // neuroSystem.SendContext("Autodrive failed for unknown reasons.");
            break;
        default:
            break;
    }
}

@addMethod(DriveToPointAutonomousUpdate)
public static func MakeNeuroAutodriveData(targetPosition: Vector4) -> ref<DriveToPointAutonomousUpdate> {
    let self = new DriveToPointAutonomousUpdate();

    self.clearTrafficOnPath = false;
    self.driveDownTheRoadIndefinitely = false;
    self.minSpeed = 15;
    self.maxSpeed = 170;
    self.minimumDistanceToTarget = 40;
    self.targetPosition = targetPosition;

    return self;
}

@addMethod(VehicleObject)
public func SetupNeurodrivePointToPoint(
    data: ref<DriveToPointAutonomousUpdate>,
    isDrivingToTrackedWaypoint: Bool,
    isUpdate: Bool
) {
    let cmd = data.CreateCmd();

    let aiCommandEvent = new AICommandEvent();
    aiCommandEvent.command = cmd;

    this.QueueEvent(aiCommandEvent);
    this.GetAIComponent().SetDriveToPointAutonomousUpdate(data);

    this.m_neurodriveAiCommand = cmd;

    if isUpdate {
        return;
    }

    this.m_isNeurodriveUsingTrackedMappin = isDrivingToTrackedWaypoint;

    if this.m_isNeurodriveUsingTrackedMappin {
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());
        journalManager
            .RegisterScriptCallback(this, n"OnJournalTrackedUpdate", gameJournalListenerType.Tracked);
    }
}

@addMethod(VehicleObject)
public cb func OnJournalTrackedUpdate(
    hash: Uint32,
    className: CName,
    notifyOption: JournalNotifyOption,
    changeType: JournalChangeType
) {
    if !IsDefined(this.m_neurodriveAiCommand) {
        return;
    }

    let journalManager = GameInstance.GetJournalManager(GetGameInstance());

    let trackedEntry = journalManager.GetTrackedEntry() as JournalQuestObjectiveBase;

    if !IsDefined(trackedEntry) {
        return;
    }

    let mappinSystem = GameInstance.GetMappinSystem(GetGameInstance());

    let positions: [Vector3];

    if !mappinSystem
        .GetQuestMappinPositionsByObjective(Cast<Uint32>(journalManager.GetEntryHash(trackedEntry)), positions) {
        return;
    }

    if ArraySize(positions) == 0 {
        return;
    }

    let targetPosition = Vector4.Vector3To4(positions[0]);

    ModLog(n"Neuro", s"Retracking to mappin \(targetPosition)!");

    let aiCommand = DriveToPointAutonomousUpdate.MakeNeuroAutodriveData(targetPosition);

    this.KillNeurodrive(true);
    this.SetupNeurodrivePointToPoint(aiCommand, false, true);
    SimpleScreenMessage.DisplayNeuroRelatedOnscreenMsg(n"Neuro-OnAutodriveRetarget");
}

@addMethod(VehicleObject)
public cb func OnNeurodriveAnnouncerEvent(evt: ref<NeurodriveAnnouncerEvent>) {
    SimpleScreenMessage.DisplayNeuroRelatedOnscreenMsg(n"Neuro-OnAutodriveStart");
}

@wrapMethod(VehicleObject)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
    let mountChild: ref<GameObject> = GameInstance.FindEntityByID(this.GetGame(), evt.request.lowLevelMountingInfo.childId) as GameObject;

    if IsDefined(mountChild) && mountChild.IsPlayer() {
        if this.KillNeurodrive(true) {
            GameInstance
                .GetNeuroSystem()
                .SendContext("Autodrive stopped due to exiting the vehicle.");
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
                        GameInstance
                            .GetNeuroSystem()
                            .SendContext("Autodrive stopped due to player input.");
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

