// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "AssetData.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "PreviewScene.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "EditorAnimUtils.h"

class UAnimSet;
class USkeletalMesh;

using namespace EditorAnimUtils;

/**
 * This below code is to select skeleton from the list 
 */
#define LOCTEXT_NAMESPACE "SkeletonWidget" 

struct FBoneTrackPair 
{
	FName Bone1;
	FName Bone2;

	FBoneTrackPair(FName InBone1, FName InBone2)
		: Bone1(InBone1)
		, Bone2(InBone2)
	{
	}
};

class SBonePairRow : public SMultiColumnTableRow< TSharedPtr<FBoneTrackPair> >
{
public:
	SLATE_BEGIN_ARGS(SBonePairRow){}
		SLATE_ARGUMENT( TSharedPtr<FBoneTrackPair>, BonePair)
	SLATE_END_ARGS()

	/**
	 * Construct child widgets that comprise this widget.
	 *
	 * @param InArgs  Declaration from which to construct this widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		this->BonePair = InArgs._BonePair;

		SMultiColumnTableRow< TSharedPtr<FBoneTrackPair> >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		TSharedPtr<SBorder> Border1, Border2;

		if (ColumnName == TEXT("Curretly Selected"))
		{
			return SAssignNew(Border1, SBorder) .Padding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromName(BonePair->Bone1))
				];
		}
		else 
		{
			if (BonePair->Bone2 == NAME_None)
			{
				return SAssignNew(Border2, SBorder) .Padding(2)
					.ColorAndOpacity(FLinearColor(1.f, 0.f, 0.f))
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MissingBone", "Missing"))
					];
			}
			else
			{
				return SAssignNew(Border2, SBorder) .Padding(2)
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromName(BonePair->Bone2))
					];
			}
		}
	}
private:
	TSharedPtr<FBoneTrackPair> BonePair;
};

class SSkeletonWidget : public SCompoundWidget
{
public:

	USkeleton* GetSelectedSkeleton() const
	{ 
		return CurSelectedSkeleton; 
	}
protected:
	USkeleton* CurSelectedSkeleton;
};

/** 1 columns - just show bone list **/
class SSkeletonListWidget : public SSkeletonWidget
{
public:
	SLATE_BEGIN_ARGS( SSkeletonListWidget ){}
	SLATE_END_ARGS()
public:
	// WIDGETS

	void Construct(const FArguments& InArgs);

	void SkeletonSelectionChanged(const FAssetData& AssetData);

	TSharedRef<ITableRow> GenerateSkeletonRow( USkeleton* InSkeleton, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return
			SNew( STableRow< USkeleton* >, OwnerTable )
			. Content()
			[
				SNew(STextBlock)
				.Text(FText::FromString(InSkeleton->GetFullName()))
			];
	}


	TSharedRef<ITableRow> GenerateSkeletonBoneRow( TSharedPtr<FName> InBoneName, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return
			SNew( STableRow< TSharedPtr<FName> >, OwnerTable )
			. Content()
			[
				SNew(STextBlock)
				.Text(FText::FromName(*InBoneName))
			];
	}

private:
	SVerticalBox::FSlot* BoneListSlot;
	TArray< TSharedPtr<FName> > BoneList;
};

/** 2 columns - bone pair widget **/
class SSkeletonCompareWidget : public SSkeletonWidget
{
public:
	SLATE_BEGIN_ARGS( SSkeletonCompareWidget )
		: _Object(NULL)
		{}

		SLATE_ARGUMENT( UObject*, Object )
		SLATE_ARGUMENT( TArray<FName>*, BoneNames )
	SLATE_END_ARGS()
public:
	// WIDGETS

	void Construct(const FArguments& InArgs);

	void SkeletonSelectionChanged(const FAssetData& AssetData);

	TSharedRef<ITableRow> GenerateBonePairRow( TSharedPtr<FBoneTrackPair> InBonePair, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return
			SNew( SBonePairRow, OwnerTable )
			.BonePair(InBonePair);
	}

private:
	TArray<FName>	BoneNames;
	SVerticalBox::FSlot * BonePairSlot;
	TArray< TSharedPtr<FBoneTrackPair> >	BonePairList;
};

class SSkeletonSelectorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSkeletonSelectorWindow )
		: _Object(NULL)
		{}

		SLATE_ARGUMENT( UObject*, Object )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
	SLATE_END_ARGS()
