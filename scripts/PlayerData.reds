module Neuro

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

        let pds = PlayerDevelopmentSystem.GetInstance(this);

        if IsDefined(pds) {
            let lifePath = pds.GetLifePath(this);

            switch lifePath {
                case gamedataLifePath.Corporate:
                    ArrayPush(stringBuilderList, "V is a corpo.");
                    break;
                case gamedataLifePath.Nomad:
                    ArrayPush(stringBuilderList, "V is a nomad.");
                    break;
                case gamedataLifePath.StreetKid:
                    ArrayPush(stringBuilderList, "V is a streetkid.");
                    break;
                default:
                    break;
            }
        }
    }

    let preventionSystem = GameInstance.GetScriptableSystemsContainer(GetGameInstance()).Get(NameOf<PreventionSystem>()) as PreventionSystem;
    let district = preventionSystem.GetCurrentDistrict();

    if IsDefined(district) {
        let locked: ref<District> = district;

        ArrayPush(stringBuilderList, s"The player is currently in \(GetLocalizedText(locked.GetDistrictRecord().LocalizedName())).");
    }

    ArrayPush(stringBuilderList, s"The player has \(hp)/\(maxHp) health.");
    let nonLoweredGender = NameToString(this.GetResolvedGenderName());
    let gender = StrLower(nonLoweredGender);

    ArrayPush(stringBuilderList, s"The player is \(gender).");

    if this.IsInCombat() {
        ArrayPush(stringBuilderList, "The player is in combat.");
    } else {
        ArrayPush(stringBuilderList, "The player is not in combat.");
    }

    let mountedVehicle = this.GetMountedVehicle();

    if IsDefined(mountedVehicle) {
        if mountedVehicle.IsPlayerDriver() {
            ArrayPush(stringBuilderList, "The player is driving a vehicle.");
        } else {
            ArrayPush(stringBuilderList, "The player is a passenger in a vehicle.");
        }

        let displayName = mountedVehicle.GetDisplayName();
        let localizedDisplayName = GetLocalizedText(displayName);
        ArrayPush(stringBuilderList, s"The vehicle is a \(localizedDisplayName).");
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

    return StringUtils.BuildString(stringBuilderList, "\r\n");
}

// Note: this method was chosen because it runs a bit after player attaches
@wrapMethod(PlayerPuppet)
protected cb func OnGameLoadedFactReset(evt: ref<GameLoadedFactReset>) -> Bool {
    wrappedMethod(evt);

    let systemRequestsHandler: ref<inkISystemRequestsHandler> = GameInstance.GetSystemRequestsHandler();

    if IsDefined(systemRequestsHandler) {
        if systemRequestsHandler.IsPreGame() {
            return true;
        }
    }

    // Simplest way to do this
    let neuroSystem = GameInstance.GetNeuroSystem();
    neuroSystem.SendPlayerInformation(this);
}

@addField(PlayerPuppet)
private let m_informedNeuroOfDeath: Bool;

@addField(PlayerPuppet)
public let m_informedNeuroOfOurSuicide: Bool;

@wrapMethod(PlayerPuppet)
protected cb func OnDeath(evt: ref<gameDeathEvent>) -> Bool {
    wrappedMethod(evt);
    // Not sure how this works with Second Heart
    if this.m_informedNeuroOfDeath {
        return true;
    }

    this.m_informedNeuroOfDeath = true;

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

    let str = StringUtils.BuildString(stringBuilder, "\r\n");

    GameInstance.GetNeuroSystem().SendContext(str);
}

@wrapMethod(PlayerPuppet)
protected cb func OnMountingEvent(evt: ref<MountingEvent>) -> Bool {
    wrappedMethod(evt);

    let stringBuilder: [String];

    let isVehicle = Equals(gameMountingObjectType.Vehicle, evt.relationship.otherMountableType);
    if isVehicle {
        ArrayPush(stringBuilder, "The player is entering a vehicle.");

        if VehicleComponent.IsDriverSlot(evt.request.lowLevelMountingInfo.slotId.id) {
            ArrayPush(stringBuilder, "The player will be driving the vehicle.");
        }

        let str = StringUtils.BuildString(stringBuilder, "\r\n");

        GameInstance.GetNeuroSystem().SendContext(str);
    }
}

@wrapMethod(PlayerPuppet)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
    wrappedMethod(evt);

    let isVehicle = Equals(gameMountingObjectType.Vehicle, evt.relationship.otherMountableType);

    if isVehicle {
        GameInstance.GetNeuroSystem().SendContext("The player is leaving a vehicle.");
    }
}

@wrapMethod(PlayerPuppet)
protected cb func OnCombatStateChanged(newState: Int32) -> Bool {
    let isInCombat = newState == 1;

    if NotEquals(this.m_inCombat, isInCombat) {
        // State changed, inform
        let neuroSystem = GameInstance.GetNeuroSystem();

        if isInCombat {
            neuroSystem.SendContext("The player has entered combat.");
        } else {
            neuroSystem.SendContext("The player has exited combat.");
        }
    }

    wrappedMethod(newState);
}

@wrapMethod(ScriptedPuppet)
protected cb func OnKillRewardEvent(evt: ref<KillRewardEvent>) -> Bool {
    wrappedMethod(evt);

    if IsDefined(this as PlayerPuppet) {
        let player = this as PlayerPuppet;

        let lock: ref<GameObject> = evt.victim;

        if player == lock {
            if !player.m_informedNeuroOfOurSuicide {
                player.m_informedNeuroOfOurSuicide = true;
                GameInstance.GetNeuroSystem().SendContext("The player managed to kill themselves.");
            }
        }

        let asPuppet = lock as ScriptedPuppet;

        if IsDefined(asPuppet) {
            let affiliationName = "None";

            if asPuppet.IsCivilian() {
                affiliationName = "Civilian";
            } else {
                let affiliation = asPuppet.GetRecord().Affiliation();

                if IsDefined(affiliation) {
                    affiliationName = GetLocalizedText(LocKeyToString(affiliation.LocalizedName()));
                }
            }

            GameInstance.GetNeuroSystem().SendContext(s"The player killed \(asPuppet.GetScannerName()) [affiliation: \(affiliationName)].");
        }
    }
}

@wrapMethod(PreventionSystem)
private final func OnHeatChanged(previousHeat: EPreventionHeatStage) -> Void {
    wrappedMethod(previousHeat);

    let currentHeat = this.m_heatStage;
    let asInt = EnumInt(currentHeat);
    let prevAsInt = EnumInt(previousHeat);
    if asInt == 0 && asInt != prevAsInt {
        GameInstance.GetNeuroSystem().SendContext("You are no longer wanted by the police.");
    } else {
        GameInstance.GetNeuroSystem().SendContext(s"You are wanted by the police with \(asInt) star(s).");
    }
}
