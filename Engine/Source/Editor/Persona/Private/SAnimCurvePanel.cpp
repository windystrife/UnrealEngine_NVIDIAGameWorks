// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimCurvePanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Layout/SSplitter.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "ScopedTransaction.h"
#include "SAnimCurveEd.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "IEditableSkeleton.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "AnimCurvePanel"

//////////////////////////////////////////////////////////////////////////
//  FAnimCurveInterface interface 

/** Interface you implement if you want the CurveEditor to be able to edit curves on you */
class FAnimCurveBaseInterface : public FCurveOwnerInterface
{
private:
	FAnimCurveBase*	CurveData;

	void UpdateNameInternal(FRawCurveTracks& RawCurveData, const SmartName::UID_Type& RequestedNameUID, FName RequestedName)
	{
		FAnimCurveBase* CurrentCurveData = RawCurveData.GetCurveData(CurveUID);
		if (CurrentCurveData)
		{
			CurrentCurveData->Name.UID = RequestedNameUID;
			CurrentCurveData->Name.DisplayName = RequestedName;
		}
	}

public:
	USkeleton::AnimCurveUID CurveUID;
	TWeakObjectPtr<UAnimSequenceBase>	AnimSequenceBase;
	TWeakObjectPtr<UAnimSequence>		AnimSequence;

	FAnimCurveBaseInterface( UAnimSequenceBase * BaseSeq, USkeleton::AnimCurveUID InCurveUID)
		: CurveUID(InCurveUID)
		, AnimSequenceBase(BaseSeq)
		, AnimSequence(Cast<UAnimSequence>(BaseSeq))
		
	{
		CurveData = AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID);
		// they should be valid
		check(AnimSequenceBase.IsValid());
		check (CurveData);
	}

	/** Returns set of curves to edit. Must not release the curves while being edited. */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override
	{
		TArray<FRichCurveEditInfoConst> Curves;
		FFloatCurve * FloatCurveData = (FFloatCurve*)(CurveData);
		Curves.Add(&FloatCurveData->FloatCurve);

		return Curves;
	}

	/** Returns set of curves to query. Must not release the curves while being edited. */
	virtual TArray<FRichCurveEditInfo> GetCurves() override
	{
		TArray<FRichCurveEditInfo> Curves;
		FFloatCurve * FloatCurveData = (FFloatCurve*)(CurveData);
		Curves.Add(&FloatCurveData->FloatCurve);

		return Curves;
	}

	/** Called to modify the owner of the curve */
	virtual void ModifyOwner() override
	{
		if (AnimSequenceBase.IsValid())
		{
			AnimSequenceBase.Get()->Modify(true);
			AnimSequenceBase.Get()->MarkRawDataAsModified();
		}
	}

	/** Returns the owner(s) of the curve */
	virtual TArray<const UObject*> GetOwners() const override
	{
		TArray<const UObject*> Owners;
		if (AnimSequenceBase.IsValid())
		{
			Owners.Add(AnimSequenceBase.Get());
		}

		return Owners;
	}

	/** Called to make curve owner transactional */
	virtual void MakeTransactional() override
	{
		if (AnimSequenceBase.IsValid())
		{
			AnimSequenceBase.Get()->SetFlags(AnimSequenceBase.Get()->GetFlags() | RF_Transactional);
		}
	}

	/** Called to get the name of a curve back from the animation skeleton */
	virtual FText GetCurveName(USkeleton::AnimCurveUID Uid) const
	{
		if (AnimSequenceBase.IsValid())
		{
			FSmartName CurveName;
			if(AnimSequenceBase.Get()->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, Uid, CurveName))
			{
				return FText::FromName(CurveName.DisplayName);
			}
		}
		return FText::GetEmpty();
	}

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override
	{
		if (AnimSequenceBase.IsValid())
		{
			AnimSequenceBase.Get()->PostEditChange();
		}
	}

	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override
	{
		// Get the curve with the ID directly from the sequence and compare it since undo/redo can cause previously
		// used curves to become invalid.
		FAnimCurveBase* CurrentCurveData = AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID);
		return CurrentCurveData != nullptr &&
			CurveInfo.CurveToEdit == &((FFloatCurve*)CurrentCurveData)->FloatCurve;
	}

	bool IsMetadata() const
	{
		return AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID)->GetCurveTypeFlag(AACF_Metadata);
	}

	void SetKeysToMetaData()
	{
		FFloatCurve* CurrentCurveData = (FFloatCurve*)AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID);
		CurrentCurveData->FloatCurve.Reset();
		CurrentCurveData->FloatCurve.AddKey(0.0f, 1.0f);
	}

	void UpdateName(const SmartName::UID_Type& RequestedNameUID, FName RequestedName)
	{
		UpdateNameInternal(AnimSequenceBase.Get()->RawCurveData, RequestedNameUID, RequestedName);
		if (UAnimSequence* Seq = AnimSequence.Get())
		{
			UpdateNameInternal(Seq->CompressedCurveData, RequestedNameUID, RequestedName);
		}
		CurveUID = RequestedNameUID;
	}

	/**
	* Set InFlag to bValue
	*/
	void SetCurveTypeFlag(EAnimAssetCurveFlags InFlag, bool bValue)
	{
		AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID)->SetCurveTypeFlag(InFlag, bValue);
		if (UAnimSequence* Seq = AnimSequence.Get())
		{
			if (FAnimCurveBase* CompressedCurve = Seq->CompressedCurveData.GetCurveData(CurveUID))
			{
				CompressedCurve->SetCurveTypeFlag(InFlag, bValue);
			}
		}
	}

	/**
	* Toggle the value of the specified flag
	*/
	void ToggleCurveTypeFlag(EAnimAssetCurveFlags InFlag)
	{
		AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID)->ToggleCurveTypeFlag(InFlag);
		if (UAnimSequence* Seq = AnimSequence.Get())
		{
			if (FAnimCurveBase* CompressedCurve = Seq->CompressedCurveData.GetCurveData(CurveUID))
			{
				CompressedCurve->ToggleCurveTypeFlag(InFlag);
			}
		}
	}

	/**
	* Return true if InFlag is set, false otherwise
	*/
	bool GetCurveTypeFlag(EAnimAssetCurveFlags InFlag) const
	{
		return AnimSequenceBase.Get()->RawCurveData.GetCurveData(CurveUID)->GetCurveTypeFlag(InFlag);
	}
};
//////////////////////////////////////////////////////////////////////////
//  SCurveEd Track : each track for curve editing 

