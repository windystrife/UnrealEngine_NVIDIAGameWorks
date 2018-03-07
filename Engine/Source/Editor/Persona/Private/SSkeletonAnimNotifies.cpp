// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SSkeletonAnimNotifies.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetData.h"
#include "Animation/AnimSequenceBase.h"
#include "EditorStyleSet.h"
#include "Animation/EditorSkeletonNotifyObj.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ScopedTransaction.h"
#include "IEditableSkeleton.h"
#include "TabSpawners.h"

#define LOCTEXT_NAMESPACE "SkeletonAnimNotifies"

static const FName ColumnId_AnimNotifyNameLabel( "AnimNotifyName" );

//////////////////////////////////////////////////////////////////////////
// SMorphTargetListRow

typedef TSharedPtr< FDisplayedAnimNotifyInfo > FDisplayedAnimNotifyInfoPtr;

class SAnimNotifyListRow
	: public SMultiColumnTableRow< FDisplayedAnimNotifyInfoPtr >
{
public:

	SLATE_BEGIN_ARGS( SAnimNotifyListRow ) {}

	/** The item for this row **/
	SLATE_ARGUMENT( FDisplayedAnimNotifyInfoPtr, Item )

	/* Widget used to display the list of morph targets */
	SLATE_ARGUMENT( TSharedPtr<SSkeletonAnimNotifies>, NotifiesListView )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView );

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

private:

	/** Widget used to display the list of notifies */
	TSharedPtr<SSkeletonAnimNotifies> NotifiesListView;

	/** The notify being displayed by this row */
	FDisplayedAnimNotifyInfoPtr	Item;
};

void SAnimNotifyListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	NotifiesListView = InArgs._NotifiesListView;

	check( Item.IsValid() );

	SMultiColumnTableRow< FDisplayedAnimNotifyInfoPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SAnimNotifyListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	check( ColumnName == ColumnId_AnimNotifyNameLabel );

	return SNew( SVerticalBox )
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding( 0.0f, 4.0f )
	.VAlign( VAlign_Center )
	[
		SAssignNew(Item->InlineEditableText, SInlineEditableTextBlock)
		.Text( FText::FromName(Item->Name) )
		.OnVerifyTextChanged(NotifiesListView.Get(), &SSkeletonAnimNotifies::OnVerifyNotifyNameCommit, Item)
		.OnTextCommitted(NotifiesListView.Get(), &SSkeletonAnimNotifies::OnNotifyNameCommitted, Item)
		.IsSelected(NotifiesListView.Get(), &SSkeletonAnimNotifies::IsSelected)
	];
}

/////////////////////////////////////////////////////
// FSkeletonAnimNotifiesSummoner

FSkeletonAnimNotifiesSummoner::FSkeletonAnimNotifiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnChangeAnimNotifies, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonAnimNotifiesID, InHostingApp)
	, EditableSkeleton(InEditableSkeleton)
	, OnChangeAnimNotifies(InOnChangeAnimNotifies)
	, OnPostUndo(InOnPostUndo)
	, OnObjectsSelected(InOnObjectsSelected)
{
	TabLabel = LOCTEXT("SkeletonAnimNotifiesTabTitle", "Animation Notifies");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.AnimationNotifies");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonAnimNotifiesMenu", "Animation Notifies");
	ViewMenuTooltip = LOCTEXT("SkeletonAnimNotifies_ToolTip", "Shows the skeletons notifies list");
}

TSharedRef<SWidget> FSkeletonAnimNotifiesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonAnimNotifies, EditableSkeleton.Pin().ToSharedRef(), OnChangeAnimNotifies, OnPostUndo)
		.OnObjectsSelected(OnObjectsSelected);
}

/////////////////////////////////////////////////////
// SSkeletonAnimNotifies

