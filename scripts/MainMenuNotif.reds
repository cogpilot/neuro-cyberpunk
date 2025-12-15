
@addField(SingleplayerMenuGameController)
private let m_neuroNotificationText: inkTextRef;

@wrapMethod(SingleplayerMenuGameController)
private final func FinishMenuInitialization() -> Void {
    wrappedMethod();

    let neuroNotificationText = new inkText();

    neuroNotificationText.SetText("Neuro-sama integration active...");
    neuroNotificationText.SetFontFamily("base\\gameplay\\gui\\fonts\\raj\\raj.inkfontfamily");
    neuroNotificationText.SetFontStyle(n"Regular");
    neuroNotificationText.SetFontSize(42);
    neuroNotificationText.SetMargin(15, 0, 0, 0);
    neuroNotificationText.SetHorizontalAlignment(textHorizontalAlignment.Left);
    neuroNotificationText.SetVerticalAlignment(textVerticalAlignment.Top);
    neuroNotificationText.SetStyle(r"base\\gameplay\\gui\\common\\main_colors.inkstyle");
    neuroNotificationText.BindProperty(n"tintColor", n"MainColors.Green");

    neuroNotificationText.Reparent(inkWidgetRef.Get(this.m_menuList) as inkCompoundWidget, 0);
    this.m_neuroNotificationText = inkWidgetRef.Create(neuroNotificationText) as inkTextRef;

    let neuroSystem = GameInstance.GetNeuroSystem();
    let state = neuroSystem.IsConnectionAlive();

    neuroNotificationText.SetVisible(state);
    neuroSystem.RegisterAliveCallback(this);
}

@wrapMethod(SingleplayerMenuGameController)
protected cb func OnUninitialize() -> Bool {
    wrappedMethod();
    GameInstance.GetNeuroSystem().UnregisterAliveCallback(this);
}

@addMethod(SingleplayerMenuGameController)
private cb func OnNeuroSocketUpdate(state: Bool) -> Void {
    inkWidgetRef.SetVisible(this.m_neuroNotificationText, state);
}

