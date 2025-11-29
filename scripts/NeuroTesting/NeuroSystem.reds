
public native class NeuroQuickhackDto {
    public native let id: Int32;
    public native let quickhackName: String;
    public native let quickhackDesc: String;
    public native let quickhackCategory: String;
    public native let quickhackStatus: String;
    public native let quickhackCost: Int32;
    public native let cooldown: Float;
    public native let uploadTime: Float;
    public native let willReveal: Bool;
    public native let canUse: Bool;
}

public native class NeuroQuickhackDataDto {
    public native let targetEntityId: EntityID;
    public native let targetName: String;
    public native let faction: String;
    public native let isHostile: Bool;
    public native let isBoss: Bool;
    public native let isInanimate: Bool;
    public native let ramAmount: Int32;
    public native let quickhacks: [ref<NeuroQuickhackDto>];
}

public native class NeuroSystem extends IGameSystem {
    public native func SendContext(msg: String) -> Void;

    public native func TrackMappin(mappin: ref<IMappin>) -> Void;

    public native func OnSceneListChoiceDataProvided(data: script_ref<ListChoiceHubData>) -> Void;

    public native func InjectKeypressChain(data: [EInputKey]) -> Void;

    public native func HasForcedActionCooldown() -> Bool;

    public native func OnQuickhackDataProvided(data: ref<NeuroQuickhackDataDto>);

    public cb func OnConnectedIngame() -> Void {
    }

    public cb func OnQuickhackTarget(entId: EntityID, hackId: Int32) -> String {
        let ent = GameInstance.FindEntityByID(GetGameInstance(), entId);

        if !IsDefined(ent) {
            return "Failed to find entity!";
        }

        let asObject = ent as GameObject;

        if !IsDefined(asObject) {
            // Should never happen
            return "Entity is not game object!";
        }

        if !asObject.IsQuickHackAble() {
            return "Entity cannot be quickhacked!";
        }

        let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject() as PlayerPuppet;

        let quickhackActions: [ref<QuickhackData>];
        // Note: very hacky

        let asDevice = asObject as Device;
        let asScriptedPuppet = asObject as ScriptedPuppet;
        let asVehicle = asObject as VehicleObject;

        if IsDefined(asDevice) {
            if asDevice.m_isQhackUploadInProgerss && !asDevice.IsActionQueueEnabled() || asDevice.IsActionQueueFull() {
                return "Device is already too hacked!";
            }
            let ctx = asDevice
                .GetDevicePS()
                .GenerateContext(
                    gamedeviceRequestType.Remote,
                    Device.GetInteractionClearance(),
                    player,
                    entId
                );

            let actions: [ref<DeviceAction>];
            asDevice.GetDevicePS().GetRemoteActions(actions, ctx);
            QuickHackableHelper
                .TranslateActionsIntoQuickSlotCommands(actions, quickhackActions, asDevice, asDevice.GetDevicePS());
        } else if IsDefined(asScriptedPuppet) {
            let ctx = asScriptedPuppet
                .GetPS()
                .GenerateContext(
                    gamedeviceRequestType.Remote,
                    Device.GetInteractionClearance(),
                    player,
                    entId
                );

            let actionRecords: [wref<ObjectAction_Record>];
            let puppetActions: [ref<PuppetAction>];

            asScriptedPuppet.GetRecord().ObjectActions(actionRecords);
            asScriptedPuppet.GetPS().GetAllChoices(actionRecords, ctx, puppetActions);
            asScriptedPuppet
                .TranslateChoicesIntoQuickSlotCommands(puppetActions, quickhackActions);
        } else if IsDefined(asVehicle) {
            if asVehicle.m_isQhackUploadInProgress && !asVehicle.IsActionQueueEnabled() || asVehicle.IsActionQueueFull() {
                return "Vehicle is already too hacked!";
            }

            let ctx = asVehicle
                .GetVehiclePS()
                .GenerateContext(
                    gamedeviceRequestType.Remote,
                    Device.GetInteractionClearance(),
                    player,
                    entId
                );

            let actions: [ref<DeviceAction>];
            asVehicle.GetVehiclePS().GetRemoteActions(actions, ctx);

            QuickHackableHelper
                .TranslateActionsIntoQuickSlotCommands(actions, quickhackActions, asVehicle, asVehicle.GetVehiclePS());
        }

        let sz = ArraySize(quickhackActions);

        if sz < hackId {
            return "Quickhack ID no longer present in list!";
        }

        let hack = quickhackActions[hackId];

        if hack.m_isLocked {
            return "Hack is locked and can\'t be used!";
        }

        if NotEquals(hack.m_actionState, EActionInactivityReson.Ready) {
            return "Hack is not ready for use!";
        }

        let cmd = new QuickSlotCommandUsed();
        cmd.action = hack.m_action;

        player.QueueEventForEntityID(entId, cmd);

        if player.GetTakeOverControlSystem().IsDeviceControlled() {
            let hackUsed = new QhackExecuted();
            player
                .QueueEventForEntityID(
                    player
                        .GetTakeOverControlSystem()
                        .GetControlledObject()
                        .GetEntityID(),
                    hackUsed
                );
        }

        return s"Tried to dispatch quickhack \"\(GetLocalizedText(hack.m_title))\" to target!";
    }

    public cb func OnSelectDialogueChoice(hubId: Int32, id: Int32) -> String {
        let uiInteractionsBlackboardID = GetAllBlackboardDefs().UIInteractions;
        let blackboard = GameInstance.GetBlackboardSystem(GetGameInstance()).Get(uiInteractionsBlackboardID);

        if !IsDefined(blackboard) {
            return "Failed to get UI interaction blackboard.";
        }

        let currentChoicehubId = blackboard.GetInt(uiInteractionsBlackboardID.ActiveChoiceHubID);

        if hubId != currentChoicehubId {
            return "The current choice hub ID doesn\'t match the sent one!";
        }

        let currentChoiceId = blackboard.GetInt(uiInteractionsBlackboardID.SelectedIndex);
        let delta = id - currentChoiceId;

        // Keys are hardcoded, you could try to dynamically resolve actions but NO
        let usedKey = EInputKey.IK_Down;
        if delta < 0 {
            usedKey = EInputKey.IK_Up;
        }

        let chain: [EInputKey];
        let abs = Abs(delta);
        let i = 0;

        while i < abs {
            ArrayPush(chain, usedKey);
            i += 1;
        }

        ArrayPush(chain, EInputKey.IK_F);

        this.InjectKeypressChain(chain);

        return "Attempting to make the choice.";
    }

    public cb func OnAutodriveToMappin(mappinId: NewMappinID) -> String {
        let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject() as PlayerPuppet;

        if !IsDefined(player) {
            return "Failed to get player.";
        }

        let vehicle = player.GetMountedVehicle();
        if !IsDefined(vehicle) {
            return "Failed to get the current vehicle.";
        }

        if !vehicle.IsPlayerDriver() {
            return "Player is not driving the vehicle.";
        }

        let mappinSystem = GameInstance.GetMappinSystem(GetGameInstance());

        let mappin = mappinSystem.GetMappin(mappinId);

        if !IsDefined(mappin) {
            return "Failed to get mappin info.";
        }

        this.TrackMappin(mappin);
        vehicle.KillNeurodrive();

        let aiCommand = new DriveToPointAutonomousUpdate();

        aiCommand.minSpeed = 5;
        aiCommand.maxSpeed = 120;
        aiCommand.targetPosition = mappin.GetWorldPosition();
        aiCommand.minimumDistanceToTarget = 40;
        aiCommand.driveDownTheRoadIndefinitely = false;

        vehicle.SetupNeurodrivePointToPoint(aiCommand);

        let warningMsg: SimpleScreenMessage;
        warningMsg.isShown = true;
        warningMsg.duration = 2;
        warningMsg.message = "NeuroDrive\u{2122} engaged. Stand back.";
        warningMsg.type = SimpleMessageType.Vehicle;

        GameInstance
            .GetBlackboardSystem(GetGameInstance())
            .Get(GetAllBlackboardDefs().UI_Notifications)
            .SetVariant(
                GetAllBlackboardDefs().UI_Notifications.WarningMessage,
                ToVariant(warningMsg),
                true
            );

        return "Autodrive started.";
    }

    public cb func OnAutodriveToDistrict(districtLocalizedName: String) -> String {
        let fastTravelSystem = GameInstance
            .GetScriptableSystemsContainer(GetGameInstance())
            .Get(n"FastTravelSystem") as FastTravelSystem;

        if !IsDefined(fastTravelSystem) {
            return "Failed to get fast travel system.";
        }

        let pointsByDistrict: [ref<FastTravelPointData>];

        for point in fastTravelSystem.GetFastTravelPoints() {
            let pointDistrict = GetLocalizedText(point.GetDistrictDisplayName());

            if Equals(districtLocalizedName, pointDistrict) {
                ArrayPush(pointsByDistrict, point);
            }
        }

        let pointCount = ArraySize(pointsByDistrict);

        if pointCount == 0 {
            return "No points were found with this district name!";
        }

        let pointIndex = RandRange(0, pointCount);

        let point = pointsByDistrict[pointIndex];

        return this.OnAutodriveToMappin(point.mappinID);
    }

    public cb func OnSummonVehicle() -> String {
        if VehicleSystem.IsSummoningVehiclesRestricted(GetGameInstance()) {
            return "Vehicle summoning is currently restricted due to gameplay reasons.";
        }

        let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject() as PlayerPuppet;

        if !IsDefined(player) {
            return "Could not obtain player.";
        }

        let quickslotsManager = player.GetQuickSlotsManager();

        if !IsDefined(quickslotsManager) {
            return "Failed to get quickslots manager.";
        }

        let activeVehicleType = quickslotsManager.GetPS().GetActiveType();

        let vehicleSystem = GameInstance.GetVehicleSystem(GetGameInstance());

        if vehicleSystem.IsActivePlayerVehicleOnCooldown(activeVehicleType) {
            return "The selected vehicle is on cooldown. Please wait.";
        }

        let dpadAction = new DPADActionPerformed();

        dpadAction.action = EHotkey.DPAD_RIGHT;
        dpadAction.state = EUIActionState.COMPLETED;
        dpadAction.successful = true;

        // Note: the return value on this seems to be bogus, returning false even if the vehicle is driving to you.
        vehicleSystem.SpawnActivePlayerVehicle(activeVehicleType);
        GameInstance.GetUISystem(GetGameInstance()).QueueEvent(dpadAction);

        return "Successfully summoned vehicle!";
    }

    public cb func OnQueryMoney() -> String {
        let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject();

        if !IsDefined(player) {
            return "Could not obtain player.";
        }

        if player.IsReplacer() {
            return "We\'re not playing as V right now.";
        }

        let transactionSystem = GameInstance.GetTransactionSystem(GetGameInstance());
        let moneyAmount = transactionSystem.GetItemQuantity(player, MarketSystem.Money());

        return s"V has \(moneyAmount) eddies.";
    }

    public cb func OnQueryTrackedQuest() -> String {
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());

        let tracked = journalManager.GetTrackedEntry() as JournalQuestObjective;

        if !IsDefined(tracked) {
            return "No quest is tracked";
        }

        // If there's a tracked entry, there's probably a tracked quest & phase?
        let parentPhase = journalManager.GetParentEntry(tracked) as JournalQuestPhase;
        let parentQuest = journalManager.GetParentEntry(parentPhase) as JournalQuest;

        return journalManager.GetNeuroFriendlyQuestData(parentQuest);
    }

    public cb func OnQueryAllQuests() -> String {
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());
        let questList: [wref<JournalEntry>];

        let ctx: JournalRequestContext;

        ctx.stateFilter.active = true;
        ctx.stateFilter.failed = false;
        ctx.stateFilter.inactive = false;
        ctx.stateFilter.succeeded = false;

        journalManager.GetQuests(ctx, questList);

        if ArraySize(questList) == 0 {
            return "No active quests found!";
        }

        let data = "# Quests:\r\n";

        for i in questList {
            let asQuest = i as JournalQuest;

            if IsDefined(asQuest) {
                data += journalManager.GetNeuroFriendlyQuestData(asQuest) + "\r\n";
            }
        }

        return data;
    }
}

