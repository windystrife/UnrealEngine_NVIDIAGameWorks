// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PathContextMenu.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "EditorStyleSet.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "AssetData.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "ContentBrowserLog.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "SourceControlWindows.h"
#include "ContentBrowserModule.h"
#include "ReferenceViewer.h"
#include "ISizeMapModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Framework/Commands/GenericCommands.h"
#include "NativeClassHierarchy.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ContentBrowserCommands.h"


#define LOCTEXT_NAMESPACE "ContentBrowser"


FPathContextMenu::FPathContextMenu(const TWeakPtr<SWidget>& InParentContent)
	: ParentContent(InParentContent)
	, bCanExecuteSCCCheckOut(false)
	, bCanExecuteSCCOpenForAdd(false)
	, bCanExecuteSCCCheckIn(false)
{
	
}

void FPathContextMenu::SetOnNewAssetRequested(const FNewAssetOrClassContextMenu::FOnNewAssetRequested& InOnNewAssetRequested)
{
	OnNewAssetRequested = InOnNewAssetRequested;
}

void FPathContextMenu::SetOnNewClassRequested(const FNewAssetOrClassContextMenu::FOnNewClassRequested& InOnNewClassRequested)
{
	OnNewClassRequested = InOnNewClassRequested;
}

void FPathContextMenu::SetOnImportAssetRequested( const FNewAssetOrClassContextMenu::FOnImportAssetRequested& InOnImportAssetRequested )
{
	OnImportAssetRequested = InOnImportAssetRequested;
}

void FPathContextMenu::SetOnRenameFolderRequested(const FOnRenameFolderRequested& InOnRenameFolderRequested)
{
	OnRenameFolderRequested = InOnRenameFolderRequested;
}

void FPathContextMenu::SetOnFolderDeleted(const FOnFolderDeleted& InOnFolderDeleted)
{
	OnFolderDeleted = InOnFolderDeleted;
}

void FPathContextMenu::SetSelectedPaths(const TArray<FString>& InSelectedPaths)
{
	SelectedPaths = InSelectedPaths;
}

TSharedRef<FExtender> FPathContextMenu::MakePathViewContextMenuExtender(const TArray<FString>& InSelectedPaths)
{
	// Cache any vars that are used in determining if you can execute any actions.
	// Useful for actions whose "CanExecute" will not change or is expensive to calculate.
	CacheCanExecuteVars();

	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender_SelectedPaths> MenuExtenderDelegates = ContentBrowserModule.GetAllPathViewContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute( SelectedPaths ));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	MenuExtender->AddMenuExtension("NewFolder", EExtensionHook::After, TSharedPtr<FUICommandList>(), FMenuExtensionDelegate::CreateSP(this, &FPathContextMenu::MakePathViewContextMenu));

	return MenuExtender.ToSharedRef();
}

