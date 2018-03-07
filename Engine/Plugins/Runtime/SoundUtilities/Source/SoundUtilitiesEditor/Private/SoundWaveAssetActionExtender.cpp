// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundWaveAssetActionExtender.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetTypeActions_Base.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "Sound/SoundWave.h"
#include "SoundSimple.h"
#include "SoundSimpleFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FSoundWaveAssetActionExtender::GetExtendedActions(const TArray<TWeakObjectPtr<USoundWave>>& InSounds, FMenuBuilder& MenuBuilder)
{
	const TAttribute<FText> Label = LOCTEXT("SoundWave_CreateSimpleSound", "Create Simple Sound");
	const TAttribute<FText> ToolTip = LOCTEXT("SoundWave_CreateSimpleSoundTooltip", "Creates a simple sound asset using the selected sound waves.");
	const FSlateIcon Icon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SoundSimple");
	const FUIAction UIAction = FUIAction(FExecuteAction::CreateSP(this, &FSoundWaveAssetActionExtender::ExecuteCreateSimpleSound, InSounds), FCanExecuteAction());

	MenuBuilder.AddMenuEntry(Label, ToolTip, Icon, UIAction);
}

void FSoundWaveAssetActionExtender::ExecuteCreateSimpleSound(TArray<TWeakObjectPtr<USoundWave>> SoundWaves)
{
	const FString DefaultSuffix = TEXT("_SimpleSound");

	if (SoundWaves.Num() > 0)
	{
		USoundWave* SoundWave = SoundWaves[0].Get();

		// Determine an appropriate name
		FString Name;
		FString PackagePath;

		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(SoundWave->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

		// Create the factory used to generate the asset
		USoundSimpleFactory* Factory = NewObject<USoundSimpleFactory>();

		for (int32 i = 0; i < SoundWaves.Num(); ++i)
		{
			Factory->SoundWaves.Add(SoundWaves[i].Get());
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundSimple::StaticClass(), Factory);

	}
}

#undef LOCTEXT_NAMESPACE