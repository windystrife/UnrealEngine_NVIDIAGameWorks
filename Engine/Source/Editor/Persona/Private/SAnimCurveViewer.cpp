// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimCurveViewer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SSpinBox.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "IEditableSkeleton.h"

#include "Framework/Commands/GenericCommands.h"
#include "CurveViewerCommands.h"
#include "Animation/EditorAnimCurveBoneLinks.h"

#define LOCTEXT_NAMESPACE "SAnimCurveViewer"

static const FName ColumnId_AnimCurveNameLabel( "Curve Name" );
static const FName ColumnID_AnimCurveTypeLabel("Type");
static const FName ColumnID_AnimCurveWeightLabel( "Weight" );
static const FName ColumnID_AnimCurveEditLabel( "Edit" );
static const FName ColumnID_AnimCurveNumBoneLabel("Num Bones");

const float MaxMorphWeight = 5.f;
//////////////////////////////////////////////////////////////////////////
// SAnimCurveListRow

typedef TSharedPtr< FDisplayedAnimCurveInfo > FDisplayedAnimCurveInfoPtr;
//  This is a flag that is used to filter UI part
enum EAnimCurveEditorFlags
{
	// Used as morph target curve
	ACEF_DriveMorphTarget		= 0x00000001, 
	// Used as triggering event
	ACEF_DriveAttribute			= 0x00000002, 
	// Used as a material curve
	ACEF_DriveMaterial			= 0x00000004, 
};

void SAnimCurveListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	Item = InArgs._Item;
	AnimCurveViewerPtr = InArgs._AnimCurveViewerPtr;
	PreviewScenePtr = InPreviewScene;

	check( Item.IsValid() );

	SMultiColumnTableRow< TSharedPtr<FDisplayedAnimCurveInfo> >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SAnimCurveListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == ColumnId_AnimCurveNameLabel )
	{
		TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
		if (AnimCurveViewer.IsValid())
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.VAlign(VAlign_Center)
				[
					SAssignNew(Item->EditableText, SInlineEditableTextBlock)
					.OnTextCommitted(AnimCurveViewer.Get(), &SAnimCurveViewer::OnNameCommitted, Item)
					.ColorAndOpacity(this, &SAnimCurveListRow::GetItemTextColor)
					.IsSelected(this, &SAnimCurveListRow::IsSelected)
					.Text(this, &SAnimCurveListRow::GetItemName)
					.HighlightText(this, &SAnimCurveListRow::GetFilterText)
				];
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
	else if (ColumnName == ColumnID_AnimCurveTypeLabel)
	{
		TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
		if (AnimCurveViewer.IsValid())
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.VAlign(VAlign_Center)
				[
					GetCurveTypeWidget()
				];
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
	else if ( ColumnName == ColumnID_AnimCurveWeightLabel )
	{
		// Encase the SSpinbox in an SVertical box so we can apply padding. Setting ItemHeight on the containing SListView has no effect :-(
		return
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 1.0f )
			.VAlign( VAlign_Center )
			[
				SNew( SSpinBox<float> )
				.MinSliderValue(-1.f)
				.MaxSliderValue(1.f)
				.MinValue(-MaxMorphWeight)
				.MaxValue(MaxMorphWeight)
				.Value( this, &SAnimCurveListRow::GetWeight )
				.OnValueChanged( this, &SAnimCurveListRow::OnAnimCurveWeightChanged )
				.OnValueCommitted( this, &SAnimCurveListRow::OnAnimCurveWeightValueCommitted )
			];
	}
	else if ( ColumnName == ColumnID_AnimCurveEditLabel)
	{
		return
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SAnimCurveListRow::OnAnimCurveAutoFillChecked)
				.IsChecked(this, &SAnimCurveListRow::IsAnimCurveAutoFillChangedChecked)
			];
	}
	else
	{
		return
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAnimCurveListRow::GetNumConntectedBones)
			];
	}
}

FText SAnimCurveListRow::GetNumConntectedBones() const
{
	const FCurveMetaData* CurveMetaData = Item->EditableSkeleton.Pin()->GetSkeleton().GetCurveMetaData(Item->SmartName);
	if (CurveMetaData)
	{
		return FText::AsNumber(CurveMetaData->LinkedBones.Num());
	}

	return FText::AsNumber(0);
}

