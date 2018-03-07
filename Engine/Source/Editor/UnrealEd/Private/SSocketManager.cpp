// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSocketManager.h"
#include "Widgets/Layout/SSplitter.h"
#include "UObject/UnrealType.h"
#include "Engine/StaticMesh.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "Components/StaticMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/StaticMeshSocket.h"
#include "UnrealEdGlobals.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor/StaticMeshEditor/Public/IStaticMeshEditor.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

#include "ScopedTransaction.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "SSCSSocketManagerEditor"

struct SocketListItem
{
public:
	SocketListItem(UStaticMeshSocket* InSocket)
		: Socket(InSocket)
	{
	}

	/** The static mesh socket this represents */
	UStaticMeshSocket* Socket;

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;
};

class SSocketDisplayItem : public STableRow< TSharedPtr<FString> >
{
public:
	
	SLATE_BEGIN_ARGS( SSocketDisplayItem )
		{}

		/** The socket this item displays. */
		SLATE_ARGUMENT( TWeakPtr< SocketListItem >, SocketItem )

		/** Pointer back to the socket manager */
		SLATE_ARGUMENT( TWeakPtr< SSocketManager >, SocketManagerPtr )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		SocketItem = InArgs._SocketItem;
		SocketManagerPtr = InArgs._SocketManagerPtr;

		TSharedPtr< SInlineEditableTextBlock > InlineWidget;

		this->ChildSlot
		.Padding( 0.0f, 3.0f, 6.0f, 3.0f )
		.VAlign(VAlign_Center)
		[
			SAssignNew( InlineWidget, SInlineEditableTextBlock )
				.Text( this, &SSocketDisplayItem::GetSocketName )
				.OnVerifyTextChanged( this, &SSocketDisplayItem::OnVerifySocketNameChanged )
				.OnTextCommitted( this, &SSocketDisplayItem::OnCommitSocketName )
				.IsSelected( this, &STableRow< TSharedPtr<FString> >::IsSelectedExclusively )
		];

		TSharedPtr<SocketListItem> SocketItemPinned = SocketItem.Pin();
		if (SocketItemPinned.IsValid())
		{
			SocketItemPinned->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
		}

		STableRow< TSharedPtr<FString> >::ConstructInternal(
			STableRow::FArguments()
				.ShowSelection(true),
			InOwnerTableView
		);	
	}
private:
	/** Returns the socket name */
	FText GetSocketName() const
	{
		TSharedPtr<SocketListItem> SocketItemPinned = SocketItem.Pin();
		return SocketItemPinned.IsValid() ? FText::FromName(SocketItemPinned->Socket->SocketName) : FText();
	}

	bool OnVerifySocketNameChanged( const FText& InNewText, FText& OutErrorMessage )
	{
		bool bVerifyName = true;

		FText NewText = FText::TrimPrecedingAndTrailing(InNewText);
		if(NewText.IsEmpty())
		{
			OutErrorMessage = LOCTEXT( "EmptySocketName_Error", "Sockets must have a name!");
			bVerifyName = false;
		}
		else
		{
			TSharedPtr<SocketListItem> SocketItemPinned = SocketItem.Pin();
			TSharedPtr<SSocketManager> SocketManagerPinned = SocketManagerPtr.Pin();

			if (SocketItemPinned.IsValid() && SocketItemPinned->Socket != nullptr && SocketItemPinned->Socket->SocketName.ToString() != NewText.ToString() &&
				SocketManagerPinned.IsValid() && SocketManagerPinned->CheckForDuplicateSocket(NewText.ToString()))
			{
				OutErrorMessage = LOCTEXT("DuplicateSocket_Error", "Socket name in use!");
				bVerifyName = false;
			}
		}

		return bVerifyName;
	}

	void OnCommitSocketName( const FText& InText, ETextCommit::Type CommitInfo )
	{
		FText NewText = FText::TrimPrecedingAndTrailing(InText);

		TSharedPtr<SocketListItem> PinnedSocketItem = SocketItem.Pin();
		if (PinnedSocketItem.IsValid())
		{
			UStaticMeshSocket* SelectedSocket = PinnedSocketItem->Socket;
			if (SelectedSocket != NULL)
			{
				FScopedTransaction Transaction( LOCTEXT("SetSocketName", "Set Socket Name") );
				
				UProperty* ChangedProperty = FindField<UProperty>( UStaticMeshSocket::StaticClass(), "SocketName" );
				
				// Pre edit, calls modify on the object
				SelectedSocket->PreEditChange(ChangedProperty);

				// Edit the property itself
				SelectedSocket->SocketName = FName(*NewText.ToString());

				// Post edit
				FPropertyChangedEvent PropertyChangedEvent( ChangedProperty );
				SelectedSocket->PostEditChangeProperty(PropertyChangedEvent);
			}
		}
	}

