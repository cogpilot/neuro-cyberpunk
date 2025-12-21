module Neuro

@wrapMethod(QuickhacksListGameController)
protected cb func OnQuickhackStarted(value: ref<RevealInteractionWheel>) -> Bool {
    wrappedMethod(value);

    if !value.shouldReveal {
        return true;
    }

    let neuroSystem = GameInstance.GetNeuroSystem();

    if neuroSystem.HasForcedActionCooldown() {
        return true;
    }

    let hackCount = ArraySize(value.commands);

    if hackCount > 0 {
        let lock: ref<GameObject> = value.lookAtObject;

        if !IsDefined(lock) {
            return true;
        }

        let collectedHackData = new NeuroQuickhackDataDto();

        collectedHackData.targetEntityId = lock.GetEntityID();
        collectedHackData.targetName = GetLocalizedText(lock.GetDisplayName());
        collectedHackData.ramAmount = FloorF(
            GameInstance
                .GetStatPoolsSystem(GetGameInstance())
                .GetStatPoolValue(
                    Cast<StatsObjectID>(this.GetPlayerControlledObject().GetEntityID()),
                    gamedataStatPoolType.Memory,
                    false
                )
        );

        let asNPC = lock as NPCPuppet;

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
            let quickhack = value.commands[i];

            let hackData = new NeuroQuickhackDto();

            hackData.id = i;
            hackData.quickhackName = GetLocalizedText(quickhack.m_title);
            hackData.quickhackDesc = GetLocalizedText(quickhack.m_description);
            hackData.quickhackCategory = GetLocalizedText(quickhack.m_category.LocalizedDescription());

            let equippedHackData = EquippedQuickHackData.Make(this.GetPlayerControlledObject() as PlayerPuppet);
            let abilities = equippedHackData.GetAbilitiesByItemID(quickhack.m_itemID);

            let abilityString = "";

            // EVIL HACK OF EVIL - formatting code is too complicated to rev
            // Neuro should be fine like this
            for ability in abilities {
                let localizedDescription = GetLocalizedText(ability.Description);
                let intCount = ArraySize(ability.LocalizationDataPackage.intValues);
                let currentIntParam = 0;

                while currentIntParam < intCount {
                    let replaced = s"{int_\(currentIntParam)}";
                    let replaceTo = s"\(ability.LocalizationDataPackage.intValues[currentIntParam])";

                    localizedDescription = StrReplaceAll(localizedDescription, replaced, replaceTo);

                    currentIntParam += 1;
                }

                let floatCount = ArraySize(ability.LocalizationDataPackage.floatValues);
                let currentFloatParam = 0;

                while currentFloatParam < floatCount {
                    let replaced = s"{float_\(currentFloatParam)}";
                    let replaceTo = s"\(ability.LocalizationDataPackage.floatValues[currentFloatParam])";

                    localizedDescription = StrReplaceAll(localizedDescription, replaced, replaceTo);

                    currentFloatParam += 1;
                }

                abilityString += s"\(localizedDescription)\r\n";
            }

            if NotEquals(abilityString, "") {
                hackData.quickhackDesc = abilityString;
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

        neuroSystem.OnQuickhackDataProvided(collectedHackData);
    }
}

@addMethod(GameObject)
public final func GetQuickhackData() -> [ref<QuickhackData>] {
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
public final func GetNeuroQuickhackInfo() -> ref<NeuroQuickhackDataDto> {
    let hacks = this.GetQuickhackData();
    let hackCount = ArraySize(hacks);

    if hackCount > 0 {
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
            let quickhack = hacks[i];

            let hackData = new NeuroQuickhackDto();

            hackData.id = i;
            hackData.quickhackName = GetLocalizedText(quickhack.m_title);
            hackData.quickhackDesc = GetLocalizedText(quickhack.m_description);
            hackData.quickhackCategory = GetLocalizedText(quickhack.m_category.LocalizedDescription());

            let equippedHackData = EquippedQuickHackData.Make(player);
            let abilities = equippedHackData.GetAbilitiesByItemID(quickhack.m_itemID);

            let abilityString = "";

            // EVIL HACK OF EVIL - formatting code is too complicated to rev
            // Neuro should be fine like this
            for ability in abilities {
                let localizedDescription = GetLocalizedText(ability.Description);
                let intCount = ArraySize(ability.LocalizationDataPackage.intValues);
                let currentIntParam = 0;

                while currentIntParam < intCount {
                    let replaced = s"{int_\(currentIntParam)}";
                    let replaceTo = s"\(ability.LocalizationDataPackage.intValues[currentIntParam])";

                    localizedDescription = StrReplaceAll(localizedDescription, replaced, replaceTo);

                    currentIntParam += 1;
                }

                let floatCount = ArraySize(ability.LocalizationDataPackage.floatValues);
                let currentFloatParam = 0;

                while currentFloatParam < floatCount {
                    let replaced = s"{float_\(currentFloatParam)}";
                    let replaceTo = s"\(ability.LocalizationDataPackage.floatValues[currentFloatParam])";

                    localizedDescription = StrReplaceAll(localizedDescription, replaced, replaceTo);

                    currentFloatParam += 1;
                }

                abilityString += s"\(localizedDescription)\r\n";
            }

            if NotEquals(abilityString, "") {
                hackData.quickhackDesc = abilityString;
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

    return null;
}

@addMethod(PlayerPuppet)
public final func GetQuickhackableTargetsForNeuro() -> [ref<NeuroQuickhackDataDto>] {
    let searchQuery: TargetSearchQuery;
    TargetSearchQuery.SetComponentFilter(searchQuery, TargetComponentFilterType.QuickHack);
    searchQuery.testedSet = TargetingSet.Visible;
    searchQuery.searchFilter = TSF_Quickhackable();

    // Hack but it's ok I think
    searchQuery.maxDistance = 30;

    let targetParts: [TS_TargetPartInfo];

    GameInstance
        .GetTargetingSystem(GetGameInstance())
        .GetTargetParts(this, searchQuery, targetParts);

    let neuroTargets: [ref<NeuroQuickhackDataDto>];

    for part in targetParts {
        let targetingComponent = TS_TargetPartInfo.GetComponent(part);

        if IsDefined(targetingComponent) {
            let ent = targetingComponent.GetEntity();

            if IsDefined(ent) {
                let asGameObject = ent as GameObject;

                if IsDefined(asGameObject) {
                    let neuroHackInfo = asGameObject.GetNeuroQuickhackInfo();
                    if IsDefined(neuroHackInfo) {
                        ArrayPush(neuroTargets, neuroHackInfo);
                    }
                }
            }
        }
    }

    return neuroTargets;
}

