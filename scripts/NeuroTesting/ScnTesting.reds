@wrapMethod(DialogHubLogicController)
private final func UpdateDialogHubData() -> Void {
    GameInstance.GetNeuroSystem().OnSceneListChoiceDataProvided(this.m_data);
    wrappedMethod();
}

@wrapMethod(dialogWidgetGameController)
protected cb func OnDialogsSelectIndex(index: Int32) -> Bool {
    wrappedMethod(index);
}