/** Widget for editing a single track of animation curve - this includes CurveEditor */
class SCurveEdTrack : public SCompoundWidget
{
private:
	/** Pointer to notify panel for drawing*/
	TSharedPtr<class SCurveEditor>			CurveEditor;

	/** Name of curve it's editing - CurveName should be unique within this tracks**/
	FAnimCurveBaseInterface	*				CurveInterface;

	/** Curve Panel Ptr **/
	TWeakPtr<SAnimCurvePanel>				PanelPtr;

	/** is using expanded editor **/
	bool									bUseExpandEditor;

public:
	SLATE_BEGIN_ARGS( SCurveEdTrack )
		: _AnimCurvePanel()
		, _Sequence()
		, _CurveUid()
		, _bIsExpanded()
		, _WidgetWidth()
		, _ViewInputMin()
		, _ViewInputMax()
		, _OnSetInputViewRange()
		, _OnGetScrubValue()		
	{}
	SLATE_ARGUMENT( TSharedPtr<SAnimCurvePanel>, AnimCurvePanel)
	// editing related variables
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
	SLATE_ARGUMENT( USkeleton::AnimCurveUID, CurveUid )
	SLATE_ARGUMENT( bool, bIsExpanded )
	// widget viewing related variables
	SLATE_ARGUMENT( float, WidgetWidth ) // @todo do I need this?
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCurveEdTrack();

	// input handling for curve name
	void NewCurveNameEntered( const FText& NewText, ETextCommit::Type CommitInfo );

	// Duplicate the current track
	void DuplicateTrack();

	// Delete current track
	void DeleteTrack();


	/**
	 * Build and display curve track context menu.
	 *
	 */
	FReply OnContextMenu();

	// expand editor mode 
	ECheckBoxState IsEditorExpanded() const;
	void ToggleExpandEditor(ECheckBoxState NewType);
	const FSlateBrush* GetExpandContent() const;
	FVector2D GetDesiredSize() const;

	// Bound to attribute for curve name, uses curve interface to request from skeleton
	FText GetCurveName(USkeleton::AnimCurveUID Uid) const;

	USkeleton::AnimCurveUID GetCurveUID() const;
	bool IsExpanded() const;

	float GetLength() const { return PanelPtr.Pin()->GetLength(); }
	TOptional<float> GetOptionalLength() const { return GetLength(); }
};

//////////////////////////////////////////////////////////////////////////
// SCurveEdTrack

