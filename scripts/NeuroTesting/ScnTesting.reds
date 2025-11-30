module Neuro

@wrapMethod(DialogHubLogicController)
private final func UpdateDialogHubData() -> Void {
    GameInstance.GetNeuroSystem().OnSceneListChoiceDataProvided(this.m_data);
    wrappedMethod();
}