void FPathContextMenu::MakePathViewContextMenu(FMenuBuilder& MenuBuilder)
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedPaths, NumAssetPaths, NumClassPaths);

	// Only add something if at least one folder is selected
	if ( SelectedPaths.Num() > 0 )
	{
		const bool bHasAssetPaths = NumAssetPaths > 0;
		const bool bHasClassPaths = NumClassPaths > 0;

		// Common operations section //
		MenuBuilder.BeginSection("PathViewFolderOptions", LOCTEXT("PathViewOptionsMenuHeading", "Folder Options") );
		{
			if(bHasAssetPaths)
			{
				FText NewAssetToolTip;
				if(SelectedPaths.Num() == 1)
				{
					if(CanCreateAsset())
					{
						NewAssetToolTip = FText::Format(LOCTEXT("NewAssetTooltip_CreateIn", "Create a new asset in {0}."), FText::FromString(SelectedPaths[0]));
					}
					else
					{
						NewAssetToolTip = FText::Format(LOCTEXT("NewAssetTooltip_InvalidPath", "Cannot create new assets in {0}."), FText::FromString(SelectedPaths[0]));
					}
				}
				else
				{
					NewAssetToolTip = LOCTEXT("NewAssetTooltip_InvalidNumberOfPaths", "Can only create assets when there is a single path selected.");
				}

				// New Asset (submenu)
				MenuBuilder.AddSubMenu(
					LOCTEXT( "NewAssetLabel", "New Asset" ),
					NewAssetToolTip,
					FNewMenuDelegate::CreateRaw( this, &FPathContextMenu::MakeNewAssetSubMenu ),
					FUIAction(
						FExecuteAction(),
						FCanExecuteAction::CreateRaw( this, &FPathContextMenu::CanCreateAsset )
						),
					NAME_None,
					EUserInterfaceActionType::Button,
					false,
					FSlateIcon()
					);
			}

			if(bHasClassPaths)
			{
				FText NewClassToolTip;
				if(SelectedPaths.Num() == 1)
				{
					if(CanCreateClass())
					{
						NewClassToolTip = FText::Format(LOCTEXT("NewClassTooltip_CreateIn", "Create a new class in {0}."), FText::FromString(SelectedPaths[0]));
					}
					else
					{
						NewClassToolTip = FText::Format(LOCTEXT("NewClassTooltip_InvalidPath", "Cannot create new classes in {0}."), FText::FromString(SelectedPaths[0]));
					}
				}
				else
				{
					NewClassToolTip = LOCTEXT("NewClassTooltip_InvalidNumberOfPaths", "Can only create classes when there is a single path selected.");
				}

				// New Class
				MenuBuilder.AddMenuEntry(
					LOCTEXT("NewClassLabel", "New C++ Class..."),
					NewClassToolTip,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.AddCodeToProject"),
					FUIAction(
						FExecuteAction::CreateRaw( this, &FPathContextMenu::ExecuteCreateClass ),
						FCanExecuteAction::CreateRaw( this, &FPathContextMenu::CanCreateClass )
						)
					);
			}

			// Explore
			MenuBuilder.AddMenuEntry(
				ContentBrowserUtils::GetExploreFolderText(),
				LOCTEXT("ExploreTooltip", "Finds this folder on disk."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteExplore ) )
				);

			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None,
				LOCTEXT("RenameFolder", "Rename"),
				LOCTEXT("RenameFolderTooltip", "Rename the selected folder.")
				);

			// If any colors have already been set, display color options as a sub menu
			if ( ContentBrowserUtils::HasCustomColors() )
			{
				// Set Color (submenu)
				MenuBuilder.AddSubMenu(
					LOCTEXT("SetColor", "Set Color"),
					LOCTEXT("SetColorTooltip", "Sets the color this folder should appear as."),
					FNewMenuDelegate::CreateRaw( this, &FPathContextMenu::MakeSetColorSubMenu ),
					false,
					FSlateIcon()
					);
			}
			else
			{
				// Set Color
				MenuBuilder.AddMenuEntry(
					LOCTEXT("SetColor", "Set Color"),
					LOCTEXT("SetColorTooltip", "Sets the color this folder should appear as."),
					FSlateIcon(),
					FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecutePickColor ) )
					);
			}			
		}
		MenuBuilder.EndSection();

		if(bHasAssetPaths)
		{
			// Bulk operations section //
			MenuBuilder.BeginSection("PathContextBulkOperations", LOCTEXT("AssetTreeBulkMenuHeading", "Bulk Operations") );
			{
				// Save
				MenuBuilder.AddMenuEntry(FContentBrowserCommands::Get().SaveAllCurrentFolder, NAME_None,
					LOCTEXT("SaveFolder", "Save All"),
					LOCTEXT("SaveFolderTooltip", "Saves all modified assets in this folder."),
					FSlateIcon()
					);

				// Resave
				MenuBuilder.AddMenuEntry(FContentBrowserCommands::Get().ResaveAllCurrentFolder);

				// Delete
				MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None,
					LOCTEXT("DeleteFolder", "Delete"),
					LOCTEXT("DeleteFolderTooltip", "Removes this folder and all assets it contains."),
					FSlateIcon()
					);

				// Reference Viewer
				MenuBuilder.AddMenuEntry(
					LOCTEXT("ReferenceViewer", "Reference Viewer..."),
					LOCTEXT("ReferenceViewerOnFolderTooltip", "Shows a graph of references for this folder."),
					FSlateIcon(),
					FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteReferenceViewer ) )
					);
    
				// Size Map
				MenuBuilder.AddMenuEntry(
					LOCTEXT("SizeMap", "Size Map..."),
					LOCTEXT("SizeMapOnFolderTooltip", "Shows an interactive map of the approximate memory used by the assets in this folder and everything they reference."),
					FSlateIcon(),
					FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteSizeMap ) )
					);

				// Fix Up Redirectors in Folder
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FixUpRedirectorsInFolder", "Fix Up Redirectors in Folder"),
					LOCTEXT("FixUpRedirectorsInFolderTooltip", "Finds referencers to all redirectors in the selected folders and resaves them if possible, then deletes any redirectors that had all their referencers fixed."),
					FSlateIcon(),
					FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteFixUpRedirectorsInFolder ) )
					);

				if ( NumAssetPaths == 1 && NumClassPaths == 0 )
				{
					// Migrate Folder
					MenuBuilder.AddMenuEntry(
						LOCTEXT("MigrateFolder", "Migrate..."),
						LOCTEXT("MigrateFolderTooltip", "Copies assets found in this folder and their dependencies to another game content folder."),
						FSlateIcon(),
						FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteMigrateFolder ) )
						);
				}
			}
			MenuBuilder.EndSection();

			// Source control section //
			MenuBuilder.BeginSection("PathContextSourceControl", LOCTEXT("AssetTreeSCCMenuHeading", "Source Control") );

			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			if ( SourceControlProvider.IsEnabled() )
			{
				// Check out
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FolderSCCCheckOut", "Check Out"),
					LOCTEXT("FolderSCCCheckOutTooltip", "Checks out all assets from source control which are in this folder."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteSCCCheckOut ),
						FCanExecuteAction::CreateSP( this, &FPathContextMenu::CanExecuteSCCCheckOut )
						)
					);

				// Open for Add
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FolderSCCOpenForAdd", "Mark For Add"),
					LOCTEXT("FolderSCCOpenForAddTooltip", "Adds all assets to source control that are in this folder and not already added."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteSCCOpenForAdd ),
						FCanExecuteAction::CreateSP( this, &FPathContextMenu::CanExecuteSCCOpenForAdd )
						)
					);

				// Check in
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FolderSCCCheckIn", "Check In"),
					LOCTEXT("FolderSCCCheckInTooltip", "Checks in all assets to source control which are in this folder."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteSCCCheckIn ),
						FCanExecuteAction::CreateSP( this, &FPathContextMenu::CanExecuteSCCCheckIn )
						)
					);

				// Sync
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FolderSCCSync", "Sync"),
					LOCTEXT("FolderSCCSyncTooltip", "Syncs all the assets in this folder to the latest version."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteSCCSync ),
						FCanExecuteAction::CreateSP( this, &FPathContextMenu::CanExecuteSCCSync )
						)
					);
			}
			else
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("FolderSCCConnect", "Connect To Source Control"),
					LOCTEXT("FolderSCCConnectTooltip", "Connect to source control to allow source control operations to be performed on content and levels."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteSCCConnect ),
						FCanExecuteAction::CreateSP( this, &FPathContextMenu::CanExecuteSCCConnect )
						)
					);	
			}

			MenuBuilder.EndSection();
		}
	}
}

