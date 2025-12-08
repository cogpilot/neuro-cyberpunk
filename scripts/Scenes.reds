module Neuro

// This seems more stable
@wrapMethod(dialogWidgetGameController)
protected func UpdateDialogsData(const data: script_ref<DialogChoiceHubs>) -> Void {
    wrappedMethod(data);

    GameInstance.GetNeuroSystem().OnSceneDialogChoiceHubsProvided(this.m_data);
}

@wrapMethod(BaseSubtitlesGameController)
private final func SpawnDialogLine(const lineData: script_ref<scnDialogLineData>) -> Void {
    let lineDataDeref = Deref(lineData);
    let speaker: ref<GameObject> = lineDataDeref.speaker;

    if !IsDefined(speaker) || lineDataDeref.isPersistent {
        wrappedMethod(lineData);
        return;
    }

    let speakerName = lineDataDeref.speakerName;
    let localizedSpeakerName = GetLocalizedText(speakerName);

    if !IsStringValid(speakerName) {
        let puppet = speaker as ScriptedPuppet;

        if IsDefined(puppet) {
            speakerName = puppet.GetScannerName();
            localizedSpeakerName = GetLocalizedText(speakerName);
        } else {
            speakerName = speaker.GetDisplayName();
            localizedSpeakerName = GetLocalizedText(speakerName);
        }
    }

    let line = lineData.text;

    if scnDialogLineData.HasKiroshiTag(lineDataDeref)
        && this.IsKiroshiEnabled(lineData)
        || scnDialogLineData.HasMothertongueTag(lineDataDeref) {
        let displayText = lineDataDeref.GetDisplayText();

        let pre = displayText.preTranslatedText;
        let mid = displayText.text;
        let post = displayText.postTranslatedText;
        let translation = displayText.translation;
        let language = displayText.language;

        line = s"\(pre) \(mid) \(post)";

        if StrLen(translation) > 0 {
            line += s"[translation: \(translation), lang: \(language)]";
        }
    }

    let neuroContext = s"Dialogue: [Type \(lineDataDeref.type)] \(localizedSpeakerName) says \"\(line)\"";

    GameInstance.GetNeuroSystem().SendContext(neuroContext);

    wrappedMethod(lineData);
}

