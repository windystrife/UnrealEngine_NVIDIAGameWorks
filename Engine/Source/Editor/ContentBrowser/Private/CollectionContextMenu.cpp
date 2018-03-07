// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CollectionContextMenu.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "EditorStyleSet.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "ContentBrowserUtils.h"
#include "CollectionViewUtils.h"
#include "ContentBrowserModule.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Framework/Commands/GenericCommands.h"
#include "Settings/ContentBrowserSettings.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"


FCollectionContextMenu::FCollectionContextMenu(const TWeakPtr<SCollectionView>& InCollectionView)
	: CollectionView(InCollectionView)
	, bProjectUnderSourceControl(false)
{
}

void FCollectionContextMenu::BindCommands(TSharedPtr< FUICommandList > InCommandList)
{
	InCommandList->MapAction( FGenericCommands::Get().Rename, FUIAction(
		FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteRenameCollection ),
		FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteRenameCollection )
		));
}

TSharedPtr<SWidget> FCollectionContextMenu::MakeCollectionTreeContextMenu(TSharedPtr< FUICommandList > InCommandList)
{
	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllCollectionListContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, InCommandList, MenuExtender);

	UpdateProjectSourceControl();

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	bool bAnyManagedBySCC = false;
	bool bAnyNeedSCCUpdate = false;
	bool bAnyNeedSave = false;
		
	for (int32 CollectionIdx = 0; CollectionIdx < SelectedCollections.Num(); ++CollectionIdx)
	{
		bAnyManagedBySCC |= SelectedCollections[CollectionIdx]->CollectionType != ECollectionShareType::CST_Local;
		bAnyNeedSCCUpdate |= SelectedCollections[CollectionIdx]->CurrentStatus == ECollectionItemStatus::IsOutOfDate;
		bAnyNeedSave |= SelectedCollections[CollectionIdx]->CurrentStatus == ECollectionItemStatus::HasLocalChanges;

		if (bAnyManagedBySCC && bAnyNeedSCCUpdate && bAnyNeedSave)
		{
			// Found collections to turn all options on, break now
			break;
		}
	}

	MenuBuilder.BeginSection("CollectionOptions", LOCTEXT("CollectionListOptionsMenuHeading", "Collection Options"));
	{
		const bool bHasSingleSelectedCollection = SelectedCollections.Num() == 1;
		const bool bIsFirstSelectedCollectionStatic = SelectedCollections.Num() > 0 && SelectedCollections[0]->StorageMode == ECollectionStorageMode::Static;

		{
			TOptional<FCollectionNameType> ParentCollection;
			if (SelectedCollections.Num() > 0) 
			{
				ParentCollection = FCollectionNameType(SelectedCollections[0]->CollectionName, SelectedCollections[0]->CollectionType);
			}

			// New... (submenu)
			MenuBuilder.AddSubMenu(
				LOCTEXT("NewChildCollection", "New..."),
				LOCTEXT("NewChildCollectionTooltip", "Create child a collection."),
				FNewMenuDelegate::CreateRaw( this, &FCollectionContextMenu::MakeNewCollectionSubMenu, ECollectionStorageMode::Static, SCollectionView::FCreateCollectionPayload( ParentCollection ) ),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction::CreateLambda( [=]{ return bHasSingleSelectedCollection && bIsFirstSelectedCollectionStatic; } )
					),
				NAME_None,
				EUserInterfaceActionType::Button
				);
		}

		// Rename
		MenuBuilder.AddMenuEntry( FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameCollection", "Rename"), LOCTEXT("RenameCollectionTooltip", "Rename this collection."));

		// Set Share Type
		MenuBuilder.AddSubMenu(
			LOCTEXT("SetCollectionShareType", "Set Share Type"),
			LOCTEXT("SetCollectionShareTypeTooltip", "Change the share type of this collection."),
			FNewMenuDelegate::CreateRaw( this, &FCollectionContextMenu::MakeCollectionShareTypeSubMenu ),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateLambda( [=]{ return bHasSingleSelectedCollection; } )
				),
			NAME_None,
			EUserInterfaceActionType::Button
			);

		// If any colors have already been set, display color options as a sub menu
		if ( CollectionViewUtils::HasCustomColors() )
		{
			// Set Color (submenu)
			MenuBuilder.AddSubMenu(
				LOCTEXT("SetColor", "Set Color"),
				LOCTEXT("SetCollectionColorTooltip", "Sets the color this collection should appear as."),
				FNewMenuDelegate::CreateRaw( this, &FCollectionContextMenu::MakeSetColorSubMenu )
				);
		}
		else
		{
			// Set Color
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetColor", "Set Color"),
				LOCTEXT("SetCollectionColorTooltip", "Sets the color this collection should appear as."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecutePickColor ) )
				);
		}
	}
	MenuBuilder.EndSection();

	if ( SelectedCollections.Num() > 0 )
	{
		MenuBuilder.BeginSection("CollectionBulkOperations", LOCTEXT("CollectionListBulkOperationsMenuHeading", "Bulk Operations"));
		{
			// Save
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SaveCollection", "Save"),
				LOCTEXT("SaveCollectionTooltip", "Save this collection."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteSaveCollection ),
					FCanExecuteAction::CreateLambda( [=]{ return bAnyNeedSave; } )
					)
				);

			// Delete
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DestroyCollection", "Delete"),
				LOCTEXT("DestroyCollectionTooltip", "Delete this collection."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteDestroyCollection ),
					FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteDestroyCollection, bAnyManagedBySCC )
					)
				);

			// Update
			MenuBuilder.AddMenuEntry(
				LOCTEXT("UpdateCollection", "Update"),
				LOCTEXT("UpdateCollectionTooltip", "Update this collection to make sure it's using the latest version from source control."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteUpdateCollection ),
					FCanExecuteAction::CreateLambda( [=]{ return bAnyNeedSCCUpdate; } )
					)
				);

			// Refresh
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RefreshCollection", "Refresh"),
				LOCTEXT("RefreshCollectionTooltip", "Refresh the source control status of this collection."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteRefreshCollection ),
					FCanExecuteAction::CreateLambda( [=]{ return bAnyManagedBySCC; } )
					)
				);
		}
		MenuBuilder.EndSection();
	}
	
	return MenuBuilder.MakeWidget();
}

