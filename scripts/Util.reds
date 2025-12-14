public native class StringUtils {
    public static native func BuildString(parts: [String], delim: String) -> String;
}

@addMethod(ScriptedPuppet)
public final func GetScannerName() -> String {
    let characterRecord = TweakDBInterface.GetCharacterRecord(this.GetRecordID());

    let archetypeName = characterRecord
        .ArchetypeData()
        .Type()
        .LocalizedName();
    let NPCName = "";

    if IsNameValid(archetypeName) {
        NPCName = GetLocalizedTextByKey(archetypeName);
    }

    if this.IsCharacterCivilian() || Equals(characterRecord.BaseAttitudeGroup(), n"child_ow") {
        if IsNameValid(characterRecord.DisplayName()) {
            NPCName = GetLocalizedTextByKey(characterRecord.DisplayName());
        }
    } else {
        if IsNameValid(characterRecord.FullDisplayName()) {
            NPCName = GetLocalizedTextByKey(characterRecord.FullDisplayName());
        } else {
            if IsNameValid(characterRecord.DisplayName()) {
                NPCName = GetLocalizedTextByKey(characterRecord.DisplayName());
            }
        }
    }

    if Equals(NPCName, "") {
        NPCName = GetLocalizedText(this.GetDisplayName());
    }

    return NPCName;
}