bool FPathContextMenu::CanCreateAsset() const
{
	// We can only create assets when we have a single asset path selected
	return SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsClassPath(SelectedPaths[0]);
}

void FPathContextMenu::MakeNewAssetSubMenu(FMenuBuilder& MenuBuilder)
{
	if ( SelectedPaths.Num() )
	{
		FNewAssetOrClassContextMenu::MakeContextMenu(
			MenuBuilder, 
			SelectedPaths, 
			OnNewAssetRequested, 
			FNewAssetOrClassContextMenu::FOnNewClassRequested(), 
			FNewAssetOrClassContextMenu::FOnNewFolderRequested(), 
			OnImportAssetRequested, 
			FNewAssetOrClassContextMenu::FOnGetContentRequested()
			);
	}
}

void FPathContextMenu::ExecuteCreateClass()
{
	OnNewClassRequested.ExecuteIfBound(SelectedPaths[0]);
}

bool FPathContextMenu::CanCreateClass() const
{
	// We can only create assets when we have a single class path selected
	return SelectedPaths.Num() == 1 && ContentBrowserUtils::IsValidPathToCreateNewClass(SelectedPaths[0]);
}

void FPathContextMenu::MakeSetColorSubMenu(FMenuBuilder& MenuBuilder)
{
	// New Color
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewColor", "New Color"),
		LOCTEXT("NewColorTooltip", "Changes the color this folder should appear as."),
		FSlateIcon(),
		FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecutePickColor ) )
		);

	// Clear Color (only required if any of the selection has one)
	if ( SelectedHasCustomColors() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearColor", "Clear Color"),
			LOCTEXT("ClearColorTooltip", "Resets the color this folder appears as."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FPathContextMenu::ExecuteResetColor ) )
			);
	}

	// Add all the custom colors the user has chosen so far
	TArray< FLinearColor > CustomColors;
	if ( ContentBrowserUtils::HasCustomColors( &CustomColors ) )
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
							.OnClicked( this, &FPathContextMenu::OnColorClicked, Color )
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