void FCollectionContextMenu::MakeNewCollectionSubMenu(FMenuBuilder& MenuBuilder, ECollectionStorageMode::Type StorageMode, SCollectionView::FCreateCollectionPayload InCreationPayload)
{
	const FText MenuHeading = FText::Format(
		(InCreationPayload.ParentCollection.IsSet()) ? LOCTEXT("NewXChildCollectionMenuHeading", "New {0} Child Collection") : LOCTEXT("NewXCollectionMenuHeading", "New {0} Collection"),
		ECollectionStorageMode::ToText(StorageMode)
		);

	MenuBuilder.BeginSection("CollectionNewCollection", MenuHeading);
	{
		const bool bCanCreateSharedChildren = !InCreationPayload.ParentCollection.IsSet() || ECollectionShareType::IsValidChildType( InCreationPayload.ParentCollection->Type, ECollectionShareType::CST_Shared );
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewCollection_Shared", "Shared Collection"),
			LOCTEXT("NewCollection_SharedTooltip", "Create a collection that can be seen by anyone."),
			FSlateIcon( FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName( ECollectionShareType::CST_Shared ) ),
			FUIAction(
				FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteNewCollection, ECollectionShareType::CST_Shared, StorageMode, InCreationPayload ),
				FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteNewCollection, ECollectionShareType::CST_Shared, bCanCreateSharedChildren )
				)
			);

		const bool bCanCreatePrivateChildren = !InCreationPayload.ParentCollection.IsSet() || ECollectionShareType::IsValidChildType( InCreationPayload.ParentCollection->Type, ECollectionShareType::CST_Private );
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewCollection_Private", "Private Collection"),
			LOCTEXT("NewCollection_PrivateTooltip", "Create a collection that can only be seen by you."),
			FSlateIcon( FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName( ECollectionShareType::CST_Private ) ),
			FUIAction(
				FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteNewCollection, ECollectionShareType::CST_Private, StorageMode, InCreationPayload ),
				FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteNewCollection, ECollectionShareType::CST_Private, bCanCreatePrivateChildren )
				)
			);

		const bool bCanCreateLocalChildren = !InCreationPayload.ParentCollection.IsSet() || ECollectionShareType::IsValidChildType( InCreationPayload.ParentCollection->Type, ECollectionShareType::CST_Local );
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewCollection_Local", "Local Collection"),
			LOCTEXT("NewCollection_LocalTooltip", "Create a collection that is not in source control and can only be seen by you."),
			FSlateIcon( FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName( ECollectionShareType::CST_Local ) ),
			FUIAction(
				FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteNewCollection, ECollectionShareType::CST_Local, StorageMode, InCreationPayload ),
				FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteNewCollection, ECollectionShareType::CST_Local, bCanCreateLocalChildren )
				)
			);
	}
	MenuBuilder.EndSection();
}