@addMethod(GameInstance)
public native static func GetNeuroSystem() -> ref<NeuroSystem>;

@wrapMethod(SubtitleLineLogicController)
public func SetLineData(lineData: script_ref<scnDialogLineData>) {
    let lineDataDeref = Deref(lineData);
    let speaker: ref<GameObject> = lineDataDeref.speaker;

    if !IsDefined(speaker) || lineDataDeref.isPersistent {
        wrappedMethod(lineData);
        return;
    }

    let speakerName = lineDataDeref.speakerName;

    if !IsStringValid(speakerName) {
        speakerName = speaker.GetDisplayName();
    }

    let localizedSpeakerName = GetLocalizedText(speakerName);

    let line = lineData.text;

    if scnDialogLineData.HasKiroshiTag(lineDataDeref)
        && this.IsKiroshiEnabled()
        || scnDialogLineData.HasMothertongueTag(lineDataDeref) {
        let displayText = lineDataDeref.GetDisplayText();

        let nativeText = displayText.text;
        // Not sure which one is correct here
        let translated = displayText.postTranslatedText;

        line = s"\(nativeText) [translation: \(translated)]";
    }

    let neuroContext = s"Dialogue: [Type \(lineDataDeref.type)] \(localizedSpeakerName) says \"\(line)\"";

    GameInstance.GetNeuroSystem().SendContext(neuroContext);

    wrappedMethod(lineData);
}