void FPathContextMenu::ExecuteMigrateFolder()
{
	const FString& SourcesPath = GetFirstSelectedPath();
	if ( ensure(SourcesPath.Len()) )
	{
		// @todo Make sure the asset registry has completed discovering assets, or else GetAssetsByPath() will not find all the assets in the folder! Add some UI to wait for this with a cancel button
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		if ( AssetRegistryModule.Get().IsLoadingAssets() )
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT( "MigrateFolderAssetsNotDiscovered", "You must wait until asset discovery is complete to migrate a folder" ));
			return;
		}

		// Get a list of package names for input into MigratePackages
		TArray<FAssetData> AssetDataList;
		TArray<FName> PackageNames;
		ContentBrowserUtils::GetAssetsInPaths(SelectedPaths, AssetDataList);
		for ( auto AssetIt = AssetDataList.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			PackageNames.Add((*AssetIt).PackageName);
		}

		// Load all the assets in the selected paths
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().MigratePackages( PackageNames );
	}
}

void FPathContextMenu::ExecuteExplore()
{
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		const FString& Path = SelectedPaths[PathIdx];
		FString FilePath;
		if (ContentBrowserUtils::IsClassPath(Path))
		{
			TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
			if (NativeClassHierarchy->GetFileSystemPath(Path, FilePath))
			{
				FilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FilePath);
			}
		}
		else
		{
			FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Path + TEXT("/")));
		}

		if (!FilePath.IsEmpty())
		{
			// If the folder has not yet been created, make is right before we try to explore to it
			if (!IFileManager::Get().DirectoryExists(*FilePath))
			{
				IFileManager::Get().MakeDirectory(*FilePath, /*Tree=*/true);
			}

			FPlatformProcess::ExploreFolder(*FilePath);
		}
	}
}

bool FPathContextMenu::CanExecuteRename() const
{
	return ContentBrowserUtils::CanRenameFromPathView(SelectedPaths);
}

void FPathContextMenu::ExecuteRename()
{
	check(SelectedPaths.Num() == 1);
	if (OnRenameFolderRequested.IsBound())
	{
		OnRenameFolderRequested.Execute(SelectedPaths[0]);
	}
}

void FPathContextMenu::ExecuteResetColor()
{
	ResetColors();
}

void FPathContextMenu::ExecutePickColor()
{
	// Spawn a color picker, so the user can select which color they want
	TArray<FLinearColor*> LinearColorArray;
	FColorPickerArgs PickerArgs;
	PickerArgs.bIsModal = false;
	PickerArgs.ParentWidget = ParentContent.Pin();
	if ( SelectedPaths.Num() > 0 )
	{
		// Make sure an color entry exists for all the paths, otherwise they won't update in realtime with the widget color
		for (int32 PathIdx = SelectedPaths.Num() - 1; PathIdx >= 0; --PathIdx)
		{
			const FString& Path = SelectedPaths[PathIdx];
			TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor( Path );
			if ( !Color.IsValid() )
			{
				Color = MakeShareable( new FLinearColor( ContentBrowserUtils::GetDefaultColor() ) );
				ContentBrowserUtils::SaveColor( Path, Color, true );
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
		
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &FPathContextMenu::NewColorComplete);

	OpenColorPicker(PickerArgs);
}

void FPathContextMenu::NewColorComplete(const TSharedRef<SWindow>& Window)
{
	// Save the colors back in the config (ptr should have already updated by the widget)
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		const FString& Path = SelectedPaths[PathIdx];
		const TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor( Path );
		check( Color.IsValid() );
		ContentBrowserUtils::SaveColor( Path, Color );		
	}
}

FReply FPathContextMenu::OnColorClicked( const FLinearColor InColor )
{
	// Make sure an color entry exists for all the paths, otherwise it can't save correctly
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		const FString& Path = SelectedPaths[PathIdx];
		TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor( Path );
		if ( !Color.IsValid() )
		{
			Color = MakeShareable( new FLinearColor() );
		}
		*Color.Get() = InColor;
		ContentBrowserUtils::SaveColor( Path, Color );
	}

	// Dismiss the menu here, as we can't make the 'clear' option appear if a folder has just had a color set for the first time
	FSlateApplication::Get().DismissAllMenus();

	return FReply::Handled();
}