private:
	/** The Socket to display. */
	TWeakPtr< SocketListItem > SocketItem;

	/** Pointer back to the socket manager */
	TWeakPtr< SSocketManager > SocketManagerPtr; 
};

TSharedPtr<ISocketManager> ISocketManager::CreateSocketManager(TSharedPtr<class IStaticMeshEditor> InStaticMeshEditor, FSimpleDelegate InOnSocketSelectionChanged )
{
	TSharedPtr<SSocketManager> SocketManager;
	SAssignNew(SocketManager, SSocketManager)
		.StaticMeshEditorPtr(InStaticMeshEditor)
		.OnSocketSelectionChanged( InOnSocketSelectionChanged );

	TSharedPtr<ISocketManager> ISocket;
	ISocket = StaticCastSharedPtr<ISocketManager>(SocketManager);
	return ISocket;
}

void SSocketManager::Construct(const FArguments& InArgs)
{
	StaticMeshEditorPtr = InArgs._StaticMeshEditorPtr;

	OnSocketSelectionChanged = InArgs._OnSocketSelectionChanged;

	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (!StaticMeshEditorPinned.IsValid())
	{
		return;
	}

	// Register a post undo function which keeps the socket manager list view consistent with the static mesh
	StaticMeshEditorPinned->RegisterOnPostUndo(IStaticMeshEditor::FOnPostUndo::CreateSP(this, &SSocketManager::PostUndo));

	StaticMesh = StaticMeshEditorPinned->GetStaticMesh();

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bLockable = false;
	Args.bAllowSearch = false;
	Args.bShowOptions = false;
	Args.NotifyHook = this;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	SocketDetailsView = PropertyModule.CreateDetailView(Args);

	WorldSpaceRotation = FVector::ZeroVector;

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(.3f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 4)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.Text(LOCTEXT("CreateSocket", "Create Socket"))
						.OnClicked(this, &SSocketManager::CreateSocket_Execute)
						.HAlign(HAlign_Center)
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(SocketListView, SListView<TSharedPtr< SocketListItem > >)

						.SelectionMode(ESelectionMode::Single)

						.ListItemsSource(&SocketList)

						// Generates the actual widget for a tree item
						.OnGenerateRow(this, &SSocketManager::MakeWidgetFromOption)

						// Find out when the user selects something in the tree
						.OnSelectionChanged(this, &SSocketManager::SocketSelectionChanged_Execute)

						// Allow for some spacing between items with a larger item height.
						.ItemHeight(20.0f)

						.OnContextMenuOpening(this, &SSocketManager::OnContextMenuOpening)
						.OnItemScrolledIntoView(this, &SSocketManager::OnItemScrolledIntoView)

						.HeaderRow
						(
						SNew(SHeaderRow)
						.Visibility(EVisibility::Collapsed)
						+ SHeaderRow::Column(TEXT("Socket"))
						)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSeparator)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SSocketManager::GetSocketHeaderText)
					]
				]
			]

			+ SSplitter::Slot()
			.Value(.7f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Visibility(this, &SSocketManager::GetSelectSocketMessageVisibility)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoSocketSelected", "Select a Socket"))
					]
				]

				+ SOverlay::Slot()
				[
					SocketDetailsView.ToSharedRef()
				]
			]
		]
	];

	RefreshSocketList();

	AddPropertyChangeListenerToSockets();
}

SSocketManager::~SSocketManager()
{
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (StaticMeshEditorPinned.IsValid())
	{
		StaticMeshEditorPinned->UnregisterOnPostUndo(this);
	}

	RemovePropertyChangeListenerFromSockets();
}

UStaticMeshSocket* SSocketManager::GetSelectedSocket() const
{
	if( SocketListView->GetSelectedItems().Num())
	{
		return SocketListView->GetSelectedItems()[0]->Socket;
	}

	return nullptr;
}