void FCollectionContextMenu::MakeSaveDynamicCollectionSubMenu(FMenuBuilder& MenuBuilder, FText InSearchQuery)
{
	auto OnCollectionCreated = FCollectionItem::FCollectionCreatedEvent::CreateSP(this, &FCollectionContextMenu::ExecuteSaveDynamicCollection, InSearchQuery);

	// Create new root level collection
	MakeNewCollectionSubMenu(MenuBuilder, ECollectionStorageMode::Dynamic, SCollectionView::FCreateCollectionPayload(OnCollectionCreated));

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	
	TArray<FCollectionNameType> AvailableCollections;
	CollectionManagerModule.Get().GetCollections(AvailableCollections);

	AvailableCollections.Sort([](const FCollectionNameType& One, const FCollectionNameType& Two) -> bool
	{
		return One.Name < Two.Name;
	});

	if (AvailableCollections.Num() > 0)
	{
		MenuBuilder.BeginSection("CollectionReplaceCollection", LOCTEXT("OverwriteDynamicCollectionMenuHeading", "Overwrite Dynamic Collection"));

		for (const FCollectionNameType& AvailableCollection : AvailableCollections)
		{
			// Never display system collections
			if (AvailableCollection.Type == ECollectionShareType::CST_System)
			{
				continue;
			}

			// Can only overwrite dynamic collections
			ECollectionStorageMode::Type StorageMode = ECollectionStorageMode::Static;
			CollectionManagerModule.Get().GetCollectionStorageMode(AvailableCollection.Name, AvailableCollection.Type, StorageMode);
			if (StorageMode != ECollectionStorageMode::Dynamic)
			{
				continue;
			}

			MenuBuilder.AddMenuEntry(
				FText::FromName(AvailableCollection.Name), 
				FText::Format(LOCTEXT("SaveDynamicCollection_OverwriteExistingCollectionToolTip", "Overwrite '{0}' with the current search query"), FText::FromName(AvailableCollection.Name)),
				FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(AvailableCollection.Type)),
				FUIAction(
					FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteSaveDynamicCollection, AvailableCollection, InSearchQuery ),
					FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteSaveDynamicCollection, AvailableCollection )
					)
				);
		}

		MenuBuilder.EndSection();
	}
}

