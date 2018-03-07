// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAssetFamilyShortcutBar.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/AssetEditorManager.h"
#include "EditorStyleSet.h"
#include "IAssetFamily.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SAssetFamilyShortcutBar"

namespace AssetShortcutConstants
{
	const int32 ThumbnailSize = 40;
	const int32 ThumbnailSizeSmall = 16;
}

class SAssetShortcut : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetShortcut)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily, const FAssetData& InAssetData, const TSharedRef<FAssetThumbnailPool>& InThumbnailPool)
	{
		AssetData = InAssetData;
		AssetFamily = InAssetFamily;
		HostingApp = InHostingApp;
		ThumbnailPoolPtr = InThumbnailPool;
		bPackageDirty = false;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &SAssetShortcut::HandleFilesLoaded);
		AssetRegistryModule.Get().OnAssetAdded().AddSP(this, &SAssetShortcut::HandleAssetAdded);
		AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &SAssetShortcut::HandleAssetRemoved);
		AssetRegistryModule.Get().OnAssetRenamed().AddSP(this, &SAssetShortcut::HandleAssetRenamed);

		FAssetEditorManager::Get().OnAssetEditorRequestedOpen().AddSP(this, &SAssetShortcut::HandleAssetOpened);
		AssetFamily->GetOnAssetOpened().AddSP(this, &SAssetShortcut::HandleAssetOpened);

		AssetThumbnail = MakeShareable(new FAssetThumbnail(InAssetData, AssetShortcutConstants::ThumbnailSize, AssetShortcutConstants::ThumbnailSize, InThumbnailPool));
		AssetThumbnailSmall = MakeShareable(new FAssetThumbnail(InAssetData, AssetShortcutConstants::ThumbnailSizeSmall, AssetShortcutConstants::ThumbnailSizeSmall, InThumbnailPool));

		TArray<FAssetData> Assets;
		InAssetFamily->FindAssetsOfType(InAssetData.GetClass(), Assets);
		bMultipleAssetsExist = Assets.Num() > 1;
		AssetDirtyBrush = FEditorStyle::GetBrush("ContentBrowser.ContentDirty");

		ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SAssignNew(CheckBox, SCheckBox)
				.Style(FEditorStyle::Get(), "ToolBar.ToggleButton")
				.ForegroundColor(FSlateColor::UseForeground())
				.Padding(0.0f)
				.OnCheckStateChanged(this, &SAssetShortcut::HandleOpenAssetShortcut)
				.IsChecked(this, &SAssetShortcut::GetCheckState)
				.Visibility(this, &SAssetShortcut::GetButtonVisibility)
				.ToolTipText(this, &SAssetShortcut::GetButtonTooltip)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBorder)
						.Padding(4.0f)
						.BorderImage(FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow"))
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							[
								SAssignNew(ThumbnailBox, SBox)
								.WidthOverride(AssetShortcutConstants::ThumbnailSize)
								.HeightOverride(AssetShortcutConstants::ThumbnailSize)
								.Visibility(this, &SAssetShortcut::GetThumbnailVisibility)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										AssetThumbnail->MakeThumbnailWidget()
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Bottom)
									[
										SNew(SImage)
										.Image(this, &SAssetShortcut::GetDirtyImage)
									]
								]
							]
							+SHorizontalBox::Slot()
							[
								SAssignNew(ThumbnailSmallBox, SBox)
								.WidthOverride(AssetShortcutConstants::ThumbnailSizeSmall)
								.HeightOverride(AssetShortcutConstants::ThumbnailSizeSmall)
								.Visibility(this, &SAssetShortcut::GetSmallThumbnailVisibility)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										AssetThumbnailSmall->MakeThumbnailWidget()
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Bottom)
									[
										SNew(SImage)
										.Image(this, &SAssetShortcut::GetDirtyImage)
									]
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f)
						[
							SNew(STextBlock)
							.Text(this, &SAssetShortcut::GetAssetText)
							.TextStyle(FEditorStyle::Get(), "Toolbar.Label")
							.ShadowOffset(FVector2D::UnitVector)
						]
					]
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.AutoWidth()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.Visibility(this, &SAssetShortcut::GetComboVisibility)
				.ContentPadding(0)
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "Toolbar.Button")
				.OnGetMenuContent(this, &SAssetShortcut::HandleGetMenuContent)
				.ToolTipText(LOCTEXT("AssetComboTooltip", "Find other assets of this type and perform asset operations./nShift-Click to open in new window."))
			]
		];

		EnableToolTipForceField(true);

		DirtyStateTimerHandle = RegisterActiveTimer(1.0f / 10.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SAssetShortcut::HandleRefreshDirtyState));
	}

	~SAssetShortcut()
	{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);
			AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
			AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
			AssetRegistryModule.Get().OnAssetRenamed().RemoveAll(this);
		}

		AssetFamily->GetOnAssetOpened().RemoveAll(this);
		FAssetEditorManager::Get().OnAssetEditorRequestedOpen().RemoveAll(this);
		UnRegisterActiveTimer(DirtyStateTimerHandle.ToSharedRef());
	}

	void HandleOpenAssetShortcut(ECheckBoxState InState)
	{
		if(AssetData.IsValid())
		{
			TArray<UObject*> Assets;
			Assets.Add(AssetData.GetAsset());
			FAssetEditorManager::Get().OpenEditorForAssets(Assets);
		}
	}

	FText GetAssetText() const
	{
		return AssetFamily->GetAssetTypeDisplayName(AssetData.GetClass());
	}

	ECheckBoxState GetCheckState() const
	{
		const TArray<UObject*>* Objects = HostingApp.Pin()->GetObjectsCurrentlyBeingEdited();
		if (Objects != nullptr)
		{
			for (UObject* Object : *Objects)
			{
				if (FAssetData(Object) == AssetData)
				{
					return ECheckBoxState::Checked;
				}
			}
		}
		return ECheckBoxState::Unchecked;
	}

	FSlateColor GetAssetTextColor() const
	{
		static const FName InvertedForeground("InvertedForeground");
		return GetCheckState() == ECheckBoxState::Checked || CheckBox->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForeground) : FSlateColor::UseForeground();
	}

	TSharedRef<SWidget> HandleGetMenuContent()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr);

		MenuBuilder.BeginSection("AssetActions", LOCTEXT("AssetActionsSection", "Asset Actions"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowInContentBrowser", "Show In Content Browser"),
				LOCTEXT("ShowInContentBrowser_ToolTip", "Show this asset in the content browser."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "PropertyWindow.Button_Browse"),
				FUIAction(FExecuteAction::CreateSP(this, &SAssetShortcut::HandleShowInContentBrowser)));
		}
		MenuBuilder.EndSection();

		if (bMultipleAssetsExist)
		{
			MenuBuilder.BeginSection("AssetSelection", LOCTEXT("AssetSelectionSection", "Select Asset"));
			{
				FAssetPickerConfig AssetPickerConfig;

				UClass* FilterClass = AssetFamily->GetAssetFamilyClass(AssetData.GetClass());
				if (FilterClass != nullptr)
				{
					AssetPickerConfig.Filter.ClassNames.Add(FilterClass->GetFName());
					AssetPickerConfig.Filter.bRecursiveClasses = true;
				}

				AssetPickerConfig.SelectionMode = ESelectionMode::SingleToggle;
				AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAssetShortcut::HandleAssetSelectedFromPicker);
				AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAssetShortcut::HandleFilterAsset);
				AssetPickerConfig.bAllowNullSelection = false;
				AssetPickerConfig.ThumbnailLabel = EThumbnailLabel::ClassName;
				AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
				AssetPickerConfig.InitialAssetSelection = AssetData;

				MenuBuilder.AddWidget(
					SNew(SBox)
					.WidthOverride(300)
					.HeightOverride(600)
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					],
					FText(), true);
			}
			MenuBuilder.EndSection();
		}
		
		return MenuBuilder.MakeWidget();
	}

	void HandleAssetSelectedFromPicker(const struct FAssetData& InAssetData)
	{
		FSlateApplication::Get().DismissAllMenus();

		if (InAssetData.IsValid())
		{
			TArray<UObject*> Assets;
			Assets.Add(InAssetData.GetAsset());
			FAssetEditorManager::Get().OpenEditorForAssets(Assets);
		}
		else if(AssetData.IsValid())
		{
			// Assume that as we are set to 'toggle' mode with no 'none' selection allowed, we are selecting the currently selected item
			TArray<UObject*> Assets;
			Assets.Add(AssetData.GetAsset());
			FAssetEditorManager::Get().OpenEditorForAssets(Assets);
		}
	}

	bool HandleFilterAsset(const struct FAssetData& InAssetData)
	{
		return !AssetFamily->IsAssetCompatible(InAssetData);
	}

	EVisibility GetButtonVisibility() const
	{
		return AssetData.IsValid() || bMultipleAssetsExist ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetComboVisibility() const
	{
		return bMultipleAssetsExist && AssetData.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	void HandleFilesLoaded()
	{
		TArray<FAssetData> Assets;
		AssetFamily->FindAssetsOfType(AssetData.GetClass(), Assets);
		bMultipleAssetsExist = Assets.Num() > 1;
	}

	void HandleAssetRemoved(const FAssetData& InAssetData)
	{
		if (AssetFamily->IsAssetCompatible(InAssetData))
		{
			TArray<FAssetData> Assets;
			AssetFamily->FindAssetsOfType(AssetData.GetClass(), Assets);
			bMultipleAssetsExist = Assets.Num() > 1;
		}
	}

	void HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
	{
		if (AssetFamily->IsAssetCompatible(InAssetData))
		{
			if(InOldObjectPath == AssetData.ObjectPath.ToString())
			{
				AssetData = InAssetData;

				RegenerateThumbnail();
			}
		}
	}

	void HandleAssetAdded(const FAssetData& InAssetData)
	{
		if (AssetFamily->IsAssetCompatible(InAssetData))
		{
			TArray<FAssetData> Assets;
			AssetFamily->FindAssetsOfType(AssetData.GetClass(), Assets);
			bMultipleAssetsExist = Assets.Num() > 1;
		}
	}

	void HandleShowInContentBrowser()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FAssetData> Assets;
		Assets.Add(AssetData);
		ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
	}

	void HandleAssetOpened(UObject* InAsset)
	{
		RefreshAsset();
	}

	EVisibility GetThumbnailVisibility() const
	{
		return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	EVisibility GetSmallThumbnailVisibility() const
	{
		return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetDirtyImage() const
	{
		return bPackageDirty ? AssetDirtyBrush : nullptr;
	}

	void RefreshAsset()
	{
		// if this is the asset being edited by our hosting asset editor, don't switch it
		bool bAssetBeingEdited = false;
		const TArray<UObject*>* Objects = HostingApp.Pin()->GetObjectsCurrentlyBeingEdited();
		if (Objects != nullptr)
		{
			for (UObject* Object : *Objects)
			{
				if (FAssetData(Object) == AssetData)
				{
					bAssetBeingEdited = true;
					break;
				}
			}
		}

		// switch to new asset if needed
		FAssetData NewAssetData = AssetFamily->FindAssetOfType(AssetData.GetClass());
		if (!bAssetBeingEdited && NewAssetData.IsValid() && NewAssetData != AssetData)
		{
			AssetData = NewAssetData;

			RegenerateThumbnail();
		}
	}

	void RegenerateThumbnail()
	{
		if(AssetData.IsValid())
		{
			AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, AssetShortcutConstants::ThumbnailSize, AssetShortcutConstants::ThumbnailSize, ThumbnailPoolPtr.Pin()));
			AssetThumbnailSmall = MakeShareable(new FAssetThumbnail(AssetData, AssetShortcutConstants::ThumbnailSizeSmall, AssetShortcutConstants::ThumbnailSizeSmall, ThumbnailPoolPtr.Pin()));

			ThumbnailBox->SetContent(AssetThumbnail->MakeThumbnailWidget());
			ThumbnailSmallBox->SetContent(AssetThumbnailSmall->MakeThumbnailWidget());
		}
	}

	EActiveTimerReturnType HandleRefreshDirtyState(double InCurrentTime, float InDeltaTime)
	{
		if (AssetData.IsAssetLoaded())
		{
			if (!AssetPackage.IsValid())
			{
				AssetPackage = AssetData.GetPackage();
			}

			if (AssetPackage.IsValid())
			{
				bPackageDirty = AssetPackage->IsDirty();
			}
		}

		return EActiveTimerReturnType::Continue;
	}

	FText GetButtonTooltip() const
	{
		return FText::Format(LOCTEXT("AssetTooltipFormat", "{0}\n{1}"), FText::FromName(AssetData.AssetName), FText::FromString(AssetData.GetFullName()));
	}

private:
	/** The current asset data for this widget */
	FAssetData AssetData;

	/** Cache the package of the object for checking dirty state */
	TWeakObjectPtr<UPackage> AssetPackage;

	/** Timer handle used to updating dirty state */
	TSharedPtr<class FActiveTimerHandle> DirtyStateTimerHandle;

	/** The asset family we are working with */
	TSharedPtr<class IAssetFamily> AssetFamily;

	/** Our asset thumbnails */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	TSharedPtr<FAssetThumbnail> AssetThumbnailSmall;

	/** Thumbnail widget containers */
	TSharedPtr<SBox> ThumbnailBox;
	TSharedPtr<SBox> ThumbnailSmallBox;

	/** The asset editor we are embedded in */
	TWeakPtr<class FWorkflowCentricApplication> HostingApp;

	/** Thumbnail pool */
	TWeakPtr<FAssetThumbnailPool> ThumbnailPoolPtr;

	/** Check box */
	TSharedPtr<SCheckBox> CheckBox;

	/** Cached dirty brush */
	const FSlateBrush* AssetDirtyBrush;

	/** Whether there are multiple (>1) of this asset type in existence */
	bool bMultipleAssetsExist;

	/** Cache the package's dirty state */
	bool bPackageDirty;
};

void SAssetFamilyShortcutBar::Construct(const FArguments& InArgs, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily)
{
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(16, false));

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	TArray<UClass*> AssetTypes;
	InAssetFamily->GetAssetTypes(AssetTypes);

	int32 AssetTypeIndex = 0;
	for (UClass* Class : AssetTypes)
	{
		FAssetData AssetData = InAssetFamily->FindAssetOfType(Class);
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, AssetTypeIndex == AssetTypes.Num() - 1 ? 0.0f: 2.0f, 0.0f)
		[
			SNew(SAssetShortcut, InHostingApp, InAssetFamily, AssetData, ThumbnailPool.ToSharedRef())
		];

		AssetTypeIndex++;
	}

	ChildSlot
	[
		HorizontalBox
	];
}

#undef LOCTEXT_NAMESPACE