TSharedRef< SWidget > SAnimCurveListRow::GetCurveTypeWidget()
{
	return SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 1.f, 1.f, 1.f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SAnimCurveListRow::OnAnimCurveTypeBoxChecked, true)
			.IsChecked(this, &SAnimCurveListRow::IsAnimCurveTypeBoxChangedChecked, true)
			.IsEnabled(false)
			.CheckedImage(FEditorStyle::GetBrush("AnimCurveViewer.MorphTargetOn"))
			.CheckedPressedImage(FEditorStyle::GetBrush("AnimCurveViewer.MorphTargetOn"))
			.UncheckedImage(FEditorStyle::GetBrush("AnimCurveViewer.MorphTargetOff"))
			.CheckedHoveredImage(FEditorStyle::GetBrush("AnimCurveViewer.MorphTargetOn"))
			.UncheckedHoveredImage(FEditorStyle::GetBrush("AnimCurveViewer.MorphTargetOff"))
			.ToolTipText(LOCTEXT("CurveTypeMorphTarget_Tooltip", "MorphTarget"))
			.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 1.f, 1.f, 1.f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SAnimCurveListRow::OnAnimCurveTypeBoxChecked, false)
			.IsChecked(this, &SAnimCurveListRow::IsAnimCurveTypeBoxChangedChecked, false)
			.CheckedImage(FEditorStyle::GetBrush("AnimCurveViewer.MaterialOn"))
			.CheckedPressedImage(FEditorStyle::GetBrush("AnimCurveViewer.MaterialOn"))
			.UncheckedImage(FEditorStyle::GetBrush("AnimCurveViewer.MaterialOff"))
			.CheckedHoveredImage(FEditorStyle::GetBrush("AnimCurveViewer.MaterialOn"))
			.UncheckedHoveredImage(FEditorStyle::GetBrush("AnimCurveViewer.MaterialOff"))
			.ToolTipText(LOCTEXT("CurveTypeMaterial_Tooltip", "Material"))
			.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
		];

}

void SAnimCurveListRow::OnAnimCurveTypeBoxChecked(ECheckBoxState InState, bool bMorphTarget)
{
	// currently only material curve is set
	bool bNewData = (InState == ECheckBoxState::Checked);
	if (!bMorphTarget)
	{
		Item->EditableSkeleton.Pin()->SetCurveMetaDataMaterial(Item->SmartName, bNewData);
	}
}

ECheckBoxState SAnimCurveListRow::IsAnimCurveTypeBoxChangedChecked(bool bMorphTarget) const
{
	const FCurveMetaData* CurveMetaData = Item->EditableSkeleton.Pin()->GetSkeleton().GetCurveMetaData(Item->SmartName);

	bool bData = false;
	if (CurveMetaData)
	{
		if (bMorphTarget)
		{
			bData = CurveMetaData->Type.bMorphtarget != 0;
		}
		else
		{
			bData = CurveMetaData->Type.bMaterial != 0;
		}
	}

	return (bData)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimCurveListRow::OnAnimCurveAutoFillChecked(ECheckBoxState InState)
{
	Item->bAutoFillData = InState == ECheckBoxState::Checked;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		if (Item->bAutoFillData)
		{
			// clear value so that it can be filled up
			AnimCurveViewer->RemoveAnimCurveOverride(Item->SmartName.DisplayName);
		}
		else
		{
			AnimCurveViewer->AddAnimCurveOverride(Item->SmartName.DisplayName, Item->Weight);
		}
	}
}