void SSkeletonAnimNotifies::Construct(const FArguments& InArgs, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnChangeAnimNotifies, FSimpleMulticastDelegate& InOnPostUndo)
{
	OnObjectsSelected = InArgs._OnObjectsSelected;
	EditableSkeleton = InEditableSkeleton;

	InOnChangeAnimNotifies.Add(FSimpleDelegate::CreateSP( this, &SSkeletonAnimNotifies::PostUndo ) );
	InOnPostUndo.Add(FSimpleDelegate::CreateSP( this, &SSkeletonAnimNotifies::RefreshNotifiesListWithFilter ) );

	EditableSkeleton->RegisterOnNotifiesChanged(FSimpleDelegate::CreateSP(this, &SSkeletonAnimNotifies::OnNotifiesChanged));

	this->ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SAssignNew( NameFilterBox, SSearchBox )
			.SelectAllTextWhenFocused( true )
			.OnTextChanged( this, &SSkeletonAnimNotifies::OnFilterTextChanged )
			.OnTextCommitted( this, &SSkeletonAnimNotifies::OnFilterTextCommitted )
			.HintText( LOCTEXT( "NotifiesSearchBoxHint", "Search Animation Notifies...") )
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( NotifiesListView, SAnimNotifyListType )
			.ListItemsSource( &NotifyList )
			.OnGenerateRow( this, &SSkeletonAnimNotifies::GenerateNotifyRow )
			.OnContextMenuOpening( this, &SSkeletonAnimNotifies::OnGetContextMenuContent )
			.OnSelectionChanged( this, &SSkeletonAnimNotifies::OnNotifySelectionChanged )
			.ItemHeight( 22.0f )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_AnimNotifyNameLabel )
				.DefaultLabel( LOCTEXT( "AnimNotifyNameLabel", "Notify Name" ) )
			)
		]
	];

	CreateNotifiesList();
}

void SSkeletonAnimNotifies::OnNotifiesChanged()
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SSkeletonAnimNotifies::GenerateNotifyRow(TSharedPtr<FDisplayedAnimNotifyInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SAnimNotifyListRow, OwnerTable )
		.Item( InInfo )
		.NotifiesListView( SharedThis( this ) );
}

TSharedPtr<SWidget> SSkeletonAnimNotifies::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("AnimNotifyAction", LOCTEXT( "AnimNotifyActions", "Selected Notify Actions" ) );
	{
		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnDeleteAnimNotify ), 
				FCanExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::CanPerformDelete ) );
			const FText Label = LOCTEXT("DeleteAnimNotifyButtonLabel", "Delete");
			const FText ToolTipText = LOCTEXT("DeleteAnimNotifyButtonTooltip", "Deletes the selected anim notifies.");
			MenuBuilder.AddMenuEntry( Label, ToolTipText, FSlateIcon(), Action);
		}

		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnRenameAnimNotify ), 
				FCanExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::CanPerformRename ) );
			const FText Label = LOCTEXT("RenameAnimNotifyButtonLabel", "Rename");
			const FText ToolTipText = LOCTEXT("RenameAnimNotifyButtonTooltip", "Renames the selected anim notifies.");
			MenuBuilder.AddMenuEntry( Label, ToolTipText, FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonAnimNotifies::OnNotifySelectionChanged(TSharedPtr<FDisplayedAnimNotifyInfo> Selection, ESelectInfo::Type SelectInfo)
{
	if(Selection.IsValid())
	{
		ShowNotifyInDetailsView(Selection->Name);
	}
}

bool SSkeletonAnimNotifies::CanPerformDelete() const
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();
	return SelectedRows.Num() > 0;
}

bool SSkeletonAnimNotifies::CanPerformRename() const
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();
	return SelectedRows.Num() == 1;
}

void SSkeletonAnimNotifies::OnDeleteAnimNotify()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	// this one deletes all notifies with same name. 
	TArray<FName> SelectedNotifyNames;

	for(int Selection = 0; Selection < SelectedRows.Num(); ++Selection)
	{
		SelectedNotifyNames.Add(SelectedRows[Selection]->Name);
	}

	int32 NumAnimationsModified = EditableSkeleton->DeleteAnimNotifies(SelectedNotifyNames);

	if(NumAnimationsModified > 0)
	{
		// Tell the user that the socket is a duplicate
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
		FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesDeleted", "{NumAnimationsModified} animation(s) modified to delete notifications" ), Args ) );

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		NotifyUser( Info );
	}

	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::OnRenameAnimNotify()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	check(SelectedRows.Num() == 1); // Should be guaranteed by CanPerformRename

	SelectedRows[0]->InlineEditableText->EnterEditingMode();
}

