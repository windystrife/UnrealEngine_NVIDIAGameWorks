// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAdvancedPreviewDetailsTab.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/STextComboBox.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"

#include "AssetViewerSettings.h"

#include "AdvancedPreviewScene.h"
#include "IDetailRootObjectCustomization.h"

#define LOCTEXT_NAMESPACE "SPrettyPreview"

SAdvancedPreviewDetailsTab::SAdvancedPreviewDetailsTab()
{
	DefaultSettings = UAssetViewerSettings::Get();
	if (DefaultSettings)
	{
		RefreshDelegate = DefaultSettings->OnAssetViewerSettingsChanged().AddRaw(this, &SAdvancedPreviewDetailsTab::OnAssetViewerSettingsRefresh);
			
		AddRemoveProfileDelegate = DefaultSettings->OnAssetViewerProfileAddRemoved().AddLambda([this]() { this->Refresh(); });

		PostUndoDelegate = DefaultSettings->OnAssetViewerSettingsPostUndo().AddRaw(this, &SAdvancedPreviewDetailsTab::OnAssetViewerSettingsPostUndo);
	}

	PerProjectSettings = GetMutableDefault<UEditorPerProjectUserSettings>();

}

SAdvancedPreviewDetailsTab::~SAdvancedPreviewDetailsTab()
{
	DefaultSettings = UAssetViewerSettings::Get();
	if (DefaultSettings)
	{
		DefaultSettings->OnAssetViewerSettingsChanged().Remove(RefreshDelegate);
		DefaultSettings->OnAssetViewerProfileAddRemoved().Remove(AddRemoveProfileDelegate);
		DefaultSettings->OnAssetViewerSettingsPostUndo().Remove(PostUndoDelegate);
		DefaultSettings->Save();
	}
}

void SAdvancedPreviewDetailsTab::Construct(const FArguments& InArgs, const TSharedRef<FAdvancedPreviewScene>& InPreviewScene)
{
	PreviewScenePtr = InPreviewScene;
	DefaultSettings = UAssetViewerSettings::Get();
	AdditionalSettings = InArgs._AdditionalSettings;
	ProfileIndex = PerProjectSettings->AssetViewerProfileIndex;
	DetailCustomizations = InArgs._DetailCustomizations;
	PropertyTypeCustomizations = InArgs._PropertyTypeCustomizations;

	CreateSettingsView();

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(2.0f, 1.0f, 2.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SettingsView->AsShared()
			]
		]

		+SVerticalBox::Slot()		
		.Padding(2.0f, 1.0f, 2.0f, 1.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				.ToolTipText(LOCTEXT("SceneProfileComboBoxToolTip", "Allows for switching between scene environment and lighting profiles."))
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SceneProfileSettingsLabel", "Profile"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				[
					SAssignNew(ProfileComboBox, STextComboBox)
					.OptionsSource(&ProfileNames)
					.OnSelectionChanged(this, &SAdvancedPreviewDetailsTab::ComboBoxSelectionChanged)				
					.IsEnabled_Lambda([this]() -> bool
					{
						return ProfileNames.Num() > 1;
					})
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SAdvancedPreviewDetailsTab::AddProfileButtonClick)
				.Text(LOCTEXT("AddProfileButton", "Add Profile"))
				.ToolTipText(LOCTEXT("SceneProfileAddProfile", "Adds a new profile."))
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SAdvancedPreviewDetailsTab::RemoveProfileButtonClick)
				.Text(LOCTEXT("RemoveProfileButton", "Remove Profile"))
				.ToolTipText(LOCTEXT("SceneProfileRemoveProfile", "Removes the currently selected profile."))
				.IsEnabled_Lambda([this]()->bool
				{
					return ProfileNames.Num() > 1; 
				})
			]
		]
	];

	UpdateProfileNames();
	UpdateSettingsView();
}

void SAdvancedPreviewDetailsTab::ComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/)
{
	int32 NewSelectionIndex;

	if (ProfileNames.Find(NewSelection, NewSelectionIndex))
	{
		ProfileIndex = NewSelectionIndex;
		PerProjectSettings->AssetViewerProfileIndex = ProfileIndex;
		UpdateSettingsView();	
		PreviewScenePtr.Pin()->SetProfileIndex(ProfileIndex);
	}
}

void SAdvancedPreviewDetailsTab::UpdateSettingsView()
{	
	TArray<UObject*> Objects;
	if (AdditionalSettings)
	{
		Objects.Add(AdditionalSettings);
	}
	Objects.Add(DefaultSettings);

	SettingsView->SetObjects(Objects, true);
}

