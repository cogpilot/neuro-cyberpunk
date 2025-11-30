module Neuro

@addMethod(JournalManager)
// Get a quest's data in a LLM-friendly fashion.
public func GetNeuroFriendlyQuestData(questEntry: wref<JournalEntry>) -> String {
    // Note: might be slow? Lots of string ops, best be careful, 
    // maybe move to C++ if *too* slow

    // Not really slow on starting-ish save? Might get chuggier with more stuff

    let asQuest = questEntry as JournalQuest;

    if !IsDefined(asQuest) {
        return "<invalid quest>";
    }

    let journalWrapper = new JournalWrapper();
    journalWrapper.Init(GetGameInstance());

    // This might be slow, as it's very recursive
    // Good thing *most* of this will be running async!
    let questData = journalWrapper.BuildQuestData(asQuest);
    
    let baseData = s"**Name: \(GetLocalizedText(questData.GetTitle())), type: \(questData.GetType())**\r\n\n**Description**\r\n\n\(GetLocalizedText(questData.GetDescription()))\r\n\nDistrict: \(questData.GetDistrict())";

    let objectives = questData.GetObjectives();

    if ArraySize(objectives) > 0 {
        baseData += "\r\n\n**Objectives**\r\n";
    }

    for objective in questData.GetObjectives() {
        if Equals(objective.GetStatus(), gameJournalEntryState.Active) {
            let objectiveDataBase = s" - \(GetLocalizedText(objective.GetDescription()))";
            
            if objective.IsTracked() {
                objectiveDataBase += " (Tracked)";
            }

            if this.GetIsObjectiveOptional(objective.GetQuestObjective()) {
                objectiveDataBase += " (Optional)";
            }

            if objective.m_totalCounter != 0 {
                objectiveDataBase += s" (\(objective.m_currentCounter)/\(objective.m_totalCounter))";
            }

            objectiveDataBase += "\r\n";

            let subObjectives = objective.GetSubObjectives();

            if ArraySize(subObjectives) > 0 {
                objectiveDataBase += " - Subobjectives:\r\n";
                for subObjective in subObjectives {
                    if Equals(subObjective.GetStatus(), gameJournalEntryState.Active) {
                        let subObjectiveDataBase = GetLocalizedText(subObjective.GetDescription());
            
                        if subObjective.IsTracked() {
                            subObjectiveDataBase += " (Tracked)";
                        }

                        if this.GetIsObjectiveOptional(subObjective.GetQuestObjective()) {
                            subObjectiveDataBase += " (Optional)";
                        }

                        if subObjective.m_totalCounter != 0 {
                            subObjectiveDataBase += s" (\(subObjective.m_currentCounter)/\(subObjective.m_totalCounter))";
                        }

                        subObjectiveDataBase += "\r\n";

                        objectiveDataBase += s"  - \(subObjectiveDataBase)";
                    }
                }
            }

            baseData += objectiveDataBase;
        }
    }

    let links = questData.GetLinks();
    let linkData = "";

    if ArraySize(links) > 0 {
        // Just their names, I guess?

        // HACK because I can't be arsed to fix this right, this is O(N^2) but for codex link size doesn't matter
        // Codex links seem to be per phase or objective I think
        let duplicateCodexNameArray: [String];

        for link in links {
            if Equals(this.GetEntryState(link), gameJournalEntryState.Active) {
                let asCodexEntry = link as JournalCodexEntry;

                if IsDefined(asCodexEntry) {
                    let title = asCodexEntry.GetTitle();
                    if !ArrayContains(duplicateCodexNameArray, title) {
                        linkData += s" - Database entry \(GetLocalizedText(title))\r\n";
                        ArrayPush(duplicateCodexNameArray, title);
                    }
                } else {
                    // Special case: Phantom Liberty prerequisite quest
                    let asQuestEntry = link as JournalQuest;

                    if IsDefined(asQuestEntry) {
                        linkData += s" - Quest \(GetLocalizedText(asQuestEntry.GetTitle(this)))\r\n";
                    }
                }
            }
        }
    }

    if NotEquals(linkData, "") {
        baseData += s"\n**Links**\r\n\(linkData)";
    }

    return baseData;
}

@wrapMethod(NewHudPhoneGameController)
public final func PushSMSNotification(msgEntry: wref<JournalPhoneMessage>, opt action: ref<GenericNotificationBaseAction>) -> Void {
    wrappedMethod(msgEntry, action);
    let journalManager = GameInstance.GetJournalManager(GetGameInstance());

    let msgConversation = this.m_journalMgr.GetParentEntry(msgEntry) as JournalPhoneConversation;
    let msgContact = this.m_journalMgr.GetParentEntry(msgConversation) as JournalContact;

    let convoName = GetLocalizedText(msgConversation.GetTitle());
    let contactName = msgContact.GetLocalizedName(journalManager);
    let text = GetLocalizedText(msgEntry.GetText());

    let str = s"New SMS message!\r\n**Sender**: \(contactName)\r\n**Convo name:** \(convoName)\r\n\(text)";

    GameInstance.GetNeuroSystem().SendContext(str);
}

@wrapMethod(IncomingCallLogicController)
public final func SetCallInfo(const contactName: script_ref<String>, contactEntry: wref<JournalContact>, journalMgr: wref<JournalManager>, isRejectable: Bool) -> Void {
    wrappedMethod(contactName, contactEntry, journalMgr, isRejectable);

    let name = Deref(contactName);
    let str = s"\(name) is calling us! Can reject: \(isRejectable)";
    
    GameInstance.GetNeuroSystem().SendContext(str);
}