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