void SCurveEdTrack::Construct(const FArguments& InArgs)
{
	TSharedRef<SAnimCurvePanel> PanelRef = InArgs._AnimCurvePanel.ToSharedRef();
	PanelPtr = InArgs._AnimCurvePanel;
	bUseExpandEditor = InArgs._bIsExpanded;
	// now create CurveInterface, 
	// find which curve this belongs to
	UAnimSequenceBase * Sequence = InArgs._Sequence;
	check (Sequence);

	// get the curve data
	FAnimCurveBase * Curve = Sequence->RawCurveData.GetCurveData(InArgs._CurveUid);
	check (Curve);

	CurveInterface = new FAnimCurveBaseInterface(Sequence, InArgs._CurveUid);
	int32 NumberOfKeys = Sequence->GetNumberOfFrames();
	//////////////////////////////
	
	TSharedPtr<SBorder> CurveBorder = nullptr;
	TSharedPtr<SHorizontalBox> InnerBox = nullptr;

	SAssignNew(CurveBorder, SBorder)
	.Padding(FMargin(2.0f, 2.0f))
	[
		SAssignNew(InnerBox, SHorizontalBox)
	];
	
	bool bIsMetadata = Curve->GetCurveTypeFlag(AACF_Metadata);
	if(!bIsMetadata)
	{
		InnerBox->AddSlot()
		.FillWidth(1)
		[
			// Notification editor panel
			SAssignNew(CurveEditor, SAnimCurveEd)
			.ViewMinInput(InArgs._ViewInputMin)
			.ViewMaxInput(InArgs._ViewInputMax)
			.DataMinInput(0.f)
			.DataMaxInput(this, &SCurveEdTrack::GetOptionalLength)
			.TimelineLength(this, &SCurveEdTrack::GetLength)
			.NumberOfKeys(NumberOfKeys)
			.DesiredSize(this, &SCurveEdTrack::GetDesiredSize)
			.OnSetInputViewRange(InArgs._OnSetInputViewRange)
			.OnGetScrubValue(InArgs._OnGetScrubValue)
		];

		//Inform track widget about the curve and whether it is editable or not.
		CurveEditor->SetCurveOwner(CurveInterface, true);
	}

	TSharedPtr<SHorizontalBox> NameBox = nullptr;
	SHorizontalBox::FSlot& CurveSlot = InnerBox->AddSlot()
	[
		SNew(SBox)
		.WidthOverride(InArgs._WidgetWidth)
		.VAlign(VAlign_Center)
		[
			SAssignNew(NameBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
			[
				// Name of track
				SNew(SEditableText)
				.MinDesiredWidth(64.0f)
				.IsEnabled(true)
				.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
				.SelectAllTextWhenFocused(true)
				.Text(this, &SCurveEdTrack::GetCurveName, Curve->Name.UID)
				.OnTextCommitted(this, &SCurveEdTrack::NewCurveNameEntered)
			]
		]
	];

	// Need to autowidth non-metadata names to maximise curve editor area and
	// add the expansion checkbox (unnecessary for metadata)
	if(!bIsMetadata)
	{
		CurveSlot.AutoWidth();

		NameBox->AddSlot()
		.AutoWidth()
		[
			// Name of track
			SNew(SCheckBox)
			.IsChecked(this, &SCurveEdTrack::IsEditorExpanded)
			.OnCheckStateChanged(this, &SCurveEdTrack::ToggleExpandEditor)
			.ToolTipText(LOCTEXT("Expand window", "Expand window"))
			.IsEnabled(true)
			[
				SNew(SImage)
				.Image(this, &SCurveEdTrack::GetExpandContent)
			]
		];
	}

	// Add track options combo button
	NameBox->AddSlot()
	.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
	.AutoWidth()
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("DisplayTrackOptionsMenuTooltip", "Display track options menu"))
		.OnClicked(this, &SCurveEdTrack::OnContextMenu)
		.Content()
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];

	this->ChildSlot
	[
		CurveBorder->AsShared()
	];
}

/** return a widget */
const FSlateBrush* SCurveEdTrack::GetExpandContent() const
{
	if (bUseExpandEditor)
	{
		return FEditorStyle::GetBrush("Kismet.VariableList.HideForInstance");
	}
	else
	{
		return FEditorStyle::GetBrush("Kismet.VariableList.ExposeForInstance");
	}

}

void SCurveEdTrack::NewCurveNameEntered( const FText& NewText, ETextCommit::Type CommitInfo )
{
	if(CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		if (USkeleton* Skeleton = CurveInterface->AnimSequenceBase->GetSkeleton())
		{
			// only do this if the name isn't same
			FText CurrentCurveName = CurveInterface->GetCurveName(CurveInterface->CurveUID);
			if (!CurrentCurveName.EqualToCaseIgnored(NewText))
			{
				// Check that the name doesn't already exist
				const FName RequestedName = FName(*NewText.ToString());

				const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

				FScopedTransaction Transaction(LOCTEXT("CurveEditor_RenameCurve", "Rename Curve"));

				FSmartName NewSmartName;
				if (NameMapping->FindSmartName(RequestedName, NewSmartName))
				{
					// Already in use in this sequence, and if it's not my UID
					if (NewSmartName.UID != CurveInterface->CurveUID && CurveInterface->AnimSequenceBase->RawCurveData.GetCurveData(NewSmartName.UID) != nullptr)
					{
						Transaction.Cancel();

						FFormatNamedArguments Args;
						Args.Add(TEXT("InvalidName"), FText::FromName(RequestedName));
						FNotificationInfo Info(FText::Format(LOCTEXT("AnimCurveRenamedInUse", "The name \"{InvalidName}\" is already used."), Args));

						Info.bUseLargeFont = false;
						Info.ExpireDuration = 5.0f;

						TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
						if (Notification.IsValid())
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}
				}
				else
				{
					if(!Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, RequestedName, NewSmartName))
					{
						Transaction.Cancel();
						FNotificationInfo Info(LOCTEXT("AnimCurveRenamedError", "Failed to rename curve smart name, check the log for errors."));

						Info.bUseLargeFont = false;
						Info.ExpireDuration = 5.0f;

						TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
						if (Notification.IsValid())
						{
							Notification->SetCompletionState(SNotificationItem::CS_Fail);
						}
						return;
					}
				}

				CurveInterface->ModifyOwner();
				CurveInterface->UpdateName(NewSmartName.UID, RequestedName);

				// Refresh the panel
				TSharedPtr<SAnimCurvePanel> SharedPanel = PanelPtr.Pin();
				if (SharedPanel.IsValid())
				{
					SharedPanel.Get()->UpdatePanel();
				}
			}
		}
	}
}