void FCollectionContextMenu::MakeCollectionShareTypeSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("CollectionShareType", LOCTEXT("CollectionShareTypeMenuHeading", "Collection Share Type"));
	{
		MenuBuilder.AddMenuEntry(
			ECollectionShareType::ToText( ECollectionShareType::CST_Shared ),
			ECollectionShareType::GetDescription( ECollectionShareType::CST_Shared ),
			FSlateIcon( FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName( ECollectionShareType::CST_Shared ) ),
			FUIAction(
				FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteSetCollectionShareType, ECollectionShareType::CST_Shared ),
				FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteSetCollectionShareType, ECollectionShareType::CST_Shared ),
				FIsActionChecked::CreateSP( this, &FCollectionContextMenu::IsSetCollectionShareTypeChecked, ECollectionShareType::CST_Shared )
				),
			NAME_None,
			EUserInterfaceActionType::Check
			);

		MenuBuilder.AddMenuEntry(
			ECollectionShareType::ToText( ECollectionShareType::CST_Private ),
			ECollectionShareType::GetDescription( ECollectionShareType::CST_Private ),
			FSlateIcon( FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName( ECollectionShareType::CST_Private ) ),
			FUIAction(
				FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteSetCollectionShareType, ECollectionShareType::CST_Private ),
				FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteSetCollectionShareType, ECollectionShareType::CST_Private ),
				FIsActionChecked::CreateSP( this, &FCollectionContextMenu::IsSetCollectionShareTypeChecked, ECollectionShareType::CST_Private )
				),
			NAME_None,
			EUserInterfaceActionType::Check
			);

		MenuBuilder.AddMenuEntry(
			ECollectionShareType::ToText( ECollectionShareType::CST_Local ),
			ECollectionShareType::GetDescription( ECollectionShareType::CST_Local ),
			FSlateIcon( FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName( ECollectionShareType::CST_Local ) ),
			FUIAction(
				FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteSetCollectionShareType, ECollectionShareType::CST_Local ),
				FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteSetCollectionShareType, ECollectionShareType::CST_Local ),
				FIsActionChecked::CreateSP( this, &FCollectionContextMenu::IsSetCollectionShareTypeChecked, ECollectionShareType::CST_Local )
				),
			NAME_None,
			EUserInterfaceActionType::Check
			);
	}
	MenuBuilder.EndSection();
}

void FCollectionContextMenu::MakeSetColorSubMenu(FMenuBuilder& MenuBuilder)
{
	// New Color
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewColor", "New Color"),
		LOCTEXT("NewCollectionColorTooltip", "Changes the color this collection should appear as."),
		FSlateIcon(),
		FUIAction( FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecutePickColor ) )
		);

	// Clear Color (only required if any of the selection has one)
	if ( SelectedHasCustomColors() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearColor", "Clear Color"),
			LOCTEXT("ClearCollectionColorTooltip", "Resets the color this collection appears as."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteResetColor ) )
			);
	}

	// Add all the custom colors the user has chosen so far
	TArray< FLinearColor > CustomColors;
	if ( CollectionViewUtils::HasCustomColors( &CustomColors ) )
	{	
		MenuBuilder.BeginSection("PathContextCustomColors", LOCTEXT("CustomColorsExistingColors", "Existing Colors") );
		{
			for ( int32 ColorIndex = 0; ColorIndex < CustomColors.Num(); ColorIndex++ )
			{
				const FLinearColor& Color = CustomColors[ ColorIndex ];
				MenuBuilder.AddWidget(
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "Menu.Button" )
							.OnClicked( this, &FCollectionContextMenu::OnColorClicked, Color )
							[
								SNew(SColorBlock)
								.Color( Color )
								.Size( FVector2D(77,16) )
							]
						],
					LOCTEXT("CustomColor", ""),
					/*bNoIndent=*/true
				);
			}
		}
		MenuBuilder.EndSection();
	}
}

void FCollectionContextMenu::UpdateProjectSourceControl()
{
	// Force update of source control so that we're always showing the valid options
	bProjectUnderSourceControl = false;
	if(ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable() && FPaths::IsProjectFilePathSet())
	{
		FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(FPaths::GetProjectFilePath(), EStateCacheUsage::ForceUpdate);
		bProjectUnderSourceControl = (SourceControlState->IsSourceControlled() && !SourceControlState->IsIgnored() && !SourceControlState->IsUnknown());
	}
}

bool FCollectionContextMenu::CanRenameSelectedCollections() const
{
	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();
	
	if(SelectedCollections.Num() == 1)
	{
		return !(SelectedCollections[0]->CollectionType != ECollectionShareType::CST_Local) || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable());
	}
	
	return false;
}

void FCollectionContextMenu::ExecuteNewCollection(ECollectionShareType::Type CollectionType, ECollectionStorageMode::Type StorageMode, SCollectionView::FCreateCollectionPayload InCreationPayload)
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	if (!GetDefault<UContentBrowserSettings>()->GetDisplayCollections())
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayCollections(true);
		GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
	}

	CollectionView.Pin()->CreateCollectionItem(CollectionType, StorageMode, InCreationPayload);
}

