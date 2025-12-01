module Neuro

public class StringUtils {
    // This could probably be more efficient native, but why bother?
    public static func BuildString(parts: [String], delim: String) -> String {
        let i = 0;
        let sz = ArraySize(parts);
        let str = "";
        while i < sz {
            str += parts[i];
            if i < sz - 1 {
                str += delim;
            }
            i += 1;
        }

        return str;
    }
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