void SAdvancedPreviewDetailsTab::UpdateProfileNames()
{
	checkf(DefaultSettings->Profiles.Num(), TEXT("There should always be at least one profile available"));
	ProfileNames.Empty();
	for (FPreviewSceneProfile& Profile : DefaultSettings->Profiles)
	{
		ProfileNames.Add(TSharedPtr<FString>(new FString(Profile.ProfileName + (Profile.bSharedProfile ? TEXT(" (Shared)") : TEXT("") ))));
	}

	ProfileComboBox->RefreshOptions();
	ProfileComboBox->SetSelectedItem(ProfileNames[ProfileIndex]);
}

FReply SAdvancedPreviewDetailsTab::AddProfileButtonClick()
{
	const FScopedTransaction Transaction(LOCTEXT("AddSceneProfile", "Adding Preview Scene Profile"));
	DefaultSettings->Modify();

	// Add new profile to settings instance
	DefaultSettings->Profiles.AddDefaulted();
	FPreviewSceneProfile& NewProfile = DefaultSettings->Profiles.Last();
	
	// Try to create a valid profile name when one is added
	bool bFoundValidName = false;
	int ProfileAppendNum = DefaultSettings->Profiles.Num();
	FString NewProfileName;
	while (!bFoundValidName)
	{
		NewProfileName = FString::Printf(TEXT("Profile_%i"), ProfileAppendNum);

		bool bValidName = true;
		for (const FPreviewSceneProfile& Profile : DefaultSettings->Profiles)
		{
			if (Profile.ProfileName == NewProfileName)
			{
				bValidName = false;				
				break;
			}
		}

		if (!bValidName)
		{
			++ProfileAppendNum;
		}

		bFoundValidName = bValidName;
	}

	NewProfile.ProfileName = NewProfileName;
	DefaultSettings->PostEditChange();

	// Change selection to new profile so the user directly sees the profile that was added
	Refresh();
	ProfileComboBox->SetSelectedItem(ProfileNames.Last());
	
	return FReply::Handled();
}

FReply SAdvancedPreviewDetailsTab::RemoveProfileButtonClick()
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveSceneProfile", "Remove Preview Scene Profile"));
	DefaultSettings->Modify();

	// Remove currently selected profile 
	DefaultSettings->Profiles.RemoveAt(ProfileIndex);
	ProfileIndex = DefaultSettings->Profiles.IsValidIndex(ProfileIndex - 1 ) ? ProfileIndex - 1 : 0;
	PerProjectSettings->AssetViewerProfileIndex = ProfileIndex;
	DefaultSettings->PostEditChange();
	
	return FReply::Handled();
}

void SAdvancedPreviewDetailsTab::OnAssetViewerSettingsRefresh(const FName& InPropertyName)
{
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, ProfileName) || InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bSharedProfile))
	{
		UpdateProfileNames();
	}
}

void SAdvancedPreviewDetailsTab::CreateSettingsView()
{	
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ nullptr,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);

	for (const FAdvancedPreviewSceneModule::FDetailCustomizationInfo& DetailCustomizationInfo : DetailCustomizations)
	{
		SettingsView->RegisterInstancedCustomPropertyLayout(DetailCustomizationInfo.Struct, DetailCustomizationInfo.OnGetDetailCustomizationInstance);
	}

	for (const FAdvancedPreviewSceneModule::FPropertyTypeCustomizationInfo& PropertyTypeCustomizationInfo : PropertyTypeCustomizations)
	{
		SettingsView->RegisterInstancedCustomPropertyTypeLayout(PropertyTypeCustomizationInfo.StructName, PropertyTypeCustomizationInfo.OnGetPropertyTypeCustomizationInstance);
	}

	class FDetailRootObjectCustomization : public IDetailRootObjectCustomization
	{
		virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) { return SNullWidget::NullWidget; }
		virtual bool IsObjectVisible(const UObject* InRootObject) const { return true; }
		virtual bool ShouldDisplayHeader(const UObject* InRootObject) const { return false; }
	};

	SettingsView->SetRootObjectCustomizationInstance(MakeShareable(new FDetailRootObjectCustomization));

	UpdateSettingsView();
}

void SAdvancedPreviewDetailsTab::Refresh()
{	
	PerProjectSettings->AssetViewerProfileIndex = DefaultSettings->Profiles.IsValidIndex(PerProjectSettings->AssetViewerProfileIndex) ? PerProjectSettings->AssetViewerProfileIndex : 0;
	ProfileIndex = PerProjectSettings->AssetViewerProfileIndex;
	UpdateProfileNames();
	PreviewScenePtr.Pin()->SetProfileIndex(ProfileIndex);
	UpdateSettingsView();
}

void SAdvancedPreviewDetailsTab::OnAssetViewerSettingsPostUndo()
{
	Refresh();
	PreviewScenePtr.Pin()->UpdateScene(DefaultSettings->Profiles[ProfileIndex]);
}

#undef LOCTEXT_NAMESPACE