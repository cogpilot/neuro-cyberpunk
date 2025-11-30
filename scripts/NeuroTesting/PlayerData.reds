@addMethod(PlayerPuppet)
public func GetNeuroPlayerContext() -> String {
    let statPoolSystem = GameInstance.GetStatPoolsSystem(GetGameInstance());
    let statsObjectId = Cast<StatsObjectID>(this.GetEntityID());

    // We don't really care about speed for this one
    let stringBuilderList: [String];

    if this.IsDead() {
        ArrayPush(stringBuilderList, "The player is dead and is thus waiting for a save reload.");
    }

    let hp = FloorF(statPoolSystem.GetStatPoolValue(statsObjectId, gamedataStatPoolType.Health, false));
    let maxHp = FloorF(statPoolSystem.GetStatPoolMaxPointValue(statsObjectId, gamedataStatPoolType.Health));

    if this.IsReplacer() {
        ArrayPush(stringBuilderList, "We are currently playing as a character other than V, so results may be misleading. They will still be referred to as V.");

        let recordId = this.GetRecordID();

        if Equals(t"Character.johnny_replacer", recordId) {
            ArrayPush(stringBuilderList, "We are playing one of Johnny Silverhand's flashbacks as Johnny Silverhand.");
        } else if Equals(t"Character.kurt_replacer", recordId) {
            ArrayPush(stringBuilderList, "We are playing as Kurt Hansen.");
        }
    } else {
        ArrayPush(stringBuilderList, "We are playing as V.");
    }

    let preventionSystem = GameInstance.GetScriptableSystemsContainer(GetGameInstance()).Get(NameOf<PreventionSystem>()) as PreventionSystem;
    let district = preventionSystem.GetCurrentDistrict();

    if IsDefined(district) {
        let locked: ref<District> = district;
        
        ArrayPush(stringBuilderList, s"The player is currently in \(GetLocalizedText(locked.GetDistrictRecord().LocalizedName()))");
    }

    ArrayPush(stringBuilderList, s"The player has \(hp)/\(maxHp) health.");

    let gender = StrLower(NameToString(this.GetResolvedGenderName()));

    ArrayPush(stringBuilderList, s"The player is \(gender)");

    if this.IsInCombat() {
        ArrayPush(stringBuilderList, "The player is in combat.");
    } else {
        ArrayPush(stringBuilderList, "The player is not in combat.");
    }

    let mountedVehicle = this.GetMountedVehicle();

    if IsDefined(mountedVehicle) {
        if mountedVehicle.IsPlayerControlled() {
            ArrayPush(stringBuilderList, "The player is driving a vehicle.");
        } else {
            ArrayPush(stringBuilderList, "The player is a passenger in a vehicle.");
        }
        
        ArrayPush(stringBuilderList, s"The vehicle is a \(GetLocalizedText(mountedVehicle.GetDisplayName()))");
    } else {
        ArrayPush(stringBuilderList, "The player is not currently in a vehicle.");
    }

    let neuroSystem = GameInstance.GetNeuroSystem();
    let equipmentSystem = EquipmentSystem.GetInstance(this).GetPlayerData(this);

    ArrayPush(stringBuilderList, "The player's equipped cyberware consists of:");
    for cwEquipmentArea in equipmentSystem.GetAllCyberwareEquipmentAreas() {
        let equipArea = equipmentSystem.GetEquipArea(cwEquipmentArea);
        for equipItem in equipArea.equipSlots {
            if ItemID.IsValid(equipItem.itemID) {
                ArrayPush(stringBuilderList, s"- \(neuroSystem.TranslateItemIdToNeuroDesc(this, equipItem.itemID))");
            }
        }
    }

    ArrayPush(stringBuilderList, "The player's equipped weapons and clothes are:");
    for weaponOrClothing in equipmentSystem.GetEquippedClothesAndWeapons() {
        if ItemID.IsValid(weaponOrClothing) {
            ArrayPush(stringBuilderList, s"- \(neuroSystem.TranslateItemIdToNeuroDesc(this, weaponOrClothing))");
        }
    }

    let str = "";

    for i in stringBuilderList {
        str += i;
        str += "\r\n";
    }

    return str;
}

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
    wrappedMethod();

    // Simplest way to do this
    let neuroSystem = GameInstance.GetNeuroSystem();
    neuroSystem.OnConnectedIngame();
}

@wrapMethod(PlayerPuppet)
protected cb func OnDeath(evt: ref<gameDeathEvent>) -> Bool {
    wrappedMethod(evt);
    let stringBuilder: [String];
    ArrayPush(stringBuilder, "The player got flatlined! Commands will not give meaningful responses until a save is reloaded");
    if IsDefined(evt.instigator) {
        let locked: ref<GameObject> = evt.instigator;

        if IsDefined(locked) {
            let displayName = locked.GetDisplayName();

            if StrLen(displayName) > 0 {
                ArrayPush(stringBuilder, s"The player got flatlined by \(GetLocalizedText(displayName))");
            }
        }
    }

    let str = "";

    for i in stringBuilder {
        str += i;
        str += "\r\n";
    }

    GameInstance.GetNeuroSystem().SendContext(str);
}

@wrapMethod(PlayerPuppet)
protected cb func OnMountingEvent(evt: ref<MountingEvent>) -> Bool {
    wrappedMethod(evt);

    let stringBuilder: [String];

    ArrayPush(stringBuilder, "The player is entering a vehicle.");

    if VehicleComponent.IsDriverSlot(evt.request.lowLevelMountingInfo.slotId.id) {
        ArrayPush(stringBuilder, "The player will be driving the vehicle.");
    }

    let str = "";

    for i in stringBuilder {
        str += i;
        str += "\r\n";
    }

    GameInstance.GetNeuroSystem().SendContext(str);
}

@wrapMethod(PlayerPuppet)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
    wrappedMethod(evt);
    GameInstance.GetNeuroSystem().SendContext("The player is leaving a vehicle.");
}