void FCollectionContextMenu::ExecuteSetCollectionShareType(ECollectionShareType::Type CollectionType)
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	if ( !ensure(SelectedCollections.Num() == 1) )
	{
		return;
	}

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	CollectionManagerModule.Get().RenameCollection(SelectedCollections[0]->CollectionName, SelectedCollections[0]->CollectionType, SelectedCollections[0]->CollectionName, CollectionType);
}

void FCollectionContextMenu::ExecuteSaveDynamicCollection(FCollectionNameType InCollection, FText InSearchQuery)
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	if (!GetDefault<UContentBrowserSettings>()->GetDisplayCollections())
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayCollections(true);
		GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
	}

	CollectionManagerModule.Get().SetDynamicQueryText(InCollection.Name, InCollection.Type, InSearchQuery.ToString());
}

void FCollectionContextMenu::ExecuteRenameCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	if ( !ensure(SelectedCollections.Num() == 1) )
	{
		return;
	}

	CollectionView.Pin()->RenameCollectionItem(SelectedCollections[0]);
}

void FCollectionContextMenu::ExecuteUpdateCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	for (const auto& SelectedCollection : SelectedCollections)
	{
		CollectionManagerModule.Get().UpdateCollection(SelectedCollection->CollectionName, SelectedCollection->CollectionType);
	}
}

void FCollectionContextMenu::ExecuteRefreshCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	TArray<FString> CollectionFilesToRefresh;
	for (const auto& SelectedCollection : SelectedCollections)
	{
		FCollectionStatusInfo StatusInfo;
		if (CollectionManagerModule.Get().GetCollectionStatusInfo(SelectedCollection->CollectionName, SelectedCollection->CollectionType, StatusInfo))
		{
			if (StatusInfo.bUseSCC && StatusInfo.SCCState.IsValid() && StatusInfo.SCCState->IsSourceControlled())
			{
				// Forcing a status update will refresh the collection state
				SourceControlProvider.GetState(StatusInfo.SCCState->GetFilename(), EStateCacheUsage::ForceUpdate);
			}
		}
	}
}

void FCollectionContextMenu::ExecuteSaveCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	for (const auto& SelectedCollection : SelectedCollections)
	{
		CollectionManagerModule.Get().SaveCollection(SelectedCollection->CollectionName, SelectedCollection->CollectionType);
	}
}

void FCollectionContextMenu::ExecuteDestroyCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	FText Prompt;
	if ( SelectedCollections.Num() == 1 )
	{
		Prompt = FText::Format(LOCTEXT("CollectionDestroyConfirm_Single", "Delete {0}?"), FText::FromName(SelectedCollections[0]->CollectionName));
	}
	else
	{
		Prompt = FText::Format(LOCTEXT("CollectionDestroyConfirm_Multiple", "Delete {0} Collections?"), FText::AsNumber(SelectedCollections.Num()));
	}

	FOnClicked OnYesClicked = FOnClicked::CreateSP( this, &FCollectionContextMenu::ExecuteDestroyCollectionConfirmed, SelectedCollections );
	ContentBrowserUtils::DisplayConfirmationPopup(
		Prompt,
		LOCTEXT("CollectionDestroyConfirm_Yes", "Delete"),
		LOCTEXT("CollectionDestroyConfirm_No", "Cancel"),
		CollectionView.Pin().ToSharedRef(),
		OnYesClicked);
}

FReply FCollectionContextMenu::ExecuteDestroyCollectionConfirmed(TArray<TSharedPtr<FCollectionItem>> CollectionList)
{
	CollectionView.Pin()->DeleteCollectionItems(CollectionList);
	return FReply::Handled();
}

void FCollectionContextMenu::ExecuteResetColor()
{
	ResetColors();
}

