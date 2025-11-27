@wrapMethod(QuickhacksListGameController)
protected cb func OnQuickhackStarted(value: ref<RevealInteractionWheel>) -> Bool {
    wrappedMethod(value);

    if ArraySize(value.commands) > 0 {
        let lock: ref<GameObject> = value.lookAtObject;

        if !IsDefined(lock) {
            return true;
        }

        let collectedHackData = new NeuroQuickhackDataDto();

        collectedHackData.targetEntityId = lock.GetEntityID();

        for quickhack in value.commands {
            let hackData = new NeuroQuickhackDto();

            hackData.quickhackName = GetLocalizedText(quickhack.m_title);
            hackData.quickhackDesc = GetLocalizedText(quickhack.m_description);
            hackData.quickhackCategory = GetLocalizedText(quickhack.m_category.LocalizedDescription());

            let statusInt = EnumInt(quickhack.m_actionState);
            hackData.quickhackStatus = EnumValueToString("EActionInactivityReson", Cast<Int64>(statusInt));

            hackData.quickhackCost = quickhack.m_cost;
            hackData.uploadTime = quickhack.m_uploadTime;
            hackData.cooldown = quickhack.m_cooldown;
            hackData.willReveal = quickhack.m_willReveal;

            ArrayPush(collectedHackData.quickhacks, hackData);
        }
    }
}