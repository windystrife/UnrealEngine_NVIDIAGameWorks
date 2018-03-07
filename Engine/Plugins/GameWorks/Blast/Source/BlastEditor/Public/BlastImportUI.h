#pragma once
#include "CoreMinimal.h"
#include "Factories/ImportSettings.h"
#include "Factories/FbxImportUI.h"
#include "BlastAssetImportOptions.h"
#include "BlastImportUI.generated.h"

/*
	This class represents the UI that will be presented to the user when importing a Blast asset. It will be presented in a modal dialog box using
	detail customization.
*/
UCLASS(config=EditorPerProjectUserSettings, HideCategories = Object, MinimalAPI)
class UBlastImportUI : public UObject, public IImportSettingsParser
{
	GENERATED_BODY()
public:
	UBlastImportUI();

	UPROPERTY(EditAnywhere, Category = "Blast Import Options", meta = (ShowOnlyInnerProperties))
	FBlastAssetImportOptions		ImportOptions;

	//This is shown by a special editor UI so it has no category
	UPROPERTY(Instanced)
	UFbxImportUI*					FBXImportUI;

	/** Open a modal dialog and return import options for this Blast asset we're importing. */
	bool GetBlastImportOptions(const FString& FullPath);

	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) override;
};