public:
	UNREALED_API void Construct(const FArguments& InArgs);

	void ConstructWindowFromAnimSet(UAnimSet* InAnimSet);

	void ConstructWindowFromMesh(USkeletalMesh* InSkeletalMesh);

	void ConstructWindow();

	void ConstructButtons(TSharedRef<SVerticalBox> ParentBox)
	{
		ParentBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+SUniformGridPanel::Slot(0,0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Accept", "Accept"))
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked_Raw( this, &SSkeletonSelectorWindow::OnAccept )
			]
			+SUniformGridPanel::Slot(1,0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked_Raw( this, &SSkeletonSelectorWindow::OnCancel )
			]
		];
	}

	FReply OnAccept()
	{
		SelectedSkeleton = SkeletonWidget->GetSelectedSkeleton();

		if (SelectedSkeleton!=NULL && WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	USkeleton* GetSelectedSkeleton()
	{
		return SelectedSkeleton;
	}

private:
	TSharedPtr<SSkeletonWidget>			SkeletonWidget;
	TWeakPtr<SWindow>					WidgetWindow;
	USkeleton *							SelectedSkeleton;
};


//////////////////////////
class SBasePoseViewport: public /*SCompoundWidget*/SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SBasePoseViewport)
	{}

	SLATE_ARGUMENT(USkeleton*, Skeleton)
SLATE_END_ARGS()

public:
	SBasePoseViewport();

	void Construct(const FArguments& InArgs);
	void SetSkeleton(USkeleton* Skeleton);

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

private:
	/** Skeleton */
	USkeleton* TargetSkeleton;

	FPreviewScene PreviewScene;

	class UDebugSkelMeshComponent* PreviewComponent;

	bool IsVisible() const override;
};

/////////////////////////////////////////////
/** 
 * Slate panel for choose Skeleton for assets to relink
 */

DECLARE_DELEGATE_SixParams(FOnRetargetAnimation, USkeleton* /*OldSkeleton*/, USkeleton* /*NewSkeleton*/, bool /*bRemapReferencedAssets*/, bool /*bAllowRemapToExisting*/, bool /*bConvertSpaces*/, const FNameDuplicationRule* /*NameRule*/)

class SAnimationRemapSkeleton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimationRemapSkeleton)
		: _CurrentSkeleton(NULL)
		, _WidgetWindow(NULL)
		, _WarningMessage()
		, _ShowRemapOption(false)
		, _ShowExistingRemapOption(false)
		{}

		/** The anim sequences to compress */
		SLATE_ARGUMENT( USkeleton*, CurrentSkeleton )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( FText, WarningMessage )
		SLATE_ARGUMENT( bool, ShowRemapOption )
		SLATE_ARGUMENT( bool, ShowExistingRemapOption )
		SLATE_ARGUMENT( bool, ShowConvertSpacesOption )
		SLATE_ARGUMENT( bool, ShowCompatibleDisplayOption )
		SLATE_ARGUMENT( bool, ShowDuplicateAssetOption )
		SLATE_EVENT(FOnRetargetAnimation, OnRetargetDelegate)
	SLATE_END_ARGS()	

	/**
	 * Constructs this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Old Skeleton that was mapped 
	 * This data is needed to prevent users from selecting same skeleton
	 */
	USkeleton *OldSkeleton;
	/**
	 * New Skeleton that they would like to map to
	 */
	USkeleton *NewSkeleton;