void FPathContextMenu::ResetColors()
{
	// Clear the custom colors for all the selected paths
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		const FString& Path = SelectedPaths[PathIdx];
		ContentBrowserUtils::SaveColor( Path, NULL );		
	}
}

void FPathContextMenu::ExecuteSaveFolder()
{
	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);

	// Form a list of packages from the assets
	TArray<UPackage*> Packages;
	for (int32 PackageIdx = 0; PackageIdx < PackageNames.Num(); ++PackageIdx)
	{
		UPackage* Package = FindPackage(NULL, *PackageNames[PackageIdx]);

		// Only save loaded and dirty packages
		if ( Package != NULL && Package->IsDirty() )
		{
			Packages.Add(Package);
		}
	}

	// Save all packages that were found
	if ( Packages.Num() )
	{
		ContentBrowserUtils::SavePackages(Packages);
	}
}

void FPathContextMenu::ExecuteResaveFolder()
{
	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);

	TArray<UPackage*> Packages;
	for (const FString& PackageName : PackageNames)
	{
		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (!Package)
		{
			Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		}

		if (Package)
		{
			Packages.Add(Package);
		}
	}

	if (Packages.Num())
	{
		ContentBrowserUtils::SavePackages(Packages);
	}
}

bool FPathContextMenu::CanExecuteDelete() const
{
	return ContentBrowserUtils::CanDeleteFromPathView(SelectedPaths);
}

void FPathContextMenu::ExecuteDelete()
{
	// Don't allow asset deletion during PIE
	if (GIsEditor)
	{
		UEditorEngine* Editor = GEditor;
		FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		if (PIEWorldContext)
		{
			FNotificationInfo Notification(LOCTEXT("CannotDeleteAssetInPIE", "Assets cannot be deleted while in PIE."));
			Notification.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Notification);
			return;
		}
	}

	check(SelectedPaths.Num() > 0);
	if (ParentContent.IsValid())
	{
		FText Prompt;
		if ( SelectedPaths.Num() == 1 )
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Single", "Delete folder '{0}'?"), FText::FromString(SelectedPaths[0]));
		}
		else
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Multiple", "Delete {0} folders?"), FText::AsNumber(SelectedPaths.Num()));
		}
		
		// Spawn a confirmation dialog since this is potentially a highly destructive operation
		FOnClicked OnYesClicked = FOnClicked::CreateSP( this, &FPathContextMenu::ExecuteDeleteFolderConfirmed );
 		ContentBrowserUtils::DisplayConfirmationPopup(
			Prompt,
			LOCTEXT("FolderDeleteConfirm_Yes", "Delete"),
			LOCTEXT("FolderDeleteConfirm_No", "Cancel"),
			ParentContent.Pin().ToSharedRef(),
			OnYesClicked);
	}
}

void FPathContextMenu::ExecuteReferenceViewer()
{
	TArray<FString> PackageNamesAsStrings;
	GetPackageNamesInSelectedPaths(PackageNamesAsStrings);

	TArray<FName> PackageNames;
	for ( auto PackageNameIt = PackageNamesAsStrings.CreateConstIterator(); PackageNameIt; ++PackageNameIt )
	{
		PackageNames.Add(**PackageNameIt);
	}

	if ( PackageNames.Num() > 0 )
	{
		IReferenceViewerModule::Get().InvokeReferenceViewerTab(PackageNames);
	}
}

void FPathContextMenu::ExecuteSizeMap()
{
	TArray<FString> PackageNamesAsStrings;
	GetPackageNamesInSelectedPaths(PackageNamesAsStrings);

	TArray<FName> PackageNames;
	for ( auto PackageNameIt = PackageNamesAsStrings.CreateConstIterator(); PackageNameIt; ++PackageNameIt )
	{
		PackageNames.Add(**PackageNameIt);
	}

	if ( PackageNames.Num() > 0 )
	{
		ISizeMapModule::Get().InvokeSizeMapTab(PackageNames);
	}
}

