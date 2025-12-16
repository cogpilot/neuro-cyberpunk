module Neuro

@wrapMethod(ComputerControllerPS)
public final func GetFileWidget(documentAddress: SDocumentAdress) -> SDocumentWidgetPackage {
    let ret = wrappedMethod(documentAddress);
    this.OnDocumentUpdateProvided(ret);
    return ret;
}

@wrapMethod(ComputerControllerPS)
public final func GetMailWidget(documentAddress: SDocumentAdress) -> SDocumentWidgetPackage {
    let ret = wrappedMethod(documentAddress);
    this.OnDocumentUpdateProvided(ret);
    return ret;
}

// documentData should be script_ref but I'm afraid of those
@addMethod(ComputerControllerPS)
private final func OnDocumentUpdateProvided(documentData: SDocumentWidgetPackage) {
    if !documentData.isValid {
        return;
    }

    let stringBuilderList: [String];

    if Equals(documentData.documentType, EDocumentType.FILE) {
        ArrayPush(stringBuilderList, "We\'re reading a file on a computer.");
    } else {
        ArrayPush(stringBuilderList, "We\'re reading an e-mail on a computer.");

        let addressee = GetLocalizedText(documentData.date);
        ArrayPush(stringBuilderList, s"The addressee is \(addressee)");
    }

    let titleStr = GetLocalizedText(documentData.title);
    ArrayPush(stringBuilderList, s"The document\'s title is \(titleStr)");

    if IsStringValid(documentData.owner) {
        let ownerStr = GetLocalizedText(documentData.owner);
        ArrayPush(stringBuilderList, s"The owner is \(ownerStr)");
    } else {
        ArrayPush(stringBuilderList, "The owner is unknown.");
    }

    if documentData.isEncrypted {
        ArrayPush(stringBuilderList, "The data is encrypted.");
    }

    if ResRef.IsValid(documentData.videoPath) {
        ArrayPush(stringBuilderList, "The document is a video.");
    } else {
        let contentStr = GetLocalizedText(documentData.content);
        ArrayPush(stringBuilderList, contentStr);

        if TDBID.IsValid(documentData.image) {
            ArrayPush(stringBuilderList, "The document contains an image");
        }
    }

    let str = StringUtils.BuildString(stringBuilderList, "\r\n");

    GameInstance.GetNeuroSystem().SendContext(str);
}