SCurveEdTrack::~SCurveEdTrack()
{
	// @fixme - check -is this okay way of doing it?
	delete CurveInterface;
}

void SCurveEdTrack::DuplicateTrack()
{
	TSharedPtr<SAnimCurvePanel> SharedPanel = PanelPtr.Pin();
	if(SharedPanel.IsValid())
	{
		SharedPanel->DuplicateTrack(CurveInterface->CurveUID);
	}
}

void SCurveEdTrack::DeleteTrack()
{
	TSharedPtr<SAnimCurvePanel> SharedPanel = PanelPtr.Pin();
	if(SharedPanel.IsValid())
	{
		SharedPanel->DeleteTrack(CurveInterface->CurveUID);
	}
}


FReply SCurveEdTrack::OnContextMenu()
{
	TSharedPtr<SAnimCurvePanel> PanelShared = PanelPtr.Pin();
	if(PanelShared.IsValid())
	{
		FSlateApplication::Get().PushMenu(SharedThis(this),
										  FWidgetPath(),
										  PanelShared->CreateCurveContextMenu(CurveInterface),
										  FSlateApplication::Get().GetCursorPos(),
										  FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
	}

	return FReply::Handled();
}

ECheckBoxState SCurveEdTrack::IsEditorExpanded() const
{
	return (bUseExpandEditor)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCurveEdTrack::ToggleExpandEditor(ECheckBoxState NewType)
{
	bUseExpandEditor = (NewType == ECheckBoxState::Checked);
}

FVector2D SCurveEdTrack::GetDesiredSize() const
{
	if (bUseExpandEditor)
	{
		return FVector2D(128.f, 128.f);
	}
	else
	{
		return FVector2D(128.f, 32.f);
	}
}

FText SCurveEdTrack::GetCurveName(USkeleton::AnimCurveUID Uid) const
{
	return CurveInterface->GetCurveName(Uid);
}

USkeleton::AnimCurveUID SCurveEdTrack::GetCurveUID() const
{
	return CurveInterface->CurveUID;
}

bool SCurveEdTrack::IsExpanded() const
{
	return bUseExpandEditor;
}

//////////////////////////////////////////////////////////////////////////
// SAnimCurvePanel

/**
 * Name validator for anim curves
 */
class FCurveNameValidator : public FStringSetNameValidator
{
public:
	FCurveNameValidator(FRawCurveTracks& Tracks, const FSmartNameMapping* NameMapping, const FString& ExistingName)
		: FStringSetNameValidator(ExistingName)
	{
		FName CurveName;
		for(FFloatCurve& Curve : Tracks.FloatCurves)
		{
			if(NameMapping->GetName(Curve.Name.UID, CurveName))
			{
				Names.Add(CurveName.ToString());
			}
		}
	}
};

void SAnimCurvePanel::Construct(const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton)
{
	SAnimTrackPanel::Construct( SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	Sequence = InArgs._Sequence;
	WidgetWidth = InArgs._WidgetWidth;
	OnGetScrubValue = InArgs._OnGetScrubValue;

	InEditableSkeleton->RegisterOnSmartNameChanged(FOnSmartNameChanged::FDelegate::CreateSP(this, &SAnimCurvePanel::HandleSmartNamesChange));

	Sequence->RegisterOnAnimCurvesChanged(UAnimSequenceBase::FOnAnimCurvesChanged::CreateSP(this, &SAnimCurvePanel::UpdatePanel));

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT("Curves", "Curves") )
			.BodyContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Name of track
						SNew(SButton)
						.Text( LOCTEXT("AddFloatTrack", "Add...") )
						.ToolTipText( LOCTEXT("AddTrackTooltip", "Add float track above here") )
						.OnClicked( this, &SAnimCurvePanel::AddButtonClicked )
						.AddMetaData<FTagMetaData>(TEXT("AnimCurve.AddFloat"))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SComboButton )
						.ContentPadding(FMargin(2.0f))
						.OnGetMenuContent(this, &SAnimCurvePanel::GenerateCurveList)
					]

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(5,0)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
						.Text(this, &SAnimCurvePanel::GetCurveNumText)
					]

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.Padding( 2.0f, 0.0f )
					[
						// Name of track
						SNew(SButton)
						.ToolTipText( LOCTEXT("DisplayTrackOptionsMenuForAllTracksTooltip", "Display track options menu for all tracks") )
						.OnClicked( this, &SAnimCurvePanel::OnContextMenu )
						.Visibility( TAttribute<EVisibility>( this, &SAnimCurvePanel::IsSetAllTracksButtonVisible ) )
						.Content()
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
							.ColorAndOpacity( FSlateColor::UseForeground() )
						]
					]

				]

				+SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
				.AutoHeight()
				[
					SAssignNew(PanelSlot, SSplitter)
					.Orientation(Orient_Vertical)
				]
			]
		]
	];

	UpdatePanel();
}