void FPathContextMenu::ExecuteFixUpRedirectorsInFolder()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Form a filter from the paths
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (const auto& Path : SelectedPaths)
	{
		Filter.PackagePaths.Emplace(*Path);
		Filter.ClassNames.Emplace(TEXT("ObjectRedirector"));
	}

	// Query for a list of assets in the selected paths
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	if (AssetList.Num() > 0)
	{
		TArray<FString> ObjectPaths;
		for (const auto& Asset : AssetList)
		{
			ObjectPaths.Add(Asset.ObjectPath.ToString());
		}

		TArray<UObject*> Objects;
		const bool bAllowedToPromptToLoadAssets = true;
		const bool bLoadRedirects = true;
		if (ContentBrowserUtils::LoadAssetsIfNeeded(ObjectPaths, Objects, bAllowedToPromptToLoadAssets, bLoadRedirects))
		{
			// Transform Objects array to ObjectRedirectors array
			TArray<UObjectRedirector*> Redirectors;
			for (auto Object : Objects)
			{
				auto Redirector = CastChecked<UObjectRedirector>(Object);
				Redirectors.Add(Redirector);
			}

			// Load the asset tools module
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			AssetToolsModule.Get().FixupReferencers(Redirectors);
		}
	}
}


FReply FPathContextMenu::ExecuteDeleteFolderConfirmed()
{
	if ( ContentBrowserUtils::DeleteFolders(SelectedPaths) )
	{
		ResetColors();
		if (OnFolderDeleted.IsBound())
		{
			OnFolderDeleted.Execute();
		}
	}

	return FReply::Handled();
}

void FPathContextMenu::ExecuteSCCCheckOut()
{
	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);

	TArray<UPackage*> PackagesToCheckOut;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		if ( FPackageName::DoesPackageExist(*PackageIt) )
		{
			// Since the file exists, create the package if it isn't loaded or just find the one that is already loaded
			// No need to load unloaded packages. It isn't needed for the checkout process
			UPackage* Package = CreatePackage(NULL, **PackageIt);
			PackagesToCheckOut.Add( CreatePackage(NULL, **PackageIt) );
		}
	}

	if ( PackagesToCheckOut.Num() > 0 )
	{
		// Update the source control status of all potentially relevant packages
		ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), PackagesToCheckOut);

		// Now check them out
		FEditorFileUtils::CheckoutPackages(PackagesToCheckOut);
	}
}

void FPathContextMenu::ExecuteSCCOpenForAdd()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);

	TArray<FString> PackagesToAdd;
	TArray<UPackage*> PackagesToSave;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageIt), EStateCacheUsage::Use);
		if ( SourceControlState.IsValid() && !SourceControlState->IsSourceControlled() )
		{
			PackagesToAdd.Add(*PackageIt);

			// Make sure the file actually exists on disk before adding it
			FString Filename;
			if ( !FPackageName::DoesPackageExist(*PackageIt, NULL, &Filename) )
			{
				UPackage* Package = FindPackage(NULL, **PackageIt);
				if ( Package )
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	if ( PackagesToAdd.Num() > 0 )
	{
		// If any of the packages are new, save them now
		if ( PackagesToSave.Num() > 0 )
		{
			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			TArray<UPackage*> FailedPackages;
			const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave, &FailedPackages);
			if(FailedPackages.Num() > 0)
			{
				// don't try and add files that failed to save - remove them from the list
				for(auto FailedPackageIt = FailedPackages.CreateConstIterator(); FailedPackageIt; FailedPackageIt++)
				{
					PackagesToAdd.Remove((*FailedPackageIt)->GetName());
				}
			}
		}

		if ( PackagesToAdd.Num() > 0 )
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlHelpers::PackageFilenames(PackagesToAdd));
		}
	}
}

void FPathContextMenu::ExecuteSCCCheckIn()
{
	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);

	// Form a list of loaded packages to prompt for save
	TArray<UPackage*> LoadedPackages;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		UPackage* Package = FindPackage(NULL, **PackageIt);
		if ( Package )
		{
			LoadedPackages.Add(Package);
		}
	}

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave( LoadedPackages, true, true );

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined );
	if ( bShouldProceed )
	{
		TArray<FString> PendingDeletePaths;
		for (const auto& Path : SelectedPaths)
		{
			PendingDeletePaths.Add(FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Path + TEXT("/"))));
		}

		const bool bUseSourceControlStateCache = false;
		FSourceControlWindows::PromptForCheckin(bUseSourceControlStateCache, PackageNames, PendingDeletePaths);
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure.") );
		}
	}
}

