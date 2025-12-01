module Neuro

@addMethod(JournalManager)
public func GetNeuroFriendlyQuestData(questEntry: wref<JournalEntry>) -> String {
    // Get a quest's data in a LLM-friendly fashion.

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
    let districtRecord = MappinUtils.GetDistrictRecord(questData.GetDistrict());

    let baseData = s"Name: \(GetLocalizedText(questData.GetTitle())), type: \(questData.GetType())\r\nDescription:\r\n\(GetLocalizedText(questData.GetDescription()))\r\nDistrict: \(GetLocalizedText(districtRecord.LocalizedName()))";

    let objectives = questData.GetObjectives();

    if ArraySize(objectives) > 0 {
        baseData += "\r\nObjectives:\r\n";
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
                objectiveDataBase
                    += s" (\(objective.m_currentCounter)/\(objective.m_totalCounter))";
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
                            subObjectiveDataBase
                                += s" (\(subObjective.m_currentCounter)/\(subObjective.m_totalCounter))";
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
                        linkData
                            += s" - Quest \(GetLocalizedText(asQuestEntry.GetTitle(this)))\r\n";
                    }
                }
            }
        }
    }

    if NotEquals(linkData, "") {
        baseData += s"Links:\r\n\(linkData)";
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
    let contactName = GetLocalizedText(msgContact.GetLocalizedName(journalManager));
    let text = GetLocalizedText(msgEntry.GetText());

    let str = s"New SMS message!\r\nSender: \(contactName)\r\nConvo name: \(convoName)\r\n\(text)";

    GameInstance.GetNeuroSystem().SendContext(str);
}

@wrapMethod(IncomingCallLogicController)
public final func SetCallInfo(
    const contactName: script_ref<String>,
    contactEntry: wref<JournalContact>,
    journalMgr: wref<JournalManager>,
    isRejectable: Bool
) -> Void {
    wrappedMethod(contactName, contactEntry, journalMgr, isRejectable);

    let localizedName = GetLocalizedText(contactEntry.GetLocalizedName(journalMgr));
    let str = s"\(localizedName) is calling us! Can reject: \(isRejectable)";

    GameInstance.GetNeuroSystem().SendContext(str);
}

@addMethod(MessengerDialogViewController)
public cb func MakeSyntheticMessageResponse(respId: Int32) -> String {
    let count = ArraySize(this.m_replyOptions);

    if respId < 0 || respId >= count {
        return "Invalid reply option specified!";
    }

    inkWidgetRef.SetVisible(this.m_replayFluff, count > 0);
    let i = 0;
    while i < count {
        if i != respId {
            this.m_journalManager.SetEntryVisited(this.m_replyOptions[i], true);
            this
                .m_journalManager
                .ChangeEntryStateByHash(
                    Cast<Uint32>(this.m_journalManager.GetEntryHash(this.m_replyOptions[i])),
                    gameJournalEntryState.Inactive,
                    JournalNotifyOption.Notify
                );
        }
        i += 1;
    }
    this.m_journalManager.SetEntryVisited(this.m_replyOptions[respId], true);
    this
        .m_journalManager
        .ChangeEntryStateByHash(
            Cast<Uint32>(this.m_journalManager.GetEntryHash(this.m_replyOptions[respId])),
            gameJournalEntryState.Succeeded,
            JournalNotifyOption.Notify
        );
    this.m_audioSystem.Play(n"ui_messenger_select");

    let messageData = this.m_replyOptions[respId] as JournalPhoneChoiceEntry;

    if !IsDefined(messageData) {
        return "Replied, but reply option no longer present!";
    }

    return s"Replied with \(GetLocalizedText(messageData.GetText()))";
}