SAnimCurvePanel::~SAnimCurvePanel()
{
	Sequence->UnregisterOnAnimCurvesChanged(this);
}

FReply SAnimCurvePanel::AddButtonClicked()
{
	USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
	check(CurrentSkeleton);

	FMenuBuilder MenuBuilder(true, NULL);
	
	MenuBuilder.BeginSection("ConstantCurves", LOCTEXT("ConstantCurveHeading", "Constant Curve"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("NewMetadataSubMenu", "Add Metadata..."),
			LOCTEXT("NewMetadataSubMenuToolTip", "Add a new metadata entry to the sequence"),
			FNewMenuDelegate::CreateRaw(this, &SAnimCurvePanel::FillMetadataEntryMenu));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Curves", LOCTEXT("CurveHeading", "Curve"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("NewVariableCurveSubMenu", "Add Variable Curve..."),
			LOCTEXT("NewVariableCurveSubMenuToolTip", "Add a new variable curve to the sequence"),
			FNewMenuDelegate::CreateRaw(this, &SAnimCurvePanel::FillVariableCurveMenu));
	}
	MenuBuilder.EndSection();

	// Show dialog to enter new track name
	FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup)
		);

	return FReply::Handled();	
}

void SAnimCurvePanel::CreateTrack(const FText& ComittedText, ETextCommit::Type CommitInfo)
{
	if ( CommitInfo == ETextCommit::OnEnter )
	{
		USkeleton* Skeleton = Sequence->GetSkeleton();
		if(Skeleton && !ComittedText.IsEmpty())
		{
			const FScopedTransaction Transaction(LOCTEXT("AnimCurve_AddTrack", "Add New Curve"));
			FSmartName NewTrackName;

			if(Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, FName(*ComittedText.ToString()), NewTrackName))
			{
				AddVariableCurve(NewTrackName.UID);
			}
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

FReply SAnimCurvePanel::DuplicateTrack(USkeleton::AnimCurveUID Uid)
{
	const FScopedTransaction Transaction( LOCTEXT("AnimCurve_DuplicateTrack", "Duplicate Curve") );
	
	const FSmartNameMapping* NameMapping = Sequence->GetSkeleton()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	FName CurveNameToCopy;

	// Must have a curve that exists to duplicate
	if(NameMapping->Exists(Uid))
	{
		NameMapping->GetName(Uid, CurveNameToCopy);
		TSharedPtr<INameValidatorInterface> Validator = MakeShareable(new FCurveNameValidator(Sequence->RawCurveData, NameMapping, FString(TEXT(""))));

		// Use the validator to pick a reasonable name for the duplicated curve.
		FString NewCurveName = CurveNameToCopy.ToString();
		Validator->FindValidString(NewCurveName);
		FSmartName NewCurve, CurveToCopy;
		//@todo : test - how to duplicate track?
		if (NameMapping->FindSmartName(CurveNameToCopy, CurveToCopy) && Sequence->GetSkeleton()->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, FName(*NewCurveName), NewCurve))
		{
			if(Sequence->RawCurveData.DuplicateCurveData(CurveToCopy, NewCurve))
			{
				Sequence->Modify();
				Sequence->MarkRawDataAsModified();
				Sequence->PostEditChange();
				UpdatePanel();

				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

void SAnimCurvePanel::DeleteTrack(USkeleton::AnimCurveUID Uid)
{
	const FScopedTransaction Transaction( LOCTEXT("AnimCurve_DeleteTrack", "Delete Curve") );
	
	if(Sequence->RawCurveData.GetCurveData(Uid))
	{
		FSmartName TrackName;
		if (Sequence->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, Uid, TrackName))
		{
			Sequence->Modify(true);
			Sequence->RawCurveData.DeleteCurveData(TrackName);
			Sequence->MarkRawDataAsModified();
			Sequence->PostEditChange();
			UpdatePanel();
		}
	}
}

void SAnimCurvePanel::DeleteAllTracks()
{
	const FScopedTransaction Transaction( LOCTEXT("AnimCurve_DeleteAllTracks", "Delete All Curves") );

	Sequence->Modify(true);
	Sequence->RawCurveData.DeleteAllCurveData();
	Sequence->MarkRawDataAsModified();
	Sequence->PostEditChange();
	UpdatePanel();
}

FReply SAnimCurvePanel::OnContextMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);


	MenuBuilder.BeginSection("AnimCurvePanelOptions", LOCTEXT("OptionsHeading", "Options"));
	{
		FUIAction NewAction;
		NewAction.ExecuteAction.BindSP(this, &SAnimCurvePanel::DeleteAllTracks);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveTracks", "Remove All Tracks"),
			LOCTEXT("RemoveTracksTooltip", "Remove all tracks"),
			FSlateIcon(),
			NewAction);
	}
	MenuBuilder.EndSection();

	FSlateApplication::Get().PushMenu(	SharedThis(this),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup) );


	return FReply::Handled();
}

EVisibility SAnimCurvePanel::IsSetAllTracksButtonVisible() const
{
	return (Tracks.Num() > 1) ? EVisibility::Visible : EVisibility::Hidden;
}