bool SSkeletonAnimNotifies::OnVerifyNotifyNameCommit( const FText& NewName, FText& OutErrorMessage, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	bool bValid(true);

	if(NewName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT( "NameMissing_Error", "You must provide a name." );
		bValid = false;
	}

	FName NotifyName( *NewName.ToString() );
	if(NotifyName != Item->Name)
	{
		if(EditableSkeleton->GetSkeleton().AnimationNotifies.Contains(NotifyName))
		{
			OutErrorMessage = FText::Format( LOCTEXT("AlreadyInUseMessage", "'{0}' is already in use."), NewName );
			bValid = false;
		}
	}

	return bValid;
}

void SSkeletonAnimNotifies::OnNotifyNameCommitted( const FText& NewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	int32 NumAnimationsModified = EditableSkeleton->RenameNotify(FName(*NewName.ToString()), Item->Name);

	if(NumAnimationsModified > 0)
	{
		// Tell the user that the socket is a duplicate
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
		FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesRenamed", "{NumAnimationsModified} animation(s) modified to rename notification" ), Args ) );

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		NotifyUser( Info );
	}

	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::RefreshNotifiesListWithFilter()
{
	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::CreateNotifiesList( const FString& SearchText )
{
	NotifyList.Empty();

	for(int i = 0; i < EditableSkeleton->GetSkeleton().AnimationNotifies.Num(); ++i)
	{
		const FName& NotifyName = EditableSkeleton->GetSkeleton().AnimationNotifies[i];
		if ( !SearchText.IsEmpty() )
		{
			if ( NotifyName.ToString().Contains( SearchText ) )
			{
				NotifyList.Add( FDisplayedAnimNotifyInfo::Make( NotifyName ) );
			}
		}
		else
		{
			NotifyList.Add( FDisplayedAnimNotifyInfo::Make( NotifyName ) );
		}
	}

	NotifiesListView->RequestListRefresh();
}

void SSkeletonAnimNotifies::ShowNotifyInDetailsView(FName NotifyName)
{
	UEditorSkeletonNotifyObj *Obj = Cast<UEditorSkeletonNotifyObj>(ShowInDetailsView(UEditorSkeletonNotifyObj::StaticClass()));
	if(Obj != NULL)
	{
		Obj->AnimationNames.Empty();

		TArray<FAssetData> CompatibleAnimSequences;
		EditableSkeleton->GetCompatibleAnimSequences(CompatibleAnimSequences);

		for( int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex )
		{
			const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
			if(UObject* AnimSeqAsset = PossibleAnimSequence.GetAsset())
			{
				UAnimSequenceBase* Sequence = CastChecked<UAnimSequenceBase>(AnimSeqAsset);
				for (int32 NotifyIndex = 0; NotifyIndex < Sequence->Notifies.Num(); ++NotifyIndex)
				{
					FAnimNotifyEvent& NotifyEvent = Sequence->Notifies[NotifyIndex];
					if (NotifyEvent.NotifyName == NotifyName)
					{
						Obj->AnimationNames.Add(MakeShareable(new FString(PossibleAnimSequence.AssetName.ToString())));
						break;
					}
				}
			}
		}

		Obj->Name = NotifyName;
	}
}

UObject* SSkeletonAnimNotifies::ShowInDetailsView( UClass* EdClass )
{
	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		TArray<UObject*> Objects;
		Objects.Add(Obj);
		OnObjectsSelected.ExecuteIfBound(Objects);
	}
	return Obj;
}

void SSkeletonAnimNotifies::ClearDetailsView()
{
	TArray<UObject*> Objects;
	OnObjectsSelected.ExecuteIfBound(Objects);
}

void SSkeletonAnimNotifies::PostUndo()
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

void SSkeletonAnimNotifies::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}
#undef LOCTEXT_NAMESPACE