ECheckBoxState SAnimCurveListRow::IsAnimCurveAutoFillChangedChecked() const
{
	return (Item->bAutoFillData)? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SAnimCurveListRow::OnAnimCurveWeightChanged( float NewWeight )
{
	float NewValidWeight = FMath::Clamp(NewWeight, -MaxMorphWeight, MaxMorphWeight);
	Item->Weight = NewValidWeight;
	Item->bAutoFillData = false;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		// If we try to slide an entry that is not selected, we select just it
		bool bItemIsSelected = AnimCurveViewer->AnimCurveListView->IsItemSelected(Item);
		if (!bItemIsSelected)
		{
			AnimCurveViewer->AnimCurveListView->SetSelection(Item, ESelectInfo::Direct);
		}

		// Add override
		AnimCurveViewer->AddAnimCurveOverride(Item->SmartName.DisplayName, Item->Weight);

		// ...then any selected rows need changing by the same delta
		TArray< TSharedPtr< FDisplayedAnimCurveInfo > > SelectedRows = AnimCurveViewer->AnimCurveListView->GetSelectedItems();
		for (auto ItemIt = SelectedRows.CreateIterator(); ItemIt; ++ItemIt)
		{
			TSharedPtr< FDisplayedAnimCurveInfo > RowItem = (*ItemIt);

			if (RowItem != Item) // Don't do "this" row again if it's selected
			{
				RowItem->Weight = NewValidWeight;
				RowItem->bAutoFillData = false;
				AnimCurveViewer->AddAnimCurveOverride(RowItem->SmartName.DisplayName, RowItem->Weight);
			}
		}

		if(PreviewScenePtr.IsValid())
		{
			PreviewScenePtr.Pin()->InvalidateViews();
		}
	}
}

void SAnimCurveListRow::OnAnimCurveWeightValueCommitted( float NewWeight, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
	{
		OnAnimCurveWeightChanged(NewWeight);
	}
}

FText SAnimCurveListRow::GetItemName() const
{
	FName ItemName;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		const FSmartNameMapping* Mapping = Item->EditableSkeleton.Pin()->GetSkeleton().GetSmartNameContainer(AnimCurveViewer->ContainerName);
		check(Mapping);
		Mapping->GetName(Item->SmartName.UID, ItemName);
	}

	return FText::FromName(ItemName);
}

FText SAnimCurveListRow::GetFilterText() const
{
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		return AnimCurveViewer->GetFilterText();
	}
	else
	{
		return FText::GetEmpty();
	}
}


bool SAnimCurveListRow::GetActiveWeight(float& OutWeight) const
{
	bool bFoundActive = false;

	// If anim viewer
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		// If anim instance
		UAnimInstance* AnimInstance = PreviewScenePtr.Pin()->GetPreviewMeshComponent()->GetAnimInstance();
		if (AnimInstance)
		{
			// See if curve is in active set, attribute curve should have everything
			TMap<FName, float> CurveList;
			AnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve, CurveList);

			float* CurrentValue = CurveList.Find(Item->SmartName.DisplayName);
			if (CurrentValue)
			{
				OutWeight = *CurrentValue;
				// Remember we found it
				bFoundActive = true;
			}
		}
	}

	return bFoundActive;
}


FSlateColor SAnimCurveListRow::GetItemTextColor() const
{
	// If row is selected, show text as black to make it easier to read
	if (IsSelected())
	{
		return FLinearColor(0, 0, 0);
	}

	// If not selected, show bright if active
	bool bItemActive = true;
	if (Item->bAutoFillData)
	{
		float Weight = 0.f;
		GetActiveWeight(Weight);
		// change so that print white if it has weight on it. 
		bItemActive = (Weight != 0.f);
	}

	return bItemActive ? FLinearColor(1, 1, 1) : FLinearColor(0.5, 0.5, 0.5);
}

float SAnimCurveListRow::GetWeight() const 
{ 
	float Weight = Item->Weight;

	if (Item->bAutoFillData)
	{
		GetActiveWeight(Weight);
	}

	return Weight;
}

//////////////////////////////////////////////////////////////////////////
// SAnimCurveTypeList

class SAnimCurveTypeList : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimCurveTypeList) {}

		/** The item for this row **/
		SLATE_ARGUMENT(int32, CurveFlags)

		/* The SAnimCurveViewer that we push the morph target weights into */
		SLATE_ARGUMENT(TWeakPtr<SAnimCurveViewer>, AnimCurveViewerPtr)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Auto fill check call back functions */
	void OnAnimCurveTypeShowChecked(ECheckBoxState InState);
	ECheckBoxState IsAnimCurveTypeShowChangedChecked() const;

	FText GetAnimCurveType() const;
private:

	/* The SAnimCurveViewer that we push the morph target weights into */
	TWeakPtr<SAnimCurveViewer> AnimCurveViewerPtr;

	/** The name and weight of the morph target */
	int32 CurveFlags;
};

