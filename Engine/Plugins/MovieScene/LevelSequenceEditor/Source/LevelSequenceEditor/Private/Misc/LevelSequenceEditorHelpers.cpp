// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequenceEditorHelpers.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GCObject.h"
#include "LevelSequence.h"
#include "Factories/Factory.h"
#include "AssetData.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Layout/Margin.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "LevelSequenceEditorModule.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/AssetEditorManager.h"
#include "Misc/LevelSequenceEditorSettings.h"
#include "Widgets/Layout/SScrollBox.h"

#include "AssetRegistryModule.h"
#include "IDetailsView.h"
#include "MovieSceneToolsProjectSettings.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"

/* LevelSequenceEditorHelpers
 *****************************************************************************/

#define LOCTEXT_NAMESPACE "LevelSequenceEditorHelpers"

TWeakPtr<SWindow> MasterSequenceSettingsWindow;

class SMasterSequenceSettings : public SCompoundWidget, public FGCObject
{
	SLATE_BEGIN_ARGS(SMasterSequenceSettings) {}
		SLATE_ARGUMENT(ULevelSequenceMasterSequenceSettings*, MasterSequenceSettings)
		SLATE_ARGUMENT(UMovieSceneToolsProjectSettings*, ToolsProjectSettings)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		const ULevelSequenceMasterSequenceSettings* LevelSequenceSettings = GetDefault<ULevelSequenceMasterSequenceSettings>();

		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs Details1ViewArgs;
		Details1ViewArgs.bUpdatesFromSelection = false;
		Details1ViewArgs.bLockable = false;
		Details1ViewArgs.bAllowSearch = false;
		Details1ViewArgs.bShowOptions = false;
		Details1ViewArgs.bAllowFavoriteSystem = false;
		Details1ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		Details1ViewArgs.ViewIdentifier = "MasterSequenceSettings";

		Details1View = PropertyEditor.CreateDetailView(Details1ViewArgs);

		FDetailsViewArgs Details2ViewArgs;
		Details2ViewArgs.bUpdatesFromSelection = false;
		Details2ViewArgs.bLockable = false;
		Details2ViewArgs.bAllowSearch = false;
		Details2ViewArgs.bShowOptions = false;
		Details2ViewArgs.bAllowFavoriteSystem = false;
		Details2ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		Details2ViewArgs.ViewIdentifier = "ToolsProjectSettings";

		Details2View = PropertyEditor.CreateDetailView(Details2ViewArgs);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(4, 4, 4, 4)
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					Details1View.ToSharedRef()
				]
				+SScrollBox::Slot()
				[
					Details2View.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(10.f)
			[
				SNew(STextBlock)
				.Text(this, &SMasterSequenceSettings::GetMasterSequenceFullPath)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5.f)
			[
				SAssignNew(ErrorText, STextBlock)
				.Text(this, &SMasterSequenceSettings::GetErrorText)
				.TextStyle(FEditorStyle::Get(), TEXT("Log.Warning"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10, 5))
				.Text(LOCTEXT("CreateMasterSequence", "Create Master Sequence"))
				.OnClicked(this, &SMasterSequenceSettings::OnCreateMasterSequence)
			]
		];

		if (InArgs._MasterSequenceSettings)
		{
			SetMasterSequenceSettings(InArgs._MasterSequenceSettings);
		}
		if (InArgs._ToolsProjectSettings)
		{
			SetToolsProjectSettings(InArgs._ToolsProjectSettings);
		}
	}

	void SetMasterSequenceSettings(ULevelSequenceMasterSequenceSettings* InMasterSequenceSettings)
	{
		MasterSequenceSettings = InMasterSequenceSettings;

		Details1View->SetObject(InMasterSequenceSettings);
	}

	void SetToolsProjectSettings(UMovieSceneToolsProjectSettings* InToolsProjectSettings)
	{
		ToolsProjectSettings = InToolsProjectSettings;

		Details2View->SetObject(InToolsProjectSettings);
	}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject(MasterSequenceSettings);
		Collector.AddReferencedObject(ToolsProjectSettings);
	}