void SAnimCurvePanel::UpdatePanel()
{
	if(Sequence != NULL)
	{
		USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
		const FSmartNameMapping* MetadataNameMap = CurrentSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		// Sort the raw curves before setting up display
		Sequence->RawCurveData.FloatCurves.Sort([MetadataNameMap](const FFloatCurve& A, const FFloatCurve& B)
		{
			bool bAMeta = A.GetCurveTypeFlag(AACF_Metadata);
			bool bBMeta = B.GetCurveTypeFlag(AACF_Metadata);
			
			if(bAMeta != bBMeta)
			{
				return !bAMeta;
			}

			FName AName;
			FName BName;
			MetadataNameMap->GetName(A.Name.UID, AName);
			MetadataNameMap->GetName(B.Name.UID, BName);

			return AName < BName;
		});

		// Store expanded state before clearing the tracks
		TMap<SmartName::UID_Type, bool> ExpandedState;
		for (TWeakPtr<class SCurveEdTrack> Track : Tracks)
		{
			if (Track.IsValid())
			{
				ExpandedState.Add(Track.Pin()->GetCurveUID(), Track.Pin()->IsExpanded());
			}
		}

		// see if we need to clear or not
		FChildren * VariableChildren = PanelSlot->GetChildren();
		for (int32 Id=VariableChildren->Num()-1; Id>=0; --Id)
		{
			PanelSlot->RemoveAt(Id);
		}

		// Clear all tracks as we're re-adding them all anyway.
		Tracks.Empty();

		// Updating new tracks
		const FSmartNameMapping* NameMapping = CurrentSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

		int32 TotalCurve = Sequence->RawCurveData.FloatCurves.Num();
		for(int32 CurrentIt = 0 ; CurrentIt < TotalCurve ; ++CurrentIt)
		{
			FFloatCurve&  Curve = Sequence->RawCurveData.FloatCurves[CurrentIt];

			const bool bEditable = Curve.GetCurveTypeFlag(AACF_Editable);
			const bool bConstant = Curve.GetCurveTypeFlag(AACF_Metadata);
			FName CurveName;

			// if editable, add to the list
			if(bEditable && NameMapping->GetName(Curve.Name.UID, CurveName))
			{
				const bool bIsExpanded = ExpandedState.Contains(Curve.Name.UID) ? ExpandedState.FindChecked(Curve.Name.UID) : false;

				TSharedPtr<SCurveEdTrack> CurrentTrack;
				PanelSlot->AddSlot()
				.SizeRule(SSplitter::SizeToContent)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SAssignNew(CurrentTrack, SCurveEdTrack)
						.Sequence(Sequence)
						.CurveUid(Curve.Name.UID)
						.AnimCurvePanel(SharedThis(this))
						.WidgetWidth(WidgetWidth)
						.ViewInputMin(ViewInputMin)
						.ViewInputMax(ViewInputMax)
						.OnGetScrubValue(OnGetScrubValue)
						.OnSetInputViewRange(OnSetInputViewRange)
						.bIsExpanded(bIsExpanded)
					]
				];
				Tracks.Add(CurrentTrack);
			}
		}
	}
}

void SAnimCurvePanel::SetSequence(class UAnimSequenceBase *	InSequence)
{
	if (InSequence != Sequence)
	{
		Sequence = InSequence;
		UpdatePanel();
	}
}

TSharedRef<SWidget> SAnimCurvePanel::GenerateCurveList()
{
	TSharedPtr<SVerticalBox> MainBox, ListBox;
	TSharedRef<SWidget> NewWidget = SAssignNew(MainBox, SVerticalBox);

	if ( Sequence && Sequence->RawCurveData.FloatCurves.Num() > 0)
	{
		MainBox->AddSlot()
			.AutoHeight()
			.MaxHeight(300)
			[
				SNew( SScrollBox )
				+SScrollBox::Slot() 
				[
					SAssignNew(ListBox, SVerticalBox)
				]
			];

		// Mapping to retrieve curve names
		const FSmartNameMapping* NameMapping = Sequence->GetSkeleton()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		check(NameMapping);

		for (auto Iter=Sequence->RawCurveData.FloatCurves.CreateConstIterator(); Iter; ++Iter)
		{
			const FFloatCurve& Curve= *Iter;

			FName CurveName;
			NameMapping->GetName(Curve.Name.UID, CurveName);

			ListBox->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding( 2.0f, 2.0f )
				[
					SNew( SCheckBox )
					.IsChecked(this, &SAnimCurvePanel::IsCurveEditable, Curve.Name.UID)
					.OnCheckStateChanged(this, &SAnimCurvePanel::ToggleEditability, Curve.Name.UID)
					.ToolTipText( LOCTEXT("Show Curves", "Show or Hide Curves") )
					.IsEnabled( true )
					[
						SNew( STextBlock )
						.Text(FText::FromName(CurveName))
					]
				];
		}

		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( SButton )
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Center )
				.OnClicked( this, &SAnimCurvePanel::RefreshPanel )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("RefreshCurve", "Refresh") )
				]
			];

		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( SButton )
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Center )
				.OnClicked( this, &SAnimCurvePanel::ShowAll, true )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("ShowAll", "Show All") )
				]
			];

		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( SButton )
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Center )
				.OnClicked( this, &SAnimCurvePanel::ShowAll, false )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("HideAll", "Hide All") )
				]
			];
	}
	else
	{
		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( STextBlock )
				.Text(LOCTEXT("Not Available", "No curve exists"))
			];
	}

	return NewWidget;
}

