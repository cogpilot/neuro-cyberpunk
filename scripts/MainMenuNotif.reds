module Neuro

@addField(SingleplayerMenuGameController)
private let m_neuroNotificationText: inkWidgetRef;

@addField(SingleplayerMenuGameController)
private let m_neuroNotificationTextController: ref<inkTextReplaceController>;

@wrapMethod(SingleplayerMenuGameController)
private final func FinishMenuInitialization() -> Void {
    wrappedMethod();

    let neuroNotificationText = new inkText();
    let controller = new inkTextReplaceController();

    neuroNotificationText.logicController = controller;
    this.m_neuroNotificationTextController = controller;

    let neuroPrimaryColor = HDRColor(0.368627, 0.964706, 1.0, 1.0);

    neuroNotificationText.SetFontFamily("base\\gameplay\\gui\\fonts\\raj\\raj.inkfontfamily");
    neuroNotificationText.SetFontStyle(n"Regular");
    neuroNotificationText.SetFontSize(42);
    neuroNotificationText.SetMargin(15, 0, 0, 0);
    neuroNotificationText.SetHorizontalAlignment(textHorizontalAlignment.Left);
    neuroNotificationText.SetVerticalAlignment(textVerticalAlignment.Top);
    neuroNotificationText.SetStyle(r"base\\gameplay\\gui\\common\\main_colors.inkstyle");
    neuroNotificationText.SetTintColor(neuroPrimaryColor);

    neuroNotificationText.Reparent(inkWidgetRef.Get(this.m_menuList) as inkCompoundWidget, 0);
    this.m_neuroNotificationText = inkWidgetRef.Create(neuroNotificationText);

    let neuroSystem = GameInstance.GetNeuroSystem();

    // Note: automagically handles new callbacks
    // This is done to smoothen anim
    neuroSystem.RegisterAliveCallback(this);
}

@wrapMethod(SingleplayerMenuGameController)
protected cb func OnUninitialize() -> Bool {
    GameInstance.GetNeuroSystem().UnregisterAliveCallback(this);
    wrappedMethod();
}

@addMethod(SingleplayerMenuGameController)
private cb func OnNeuroSocketUpdate(state: Bool) -> Void {
    if inkWidgetRef.IsValid(this.m_neuroNotificationText) {
        if state {
            let widget = inkWidgetRef.Get(this.m_neuroNotificationText) as inkText;

            widget.SetText("");

            this
                .m_neuroNotificationTextController
                .SetTargetText("ALERT: BREACHED BY N3-UR0...");
            this.m_neuroNotificationTextController.SetBaseText("");
            this.m_neuroNotificationTextController.SetDuration(5);
            this.m_neuroNotificationTextController.SetDelay(1);
            this.m_neuroNotificationTextController.PlaySetAnimation();
        }

        inkWidgetRef.SetVisible(this.m_neuroNotificationText, state);
    }
}