void SAnimCurveTypeList::Construct(const FArguments& InArgs)
{
	CurveFlags = InArgs._CurveFlags;
	AnimCurveViewerPtr = InArgs._AnimCurveViewerPtr;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SAnimCurveTypeList::OnAnimCurveTypeShowChecked)
				.IsChecked(this, &SAnimCurveTypeList::IsAnimCurveTypeShowChangedChecked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(3.0f, 1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAnimCurveTypeList::GetAnimCurveType)
				.HighlightText(AnimCurveViewer->GetFilterText())
			]
		];
	}

}

void SAnimCurveTypeList::OnAnimCurveTypeShowChecked(ECheckBoxState InState)
{
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		// clear value so that it can be filled up
		if (InState == ECheckBoxState::Checked)
		{
			AnimCurveViewer->CurrentCurveFlag |= CurveFlags;
		}
		else
		{
			AnimCurveViewer->CurrentCurveFlag &= ~CurveFlags;
		}

		AnimCurveViewer->RefreshCurveList();
	}
}

ECheckBoxState SAnimCurveTypeList::IsAnimCurveTypeShowChangedChecked() const
{
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		return (AnimCurveViewer->CurrentCurveFlag & CurveFlags) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

FText SAnimCurveTypeList::GetAnimCurveType() const
{
	switch (CurveFlags)
	{
	case ACEF_DriveMorphTarget:
		return LOCTEXT("AnimCurveType_Morphtarget", "Morph Target");
	case ACEF_DriveAttribute:
		return LOCTEXT("AnimCurveType_Attribute", "Attribute");
	case ACEF_DriveMaterial:
		return LOCTEXT("AnimCurveType_Material", "Material");
	}

	return LOCTEXT("AnimCurveType_Unknown", "Unknown");
}
//////////////////////////////////////////////////////////////////////////
// SAnimCurveViewer

void SAnimCurveViewer::Construct(const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected)
{
	OnObjectsSelected = InOnObjectsSelected;

	bShowAllCurves = true;

	EditorObjectTracker.SetAllowOnePerClass(false);

	ContainerName = USkeleton::AnimCurveMappingName;

	PreviewScenePtr = InPreviewScene;
	EditableSkeletonPtr = InEditableSkeleton;

	InPreviewScene->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &SAnimCurveViewer::OnPreviewMeshChanged));
	InPreviewScene->RegisterOnAnimChanged(FOnAnimChanged::CreateSP(this, &SAnimCurveViewer::OnPreviewAssetChanged));
	InOnPostUndo.Add(FSimpleDelegate::CreateSP(this, &SAnimCurveViewer::OnPostUndo));

	SmartNameChangedHandle = InEditableSkeleton->RegisterOnSmartNameChanged(FOnSmartNameChanged::FDelegate::CreateSP(this, &SAnimCurveViewer::HandleSmartNamesChange));

	// Register and bind all our menu commands
	FCurveViewerCommands::Register();
	BindCommands();

	// @todo fix this to be filtered
	CurrentCurveFlag = ACEF_DriveMorphTarget | ACEF_DriveMaterial | ACEF_DriveAttribute;
	 
	TSharedPtr<SHorizontalBox> AnimTypeBoxContainer = SNew(SHorizontalBox);

	ChildSlot
	[
		SNew( SVerticalBox )
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0,2)
		[
			SNew(SHorizontalBox)
			// Filter entry
			+SHorizontalBox::Slot()
			.FillWidth( 1 )
			[
				SAssignNew( NameFilterBox, SSearchBox )
				.SelectAllTextWhenFocused( true )
				.OnTextChanged( this, &SAnimCurveViewer::OnFilterTextChanged )
				.OnTextCommitted( this, &SAnimCurveViewer::OnFilterTextCommitted )
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SNew(SBox)
			.WidthOverride(150)
			.Content()
			[
				AnimTypeBoxContainer.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( AnimCurveListView, SAnimCurveListType )
			.ListItemsSource( &AnimCurveList )
			.OnGenerateRow( this, &SAnimCurveViewer::GenerateAnimCurveRow )
			.OnContextMenuOpening( this, &SAnimCurveViewer::OnGetContextMenuContent )
			.ItemHeight( 22.0f )
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged( this, &SAnimCurveViewer::OnSelectionChanged )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_AnimCurveNameLabel )
				.FillWidth(1.f)
				.DefaultLabel( LOCTEXT( "AnimCurveNameLabel", "Curve Name" ) )

				+ SHeaderRow::Column(ColumnID_AnimCurveTypeLabel)
				.FillWidth(0.5f)
				.DefaultLabel(LOCTEXT("AnimCurveTypeLabel", "Type"))

				+ SHeaderRow::Column( ColumnID_AnimCurveWeightLabel )
				.FillWidth(1.f)
				.DefaultLabel( LOCTEXT( "AnimCurveWeightLabel", "Weight" ) )

				+ SHeaderRow::Column(ColumnID_AnimCurveEditLabel)
				.FillWidth(0.25f)
				.DefaultLabel(LOCTEXT("AnimCurveEditLabel", "Auto"))

				+ SHeaderRow::Column(ColumnID_AnimCurveNumBoneLabel)
				.FillWidth(0.5f)
				.DefaultLabel(LOCTEXT("AnimCurveNumBoneLabel", "Bones"))
			)
		]
	];

	CreateAnimCurveTypeList(AnimTypeBoxContainer.ToSharedRef());
	CreateAnimCurveList();
}