@wrapMethod(MessengerDialogViewController)
public final func UpdateData(animateLastMessage: Bool, setVisited: Bool) -> Void {
    wrappedMethod(animateLastMessage, setVisited);

    if ArraySize(this.m_replyOptions) == 0 {
        return;
    }

    let neuroInfo = new NeuroPhoneMessageDto();

    neuroInfo.messageController = this;

    let conversation = this.m_journalManager.GetEntry(Cast<Uint32>(this.m_conversationHash)) as JournalPhoneConversation;
    if IsDefined(conversation) {
        // Not sure if needs localized text, but yeah
        neuroInfo.convoNameIfApplicable = GetLocalizedText(GetLocalizedText(conversation.GetTitle()));
    }

    let contact = this.m_journalManager.GetEntry(Cast<Uint32>(this.m_contactHash)) as JournalContact;
    if IsDefined(contact) {
        neuroInfo.contact = GetLocalizedText(contact.GetLocalizedName(this.m_journalManager));
    }

    for message in this.m_messages {
        let asPhoneMessage = message as JournalPhoneMessage;
        if IsDefined(asPhoneMessage) {
            if Equals(asPhoneMessage.GetSender(), gameMessageSender.Player) {
                neuroInfo.messageHistory += "Player:\r\n";
            } else {
                neuroInfo.messageHistory += s"\(neuroInfo.contact):\r\n";
            }
            neuroInfo.messageHistory += GetLocalizedText(asPhoneMessage.GetText());
            neuroInfo.messageHistory += "\r\n";
        }
    }

    for reply in this.m_replyOptions {
        let asReplyOption = reply as JournalPhoneChoiceEntry;
        if IsDefined(asReplyOption) {
            ArrayPush(neuroInfo.responseOptions, GetLocalizedText(asReplyOption.GetText()));
        }
    }

    GameInstance.GetNeuroSystem().OnSMSMessageDataProvided(neuroInfo);
}

@wrapMethod(PopupsManager)
public final func ShardRead() -> Void {
    let neuroSystem = GameInstance.GetNeuroSystem();

    let title = CodexUtils
        .GetShardTitleString(this.m_shardReadData.isCrypted, this.m_shardReadData.title);
    let text = CodexUtils
        .GetShardTextString(this.m_shardReadData.isCrypted, this.m_shardReadData.text);

    if this.m_shardReadData.isCrypted {
        neuroSystem
            .SendContext(
                s"Trying to read encrypted shard \"\(title)\"! Encrypted contents:\r\n\(text)"
            );
    } else {
        neuroSystem.SendContext(s"Shard \"\(title)\" contents:\r\n\(text)");
    }

    wrappedMethod();
}

@wrapMethod(JournalNotificationQueue)
protected cb func OnNewLocationDiscovered(newLocation: Bool) -> Bool {
    let neuroSystem = GameInstance.GetNeuroSystem();

    if newLocation {
        neuroSystem
            .SendContext(
                s"Entering new area: \(GetLocalizedText(
                    this
                        .m_newAreablackboard
                        .GetString(this.m_newAreaDef.currentLocation)
                ))"
            );
    }

    wrappedMethod(newLocation);
}

@wrapMethod(JournalNotificationQueue)
private final func PushQuestNotification(questEntry: wref<JournalQuest>, state: gameJournalEntryState) -> Void {
    wrappedMethod(questEntry, state);
    let neuroSystem = GameInstance.GetNeuroSystem();
    let questName = GetLocalizedText(questEntry.GetTitle(this.m_journalMgr));

    switch state {
        case gameJournalEntryState.Active:
            neuroSystem.SendContext(s"New quest \"\(questName)\" available!");
            break;
        case gameJournalEntryState.Succeeded:
            neuroSystem.SendContext(s"Quest \"\(questName)\" succeeded!");
            break;
        case gameJournalEntryState.Failed:
            neuroSystem.SendContext(s"Quest \"\(questName)\" failed!");
            break;
    }
}

@addField(QuestTrackerObjectiveLogicController)
private let m_objectiveState: CName;

@addField(QuestTrackerObjectiveLogicController)
private let m_finalTitle: String;