EVisibility SSocketManager::GetSelectSocketMessageVisibility() const
{
	return SocketListView->GetSelectedItems().Num() > 0 ? EVisibility::Hidden : EVisibility::Visible;
}

void SSocketManager::SetSelectedSocket(UStaticMeshSocket* InSelectedSocket)
{
	if (InSelectedSocket)
	{
		for( int32 i=0; i < SocketList.Num(); i++)
		{
			if(SocketList[i]->Socket == InSelectedSocket)
			{
				SocketListView->SetSelection(SocketList[i]);

				SocketListView->RequestListRefresh();

				SocketSelectionChanged(InSelectedSocket);

				break;
			}
		}
	}
	else
	{
		SocketListView->ClearSelection();

		SocketListView->RequestListRefresh();

		SocketSelectionChanged(NULL);
	}
}

TSharedRef< ITableRow > SSocketManager::MakeWidgetFromOption( TSharedPtr<SocketListItem> InItem, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew( SSocketDisplayItem, OwnerTable )
				.SocketItem(InItem)
				.SocketManagerPtr(SharedThis(this));
}

void SSocketManager::CreateSocket()
{
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (StaticMeshEditorPinned.IsValid())
	{
		UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();

		const FScopedTransaction Transaction( LOCTEXT( "CreateSocket", "Create Socket" ) );

		UStaticMeshSocket* NewSocket = NewObject<UStaticMeshSocket>(CurrentStaticMesh);
		check(NewSocket);

		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.CreateSocket"));
		}

		FString SocketNameString = TEXT("Socket");
		FName SocketName = FName(*SocketNameString);

		// Make sure the new name is valid
		int32 Index = 0;
		while (CheckForDuplicateSocket(SocketName.ToString()))
		{
			SocketName = FName(*FString::Printf(TEXT("%s%i"), *SocketNameString, Index));
			++Index;
		}


		NewSocket->SocketName = SocketName;
		NewSocket->SetFlags( RF_Transactional );
		NewSocket->OnPropertyChanged().AddSP( this, &SSocketManager::OnSocketPropertyChanged );

		CurrentStaticMesh->PreEditChange(NULL);
		CurrentStaticMesh->Sockets.Add(NewSocket);
		CurrentStaticMesh->PostEditChange();
		CurrentStaticMesh->MarkPackageDirty();

		TSharedPtr< SocketListItem > SocketItem = MakeShareable( new SocketListItem(NewSocket) );
		SocketList.Add( SocketItem );
		SocketListView->RequestListRefresh();

		SocketListView->SetSelection(SocketItem);
		RequestRenameSelectedSocket();
	}
}

void SSocketManager::DuplicateSelectedSocket()
{
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();

	UStaticMeshSocket* SelectedSocket = GetSelectedSocket();

	if(StaticMeshEditorPinned.IsValid() && SelectedSocket)
	{
		const FScopedTransaction Transaction( LOCTEXT( "SocketManager_DuplicateSocket", "Duplicate Socket" ) );

		UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();

		UStaticMeshSocket* NewSocket = DuplicateObject(SelectedSocket, CurrentStaticMesh);

		// Create a unique name for this socket
		NewSocket->SocketName = MakeUniqueObjectName(CurrentStaticMesh, UStaticMeshSocket::StaticClass(), NewSocket->SocketName);

		// Add the new socket to the static mesh
		CurrentStaticMesh->PreEditChange(NULL);
		CurrentStaticMesh->Sockets.Add(NewSocket);
		CurrentStaticMesh->PostEditChange();
		CurrentStaticMesh->MarkPackageDirty();

		RefreshSocketList();

		// Select the duplicated socket
		SetSelectedSocket(NewSocket);
	}
}


void SSocketManager::UpdateStaticMesh()
{
	RefreshSocketList();
}

void SSocketManager::RequestRenameSelectedSocket()
{
	if(SocketListView->GetSelectedItems().Num() == 1)
	{
		TSharedPtr< SocketListItem > SocketItem = SocketListView->GetSelectedItems()[0];
		SocketListView->RequestScrollIntoView(SocketItem);
		DeferredRenameRequest = SocketItem;
	}
}