SAnimCurveViewer::~SAnimCurveViewer()
{
	if (PreviewScenePtr.IsValid() )
	{
		PreviewScenePtr.Pin()->UnregisterOnPreviewMeshChanged(this);
		PreviewScenePtr.Pin()->UnregisterOnAnimChanged(this);
	}

	if (EditableSkeletonPtr.IsValid())
	{
		EditableSkeletonPtr.Pin()->UnregisterOnSmartNameChanged(SmartNameChangedHandle);
	}
}

bool SAnimCurveViewer::IsCurveFilterEnabled() const
{
	return !bShowAllCurves;
}

ECheckBoxState SAnimCurveViewer::IsShowingAllCurves() const
{
	return bShowAllCurves ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimCurveViewer::OnToggleShowingAllCurves(ECheckBoxState NewState)
{
	bShowAllCurves = (NewState == ECheckBoxState::Checked);

	RefreshCurveList();
}
FReply SAnimCurveViewer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SAnimCurveViewer::BindCommands()
{
	// This should not be called twice on the same instance
	check(!UICommandList.IsValid());

	UICommandList = MakeShareable(new FUICommandList);

	FUICommandList& CommandList = *UICommandList;

	// Grab the list of menu commands to bind...
	const FCurveViewerCommands& MenuActions = FCurveViewerCommands::Get();

	// ...and bind them all

	CommandList.MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SAnimCurveViewer::OnRenameClicked),
		FCanExecuteAction::CreateSP(this, &SAnimCurveViewer::CanRename));

	CommandList.MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SAnimCurveViewer::OnDeleteNameClicked),
		FCanExecuteAction::CreateSP(this, &SAnimCurveViewer::CanDelete));

	CommandList.MapAction(
		MenuActions.AddCurve,
		FExecuteAction::CreateSP(this, &SAnimCurveViewer::OnAddClicked),
		FCanExecuteAction());
}

void SAnimCurveViewer::OnPreviewMeshChanged(class USkeletalMesh* OldPreviewMesh, class USkeletalMesh* NewPreviewMesh)
{
	RefreshCurveList();
}

void SAnimCurveViewer::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshCurveList();
}

void SAnimCurveViewer::OnCurvesChanged()
{
	RefreshCurveList();
}

void SAnimCurveViewer::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SAnimCurveViewer::GenerateAnimCurveRow(TSharedPtr<FDisplayedAnimCurveInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SAnimCurveListRow, OwnerTable, PreviewScenePtr.Pin().ToSharedRef() )
		.Item( InInfo )
		.AnimCurveViewerPtr( SharedThis(this) );
}

TSharedPtr<SWidget> SAnimCurveViewer::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, UICommandList);

	const FCurveViewerCommands& Actions = FCurveViewerCommands::Get();

	MenuBuilder.BeginSection("AnimCurveAction", LOCTEXT( "CurveAction", "Curve Actions" ) );

	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameSmartNameLabel", "Rename Curve"), LOCTEXT("RenameSmartNameToolTip", "Rename the selected curve"));
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("DeleteSmartNameLabel", "Delete Curve"), LOCTEXT("DeleteSmartNameToolTip", "Delete the selected curve"));
	MenuBuilder.AddMenuEntry(Actions.AddCurve);

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAnimCurveViewer::OnRenameClicked()
{
	TArray< TSharedPtr<FDisplayedAnimCurveInfo> > SelectedItems = AnimCurveListView->GetSelectedItems();

	SelectedItems[0]->EditableText->EnterEditingMode();
}