void FPathContextMenu::ExecuteSCCSync() const
{
	ContentBrowserUtils::SyncPathsFromSourceControl(SelectedPaths);
}

void FPathContextMenu::ExecuteSCCConnect() const
{
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless);
}

bool FPathContextMenu::CanExecuteSCCCheckOut() const
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedPaths, NumAssetPaths, NumClassPaths);

	// Can only perform SCC operations on asset paths
	return bCanExecuteSCCCheckOut && (NumAssetPaths > 0 && NumClassPaths == 0);
}

bool FPathContextMenu::CanExecuteSCCOpenForAdd() const
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedPaths, NumAssetPaths, NumClassPaths);

	// Can only perform SCC operations on asset paths
	return bCanExecuteSCCOpenForAdd && (NumAssetPaths > 0 && NumClassPaths == 0);
}

bool FPathContextMenu::CanExecuteSCCCheckIn() const
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedPaths, NumAssetPaths, NumClassPaths);

	// Can only perform SCC operations on asset paths
	return bCanExecuteSCCCheckIn && (NumAssetPaths > 0 && NumClassPaths == 0);
}

bool FPathContextMenu::CanExecuteSCCSync() const
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedPaths, NumAssetPaths, NumClassPaths);

	// Can only perform SCC operations on asset paths
	return (NumAssetPaths > 0 && NumClassPaths == 0);
}

bool FPathContextMenu::CanExecuteSCCConnect() const
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedPaths, NumAssetPaths, NumClassPaths);

	// Can only perform SCC operations on asset paths
	return (!ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable()) && (NumAssetPaths > 0 && NumClassPaths == 0);
}

void FPathContextMenu::CacheCanExecuteVars()
{
	// Cache whether we can execute any of the source control commands
	bCanExecuteSCCCheckOut = false;
	bCanExecuteSCCOpenForAdd = false;
	bCanExecuteSCCCheckIn = false;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.IsEnabled() && SourceControlProvider.IsAvailable() )
	{
		TArray<FString> PackageNames;
		GetPackageNamesInSelectedPaths(PackageNames);

		// Check the SCC state for each package in the selected paths
		for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
		{
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageIt), EStateCacheUsage::Use);
			if(SourceControlState.IsValid())
			{
				if ( SourceControlState->CanCheckout() )
				{
					bCanExecuteSCCCheckOut = true;
				}
				else if ( !SourceControlState->IsSourceControlled() )
				{
					bCanExecuteSCCOpenForAdd = true;
				}
				else if ( SourceControlState->CanCheckIn() )
				{
					bCanExecuteSCCCheckIn = true;
				}
			}

			if ( bCanExecuteSCCCheckOut && bCanExecuteSCCOpenForAdd && bCanExecuteSCCCheckIn )
			{
				// All SCC options are available, no need to keep iterating
				break;
			}
		}
	}
}

void FPathContextMenu::GetPackageNamesInSelectedPaths(TArray<FString>& OutPackageNames) const
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Form a filter from the paths
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		const FString& Path = SelectedPaths[PathIdx];
		new (Filter.PackagePaths) FName(*Path);
	}

	// Query for a list of assets in the selected paths
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	// Form a list of unique package names from the assets
	TSet<FName> UniquePackageNames;
	for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); ++AssetIdx)
	{
		UniquePackageNames.Add(AssetList[AssetIdx].PackageName);
	}

	// Add all unique package names to the output
	for ( auto PackageIt = UniquePackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		OutPackageNames.Add( (*PackageIt).ToString() );
	}
}

FString FPathContextMenu::GetFirstSelectedPath() const
{
	return SelectedPaths.Num() > 0 ? SelectedPaths[0] : TEXT("");
}

bool FPathContextMenu::SelectedHasCustomColors() const
{
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		// Ignore any that are the default color
		const FString& Path = SelectedPaths[PathIdx];
		const TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor( Path );
		if ( Color.IsValid() && !Color->Equals( ContentBrowserUtils::GetDefaultColor() ) )
		{
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
