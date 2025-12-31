
import Neuro.*

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

public native class NeuroPhoneMessageDto {
    public native let messageController: wref<IScriptable>;
    public native let contact: String;
    public native let convoNameIfApplicable: String;
    public native let messageHistory: String;
    public native let responseOptions: [String];
}

public native class NeuroSystem extends IGameSystem {
    public native func SendContext(msg: String) -> Void;

    public native func SendContextSilent(msg: String) -> Void;

    public native func TrackMappin(mappin: ref<IMappin>) -> Void;

    public native func InjectKeypressChain(data: [EInputKey]) -> Void;

    public native func HasForcedActionCooldown() -> Bool;

    public native func OnQuickhackDataProvided(data: ref<NeuroQuickhackDataDto>);

    public native func OnSMSMessageDataProvided(data: ref<NeuroPhoneMessageDto>);

    public native func OnSceneDialogChoiceHubsProvided(data: DialogChoiceHubs);

    public native func IsConnectionAlive() -> Bool;

    public native func RegisterAliveCallback(ctx: wref<IScriptable>) -> Void;

    public native func UnregisterAliveCallback(ctx: wref<IScriptable>) -> Void;

    public cb func OnConnectionFailure() -> Void {
        GameInstance
            .GetSystemRequestsHandler()
            .RequestSystemNotificationGeneric(n"Neuro-NotifyBadConnectionTitle", n"Neuro-NotifyBadConnectionDesc");
    }

    public cb func OnConnectedIngame() -> Void {
        let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject();

        // Should never happen
        if !IsDefined(player) {
            ModLog(n"Neuro", "OnConnectedIngame, player puppet is not defined!");
            return;
        }

        let puppet = player as PlayerPuppet;
        if !IsDefined(puppet) {
            ModLog(n"Neuro", "OnConnectedIngame, player puppet is not PlayerPuppet!");
            return;
        }

        this.SendPlayerInformation(puppet);
    }

    public func SendPlayerInformation(puppet: ref<PlayerPuppet>) -> Void {
        this.SendContext(puppet.GetNeuroPlayerContext());
        this.SendContext(this.OnQueryAllQuests());
    }

    public cb func OnQuickhackTarget(entId: EntityID, hackId: Int32) -> String {
        // Note: could be cool if Neuro could queue multiple quickhacks, but don't see good way to do that yet
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

        let maxQueueSize = 1;

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

            maxQueueSize = FloorF(
                GameInstance
                    .GetStatsSystem(GetGameInstance())
                    .GetStatValue(
                        Cast<StatsObjectID>(player.GetEntityID()),
                        gamedataStatType.QuickHackQueueSize
                    )
            );

            // Already uploading hacks
            maxQueueSize -= asScriptedPuppet.GetDeviceActionQueueSize();

            if maxQueueSize == 0 {
                return "Cannot quickhack target more!";
            }
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

        let responseStringBuilder: [String];
        let cnt = 0;

        for hackId in hackIds {
            if cnt >= maxQueueSize {
                break;
            }

            if sz < hackId {
                let s = s"\(hackId) Quickhack ID no longer present in list!";
                ArrayPush(responseStringBuilder, s);
            } else {
                let hack = quickhackActions[hackId];

                ArrayPush(
                    responseStringBuilder,
                    this.OnQuickhackTargetInternal(entId, hack, player, cnt == 0)
                );

                cnt += 1;
            }
        }

        return StringUtils.BuildString(responseStringBuilder, "\r\n");
    }

    public cb func OnSelectDialogueChoice(id: Int32, out success: Bool) -> String {
        let uiInteractionsBlackboardID = GetAllBlackboardDefs().UIInteractions;
        let blackboard = GameInstance.GetBlackboardSystem(GetGameInstance()).Get(uiInteractionsBlackboardID);

        if !IsDefined(blackboard) {
            success = true;
            return "Failed to get UI interaction blackboard.";
        }

        let hubs = FromVariant<DialogChoiceHubs>(blackboard.GetVariant(uiInteractionsBlackboardID.DialogChoiceHubs));

        if ArraySize(hubs.choiceHubs) == 0 {
            success = true;
            return "No choice hubs available, cannot make a choice.";
        }

        let currentChoicehubId = blackboard.GetInt(uiInteractionsBlackboardID.ActiveChoiceHubID);

        if currentChoicehubId == -1 {
            success = true;
            return "No choices available, cannot make a choice";
        }

        let currentChoiceId = blackboard.GetInt(uiInteractionsBlackboardID.SelectedIndex);

        let currAbsoluteChoiceId = 0;
        let totalChoiceCount = 0;

        for hub in hubs.choiceHubs {
            totalChoiceCount += ArraySize(hub.choices);
        }

        if id >= totalChoiceCount {
            success = false;
            return "Invalid choice ID, too high!";
        }

        if id < 0 {
            // Intentional rejection of a choice
            success = true;
            let hasTimedChoice = false;

            for hub in hubs.choiceHubs {
                if IsDefined(hub.timeProvider) {
                    hasTimedChoice = true;
                    break;
                }
            }

            if hasTimedChoice {
                return "You chose to wait and not pick anything just yet.";
            } else {
                return "You chose to not select a choice. Let the player know why.";
            }
        }

        for hub in hubs.choiceHubs {
            if hub.id == currentChoicehubId {
                currAbsoluteChoiceId += currentChoiceId;
                break;
            } else {
                currAbsoluteChoiceId += ArraySize(hub.choices);
            }
        }

        ModLog(
            n"Neuro",
            s"Absolute current choice ID: \(currAbsoluteChoiceId), relative: \(currentChoiceId)"
        );
        let delta = id - currAbsoluteChoiceId;

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

        success = true;
        return "You have chosen a dialogue choice, which your player in-game will now perform. Limit your reaction to one sentence.";
    }

    private final func OnAutodriveInternal(mappinId: NewMappinID, isTracked: Bool) -> String {
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
        vehicle.KillNeurodrive(true);

        let aiCommand = DriveToPointAutonomousUpdate.MakeNeuroAutodriveData(mappin.GetWorldPosition());
        vehicle.SetupNeurodrivePointToPoint(aiCommand, isTracked, false);

        let announcerEvent = new NeurodriveAnnouncerEvent();
        vehicle.QueueEvent(announcerEvent);

        return "Autodrive started.";
    }

    public cb func OnAutodriveToMappin(mappinId: NewMappinID) -> String {
        return this.OnAutodriveInternal(mappinId, false);
    }

    public cb func OnAutodriveToTracked() -> String {
        let mappinSystem = GameInstance.GetMappinSystem(GetGameInstance());
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());

        let trackedEntry = journalManager.GetTrackedEntry();

        if IsDefined(trackedEntry) {
            let phase = journalManager.GetParentEntry(trackedEntry);

            if IsDefined(phase) {
                let mappin = mappinSystem.GetMappinFromObjective(phase, trackedEntry);

                if IsDefined(mappin) {
                    return this.OnAutodriveInternal(mappin.GetNewMappinID(), true);
                }
            }
        }

        // Note: don't autoupdate tracker for this one prob
        return this.OnAutodriveInternal(mappinSystem.GetManuallyTrackedMappinID(), false);
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
            let asQuest: ref<JournalQuest> = i as JournalQuest;

            if IsDefined(asQuest) {
                data += journalManager.GetNeuroFriendlyQuestData(asQuest) + "\r\n";
            }
        }

        return data;
    }

    public cb func OnQueryPlayerInfo() -> String {
        let player = GameInstance.GetPlayerSystem(GetGameInstance()).GetLocalPlayerControlledGameObject();

        if !IsDefined(player) {
            return "Failed to get player puppet!";
        }

        let puppet = player as PlayerPuppet;

        if !IsDefined(puppet) {
            return "Failed to get player puppet!";
        }

        return puppet.GetNeuroPlayerContext();
    }

    public cb func OnQueryInventory() -> String {
        let uiInventorySystem = UIInventoryScriptableSystem.GetInstance(GetGameInstance());

        let filteredTags = [n"HideInBackpackUI", n"SoftwareShard"];
        let playerItemValues: [wref<IScriptable>];

        uiInventorySystem.GetPlayerItemsMap().GetValues(playerItemValues);

        let stringBuilder: [String];
        ArrayPush(stringBuilder, this.OnQueryMoney());
        ArrayPush(stringBuilder, "Inventory items:");

        for itemWref in playerItemValues {
            let item: ref<UIInventoryItem> = itemWref as UIInventoryItem;
            if !item.HasAnyTag(filteredTags) && !ItemID.HasFlag(item.GetID(), gameEItemIDFlag.Preview) {
                let itemStructure = ItemID.GetStructure(item.GetID());
                let hasQuantity = NotEquals(itemStructure, gamedataItemStructure.Unique);

                let itemQualityStr = item.GetQualityText();
                let displayName = item.GetName();
                let displayType = GetLocalizedTextByKey(item.GetItemData().GetLocalizedItemType());
                let price = FloorF(item.GetSellPrice());

                let str = s"\(itemQualityStr) \(displayName), type \(displayType), sell price \(price) \u{20ac}$\r\n";
                let description = item.GetDescription();
                if StrLen(description) > 0 {
                    str += s"Description:\r\n\(GetLocalizedText(description))\r\n";
                }

                let gmplDescription = item.GetGameplayDescription();
                if StrLen(gmplDescription) > 0 {
                    str
                        += s"Gameplay description:\r\n\(GetLocalizedText(gmplDescription))\r\n";
                }

                if hasQuantity {
                    str += s"Quantity: \(item.GetQuantity(false))\r\n";
                }

                ArrayPush(stringBuilder, str);
            }
        }

        return StringUtils.BuildString(stringBuilder, "\r\n");
    }

    public cb func OnTrackQuest(name: String) -> String {
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());
        return journalManager.TrackQuestByName(name);
    }

    public cb func OnQueryQuickhackTargets() -> [ref<NeuroQuickhackDataDto>] {
        let player = GetPlayer(GetGameInstance());

        if !IsDefined(player) {
            return [];
        }

        return player.GetQuickhackableTargetsForNeuro();
    }

    public func TranslateItemIdToNeuroDesc(owner: ref<PlayerPuppet>, id: ItemID) -> String {
        let transactionSystem = GameInstance.GetTransactionSystem(GetGameInstance());

        let itemData = transactionSystem.GetItemData(owner, id);

        if !IsDefined(itemData) {
            return "<undefined>";
        }

        let uiInventoryItemsManager = UIInventoryItemsManager
            .Make(
                owner,
                transactionSystem,
                UIScriptableSystem.GetInstance(GetGameInstance())
            );
        let uiItem = UIInventoryItem.Make(owner, itemData, uiInventoryItemsManager);

        let itemQualityStr = uiItem.GetQualityText();
        let displayName = uiItem.GetName();

        let displayType = GetLocalizedTextByKey(itemData.GetLocalizedItemType());

        let str = s"\(itemQualityStr) \(displayName), type \(displayType)";

        if uiItem.IsQuestItem() {
            str += " (Quest)";
        }

        return str;
    }

    public cb func OnLogNeuroAction(actionName: String) -> Void {
        // NOTE: can be done in native, but fmt is more problematic in native :(
        let str = s"Neuro used action \(actionName)";
        GameInstance.GetActivityLogSystem(GetGameInstance()).AddLog(str);
    }
}

@addMethod(GameInstance)
public native static func GetNeuroSystem() -> ref<NeuroSystem>;