FText SAnimCurvePanel::GetCurveNumText() const
{
	return FText::FromString(FString::Printf(TEXT(" Total Number : %d "), Sequence->RawCurveData.FloatCurves.Num()));
}

ECheckBoxState SAnimCurvePanel::IsCurveEditable(USkeleton::AnimCurveUID Uid) const
{
	if ( Sequence )
	{
		const FFloatCurve* Curve = static_cast<const FFloatCurve *>(Sequence->RawCurveData.GetCurveData(Uid, ERawCurveTrackTypes::RCT_Float));
		if ( Curve )
		{
			return Curve->GetCurveTypeFlag(AACF_Editable)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SAnimCurvePanel::ToggleEditability(ECheckBoxState NewType, USkeleton::AnimCurveUID Uid)
{
	bool bEdit = (NewType == ECheckBoxState::Checked);

	if ( Sequence )
	{
		FFloatCurve * Curve = static_cast<FFloatCurve *>(Sequence->RawCurveData.GetCurveData(Uid, ERawCurveTrackTypes::RCT_Float));
		if ( Curve )
		{
			Curve->SetCurveTypeFlag(AACF_Editable, bEdit);
		}
	}
}

FReply		SAnimCurvePanel::RefreshPanel()
{
	UpdatePanel();
	return FReply::Handled();
}

FReply		SAnimCurvePanel::ShowAll(bool bShow)
{
	if ( Sequence )
	{
		for (auto Iter = Sequence->RawCurveData.FloatCurves.CreateIterator(); Iter; ++Iter)
		{
			FFloatCurve & Curve = *Iter;
			Curve.SetCurveTypeFlag(AACF_Editable, bShow);
		}

		UpdatePanel();
	}

	return FReply::Handled();
}

void SAnimCurvePanel::FillMetadataEntryMenu(FMenuBuilder& Builder)
{
	USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
	check(CurrentSkeleton);

	const FSmartNameMapping* Mapping = CurrentSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	TArray<USkeleton::AnimCurveUID> CurveUids;
	Mapping->FillUidArray(CurveUids);

	Builder.BeginSection(NAME_None, LOCTEXT("MetadataMenu_ListHeading", "Available Names"));
	{
		TArray<FSmartNameSortItem> SmartNameList;

		for (USkeleton::AnimCurveUID Id : CurveUids)
		{
			if (!Sequence->RawCurveData.GetCurveData(Id))
			{
				FName CurveName;
				if (Mapping->GetName(Id, CurveName))
				{
					SmartNameList.Add(FSmartNameSortItem(CurveName, Id));
				}
			}
		}

		{
			SmartNameList.Sort(FSmartNameSortItemSortOp());

			for (FSmartNameSortItem SmartNameItem : SmartNameList)
			{
				const FText Description = LOCTEXT("NewMetadataSubMenu_ToolTip", "Add an existing metadata curve");
				const FText Label = FText::FromName(SmartNameItem.SmartName);

				FUIAction UIAction;
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimCurvePanel::AddMetadataEntry,
					SmartNameItem.ID);

				Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
			}
		}
	}
	Builder.EndSection();

	Builder.AddMenuSeparator();

	const FText Description = LOCTEXT("NewMetadataCreateNew_ToolTip", "Create a new metadata entry");
	const FText Label = LOCTEXT("NewMetadataCreateNew_Label","Create New");
	FUIAction UIAction;
	UIAction.ExecuteAction.BindRaw(this, &SAnimCurvePanel::CreateNewMetadataEntryClicked);

	Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
}

void SAnimCurvePanel::FillVariableCurveMenu(FMenuBuilder& Builder)
{
	FText Description = LOCTEXT("NewVariableCurveCreateNew_ToolTip", "Create a new variable curve");
	FText Label = LOCTEXT("NewVariableCurveCreateNew_Label", "Create Curve");
	FUIAction UIAction;
	UIAction.ExecuteAction.BindRaw(this, &SAnimCurvePanel::CreateNewCurveClicked);

	Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);

	USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
	check(CurrentSkeleton);

	const FSmartNameMapping* Mapping = CurrentSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	TArray<USkeleton::AnimCurveUID> CurveUids;
	Mapping->FillUidArray(CurveUids);

	Builder.BeginSection(NAME_None, LOCTEXT("VariableMenu_ListHeading", "Available Names"));
	{
		TArray<FSmartNameSortItem> SmartNameList;

		for (USkeleton::AnimCurveUID Id : CurveUids)
		{
			if (!Sequence->RawCurveData.GetCurveData(Id))
			{
				FName CurveName;
				if (Mapping->GetName(Id, CurveName))
				{
					SmartNameList.Add(FSmartNameSortItem(CurveName, Id));
				}
			}
		}

		{
			SmartNameList.Sort(FSmartNameSortItemSortOp());

			for (FSmartNameSortItem SmartNameItem : SmartNameList)
			{
				Description = LOCTEXT("NewVariableSubMenu_ToolTip", "Add an existing variable curve");
				Label = FText::FromName(SmartNameItem.SmartName);

				UIAction.ExecuteAction.BindRaw(
					this, &SAnimCurvePanel::AddVariableCurve,
					SmartNameItem.ID);

				Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
			}
		}
	}
	Builder.EndSection();
}

void SAnimCurvePanel::AddMetadataEntry(USkeleton::AnimCurveUID Uid)
{
	FSmartName NewName;
	ensureAlways(Sequence->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, Uid, NewName));
	if(Sequence->RawCurveData.AddCurveData(NewName))
	{
		Sequence->Modify(true);
		Sequence->MarkRawDataAsModified();
		FFloatCurve* Curve = static_cast<FFloatCurve *>(Sequence->RawCurveData.GetCurveData(Uid, ERawCurveTrackTypes::RCT_Float));
		Curve->FloatCurve.AddKey(0.0f, 1.0f);
		Curve->SetCurveTypeFlag(AACF_Metadata, true);
		RefreshPanel();
		Sequence->PostEditChange();
	}
}

