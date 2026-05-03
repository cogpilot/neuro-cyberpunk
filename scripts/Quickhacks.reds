module Neuro

@wrapMethod(QuickhacksListGameController)
protected cb func OnQuickhackStarted(value: ref<RevealInteractionWheel>) -> Bool {
    wrappedMethod(value);

    if !value.shouldReveal {
        return true;
    }

    let lock: ref<GameObject> = value.lookAtObject;

    if !IsDefined(lock) {
        return true;
    }

    let desc = lock.TranslateQuickhackDataToNeuroDesc(value.commands);

    if !IsDefined(desc) {
        return true;
    }

    let neuroSystem = GameInstance.GetNeuroSystem();

    neuroSystem.OnQuickhackDataProvided(desc, false);
}

@addMethod(GameObject)
public final func GetQuickhackData() -> [ref<QuickhackData>] {
    // NOTE: this is very heavy!
    let player = GetPlayer(GetGameInstance());

    if !IsDefined(player) {
        return [];
    }

    let entId = this.GetEntityID();

    let quickhackActions: [ref<QuickhackData>];

    let asDevice = this as Device;
    let asScriptedPuppet = this as ScriptedPuppet;
    let asVehicle = this as VehicleObject;

    if IsDefined(asDevice) {
        if asDevice.m_isQhackUploadInProgerss && !asDevice.IsActionQueueEnabled() || asDevice.IsActionQueueFull() {
            return [];
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
        asScriptedPuppet.TranslateChoicesIntoQuickSlotCommands(puppetActions, quickhackActions);
    } else if IsDefined(asVehicle) {
        if asVehicle.m_isQhackUploadInProgress && !asVehicle.IsActionQueueEnabled() || asVehicle.IsActionQueueFull() {
            return [];
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

    return quickhackActions;
}

@addMethod(GameObject)
public final func GetMaxQuickhackQueueSizeForObject() -> Int32 {
    let player = GetPlayer(GetGameInstance());
    let playerStatsObjectId = Cast<StatsObjectID>(player.GetEntityID());
    let asScriptedPuppet = this as ScriptedPuppet;
    if IsDefined(asScriptedPuppet) {
        let queueSize = 1;
        if QuickHackableQueueHelper.IsQueuePerkBought(player) {
            queueSize += FloorF(
                GameInstance
                    .GetStatsSystem(GetGameInstance())
                    .GetStatValue(playerStatsObjectId, gamedataStatType.QuickHackQueueSize)
            );
        }
        // Already uploading hacks
        queueSize -= asScriptedPuppet.GetDeviceActionQueueSize();
        return queueSize;
    }

    return 1;
}

@addMethod(GameObject)
public final func TranslateQuickhackDataToNeuroDesc(data: [ref<QuickhackData>]) -> ref<NeuroQuickhackDataDto> {
    let hackCount = ArraySize(data);

    if hackCount == 0 {
        return null;
    }

    let usableHackCount = this.GetMaxQuickhackQueueSizeForObject();

    if usableHackCount <= 0 {
        return null;
    }

    let player = GetPlayer(GetGameInstance());
    let collectedHackData = new NeuroQuickhackDataDto();

    collectedHackData.targetEntityId = this.GetEntityID();
    collectedHackData.targetName = GetLocalizedText(this.GetDisplayName());
    collectedHackData.ramAmount = FloorF(
        GameInstance
            .GetStatPoolsSystem(GetGameInstance())
            .GetStatPoolValue(
                Cast<StatsObjectID>(player.GetEntityID()),
                gamedataStatPoolType.Memory,
                false
            )
    );
    collectedHackData.maxQueueSize = usableHackCount;

    let asNPC = this as NPCPuppet;

    if IsDefined(asNPC) {
        collectedHackData.isBoss = asNPC.IsBoss();
        collectedHackData.isHostile = asNPC.IsHostile();
        collectedHackData.targetName = asNPC.GetScannerName();

        let affiliation = asNPC.GetRecord().Affiliation();

        if IsDefined(affiliation) {
            collectedHackData.faction = GetLocalizedText(LocKeyToString(affiliation.LocalizedName()));
        } else {
            collectedHackData.faction = "N/A";
        }
    } else {
        collectedHackData.isInanimate = true;
        collectedHackData.faction = "N/A";
    }

    let i = 0;

    while i < hackCount {
        let quickhack = data[i];

        let hackData = new NeuroQuickhackDto();

        hackData.id = i;
        hackData.quickhackName = GetLocalizedText(quickhack.m_title);
        hackData.quickhackDesc = GetLocalizedText(quickhack.m_description);
        hackData.quickhackCategory = GetLocalizedText(quickhack.m_category.LocalizedDescription());

        let equippedHackData = EquippedQuickHackData.Make(player);
        let abilities = equippedHackData.GetAbilitiesByItemID(quickhack.m_itemID);

        let abilityStringBuilder: [String];

        for ability in abilities {
            let localizedDescription = GetLocalizedText(ability.Description);

            let textParams = ability.LocalizationDataPackage.GetTextParams();
            let text = StringUtils.FormatString(localizedDescription, textParams);

            ArrayPush(abilityStringBuilder, text);
        }

        if ArraySize(abilityStringBuilder) > 0 {
            hackData.quickhackDesc = StringUtils.BuildString(abilityStringBuilder, "\r\n");
        }

        let statusInt = EnumInt(quickhack.m_actionState);
        hackData.quickhackStatus = EnumValueToString("EActionInactivityReson", Cast<Int64>(statusInt));
        hackData.canUse = Equals(quickhack.m_actionState, EActionInactivityReson.Ready);

        hackData.quickhackCost = quickhack.m_cost;
        hackData.uploadTime = quickhack.m_uploadTime;
        hackData.cooldown = quickhack.m_cooldown;
        hackData.willReveal = quickhack.m_willReveal;

        ArrayPush(collectedHackData.quickhacks, hackData);

        i += 1;
    }

    return collectedHackData;
}

@addMethod(GameObject)
public final func GetNeuroQuickhackInfo() -> ref<NeuroQuickhackDataDto> {
    // Note: maybe we can cache quickhack data somehow?
    let hacks = this.GetQuickhackData();
    return this.TranslateQuickhackDataToNeuroDesc(hacks);
}

@addMethod(PlayerPuppet)
public final func GetQuickhackableTargetsForNeuro() -> [ref<NeuroQuickhackDataDto>] {
    let searchQuery: TargetSearchQuery;

    searchQuery.testedSet = TargetingSet.Complete;
    searchQuery.includeSecondaryTargets = false;
    searchQuery.filterObjectByDistance = true;
    searchQuery.searchFilter = TSF_Quickhackable();

    // Note: GetMaxDisplayRange is 50, I feel like we should limit it a lil
    searchQuery.maxDistance = SNameplateRangesData.GetMaxDisplayRange() * 0.5;

    let targetParts: [TS_TargetPartInfo];

    GameInstance
        .GetTargetingSystem(GetGameInstance())
        .GetTargetParts(this, searchQuery, targetParts);

    let processedEntityIDs: [EntityID];
    let hackables: [HandleWithValue];

    for part in targetParts {
        let targetingComponent = TS_TargetPartInfo.GetComponent(part);

        if IsDefined(targetingComponent) {
            let ent = targetingComponent.GetEntity();
            if IsDefined(ent) {
                let entId = ent.GetEntityID();
                if !ArrayContains(processedEntityIDs, entId) {
                    let asGameObject: ref<GameObject> = ent as GameObject;
                    if IsDefined(asGameObject) {
                        ArrayPush(processedEntityIDs, entId);
                        let dt = TS_TargetPartInfo.GetPlayerAngleDistance(part);
                        let hyp = SqrtF(PowF(dt.Pitch, 2) + PowF(dt.Yaw, 2));
                        let sortable: HandleWithValue;

                        sortable.value = hyp;
                        sortable.handle = asGameObject;

                        ArrayPush(hackables, sortable);
                    }
                }
            }
        }
    }

    SortHandleWithValueArray(hackables);

    let max = ArraySize(hackables);

    let MAX_QUICKHACKABLES_FOR_NEURO_GATHER = 12;
    if max > MAX_QUICKHACKABLES_FOR_NEURO_GATHER {
        max = MAX_QUICKHACKABLES_FOR_NEURO_GATHER;
    }

    let neuroTargets: [ref<NeuroQuickhackDataDto>];

    let i = 0;
    while i < max {
        let obj = hackables[i].handle as GameObject;

        let info = obj.GetNeuroQuickhackInfo();

        if IsDefined(info) {
            if info.HasUsableQuickhacks() {
                ArrayPush(neuroTargets, info);
            }
        }

        i += 1;
    }

    return neuroTargets;
}

@wrapMethod(ScriptedPuppet)
protected cb func OnNetworkLinkQuickhackEvent(evt: ref<NetworkLinkQuickhackEvent>) -> Bool
{
    wrappedMethod(evt);

    if Equals(evt.targetID, GetPlayer(GetGameInstance()).GetEntityID()) {
        let hasCounterhackPerk = GameInstance.GetStatsSystem(GetGameInstance()).GetStatValue(Cast<StatsObjectID>(evt.targetID), gamedataStatType.RevealNetrunnerWhenHacked) > 0.0;
        if hasCounterhackPerk {
            let netrunner = GameInstance.FindEntityByID(GetGameInstance(), evt.netrunnerID);
            if IsDefined(netrunner) {
                let netrunnerAsGameObject = netrunner as GameObject;

                if IsDefined(netrunnerAsGameObject) {
                    let hackData = netrunnerAsGameObject.GetNeuroQuickhackInfo();
                    if IsDefined(hackData) {
                        GameInstance.GetNeuroSystem().OnQuickhackDataProvided(hackData, true);
                    }
                }
            }
        }
    }
}