void FCollectionContextMenu::ExecutePickColor()
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Spawn a color picker, so the user can select which color they want
	TArray<FLinearColor*> LinearColorArray;
	FColorPickerArgs PickerArgs;
	PickerArgs.bIsModal = false;
	PickerArgs.ParentWidget = CollectionViewPtr;
	if ( SelectedCollections.Num() > 0 )
	{
		// Make sure an color entry exists for all the collections, otherwise they won't update in realtime with the widget color
		for (int32 PathIdx = SelectedCollections.Num() - 1; PathIdx >= 0; --PathIdx)
		{
			const FCollectionNameType& SelectedCollection = SelectedCollections[PathIdx];
			TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
			if ( !Color.IsValid() )
			{
				Color = MakeShareable( new FLinearColor( CollectionViewUtils::GetDefaultColor() ) );
				CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, Color, true );
			}
			else
			{
				// Default the color to the first valid entry
				PickerArgs.InitialColorOverride = *Color.Get();
			}
			LinearColorArray.Add( Color.Get() );
		}	
		PickerArgs.LinearColorArray = &LinearColorArray;
	}
		
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &FCollectionContextMenu::NewColorComplete);

	OpenColorPicker(PickerArgs);
}

bool FCollectionContextMenu::CanExecuteNewCollection(ECollectionShareType::Type CollectionType, bool bIsValidChildType) const
{
	return bIsValidChildType && (CollectionType == ECollectionShareType::CST_Local || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable()));
}

bool FCollectionContextMenu::CanExecuteSetCollectionShareType(ECollectionShareType::Type CollectionType) const
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return false;
	}

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	if ( !ensure(SelectedCollections.Num() == 1) )
	{
		return false;
	}

	const bool bIsSourceControlAvailable = bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable();
	const bool bIsCurrentTypeLocal = SelectedCollections[0]->CollectionType == ECollectionShareType::CST_Local;
	const bool bIsNewTypeLocal = CollectionType == ECollectionShareType::CST_Local;
	const bool bIsNewShareTypeDifferent = SelectedCollections[0]->CollectionType != CollectionType;

	return bIsNewShareTypeDifferent && ((bIsCurrentTypeLocal && bIsNewTypeLocal) || bIsSourceControlAvailable);
}

bool FCollectionContextMenu::IsSetCollectionShareTypeChecked(ECollectionShareType::Type CollectionType) const
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return false;
	}

	TArray<TSharedPtr<FCollectionItem>> SelectedCollections = CollectionView.Pin()->CollectionTreePtr->GetSelectedItems();

	if ( !ensure(SelectedCollections.Num() == 1) )
	{
		return false;
	}

	return SelectedCollections[0]->CollectionType == CollectionType;
}

bool FCollectionContextMenu::CanExecuteSaveDynamicCollection(FCollectionNameType InCollection) const
{
	return InCollection.Type == ECollectionShareType::CST_Local || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable());
}

bool FCollectionContextMenu::CanExecuteRenameCollection() const
{
	return CanRenameSelectedCollections();
}

bool FCollectionContextMenu::CanExecuteDestroyCollection(bool bAnyManagedBySCC) const
{
	return !bAnyManagedBySCC || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable());
}

bool FCollectionContextMenu::SelectedHasCustomColors() const
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		// Ignore any that are the default color
		const TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
		if ( Color.IsValid() && !Color->Equals( CollectionViewUtils::GetDefaultColor() ) )
		{
			return true;
		}
	}
	return false;
}

void FCollectionContextMenu::NewColorComplete(const TSharedRef<SWindow>& Window)
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Save the colors back in the config (ptr should have already updated by the widget)
	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		const TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
		check( Color.IsValid() );
		CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, Color );
	}
}

FReply FCollectionContextMenu::OnColorClicked( const FLinearColor InColor )
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Make sure an color entry exists for all the collections, otherwise it can't save correctly
	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
		if ( !Color.IsValid() )
		{
			Color = MakeShareable( new FLinearColor() );
		}
		*Color.Get() = InColor;
		CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, Color );
	}

	// Dismiss the menu here, as we can't make the 'clear' option appear if a folder has just had a color set for the first time
	FSlateApplication::Get().DismissAllMenus();

	return FReply::Handled();
}

void FCollectionContextMenu::ResetColors()
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Clear the custom colors for all the selected collections
	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, nullptr );
	}
}

#undef LOCTEXT_NAMESPACE
