module Neuro

class NeuroSMSNotifierSystem extends ScriptableSystem {
    public func OnAttach() {
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());
        journalManager
            .RegisterScriptCallback(this, n"OnJournalUpdateNeuro", gameJournalListenerType.State);
    }

    public func OnDetach() {
        let journalManager = GameInstance.GetJournalManager(GetGameInstance());
        journalManager.UnregisterScriptCallback(this, n"OnJournalUpdateNeuro");
    }

    public cb func OnJournalUpdateNeuro(
        hash: Uint32,
        className: CName,
        notifyOption: JournalNotifyOption,
        changeType: JournalChangeType
    ) {
        if NotEquals(className, n"gameJournalPhoneMessage") {
            return;
        }

        let journalManager = GameInstance.GetJournalManager(GetGameInstance());

        let entry = journalManager.GetEntry(hash);

        if !IsDefined(entry) {
            // This should never happen, but just in case :P
            return;
        }

        let asPhoneMessage = entry as JournalPhoneMessage;

        if !IsDefined(asPhoneMessage) {
            return;
        }

        let conversation = journalManager.GetParentEntry(asPhoneMessage) as JournalPhoneConversation;

        if !IsDefined(conversation) {
            return;
        }

        let contact = journalManager.GetParentEntry(conversation) as JournalContact;

        if !IsDefined(contact) {
            return;
        }

        let state = journalManager.GetEntryState(asPhoneMessage);

        if Equals(notifyOption, JournalNotifyOption.Notify)
            && Equals(state, gameJournalEntryState.Active)
            && Equals(changeType, JournalChangeType.Direct) {
            let sender = "We";

            if Equals(asPhoneMessage.GetSender(), gameMessageSender.NPC) {
                sender = GetLocalizedText(contact.GetLocalizedName(journalManager));
            }

            let convoTitle = GetLocalizedText(conversation.GetTitle());
            let messageText = GetLocalizedText(asPhoneMessage.GetText());

            let str = s"New SMS!\r\nConvo title: \"\(convoTitle)\"\r\n\(sender) sent:\r\n\"\(messageText)\"";

            GameInstance.GetNeuroSystem().SendContext(str);
        }
    }
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
                neuroInfo.messageHistory += "Us:\r\n";
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