bool SAnimCurveViewer::CanRename()
{
	return AnimCurveListView->GetNumItemsSelected() == 1;
}

void SAnimCurveViewer::OnAddClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewSmartnameLabel", "New Name"))
		.OnTextCommitted(this, &SAnimCurveViewer::CreateNewNameEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}



void SAnimCurveViewer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UDebugSkelMeshComponent* MeshComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();

	if (AnimInstance)
	{
		RefreshCurveList();
	}
}

void SAnimCurveViewer::CreateNewNameEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if (!CommittedText.IsEmpty() && CommitType == ETextCommit::OnEnter)
	{
		if (const FSmartNameMapping* NameMapping = GetAnimCurveMapping())
		{
			FName NewName = FName(*CommittedText.ToString());
			FSmartName NewCurveName;
			if (EditableSkeletonPtr.Pin()->AddSmartname(ContainerName, NewName, NewCurveName))
			{
				// Successfully added
				RefreshCurveList();
			}
		}
	}
}


const FSmartNameMapping* SAnimCurveViewer::GetAnimCurveMapping()
{
	return EditableSkeletonPtr.Pin()->GetSkeleton().GetSmartNameContainer(ContainerName);
}

UAnimInstance* SAnimCurveViewer::GetAnimInstance() const
{
	return PreviewScenePtr.Pin()->GetPreviewMeshComponent()->GetAnimInstance();
}

int32 FindIndexOfAnimCurveInfo(TArray<TSharedPtr<FDisplayedAnimCurveInfo>>& AnimCurveInfos, const FSmartName& CurveName)
{
	for (int32 CurveIdx = 0; CurveIdx < AnimCurveInfos.Num(); CurveIdx++)
	{
		// check UID to make sure they match what it's looking for
		if (AnimCurveInfos[CurveIdx]->SmartName.UID == CurveName.UID)
		{
			return CurveIdx;
		}
	}

	return INDEX_NONE;
}

void SAnimCurveViewer::CreateAnimCurveList( const FString& SearchText )
{
	const FSmartNameMapping* Mapping = GetAnimCurveMapping();
	if (Mapping)
	{
		TArray<SmartName::UID_Type> UidList;
		Mapping->FillUidArray(UidList);

		// Get set of active curves
		TMap<FName, float> ActiveCurves;
		UDebugSkelMeshComponent* MeshComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
		UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
		if (!bShowAllCurves && AnimInstance)
		{
			// attribute curve should contain everything
			// so only search other container if attribute is off
			if (CurrentCurveFlag & ACEF_DriveAttribute)
			{
				AnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve, ActiveCurves);
			}
			else
			{
				if (CurrentCurveFlag & ACEF_DriveMorphTarget)
				{
					AnimInstance->GetAnimationCurveList(EAnimCurveType::MorphTargetCurve, ActiveCurves);
				}
				if (CurrentCurveFlag & ACEF_DriveMaterial)
				{
					AnimInstance->GetAnimationCurveList(EAnimCurveType::MaterialCurve, ActiveCurves);
				}
			}
		}
		
		// Iterate through all curves..
		for (SmartName::UID_Type Uid : UidList)
		{
			bool bAddToList = true;

			FSmartName SmartName;
			Mapping->FindSmartNameByUID(Uid, SmartName);

			// See if we pass the search filter
			if (!FilterText.IsEmpty())
			{
				if (!SmartName.DisplayName.ToString().Contains(*FilterText.ToString()))
				{
					bAddToList = false;
				}
			}

			// If we passed that, see if we are filtering to only active
			if (bAddToList && !bShowAllCurves)
			{
				bAddToList = ActiveCurves.Contains(SmartName.DisplayName);
			}

			// If we still want to add
			if (bAddToList)
			{
				// If not already in list, add it
				if (FindIndexOfAnimCurveInfo(AnimCurveList, SmartName) == INDEX_NONE)
				{
					UEditorAnimCurveBoneLinks* EditorMirrorObj = Cast<UEditorAnimCurveBoneLinks> (EditorObjectTracker.GetEditorObjectForClass(UEditorAnimCurveBoneLinks::StaticClass()));
					EditorMirrorObj->Initialize(EditableSkeletonPtr, SmartName, FOnAnimCurveBonesChange::CreateSP(this, &SAnimCurveViewer::ApplyCurveBoneLinks));
					TSharedRef<FDisplayedAnimCurveInfo> NewInfo = FDisplayedAnimCurveInfo::Make(EditableSkeletonPtr, ContainerName, SmartName, EditorMirrorObj);

					// See if we have an override set, and if so, grab info
					float Weight = 0.f;
					bool bOverride = GetAnimCurveOverride(SmartName.DisplayName, Weight);
					NewInfo->bAutoFillData = !bOverride;
					NewInfo->Weight = Weight;

					AnimCurveList.Add(NewInfo);
				}
			}
			// Don't want in list
			else
			{
				// If already in list, remove it
				int32 CurrentIndex = FindIndexOfAnimCurveInfo(AnimCurveList, SmartName);
				if (CurrentIndex != INDEX_NONE)
				{
					AnimCurveList.RemoveAt(CurrentIndex);
				}
			}
		}

		// Sort final list
		struct FSortSmartNamesAlphabetically
		{
			bool operator()(const TSharedPtr<FDisplayedAnimCurveInfo>& A, const TSharedPtr<FDisplayedAnimCurveInfo>& B) const
			{
				return (A.Get()->SmartName.DisplayName.Compare(B.Get()->SmartName.DisplayName) < 0);
			}
		};

		AnimCurveList.Sort(FSortSmartNamesAlphabetically());
	}

	AnimCurveListView->RequestListRefresh();
}