@addField(QuestTrackerObjectiveLogicController)
private let m_isTracked: Bool;

@replaceMethod(QuestTrackerObjectiveLogicController)
public final func SetData(
    const objectiveTitle: script_ref<String>,
    isTracked: Bool,
    isOptional: Bool,
    currentCounter: Int32,
    totalCounter: Int32,
    objectiveEntry: wref<JournalQuestObjective>,
    isQuestType: Bool
) -> Void {
    let itemID: TweakDBID;
    let itemRecord: ref<Item_Record>;
    let state: CName;
    this.m_objectiveEntry = objectiveEntry;
    let finalTitle: String = Deref(objectiveTitle);

    finalTitle = GetLocalizedText(finalTitle);

    if totalCounter > 0 {
        finalTitle += " ["
            + IntToString(currentCounter)
            + "/"
            + IntToString(totalCounter)
            + "]";
    }
    if isOptional {
        finalTitle += " [" + GetLocalizedText("UI-ScriptExports-Optional0") + "]";
    }

    itemID = this.m_objectiveEntry.GetItemID();
    if TDBID.IsValid(itemID) {
        itemRecord = TweakDBInterface.GetItemRecord(itemID);
        finalTitle
            += GetLocalizedText("Common-Characters-Semicolon") + " " + GetLocalizedText(NameToString(itemRecord.DisplayName()));
    }

    this.m_finalTitle = finalTitle;
    this.m_isTracked = isTracked;

    inkTextRef.SetText(this.m_objectiveTitle, finalTitle);
    inkWidgetRef.SetVisible(this.m_trackingIcon, isTracked);
    if isTracked {
        state = isQuestType ? n"tracked_quest" : n"Gigs";
    } else {
        state = n"untracked";
    }
    this.SetObjectiveState(state);
}

@wrapMethod(QuestTrackerObjectiveLogicController)
private final func SetObjectiveState(state: CName) -> Void {
    wrappedMethod(state);
    this.m_objectiveState = state;
}

@addMethod(QuestTrackerObjectiveLogicController)
public final func GetNeuroQuestTrackerObjectiveDescription() -> String {
    let objectiveStateTranslationString: String;

    switch this.m_objectiveState {
        case n"tracked_quest":
            objectiveStateTranslationString = "Tracked quest";
            break;
        case n"untracked":
            objectiveStateTranslationString = "Untracked";
            break;
        case n"Gigs":
            objectiveStateTranslationString = "Tracked gig";
            break;
        case n"succeeded":
            objectiveStateTranslationString = "Succeeded";
            break;
        case n"failed":
            objectiveStateTranslationString = "Failed";
            break;
        default:
            objectiveStateTranslationString = "Unknown";
            break;
    }

    let str = s"\(this.m_finalTitle) (state: \(objectiveStateTranslationString))";

    return str;
}

@addField(QuestTrackerGameController)
private let m_lastNeuroQuestTrackerUpdateHash: Uint64;