private:
	/**
	 * Whether we are remapping assets that are referenced by the assets the user selects to remap
	 */
	bool bRemapReferencedAssets;

	/**
	 * Whether we allow remapping to existing assets for the new skeleton
	 */
	bool bAllowRemappingToExistingAssets;

	/**
	 * Whether we are remapping assets that are referenced by the assets the user selects to remap
	 */
	bool bConvertSpaces;

	/**
	 * Whether to show skeletons with the same rig set up
	*/
	bool bShowOnlyCompatibleSkeletons;

	TSharedPtr<SBasePoseViewport> SourceViewport;
	TSharedPtr<SBasePoseViewport> TargetViewport;

	TSharedPtr<SBox> AssetPickerBox;

	TWeakPtr<SWindow> WidgetWindow;

	FOnRetargetAnimation OnRetargetAnimationDelegate;

	/** Handlers for check box for remapping assets option */
	ECheckBoxState IsRemappingReferencedAssets() const;
	void OnRemappingReferencedAssetsChanged(ECheckBoxState InNewRadioState);

	/** Handlers for check box for remapping to existing assets */
	ECheckBoxState IsRemappingToExistingAssetsChecked() const;
	bool IsRemappingToExistingAssetsEnabled() const;
	void OnRemappingToExistingAssetsChanged(ECheckBoxState InNewRadioState);

	/** Handlers for check box for converting spaces*/
	ECheckBoxState IsConvertSpacesChecked() const;
	void OnConvertSpacesCheckChanged(ECheckBoxState InNewRadioState);

	/** Handlers for check box for converting spaces*/
	ECheckBoxState IsShowOnlyCompatibleSkeletonsChecked() const;
	bool IsShowOnlyCompatibleSkeletonsEnabled() const;
	void OnShowOnlyCompatibleSkeletonsCheckChanged(ECheckBoxState InNewRadioState);

	/** should filter asset */
	bool OnShouldFilterAsset(const struct FAssetData& AssetData);

	/**
	 * return true if it can apply 
	 */
	bool CanApply() const;

	FReply OnApply();
	FReply OnCancel();

	void CloseWindow();

	/** Handler for dialog window close button */
	void OnRemapDialogClosed(const TSharedRef<SWindow>& Window);

	/**
	 * Handler for when asset is selected
	 */
	void OnAssetSelectedFromPicker(const FAssetData& AssetData);

	/*
	* Refreshes asset picker - call when asset picker option changes
	*/
	void UpdateAssetPicker();

	/*
	 * Duplicate Name Rule 
	 */
	FNameDuplicationRule NameDuplicateRule;
	FText ExampleText;

	FText GetPrefixName() const
	{
		return FText::FromString(NameDuplicateRule.Prefix);
	}

	void SetPrefixName(const FText &InText)
	{
		NameDuplicateRule.Prefix = InText.ToString();
		UpdateExampleText();
	}

	FText GetSuffixName() const
	{
		return FText::FromString(NameDuplicateRule.Suffix);
	}

	void SetSuffixName(const FText &InText)
	{
		NameDuplicateRule.Suffix = InText.ToString();
		UpdateExampleText();
	}

	FText GetReplaceFrom() const
	{
		return FText::FromString(NameDuplicateRule.ReplaceFrom);
	}

	void SetReplaceFrom(const FText &InText)
	{
		NameDuplicateRule.ReplaceFrom = InText.ToString();
		UpdateExampleText();
	}

	FText GetReplaceTo() const
	{
		return FText::FromString(NameDuplicateRule.ReplaceTo);
	}

	void SetReplaceTo(const FText &InText)
	{
		NameDuplicateRule.ReplaceTo = InText.ToString();
		UpdateExampleText();
	}

	FText GetExampleText() const
	{
		return ExampleText;
	}

	void UpdateExampleText();

	FReply ShowFolderOption();
	FText GetFolderPath() const
	{
		return FText::FromString(NameDuplicateRule.FolderPath);
	}

public:

	/**
	 *  Show window
	 *
	 * @param OldSkeleton		Old Skeleton to change from
	 *
	 * @return true if successfully selected new skeleton
	 */
	static UNREALED_API void ShowWindow(USkeleton* OldSkeleton, const FText& WarningMessage, bool bDuplicateAssets, FOnRetargetAnimation RetargetDelegate);

	static TSharedPtr<SWindow> DialogWindow;

	bool bShowDuplicateAssetOption;
};

//////////////////////////////////////////////////////////////////////////

class FDisplayedAssetEntryInfo
{
public:
	USkeleton* NewSkeleton;
	UObject* AnimAsset;
	UObject* RemapAsset;

	static TSharedRef<FDisplayedAssetEntryInfo> Make(UObject* InAsset, USkeleton* InNewSkeleton);

protected:

	FDisplayedAssetEntryInfo(){};
	FDisplayedAssetEntryInfo(UObject* InAsset, USkeleton* InNewSkeleton);
};

class SAssetEntryRow : public SMultiColumnTableRow<TSharedPtr<FDisplayedAssetEntryInfo>>
{
public:
	SLATE_BEGIN_ARGS(SAssetEntryRow)
	{}

		SLATE_ARGUMENT(TSharedPtr<FDisplayedAssetEntryInfo>, DisplayedInfo)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

protected:

	TSharedRef<SWidget> GetRemapMenuContent();
	FText GetRemapMenuButtonText() const;
	void OnAssetSelected(const FAssetData& AssetData);
	bool OnShouldFilterAsset(const FAssetData& AssetData) const;

	TSharedPtr<FDisplayedAssetEntryInfo> DisplayedInfo;

	TWeakObjectPtr<UObject> RemapAsset;

	FString SkeletonExportName;
};

typedef SListView<TSharedPtr<FDisplayedAssetEntryInfo>> SRemapAssetEntryList;