private:

	FText GetMasterSequenceFullPath() const
	{
		const ULevelSequenceMasterSequenceSettings* LevelSequenceSettings = GetDefault<ULevelSequenceMasterSequenceSettings>();
		FString FullPath = LevelSequenceSettings->MasterSequenceBasePath.Path;
		FullPath /= LevelSequenceSettings->MasterSequenceName;
		FullPath /= LevelSequenceSettings->MasterSequenceName;
		FullPath += LevelSequenceSettings->MasterSequenceSuffix;
		FullPath += TEXT(".uasset");
		return FText::FromString(FullPath);
	}

	FText GetErrorText() const
	{			
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		const ULevelSequenceMasterSequenceSettings* LevelSequenceSettings = GetDefault<ULevelSequenceMasterSequenceSettings>();
		FString FullPath = LevelSequenceSettings->MasterSequenceBasePath.Path;
		FullPath /= LevelSequenceSettings->MasterSequenceName;
		FullPath /= LevelSequenceSettings->MasterSequenceName;
		FullPath += LevelSequenceSettings->MasterSequenceSuffix;
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*FullPath));
		if (AssetData.IsValid())
		{
			return LOCTEXT("MasterSequenceExists", "Warning: Master Sequence Exists");
		}

		return FText::GetEmpty();
	}


	FReply OnCreateMasterSequence()
	{
		const ULevelSequenceMasterSequenceSettings* LevelSequenceSettings = GetDefault<ULevelSequenceMasterSequenceSettings>();
		
		// Create a master sequence
		FString MasterSequenceAssetName = LevelSequenceSettings->MasterSequenceName;
		FString MasterSequencePackagePath = LevelSequenceSettings->MasterSequenceBasePath.Path;
		MasterSequencePackagePath /= MasterSequenceAssetName;
		MasterSequenceAssetName += LevelSequenceSettings->MasterSequenceSuffix;

		UObject* MasterSequenceAsset = LevelSequenceEditorHelpers::CreateLevelSequenceAsset(MasterSequenceAssetName, MasterSequencePackagePath);
		if (MasterSequenceAsset)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(MasterSequenceAsset);
		
			ILevelSequenceEditorModule& LevelSequenceEditorModule = FModuleManager::LoadModuleChecked<ILevelSequenceEditorModule>("LevelSequenceEditor");		
			LevelSequenceEditorModule.OnMasterSequenceCreated().Broadcast(MasterSequenceAsset);
			
			MasterSequenceSettingsWindow.Pin()->RequestDestroyWindow();
		}

		return FReply::Handled();
	}

	TSharedPtr<IDetailsView> Details1View;
	TSharedPtr<IDetailsView> Details2View;
	TSharedPtr<SEditableTextBox> MasterSequencePathText;
	TSharedPtr<STextBlock> ErrorText;
	ULevelSequenceMasterSequenceSettings* MasterSequenceSettings;
	UMovieSceneToolsProjectSettings* ToolsProjectSettings;
};
	

void LevelSequenceEditorHelpers::OpenMasterSequenceDialog(const TSharedRef<FTabManager>& TabManager)
{
	TSharedPtr<SWindow> ExistingWindow = MasterSequenceSettingsWindow.Pin();
	if (ExistingWindow.IsValid())
	{
		ExistingWindow->BringToFront();
	}
	else
	{
		ExistingWindow = SNew(SWindow)
			.Title( LOCTEXT("MasterSequenceSettingsTitle", "Master Sequence Settings") )
			.HasCloseButton(true)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.ClientSize(FVector2D(600, 600));

		TSharedPtr<SDockTab> OwnerTab = TabManager->GetOwnerTab();
		TSharedPtr<SWindow> RootWindow = OwnerTab.IsValid() ? OwnerTab->GetParentWindow() : TSharedPtr<SWindow>();
		if(RootWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(ExistingWindow.ToSharedRef(), RootWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(ExistingWindow.ToSharedRef());
		}
	}

	ULevelSequenceMasterSequenceSettings* MasterSequenceSettings = GetMutableDefault<ULevelSequenceMasterSequenceSettings>();
	UMovieSceneToolsProjectSettings* ToolsProjectSettings = GetMutableDefault<UMovieSceneToolsProjectSettings>();

	ExistingWindow->SetContent(
		SNew(SMasterSequenceSettings)
		.MasterSequenceSettings(MasterSequenceSettings)
		.ToolsProjectSettings(ToolsProjectSettings)
	);

	MasterSequenceSettingsWindow = ExistingWindow;
}

UObject* LevelSequenceEditorHelpers::CreateLevelSequenceAsset(const FString& AssetName, const FString& PackagePath, UObject* AssetToDuplicate)
{
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = nullptr;
	for (TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* CurrentClass = *It;
		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass())
			{
				if (AssetToDuplicate != nullptr)
				{
					NewAsset = AssetTools.DuplicateAsset(AssetName, PackagePath, AssetToDuplicate);
				}
				else
				{
					NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, ULevelSequence::StaticClass(), Factory);
				}
				break;
			}
		}
	}
	return NewAsset;
}

#undef LOCTEXT_NAMESPACE
