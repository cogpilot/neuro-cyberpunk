module Neuro

import Codeware.Localization.*

// We do not elect for proper ArchiveXL localization here because I don't want to bundle an archive
// English will likely be the only used localization language for this
public class NeuroEnglishLocalization extends ModLocalizationPackage {
    protected func DefineTexts() -> Void {
        this.Text("Neuro-OnAutodriveStart", "NeuroDrive\u{2122} engaged. Stand back.");
        this.Text("Neuro-OnAutodriveKilled", "NeuroDrive\u{2122} stopped.");
        this.Text("Neuro-NotifyBadConnectionTitle", "Neuro socket failure");
        this
            .Text(
                "Neuro-NotifyBadConnectionDesc",
                "The Neuro socket failed to connect five times in a row. Connections will not be retried. Restart the game or use the CET console command Game.GetNeuroSystem().ResetBadConnectionCounter()."
            );
    }
}

public class NeuroLocalizationProvider extends ModLocalizationProvider {
    public func GetPackage(language: CName) -> ref<ModLocalizationPackage> {
        if Equals(language, n"en-us") {
            return new NeuroEnglishLocalization();
        }
        return null;
    }

    public func GetFallback() -> CName {
        return n"en-us";
    }
}