class SAnimationRemapAssets : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimationRemapAssets)
		: _NewSkeleton(nullptr)
		, _RetargetContext(nullptr)
	{}

		SLATE_ARGUMENT(USkeleton*, NewSkeleton)
		SLATE_ARGUMENT(FAnimationRetargetContext*, RetargetContext)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)

	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

	static UNREALED_API void ShowWindow(FAnimationRetargetContext& RetargetContext, USkeleton* RetargetToSkeleton);
	static TSharedPtr<SWindow> DialogWindow;

private:

	TSharedRef<ITableRow> OnGenerateMontageReferenceRow( TSharedPtr<FDisplayedAssetEntryInfo> Item, const TSharedRef<STableViewBase>& OwnerTable );

	void OnDialogClosed(const TSharedRef<SWindow>& Window);

	/** Button Handlers */
	FReply OnOkClicked();
	FReply OnBestGuessClicked();

	/** Best guess functions to try and match asset names */
	const FAssetData* FindBestGuessMatch(const FAssetData& AssetName, const TArray<FAssetData>& PossibleAssets) const;

	/** The retargetting context we're managing*/
	FAnimationRetargetContext* RetargetContext;

	/** List of entries to the remap list*/
	TArray<TSharedPtr<FDisplayedAssetEntryInfo>> AssetListInfo;

	/** Skeleton we're about to retarget to*/
	USkeleton* NewSkeleton;

	/** The list viewing AssetListInfo*/
	TSharedPtr<SRemapAssetEntryList> ListWidget;
};

////////////////////////////////////////////////////
/** 
* FDlgRemapSkeleton
* 
* Wrapper class for SAnimationRemapSkeleton. This class creates and launches a dialog then awaits the
* result to return to the user. 
*/
class UNREALED_API FDlgRemapSkeleton 
{
public:
	FDlgRemapSkeleton( USkeleton* Skeleton );

	/**  Shows the dialog box and waits for the user to respond. */
	bool ShowModal();

	/** New Skeleton that is chosen **/
	USkeleton* NewSkeleton;

	/** true if you'd like to retarget skeletal meshes as well **/
	bool RetargetSkeletalMesh;

private:

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> DialogWindow;

	/** Cached pointer to the LOD Chain widget */
	TSharedPtr<class SAnimationRemapSkeleton> DialogWidget;
};


class SRemapFailures : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRemapFailures){}
		SLATE_ARGUMENT(TArray<FText>, FailedRemaps)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
	static UNREALED_API void OpenRemapFailuresDialog(const TArray<FText>& InFailedRemaps);

private:
	TSharedRef<ITableRow> MakeListViewWidget(TSharedRef<FText> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply CloseClicked();

private:
	TArray< TSharedRef<FText> > FailedRemaps;
};

/////////////////////////////////////////////
/** 
 * Slate panel for choose displaying bones to remove
 */
class SSkeletonBoneRemoval : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSkeletonBoneRemoval){}

		/** The bones to remove (for list display) */
		SLATE_ARGUMENT( TArray<FName>, BonesToRemove )

		/** The window this panel has been placed in */
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )

		/** Message to display to the user */
		SLATE_ARGUMENT( FText, WarningMessage )

	SLATE_END_ARGS()	

	/**
	 * Constructs this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Reference to our window */
	TWeakPtr<SWindow> WidgetWindow;

	/** Button Handlers */
	FReply OnOk();
	FReply OnCancel();

	/** Handle closing to dialog window */
	void CloseWindow();

	/** Create an individual row for the bone name list */
	TSharedRef<ITableRow> GenerateSkeletonBoneRow( TSharedPtr<FName> InBoneName, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return
		SNew( STableRow< TSharedPtr<FName> >, OwnerTable )
		. Content()
		[
			SNew(STextBlock)
			.Text(FText::FromName(*InBoneName))
		];
	}

	/**
	 *  Show Modal window
	 *
	 * @param BonesToRemove		List of bones that will be removed
	 * @param WarningMessage	Message to display to the user so they know what is going on
	 *
	 * @return true if successfully selected new skeleton
	 */
	static UNREALED_API bool ShowModal(const TArray<FName> BonesToRemove, const FText& WarningMessage);

	/** Did the user choose to continue */
	bool bShouldContinue;

	/** List of bone names that will be removed */
	TArray< TSharedPtr<FName> > BoneNames;
};

class SSelectFolderDlg: public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSelectFolderDlg)
	{
	}

	SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

		SSelectFolderDlg()
		: UserResponse(EAppReturnType::Cancel)
	{
		}

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

protected:
	void OnPathChange(const FString& NewPath);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	EAppReturnType::Type UserResponse;
	FText AssetPath;
};

#undef LOCTEXT_NAMESPACE