void SAnimCurveViewer::CreateAnimCurveTypeList(TSharedRef<SHorizontalBox> HorizontalBox)
{
	// Add toggle button for 'all curves'
	HorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(3, 1)
	[
		SNew(SCheckBox)
		.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
		.ToolTipText(LOCTEXT("ShowAllCurvesTooltip", "Show all curves, or only active curves."))
		.Type(ESlateCheckBoxType::ToggleButton)
		.IsChecked(this, &SAnimCurveViewer::IsShowingAllCurves)
		.OnCheckStateChanged(this, &SAnimCurveViewer::OnToggleShowingAllCurves)
		.Padding(4)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ShowAllCurves", "All Curves"))
		]
	];

	// Add check box for each curve type flag
	TArray<int32> CurveFlagsToList;
	CurveFlagsToList.Add(ACEF_DriveMorphTarget);
	CurveFlagsToList.Add(ACEF_DriveAttribute);
	CurveFlagsToList.Add(ACEF_DriveMaterial);

	for (int32 CurveFlagIndex = 0; CurveFlagIndex < CurveFlagsToList.Num(); ++CurveFlagIndex)
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(3, 1)
		[
			SNew(SAnimCurveTypeList)
			.IsEnabled(this, &SAnimCurveViewer::IsCurveFilterEnabled)
			.CurveFlags(CurveFlagsToList[CurveFlagIndex])
			.AnimCurveViewerPtr(SharedThis(this))
		];
	}
}

void SAnimCurveViewer::AddAnimCurveOverride( FName& Name, float Weight)
{
	float& Value = OverrideCurves.FindOrAdd(Name);
	Value = Weight;

	UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(GetAnimInstance());
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPreviewCurveOverride(Name, Value, false);
	}
}

void SAnimCurveViewer::RemoveAnimCurveOverride(FName& Name)
{
	OverrideCurves.Remove(Name);

	UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(GetAnimInstance());
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPreviewCurveOverride(Name, 0.f, true);
	}
}

bool SAnimCurveViewer::GetAnimCurveOverride(FName& Name, float& Weight)
{
	Weight = 0.f;
	float* WeightPtr = OverrideCurves.Find(Name);
	if (WeightPtr)
	{
		Weight = *WeightPtr;
		return true;
	}
	else
	{
		return false;
	}
}

void SAnimCurveViewer::OnPostUndo()
{
	RefreshCurveList();
}

void SAnimCurveViewer::OnPreviewAssetChanged(class UAnimationAsset* NewAsset)
{
	OverrideCurves.Empty();
	RefreshCurveList();
}