void SAnimCurvePanel::CreateNewMetadataEntryClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewMetadataCurveEntryLabal", "Metadata Name"))
		.OnTextCommitted(this, &SAnimCurvePanel::CreateNewMetadataEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

void SAnimCurvePanel::CreateNewMetadataEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if(CommitType == ETextCommit::OnEnter)
	{
		// Add the name to the skeleton and then add the new curve to the sequence
		USkeleton* Skeleton = Sequence->GetSkeleton();
		if(Skeleton && !CommittedText.IsEmpty())
		{
			FSmartName CurveName;

			if(Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, FName(*CommittedText.ToString()), CurveName))
			{
				AddMetadataEntry(CurveName.UID);
			}
		}
	}
}

void SAnimCurvePanel::CreateNewCurveClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewCurveEntryLabal", "Curve Name"))
		.OnTextCommitted(this, &SAnimCurvePanel::CreateTrack);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

TSharedRef<SWidget> SAnimCurvePanel::CreateCurveContextMenu(FAnimCurveBaseInterface* Curve) const
{
	FMenuBuilder MenuBuilder(true, NULL);


	MenuBuilder.BeginSection("AnimCurvePanelTrackOptions", LOCTEXT("TrackOptionsHeading", "Track Options"));
	{
		FText TypeToggleLabel = LOCTEXT("TypeToggleToMetadataLabel", "Convert to Metadata");
		FText TypeToggleToolTip = LOCTEXT("TypeToggleToMetadataToolTip", "Turns this curve into a Metadata entry. This is a destructive operation and will remove the keys in this curve");
		bool bIsConstantCurve = Curve->IsMetadata();

		FUIAction NewAction;

		if(bIsConstantCurve)
		{
			TypeToggleLabel = LOCTEXT("TypeToggleToVariableLabel", "Convert to Variable Curve");
			TypeToggleToolTip = LOCTEXT("TypeToggleToVariableToolTip", "Turns this curve into a variable curve.");
		}

		NewAction.ExecuteAction.BindSP(this, &SAnimCurvePanel::ToggleCurveTypeMenuCallback, Curve);
		MenuBuilder.AddMenuEntry(
			TypeToggleLabel,
			TypeToggleToolTip,
			FSlateIcon(),
			NewAction);

		NewAction.ExecuteAction.BindSP(this, &SAnimCurvePanel::DeleteTrack, Curve->CurveUID);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveTrack", "Remove Track"),
			LOCTEXT("RemoveTrackTooltip", "Remove this track"),
			FSlateIcon(),
			NewAction);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAnimCurvePanel::ToggleCurveTypeMenuCallback(FAnimCurveBaseInterface* Curve)
{
	check(Curve);

	FScopedTransaction Transaction(LOCTEXT("CurvePanel_ToggleCurveType", "Toggle curve type"));
	Sequence->Modify(true);
	bool bIsSet = Curve->GetCurveTypeFlag(AACF_Metadata);
	Curve->SetCurveTypeFlag(AACF_Metadata, !bIsSet);

	if(!bIsSet)
	{
		// We're moving to a metadata curve, we need to clear out the keys.
		Curve->SetKeysToMetaData();
	}

	UpdatePanel();
}

void SAnimCurvePanel::AddVariableCurve(USkeleton::AnimCurveUID CurveUid)
{
	Sequence->Modify(true);
	
	USkeleton* Skeleton = Sequence->GetSkeleton();
	FSmartName NewName;
	ensureAlways(Skeleton->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveUid, NewName));
	Sequence->RawCurveData.AddCurveData(NewName);
	Sequence->MarkRawDataAsModified();
	Sequence->PostEditChange();
	UpdatePanel();
}

void SAnimCurvePanel::HandleSmartNamesChange(const FName& InContainerName)
{
	UpdatePanel();
}

#undef LOCTEXT_NAMESPACE