@addMethod(QuestTrackerGameController)
public final func UpdateNeuroQuestTrackerView() -> Void {
    let questTitle = GetLocalizedText(inkTextRef.GetText(this.m_QuestTitle));

    if StrLen(questTitle) == 0 {
        return;
    }

    let stringBuilder: [String];

    ArrayPush(stringBuilder, s"Tracking quest \"\(questTitle)\"");

    let objectiveCount = inkCompoundRef.GetNumChildren(this.m_ObjectiveContainer);

    if objectiveCount > 0 {
        ArrayPush(stringBuilder, "Objectives:");

        let objectiveIt = 0;
        while objectiveIt < objectiveCount {
            let controller = inkCompoundRef.GetWidgetByIndex(this.m_ObjectiveContainer, objectiveIt).GetController() as QuestTrackerObjectiveLogicController;

            if IsDefined(controller) {
                ArrayPush(stringBuilder, controller.GetNeuroQuestTrackerObjectiveDescription());
            }

            objectiveIt += 1;
        }
    }

    let str = StringUtils.BuildString(stringBuilder, "\r\n");
    // Prevent spamming Neuro with tons of messages...
    let hash = FNV1a64(str);

    if NotEquals(hash, this.m_lastNeuroQuestTrackerUpdateHash) {
        this.m_lastNeuroQuestTrackerUpdateHash = hash;
        GameInstance.GetNeuroSystem().SendContextSilent(str);
    }
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnInitialize() -> Bool {
    wrappedMethod();
    this.UpdateNeuroQuestTrackerView();
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnStateChanges(hash: Uint32, className: CName, notifyOption: JournalNotifyOption, changeType: JournalChangeType) -> Bool {
    wrappedMethod(hash, className, notifyOption, changeType);
    this.UpdateNeuroQuestTrackerView();
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnTrackedEntryChanges(hash: Uint32, className: CName, notifyOption: JournalNotifyOption, changeType: JournalChangeType) -> Bool {
    wrappedMethod(hash, className, notifyOption, changeType);
    this.UpdateNeuroQuestTrackerView();
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnCounterChanged(hash: Uint32, className: CName, notifyOption: JournalNotifyOption, changeType: JournalChangeType) -> Bool {
    wrappedMethod(hash, className, notifyOption, changeType);
    this.UpdateNeuroQuestTrackerView();
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnObjectiveIsOptionalChanged(hash: Uint32, className: CName, notifyOption: JournalNotifyOption, changeType: JournalChangeType) -> Bool {
    wrappedMethod(hash, className, notifyOption, changeType);
    this.UpdateNeuroQuestTrackerView();
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnMenuUpdate(value: Bool) -> Bool {
    wrappedMethod(value);
    this.UpdateNeuroQuestTrackerView();
}

@wrapMethod(QuestTrackerGameController)
protected cb func OnTrackedQuestPhaseUpdateRequest(evt: ref<TrackedQuestPhaseUpdateRequest>) -> Bool {
    wrappedMethod(evt);
    this.UpdateNeuroQuestTrackerView();
}

// Might as well put these here too...
@wrapMethod(ItemsNotificationQueue)
protected cb func OnNewTarotCardAdded(evt: ref<TarotCardAdded>) -> Bool {
    wrappedMethod(evt);
    GameInstance.GetNeuroSystem().SendContext(s"Obtained Tarot card \"\(GetLocalizedText(evt.cardName))\"");
}

@wrapMethod(ItemsNotificationQueue)
public final func PushCurrencyNotification(diff: Int32, total: Uint32) -> Void {
    wrappedMethod(diff, total);
    let str = "";
    if diff == 0 {
        return;
    }

    if diff >= 0 {
        str = s"Gained \(diff) eddies! Total \(total) €$.";
    } else {
        str = s"Lost \(Abs(diff)) eddies! Total \(total) €$.";
    }

    GameInstance.GetNeuroSystem().SendContextSilent(str);
}

@wrapMethod(ItemsNotificationQueue)
public final func PushItemNotification(itemID: ItemID, itemRarity: CName) -> Void {
    wrappedMethod(itemID, itemRarity);
    let currentItemRecord: ref<Item_Record> = TweakDBInterface.GetItemRecord(ItemID.GetTDBID(itemID));
    let tags: [CName];

    if IsDefined(currentItemRecord) {
      tags = currentItemRecord.Tags();
    };
    let isShard = ArrayContains(tags, n"Shard");
    if itemID != MarketSystem.Money() && !isShard && !ArrayContains(tags, n"DontShowLooted") {
        let neuroSystem = GameInstance.GetNeuroSystem();

        let itemDesc = neuroSystem.TranslateItemIdToNeuroDesc(this.GetPlayerControlledObject() as PlayerPuppet, itemID);

        neuroSystem.SendContextSilent(s"Looted new item! \(itemDesc).");
    }
}