void SAnimCurveViewer::ApplyCustomCurveOverride(UAnimInstance* AnimInstance) const
{
	for (auto Iter = OverrideCurves.CreateConstIterator(); Iter; ++Iter)
	{ 
		// @todo we might want to save original curve flags? or just change curve to apply flags only
		AnimInstance->AddCurveValue(Iter.Key(), Iter.Value());
	}
}

void SAnimCurveViewer::RefreshCurveList()
{
	CreateAnimCurveList(FilterText.ToString());
}

void SAnimCurveViewer::OnNameCommitted(const FText& InNewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimCurveInfo> Item)
{
	const FSmartNameMapping* Mapping = GetAnimCurveMapping();
	if (Mapping)
	{
		FName NewName(*InNewName.ToString());
		if (NewName == Item->SmartName.DisplayName)
		{
			// Do nothing if trying to rename to existing name...
		}
		else if (!Mapping->Exists(NewName))
		{
			EditableSkeletonPtr.Pin()->RenameSmartname(ContainerName, Item->SmartName.UID, NewName);
			// remove it, so that it can readd it. 
			AnimCurveList.Remove(Item);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("InvalidName"), FText::FromName(NewName) );
			FNotificationInfo Info(FText::Format(LOCTEXT("AnimCurveRenamed", "The name \"{InvalidName}\" is already used."), Args));

			Info.bUseLargeFont = false;
			Info.ExpireDuration = 5.0f;

			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}
}

void SAnimCurveViewer::OnDeleteNameClicked()
{
	TArray< TSharedPtr<FDisplayedAnimCurveInfo> > SelectedItems = AnimCurveListView->GetSelectedItems();
	TArray<FName> SelectedNames;

	for (TSharedPtr<FDisplayedAnimCurveInfo> Item : SelectedItems)
	{
		SelectedNames.Add(Item->SmartName.DisplayName);
	}

	EditableSkeletonPtr.Pin()->RemoveSmartnamesAndFixupAnimations(ContainerName, SelectedNames);
}

bool SAnimCurveViewer::CanDelete()
{
	return AnimCurveListView->GetNumItemsSelected() > 0;
}

void SAnimCurveViewer::OnSelectionChanged(TSharedPtr<FDisplayedAnimCurveInfo> InItem, ESelectInfo::Type SelectInfo)
{
	// make sure the currently selected ones are refreshed if it's first time
	TArray<UObject*> SelectedObjects;

	TArray< TSharedPtr< FDisplayedAnimCurveInfo > > SelectedRows = AnimCurveListView->GetSelectedItems();
	for (auto ItemIt = SelectedRows.CreateIterator(); ItemIt; ++ItemIt)
	{
		TSharedPtr< FDisplayedAnimCurveInfo > RowItem = (*ItemIt);
		UEditorAnimCurveBoneLinks* EditorMirrorObj = RowItem->EditorMirrorObject;
		if (RowItem == InItem)
		{
			// first time selected, refresh
			TArray<FBoneReference> BoneLinks;
			FSmartName CurrentName = RowItem->SmartName;
			const FCurveMetaData* CurveMetaData = EditableSkeletonPtr.Pin()->GetSkeleton().GetCurveMetaData(CurrentName);
			uint32 MaxLOD = 0xFF;
			if (CurveMetaData)
			{
				BoneLinks = CurveMetaData->LinkedBones;
				MaxLOD = CurveMetaData->MaxLOD;
			}

			EditorMirrorObj->Refresh(CurrentName, BoneLinks, MaxLOD);
		}

		SelectedObjects.Add(EditorMirrorObj);
	}

	OnObjectsSelected.ExecuteIfBound(SelectedObjects);
}

void SAnimCurveViewer::ApplyCurveBoneLinks(class UEditorAnimCurveBoneLinks* EditorObj)
{
	if (EditorObj)
	{
		EditableSkeletonPtr.Pin()->SetCurveMetaBoneLinks(EditorObj->CurveName, EditorObj->ConnectedBones, EditorObj->MaxLOD);
	}
}

void SAnimCurveViewer::HandleSmartNamesChange(const FName& InContainerName)
{
	AnimCurveList.Empty();
	RefreshCurveList();
}

#undef LOCTEXT_NAMESPACE

