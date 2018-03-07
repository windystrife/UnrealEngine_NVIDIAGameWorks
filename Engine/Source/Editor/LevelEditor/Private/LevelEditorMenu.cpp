// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelEditorMenu.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Interfaces/IMainFrameModule.h"
#include "MRUFavoritesList.h"
#include "Framework/Commands/GenericCommands.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "LevelEditorMenu"

TSharedRef< SWidget > FLevelEditorMenu::MakeLevelEditorMenu( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor )
{
	struct Local
	{
		static void FillFileLoadAndSaveItems( FMenuBuilder& MenuBuilder )
		{
			// New Level
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().NewLevel );

			// Open Level
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().OpenLevel );

			// Open Asset
			//@TODO: Doesn't work when summoned from here: MenuBuilder.AddMenuEntry( FGlobalEditorCommonCommands::Get().SummonOpenAssetDialog );

			// Save
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().Save );
	
			// Save As
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().SaveAs );

			// Save Levels
			MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().SaveAllLevels );
		}

		static void FillFileRecentAndFavoriteFileItems( FMenuBuilder& MenuBuilder )
		{
			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
			const FMainMRUFavoritesList& RecentsAndFavorites = *MainFrameModule.GetMRUFavoritesList();

			// Import/Export
			{
				MenuBuilder.BeginSection("FileActors", LOCTEXT("ImportExportHeading", "Actors") );
				{
					// Import Into Level
					MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ImportScene);

					// Export All
					MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ExportAll );

					// Export Selected
					MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ExportSelected );
				}
				MenuBuilder.EndSection();
			}


			// Favorite Menus
			{
				struct FFavoriteLevelMenu
				{
					// Add a button to add/remove the currently loaded map as a favorite
					struct Local
					{
						static FText GetToggleFavoriteLabelText()
						{
							const FText LevelName = FText::FromString(FPackageName::GetShortName(GWorld->GetOutermost()->GetFName()));
							if (FLevelEditorActionCallbacks::ToggleFavorite_CanExecute())
							{

								if (!FLevelEditorActionCallbacks::ToggleFavorite_IsChecked())
								{
									return FText::Format(LOCTEXT("ToggleFavorite_Add", "Add {0} to Favorites"), LevelName);
								}
							}
							return FText::Format(LOCTEXT("ToggleFavorite_Remove", "Remove {0} from Favorites"), LevelName);
						}
					};

					static void MakeFavoriteLevelMenu(FMenuBuilder& InMenuBuilder)
					{
						// Add a button to add/remove the currently loaded map as a favorite
						if (FLevelEditorActionCallbacks::ToggleFavorite_CanExecute())
						{
							InMenuBuilder.BeginSection("LevelEditorToggleFavorite");
							{
								const FString LevelName = FPackageName::GetShortName(GWorld->GetOutermost()->GetFName());

								TAttribute<FText> ToggleFavoriteLabel;
								ToggleFavoriteLabel.BindStatic(&Local::GetToggleFavoriteLabelText);
								InMenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ToggleFavorite, NAME_None, ToggleFavoriteLabel);
							}
							InMenuBuilder.EndSection();
							InMenuBuilder.AddMenuSeparator();
						}
						const FMainMRUFavoritesList& MRUFavorites = *FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame").GetMRUFavoritesList();
						const int32 NumFavorites = MRUFavorites.GetNumFavorites();
						
						const bool bNoIndent = false;
						const int32 AllowedFavorites = FMath::Min(NumFavorites, FLevelEditorCommands::Get().OpenFavoriteFileCommands.Num());
						for (int32 CurFavoriteIndex = 0; CurFavoriteIndex < AllowedFavorites; ++CurFavoriteIndex)
						{
							TSharedPtr< FUICommandInfo > OpenFavoriteFile = FLevelEditorCommands::Get().OpenFavoriteFileCommands[CurFavoriteIndex];
							const FString CurFavorite = FPaths::GetBaseFilename(MRUFavorites.GetFavoritesItem(CurFavoriteIndex));
							const FText ToolTip = FText::Format(LOCTEXT("FavoriteLevelToolTip", "Opens favorite level: {0}"), FText::FromString(CurFavorite));
							const FText Label = FText::FromString(FPaths::GetBaseFilename(CurFavorite));

							InMenuBuilder.AddMenuEntry(OpenFavoriteFile, NAME_None, Label, ToolTip);
						}
					}
				};

				const int32 NumRecents = RecentsAndFavorites.GetNumItems();

				MenuBuilder.BeginSection("FileFavoriteLevels");

				if (NumRecents > 0)
				{
					MenuBuilder.AddSubMenu(
						LOCTEXT("FavoriteLevelsSubMenu", "Favorite Levels"),
						LOCTEXT("RecentLevelsSubMenu_ToolTip", "Select a level to load"),
						FNewMenuDelegate::CreateStatic(&FFavoriteLevelMenu::MakeFavoriteLevelMenu), false, FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.FavoriteLevels"));
				}

				MenuBuilder.EndSection();
			}

			// Recent files
			{
				struct FRecentLevelMenu
				{
					static void MakeRecentLevelMenu( FMenuBuilder& InMenuBuilder )
					{
						const FMainMRUFavoritesList& MRUFavorites = *FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" ).GetMRUFavoritesList();
						const int32 NumRecents = MRUFavorites.GetNumItems();

						const int32 AllowedRecents = FMath::Min( NumRecents, FLevelEditorCommands::Get().OpenRecentFileCommands.Num() );
						for ( int32 CurRecentIndex = 0; CurRecentIndex < AllowedRecents; ++CurRecentIndex )
						{
							TSharedPtr< FUICommandInfo > OpenRecentFile = FLevelEditorCommands::Get().OpenRecentFileCommands[ CurRecentIndex ];

							const FString CurRecent = MRUFavorites.GetMRUItem( CurRecentIndex );

							const FText ToolTip = FText::Format( LOCTEXT( "RecentLevelToolTip", "Opens recent level: {0}" ), FText::FromString( CurRecent ) );
							const FText Label = FText::FromString( FPaths::GetBaseFilename( CurRecent ) );

							InMenuBuilder.AddMenuEntry( OpenRecentFile, NAME_None, Label, ToolTip );
						}
					}
				};

				const int32 NumRecents = RecentsAndFavorites.GetNumItems();

				MenuBuilder.BeginSection( "FileRecentLevels" );

				if ( NumRecents > 0 )
				{
					MenuBuilder.AddSubMenu(
						LOCTEXT("RecentLevelsSubMenu", "Recent Levels"),
						LOCTEXT("RecentLevelsSubMenu_ToolTip", "Select a level to load"),
						FNewMenuDelegate::CreateStatic( &FRecentLevelMenu::MakeRecentLevelMenu ), false, FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.RecentLevels") );
				}

				MenuBuilder.EndSection();
			}
		}

		static void FillEditMenu( FMenuBuilder& MenuBuilder )
		{
			// Edit Actor
			{
				MenuBuilder.BeginSection("EditMain", LOCTEXT("MainHeading", "Edit") );
				{		
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Cut );
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Copy );
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Paste );

					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Duplicate );
					MenuBuilder.AddMenuEntry( FGenericCommands::Get().Delete );
				}
				MenuBuilder.EndSection();
			}
		}

		static void ExtendHelpMenu( FMenuBuilder& MenuBuilder )
		{
			MenuBuilder.BeginSection("HelpBrowse", NSLOCTEXT("MainHelpMenu", "Browse", "Browse"));
			{
				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseDocumentation );

				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseAPIReference );

				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseCVars );

				MenuBuilder.AddMenuSeparator();

				MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BrowseViewportControls );
			}
			MenuBuilder.EndSection();

			
		}
	};

	TSharedRef< FExtender > Extender( new FExtender() );
	
	// Add level loading and saving menu items
	Extender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::First,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillFileLoadAndSaveItems ) );

	// Add recent / favorites
	Extender->AddMenuExtension(
		"FileRecentFiles",
		EExtensionHook::Before,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillFileRecentAndFavoriteFileItems ) );

	// Extend the Edit menu
	Extender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillEditMenu ) );

	// Extend the Help menu
	Extender->AddMenuExtension(
		"HelpOnline",
		EExtensionHook::Before,
		CommandList.ToSharedRef(),
		FMenuExtensionDelegate::CreateStatic( &Local::ExtendHelpMenu ) );

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( Extender );
	TSharedPtr<FExtender> Extenders = LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
	TSharedRef< SWidget > MenuBarWidget = MainFrameModule.MakeMainTabMenu( LevelEditor->GetTabManager(), Extenders.ToSharedRef() );

	return MenuBarWidget;
}

TSharedRef< SWidget > FLevelEditorMenu::MakeNotificationBar( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor )
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
	const TSharedPtr<FExtender> NotificationBarExtenders = LevelEditorModule.GetNotificationBarExtensibilityManager()->GetAllExtenders();

	FToolBarBuilder NotificationBarBuilder( CommandList, FMultiBoxCustomization::None, NotificationBarExtenders, Orient_Horizontal );
	NotificationBarBuilder.SetStyle(&FEditorStyle::Get(), "NotificationBar");
	{
		NotificationBarBuilder.BeginSection("Start");
		NotificationBarBuilder.EndSection();
	}

	return NotificationBarBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