void SSocketManager::DeleteSelectedSocket()
{
	if(SocketListView->GetSelectedItems().Num())
	{
		const FScopedTransaction Transaction( LOCTEXT( "DeleteSocket", "Delete Socket" ) );

		TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
		if (StaticMeshEditorPinned.IsValid())
		{
			UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();
			CurrentStaticMesh->PreEditChange(NULL);
			UStaticMeshSocket* SelectedSocket = SocketListView->GetSelectedItems()[ 0 ]->Socket;
			SelectedSocket->OnPropertyChanged().RemoveAll( this );
			CurrentStaticMesh->Sockets.Remove(SelectedSocket);
			CurrentStaticMesh->PostEditChange();

			RefreshSocketList();
		}
	}
}

void SSocketManager::RefreshSocketList()
{
	// The static mesh might not be the same one we built the SocketListView with
	// check it here and update it if necessary.
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (StaticMeshEditorPinned.IsValid())
	{
		bool bIsSameStaticMesh = true;
		UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();
		if (!StaticMesh.IsValid() || CurrentStaticMesh != StaticMesh.Get())
		{
			StaticMesh = CurrentStaticMesh;
			bIsSameStaticMesh = false;
		}
		// Only rebuild the socket list if it differs from the static meshes socket list
		// This is done so that an undo on a socket property doesn't cause the selected
		// socket to be de-selected, thus hiding the socket properties on the detail view.
		// NB: Also force a rebuild if the underlying StaticMesh has been changed.
		if (StaticMesh->Sockets.Num() != SocketList.Num() || !bIsSameStaticMesh)
		{
			SocketList.Empty();
			for (int32 i = 0; i < StaticMesh->Sockets.Num(); i++)
			{
				UStaticMeshSocket* Socket = StaticMesh->Sockets[i];
				SocketList.Add(MakeShareable(new SocketListItem(Socket)));
			}

			SocketListView->RequestListRefresh();
		}

		// Set the socket on the detail view to keep it in sync with the sockets properties
		if (SocketListView->GetSelectedItems().Num())
		{
			TArray< UObject* > ObjectList;
			ObjectList.Add(SocketListView->GetSelectedItems()[0]->Socket);
			SocketDetailsView->SetObjects(ObjectList, true);
		}

		StaticMeshEditorPinned->RefreshViewport();
	}
	else
	{
		SocketList.Empty();
		SocketListView->ClearSelection();
		SocketListView->RequestListRefresh();
	}
}

bool SSocketManager::CheckForDuplicateSocket(const FString& InSocketName)
{
	for( int32 i=0; i < SocketList.Num(); i++)
	{
		if(SocketList[i]->Socket->SocketName.ToString() == InSocketName)
		{
			return true;
		}
	}

	return false;
}

void SSocketManager::SocketSelectionChanged(UStaticMeshSocket* InSocket)
{
	TArray<UObject*> SelectedObject;

	if(InSocket)
	{
		SelectedObject.Add(InSocket);
	}

	SocketDetailsView->SetObjects(SelectedObject);

	// Notify listeners
	OnSocketSelectionChanged.ExecuteIfBound();
}

void SSocketManager::SocketSelectionChanged_Execute( TSharedPtr<SocketListItem> InItem, ESelectInfo::Type /*SelectInfo*/ )
{
	if(InItem.IsValid())
	{
		SocketSelectionChanged(InItem->Socket);
	}
	else
	{
		SocketSelectionChanged(NULL);
	}
}

FReply SSocketManager::CreateSocket_Execute()
{
	CreateSocket();

	return FReply::Handled();
}

FText SSocketManager::GetSocketHeaderText() const
{
	UStaticMesh* CurrentStaticMesh = nullptr;
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (StaticMeshEditorPinned.IsValid())
	{
		CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();
	}
	return FText::Format(LOCTEXT("SocketHeader_TotalFmt", "{0} sockets"), FText::AsNumber((CurrentStaticMesh != nullptr) ? CurrentStaticMesh->Sockets.Num() : 0));
}

void SSocketManager::SocketName_TextChanged(const FText& InText)
{
	CheckForDuplicateSocket(InText.ToString());
}

TSharedPtr<SWidget> SSocketManager::OnContextMenuOpening()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;

	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (!StaticMeshEditorPinned.IsValid())
	{
		return TSharedPtr<SWidget>();
	}

	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, StaticMeshEditorPinned->GetToolkitCommands());

	{
		MenuBuilder.BeginSection("BasicOperations");
		{
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SSocketManager::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged )
{
	TArray< TSharedPtr< SocketListItem > > SelectedList = SocketListView->GetSelectedItems();
	if(SelectedList.Num())
	{
		if(PropertyThatChanged->GetName() == TEXT("Pitch") || PropertyThatChanged->GetName() == TEXT("Yaw") || PropertyThatChanged->GetName() == TEXT("Roll"))
		{
			const UStaticMeshSocket* Socket = SelectedList[0]->Socket;
			WorldSpaceRotation.Set( Socket->RelativeRotation.Pitch, Socket->RelativeRotation.Yaw, Socket->RelativeRotation.Roll );
		}
	}
}

void SSocketManager::AddPropertyChangeListenerToSockets()
{
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (StaticMeshEditorPinned.IsValid())
	{
		UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();
		for (int32 i = 0; i < CurrentStaticMesh->Sockets.Num(); ++i)
		{
			CurrentStaticMesh->Sockets[i]->OnPropertyChanged().AddSP(this, &SSocketManager::OnSocketPropertyChanged);
		}
	}
}

void SSocketManager::RemovePropertyChangeListenerFromSockets()
{
	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (StaticMeshEditorPinned.IsValid())
	{
		UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();
		if (CurrentStaticMesh)
		{
			for (int32 i = 0; i < CurrentStaticMesh->Sockets.Num(); ++i)
			{
				CurrentStaticMesh->Sockets[i]->OnPropertyChanged().RemoveAll(this);
			}
		}
	}
}

void SSocketManager::OnSocketPropertyChanged( const UStaticMeshSocket* Socket, const UProperty* ChangedProperty )
{
	static FName RelativeRotationName(TEXT("RelativeRotation"));
	static FName RelativeLocationName(TEXT("RelativeLocation"));

	check(Socket != nullptr);

	FName ChangedPropertyName = ChangedProperty->GetFName();

	if ( ChangedPropertyName == RelativeRotationName )
	{
		const UStaticMeshSocket* SelectedSocket = GetSelectedSocket();

		if( Socket == SelectedSocket )
		{
			WorldSpaceRotation.Set( Socket->RelativeRotation.Pitch, Socket->RelativeRotation.Yaw, Socket->RelativeRotation.Roll );
		}
	}

	TSharedPtr<IStaticMeshEditor> StaticMeshEditorPinned = StaticMeshEditorPtr.Pin();
	if (!StaticMeshEditorPinned.IsValid())
	{
		return;
	}

	if (ChangedPropertyName == RelativeRotationName || ChangedPropertyName == RelativeLocationName)
	{
		// If socket location or rotation is changed, update the position of any actors attached to it in instances of this mesh
		UStaticMesh* CurrentStaticMesh = StaticMeshEditorPinned->GetStaticMesh();
		if (CurrentStaticMesh != nullptr)
		{
			bool bUpdatedChild = false;

			for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
			{
				if (It->GetStaticMesh() == CurrentStaticMesh)
				{
					const AActor* Actor = It->GetOwner();
					if (Actor != nullptr)
					{
						const USceneComponent* Root = Actor->GetRootComponent();
						if (Root != nullptr)
						{
							for (USceneComponent* Child : Root->GetAttachChildren())
							{
								if (Child != nullptr && Child->GetAttachSocketName() == Socket->SocketName)
								{
									Child->UpdateComponentToWorld();
									bUpdatedChild = true;
								}
							}
						}
					}
				}
			}

			if (bUpdatedChild)
			{
				GUnrealEd->RedrawLevelEditingViewports();
			}
		}
	}
}

void SSocketManager::PostUndo()
{
	RefreshSocketList();
}

void SSocketManager::OnItemScrolledIntoView( TSharedPtr<SocketListItem> InItem, const TSharedPtr<ITableRow>& InWidget)
{
	TSharedPtr<SocketListItem> DeferredRenameRequestPinned = DeferredRenameRequest.Pin();
	if( DeferredRenameRequestPinned.IsValid() )
	{
		DeferredRenameRequestPinned->OnRenameRequested.ExecuteIfBound();
		DeferredRenameRequest.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
