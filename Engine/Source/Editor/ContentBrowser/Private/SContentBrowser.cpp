// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SContentBrowser.h"
#include "Factories/Factory.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"
#include "Settings/ContentBrowserSettings.h"
#include "Settings/EditorSettings.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "ContentBrowserLog.h"
#include "FrontendFilters.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "SAssetSearchBox.h"
#include "SFilterList.h"
#include "SPathView.h"
#include "SCollectionView.h"
#include "SAssetView.h"
#include "AssetContextMenu.h"
#include "NewAssetOrClassContextMenu.h"
#include "PathContextMenu.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserCommands.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "IAddContentDialogModule.h"
#include "Engine/Selection.h"
#include "NativeClassHierarchy.h"
#include "AddToProjectConfig.h"
#include "GameProjectGenerationModule.h"
#include "GlobalEditorCommonCommands.h"
#include "ReferenceViewer.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

const FString SContentBrowser::SettingsIniSection = TEXT("ContentBrowser");

SContentBrowser::~SContentBrowser()
{
	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll( this );

	// Remove listeners for when collections/paths are renamed/deleted
	if (FCollectionManagerModule::IsModuleAvailable())
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		CollectionManagerModule.Get().OnCollectionRenamed().RemoveAll(this);
		CollectionManagerModule.Get().OnCollectionDestroyed().RemoveAll(this);
	}

	FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if (AssetRegistryModule != nullptr)
	{
		AssetRegistryModule->Get().OnPathRemoved().RemoveAll(this);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SContentBrowser::Construct( const FArguments& InArgs, const FName& InInstanceName, const FContentBrowserConfig* Config )
{
	if ( InArgs._ContainingTab.IsValid() )
	{
		// For content browsers that are placed in tabs, save settings when the tab is closing.
		ContainingTab = InArgs._ContainingTab;
		InArgs._ContainingTab->SetOnPersistVisualState( SDockTab::FOnPersistVisualState::CreateSP( this, &SContentBrowser::OnContainingTabSavingVisualState ) );
		InArgs._ContainingTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateSP( this, &SContentBrowser::OnContainingTabClosed ) );
		InArgs._ContainingTab->SetOnTabActivated( SDockTab::FOnTabActivatedCallback::CreateSP( this, &SContentBrowser::OnContainingTabActivated ) );
	}
	
	bIsLocked = InArgs._InitiallyLocked;
	bAlwaysShowCollections = Config != nullptr ? Config->bAlwaysShowCollections : false;

	HistoryManager.SetOnApplyHistoryData(FOnApplyHistoryData::CreateSP(this, &SContentBrowser::OnApplyHistoryData));
	HistoryManager.SetOnUpdateHistoryData(FOnUpdateHistoryData::CreateSP(this, &SContentBrowser::OnUpdateHistoryData));

	PathContextMenu = MakeShareable(new FPathContextMenu( AsShared() ));
	PathContextMenu->SetOnNewAssetRequested( FNewAssetOrClassContextMenu::FOnNewAssetRequested::CreateSP(this, &SContentBrowser::NewAssetRequested) );
	PathContextMenu->SetOnNewClassRequested( FNewAssetOrClassContextMenu::FOnNewClassRequested::CreateSP(this, &SContentBrowser::NewClassRequested) );
	PathContextMenu->SetOnImportAssetRequested(FNewAssetOrClassContextMenu::FOnImportAssetRequested::CreateSP(this, &SContentBrowser::ImportAsset));
	PathContextMenu->SetOnRenameFolderRequested(FPathContextMenu::FOnRenameFolderRequested::CreateSP(this, &SContentBrowser::OnRenameFolderRequested));
	PathContextMenu->SetOnFolderDeleted(FPathContextMenu::FOnFolderDeleted::CreateSP(this, &SContentBrowser::OnOpenedFolderDeleted));

	FrontendFilters = MakeShareable(new FAssetFilterCollectionType());
	TextFilter = MakeShareable( new FFrontendFilter_Text() );

	static const FName DefaultForegroundName("DefaultForeground");

	BindCommands();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Path and history
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0, 0, 0, 0 )
		[
			SNew( SWrapBox )
			.UseAllottedWidth( true )
			.InnerSlotPadding( FVector2D( 5, 2 ) )

			+ SWrapBox::Slot()
			.FillLineWhenWidthLessThan( 600 )
			.FillEmptySpace( true )
			[
				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SBorder )
					.Padding( FMargin( 3 ) )
					.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
					[
						SNew( SHorizontalBox )

						// New
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Left )
						[
							SNew( SComboButton )
							.ComboButtonStyle( FEditorStyle::Get(), "ToolbarComboButton" )
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
							.ForegroundColor(FLinearColor::White)
							.ContentPadding(FMargin(6, 2))
							.OnGetMenuContent_Lambda( [this]{ return MakeAddNewContextMenu( true, false ); } )
							.ToolTipText( this, &SContentBrowser::GetAddNewToolTipText )
							.IsEnabled( this, &SContentBrowser::IsAddNewEnabled )
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserNewAsset")))
							.HasDownArrow(false)
							.ButtonContent()
							[
								SNew( SHorizontalBox )

								// New Icon
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
									.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
									.Text(FEditorFontGlyphs::File)
								]

								// New Text
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4, 0, 0, 0)
								[
									SNew( STextBlock )
									.TextStyle( FEditorStyle::Get(), "ContentBrowser.TopBar.Font" )
									.Text( LOCTEXT( "NewButton", "Add New" ) )
								]

								// Down Arrow
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								.Padding(4, 0, 0, 0)
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
									.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
									.Text(FEditorFontGlyphs::Caret_Down)
								]
							]
						]

						// Import
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Left )
						.Padding(6, 0)
						[
							SNew( SButton )
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ToolTipText( this, &SContentBrowser::GetImportTooltipText )
							.IsEnabled( this, &SContentBrowser::IsImportEnabled )
							.OnClicked( this, &SContentBrowser::HandleImportClicked )
							.ContentPadding(FMargin(6, 2))
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserImportAsset")))
							[
								SNew( SHorizontalBox )

								// Import Icon
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
									.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
									.Text(FEditorFontGlyphs::Download)
								]

								// Import Text
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4, 0, 0, 0)
								[
									SNew( STextBlock )
									.TextStyle( FEditorStyle::Get(), "ContentBrowser.TopBar.Font" )
									.Text( LOCTEXT( "Import", "Import" ) )
								]
							]
						]

						// Save
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew( SButton )
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ToolTipText( LOCTEXT( "SaveDirtyPackagesTooltip", "Save all modified assets." ) )
							.ContentPadding(FMargin(6, 2))
							.OnClicked( this, &SContentBrowser::OnSaveClicked )
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSaveDirtyPackages")))
							[
								SNew( SHorizontalBox )

								// Save All Icon
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
									.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
									.Text(FEditorFontGlyphs::Floppy_O)
								]

								// Save All Text
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4, 0, 0, 0)
								[
									SNew( STextBlock )
									.TextStyle( FEditorStyle::Get(), "ContentBrowser.TopBar.Font" )
									.Text( LOCTEXT( "SaveAll", "Save All" ) )
								]
							]
						]
					]
				]
			]

			+ SWrapBox::Slot()
			.FillEmptySpace( true )
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew(SHorizontalBox)

					// History Back Button
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SButton)
							.VAlign(EVerticalAlignment::VAlign_Center)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
							.ToolTipText( this, &SContentBrowser::GetHistoryBackTooltip )
							.ContentPadding( FMargin(1, 0) )
							.OnClicked(this, &SContentBrowser::BackClicked)
							.IsEnabled(this, &SContentBrowser::IsBackEnabled)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserHistoryBack")))
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
								.Text(FText::FromString(FString(TEXT("\xf060"))) /*fa-arrow-left*/)
							]
						]
					]

					// History Forward Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SButton)
							.VAlign(EVerticalAlignment::VAlign_Center)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
							.ToolTipText( this, &SContentBrowser::GetHistoryForwardTooltip )
							.ContentPadding( FMargin(1, 0) )
							.OnClicked(this, &SContentBrowser::ForwardClicked)
							.IsEnabled(this, &SContentBrowser::IsForwardEnabled)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserHistoryForward")))
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
								.Text(FText::FromString(FString(TEXT("\xf061"))) /*fa-arrow-right*/)
							]
						]
					]

					// Separator
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3, 0)
					[
						SNew(SSeparator)
						.Orientation(Orient_Vertical)
					]

					// Path picker
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign( VAlign_Fill )
					[
						SAssignNew( PathPickerButton, SComboButton )
						.Visibility( ( Config != nullptr ? Config->bUsePathPicker : true ) ? EVisibility::Visible : EVisibility::Collapsed )
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ForegroundColor(FLinearColor::White)
						.ToolTipText( LOCTEXT( "PathPickerTooltip", "Choose a path" ) )
						.OnGetMenuContent( this, &SContentBrowser::GetPathPickerContent )
						.HasDownArrow( false )
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserPathPicker")))
						.ContentPadding(FMargin(3, 3))
						.ButtonContent()
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
							.Text(FText::FromString(FString(TEXT("\xf07c"))) /*fa-folder-open*/)
						]
					]

					// Path
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.FillWidth(1.0f)
					.Padding(FMargin(0))
					[
						SAssignNew(PathBreadcrumbTrail, SBreadcrumbTrail<FString>)
						.ButtonContentPadding(FMargin(2, 2))
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.DelimiterImage(FEditorStyle::GetBrush("ContentBrowser.PathDelimiter"))
						.TextStyle(FEditorStyle::Get(), "ContentBrowser.PathText")
						.ShowLeadingDelimiter(false)
						.InvertTextColorOnHover(false)
						.OnCrumbClicked(this, &SContentBrowser::OnPathClicked)
						.GetCrumbMenuContent(this, &SContentBrowser::OnGetCrumbDelimiterContent)
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserPath")))
					]

					// Lock button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						.Visibility( ( Config != nullptr ? Config->bCanShowLockButton : true ) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed )

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SButton)
							.VAlign(EVerticalAlignment::VAlign_Center)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ToolTipText( LOCTEXT("LockToggleTooltip", "Toggle lock. If locked, this browser will ignore Find in Content Browser requests.") )
							.ContentPadding( FMargin(1, 0) )
							.OnClicked(this, &SContentBrowser::ToggleLockClicked)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserLock")))
							[
								SNew(SImage)
								.Image( this, &SContentBrowser::GetToggleLockImage)
							]
						]
					]
				]
			]
		]

		// Assets/tree
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0,2,0,0)
		[
			// The tree/assets splitter
			SAssignNew(PathAssetSplitterPtr, SSplitter)

			// Sources View
			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SVerticalBox)
				.Visibility( this, &SContentBrowser::GetSourcesViewVisibility )

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(PathCollectionSplitterPtr, SSplitter)
					.Style( FEditorStyle::Get(), "ContentBrowser.Splitter" )
					.Orientation( Orient_Vertical )

					// Path View
					+ SSplitter::Slot()
					.Value(0.9f)
					[
						SNew(SBorder)
						.Visibility( ( Config != nullptr ? Config->bShowAssetPathTree : true ) ? EVisibility::Visible : EVisibility::Collapsed )
						.Padding(FMargin(3))
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						[
							SAssignNew( PathViewPtr, SPathView )
							.OnPathSelected( this, &SContentBrowser::PathSelected )
							.OnGetFolderContextMenu( this, &SContentBrowser::GetFolderContextMenu, true )
							.OnGetPathContextMenuExtender( this, &SContentBrowser::GetPathContextMenuExtender )
							.FocusSearchBoxWhenOpened( false )
							.ShowTreeTitle( false )
							.ShowSeparator( false )
							.AllowClassesFolder( true )
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSources")))
							.SearchContent()
							[
								SNew( SVerticalBox )
								.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSourcesToggle1")))
								+ SVerticalBox::Slot()
								.FillHeight( 1.0f )
								.Padding(0,0,2,0)
								[
									SNew( SButton )
									.VAlign( EVerticalAlignment::VAlign_Center )
									.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
									.ToolTipText( LOCTEXT( "SourcesTreeToggleTooltip", "Show or hide the sources panel" ) )
									.ContentPadding( FMargin( 1, 0 ) )
									.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
									.OnClicked( this, &SContentBrowser::SourcesViewExpandClicked )
									[
										SNew( SImage )
										.Image( this, &SContentBrowser::GetSourcesToggleImage )
									]
								]
							]
						]
					]

					// Collection View
					+ SSplitter::Slot()
					.Value(0.9f)
					[
						SNew(SBorder)
						.Visibility( this, &SContentBrowser::GetCollectionViewVisibility )
						.Padding(FMargin(3))
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						[
							SAssignNew(CollectionViewPtr, SCollectionView)
							.OnCollectionSelected(this, &SContentBrowser::CollectionSelected)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserCollections")))
							.AllowCollectionDrag(true)
							.AllowQuickAssetManagement(true)
						]
					]
				]
			]

			// Asset View
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew(SVerticalBox)

					// Search and commands
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 2)
					[
						SNew(SHorizontalBox)

						// Expand/collapse sources button
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding( 0, 0, 4, 0 )
						[
							SNew( SVerticalBox )
							.Visibility(( Config != nullptr ? Config->bUseSourcesView : true ) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSourcesToggle2")))
							+ SVerticalBox::Slot()
							.FillHeight( 1.0f )
							[
								SNew( SButton )
								.VAlign( EVerticalAlignment::VAlign_Center )
								.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
								.ToolTipText( LOCTEXT( "SourcesTreeToggleTooltip", "Show or hide the sources panel" ) )
								.ContentPadding( FMargin( 1, 0 ) )
								.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
								.OnClicked( this, &SContentBrowser::SourcesViewExpandClicked )
								.Visibility( this, &SContentBrowser::GetPathExpanderVisibility )
								[
									SNew( SImage )
									.Image( this, &SContentBrowser::GetSourcesToggleImage )
								]
							]
						]

						// Filter
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew( SComboButton )
							.ComboButtonStyle( FEditorStyle::Get(), "GenericFilters.ComboButtonStyle" )
							.ForegroundColor(FLinearColor::White)
							.ContentPadding(0)
							.ToolTipText( LOCTEXT( "AddFilterToolTip", "Add an asset filter." ) )
							.OnGetMenuContent( this, &SContentBrowser::MakeAddFilterMenu )
							.HasDownArrow( true )
							.ContentPadding( FMargin( 1, 0 ) )
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserFiltersCombo")))
							.Visibility( ( Config != nullptr ? Config->bCanShowFilters : true ) ? EVisibility::Visible : EVisibility::Collapsed )
							.ButtonContent()
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
									.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
									.Text(FText::FromString(FString(TEXT("\xf0b0"))) /*fa-filter*/)
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2,0,0,0)
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
									.Text(LOCTEXT("Filters", "Filters"))
								]
							]
						]

						// Search
						+SHorizontalBox::Slot()
						.Padding(4, 1, 0, 0)
						.FillWidth(1.0f)
						[
							SAssignNew(SearchBoxPtr, SAssetSearchBox)
							.HintText( this, &SContentBrowser::GetSearchAssetsHintText )
							.OnTextChanged( this, &SContentBrowser::OnSearchBoxChanged )
							.OnTextCommitted( this, &SContentBrowser::OnSearchBoxCommitted )
							.PossibleSuggestions( this, &SContentBrowser::GetAssetSearchSuggestions )
							.DelayChangeNotificationsWhileTyping( true )
							.Visibility( ( Config != nullptr ? Config->bCanShowAssetSearch : true ) ? EVisibility::Visible : EVisibility::Collapsed )
							.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSearchAssets")))
						]

						// Save Search
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ToolTipText(LOCTEXT("SaveSearchButtonTooltip", "Save the current search as a dynamic collection."))
							.IsEnabled(this, &SContentBrowser::IsSaveSearchButtonEnabled)
							.OnClicked(this, &SContentBrowser::OnSaveSearchButtonClicked)
							.ContentPadding( FMargin(1, 1) )
							.Visibility( ( Config != nullptr ? Config->bCanShowAssetSearch : true ) ? EVisibility::Visible : EVisibility::Collapsed )
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
								.Text(FEditorFontGlyphs::Floppy_O)
							]
						]
					]

					// Filters
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(FilterListPtr, SFilterList)
						.OnFilterChanged(this, &SContentBrowser::OnFilterChanged)
						.OnGetContextMenu(this, &SContentBrowser::GetFilterContextMenu)
						.Visibility( ( Config != nullptr ? Config->bCanShowFilters : true ) ? EVisibility::Visible : EVisibility::Collapsed )
						.FrontendFilters(FrontendFilters)
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserFilters")))
					]

					// Assets
					+ SVerticalBox::Slot()
					.FillHeight( 1.0f )
					.Padding( 0 )
					[
						SAssignNew(AssetViewPtr, SAssetView)
						.ThumbnailLabel( Config != nullptr ? Config->ThumbnailLabel : EThumbnailLabel::ClassName )
						.ThumbnailScale( Config != nullptr ? Config->ThumbnailScale : 0.18f )
						.InitialViewType( Config != nullptr ? Config->InitialAssetViewType : EAssetViewType::Tile )
						.ShowBottomToolbar( Config != nullptr ? Config->bShowBottomToolbar : true )
						.OnPathSelected(this, &SContentBrowser::FolderEntered)
						.OnAssetSelected(this, &SContentBrowser::OnAssetSelectionChanged)
						.OnAssetsActivated(this, &SContentBrowser::OnAssetsActivated)
						.OnGetAssetContextMenu(this, &SContentBrowser::OnGetAssetContextMenu)
						.OnGetFolderContextMenu(this, &SContentBrowser::GetFolderContextMenu, false)
						.OnGetPathContextMenuExtender(this, &SContentBrowser::GetPathContextMenuExtender)
						.OnFindInAssetTreeRequested(this, &SContentBrowser::OnFindInAssetTreeRequested)
						.OnAssetRenameCommitted(this, &SContentBrowser::OnAssetRenameCommitted)
						.AreRealTimeThumbnailsAllowed(this, &SContentBrowser::IsHovered)
						.FrontendFilters(FrontendFilters)
						.HighlightedText(this, &SContentBrowser::GetHighlightedText)
						.AllowThumbnailEditMode(true)
						.AllowThumbnailHintLabel(false)
						.CanShowFolders(Config != nullptr ? Config->bCanShowFolders : true)
						.CanShowClasses(Config != nullptr ? Config->bCanShowClasses : true)
						.CanShowRealTimeThumbnails( Config != nullptr ? Config->bCanShowRealTimeThumbnails : true)
						.CanShowDevelopersFolder( Config != nullptr ? Config->bCanShowDevelopersFolder : true)
						.CanShowCollections(true)
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserAssets")))
					]
				]
			]
		]
	];

	AssetContextMenu = MakeShareable(new FAssetContextMenu(AssetViewPtr));
	AssetContextMenu->BindCommands(Commands);
	AssetContextMenu->SetOnFindInAssetTreeRequested( FOnFindInAssetTreeRequested::CreateSP(this, &SContentBrowser::OnFindInAssetTreeRequested) );
	AssetContextMenu->SetOnRenameRequested( FAssetContextMenu::FOnRenameRequested::CreateSP(this, &SContentBrowser::OnRenameRequested) );
	AssetContextMenu->SetOnRenameFolderRequested( FAssetContextMenu::FOnRenameFolderRequested::CreateSP(this, &SContentBrowser::OnRenameFolderRequested) );
	AssetContextMenu->SetOnDuplicateRequested( FAssetContextMenu::FOnDuplicateRequested::CreateSP(this, &SContentBrowser::OnDuplicateRequested) );
	AssetContextMenu->SetOnAssetViewRefreshRequested( FAssetContextMenu::FOnAssetViewRefreshRequested::CreateSP( this, &SContentBrowser::OnAssetViewRefreshRequested) );


	if( Config != nullptr && Config->SelectedCollectionName.Name != NAME_None )
	{
		// Select the specified collection by default
		FSourcesData DefaultSourcesData( Config->SelectedCollectionName );
		TArray<FString> SelectedPaths;
		AssetViewPtr->SetSourcesData( DefaultSourcesData );
	}
	else
	{
		// Select /Game by default
		FSourcesData DefaultSourcesData(FName("/Game"));
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(TEXT("/Game"));
		PathViewPtr->SetSelectedPaths(SelectedPaths);
		AssetViewPtr->SetSourcesData(DefaultSourcesData);
	}

	// Set the initial history data
	HistoryManager.AddHistoryData();

	// Load settings if they were specified
	this->InstanceName = InInstanceName;
	LoadSettings(InInstanceName);

	if( Config != nullptr )
	{
		// Make sure the sources view is initially visible if we were asked to show it
		if( ( bSourcesViewExpanded && ( !Config->bExpandSourcesView || !Config->bUseSourcesView ) ) ||
			( !bSourcesViewExpanded && Config->bExpandSourcesView && Config->bUseSourcesView ) )
		{
			SourcesViewExpandClicked();
		}
	}
	else
	{
		// in case we do not have a config, see what the global default settings are for the Sources Panel
		if (!bSourcesViewExpanded && GetDefault<UContentBrowserSettings>()->bOpenSourcesPanelByDefault)
		{
			SourcesViewExpandClicked();
		}
	}

	// Bindings to manage history when items are deleted
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	CollectionManagerModule.Get().OnCollectionRenamed().AddSP(this, &SContentBrowser::HandleCollectionRenamed);
	CollectionManagerModule.Get().OnCollectionDestroyed().AddSP(this, &SContentBrowser::HandleCollectionRemoved);
	CollectionManagerModule.Get().OnCollectionUpdated().AddSP(this, &SContentBrowser::HandleCollectionUpdated);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnPathRemoved().AddSP(this, &SContentBrowser::HandlePathRemoved);

	// We want to be able to search the feature packs in the super search so we need the module loaded 
	IAddContentDialogModule& AddContentDialogModule = FModuleManager::LoadModuleChecked<IAddContentDialogModule>("AddContentDialog");

	// Update the breadcrumb trail path
	UpdatePath();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SContentBrowser::BindCommands()
{
	Commands = TSharedPtr< FUICommandList >(new FUICommandList);

	Commands->MapAction(FGenericCommands::Get().Rename, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleRenameCommand),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleRenameCommandCanExecute)
	));

	Commands->MapAction(FGenericCommands::Get().Delete, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleDeleteCommandExecute),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleDeleteCommandCanExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().OpenAssetsOrFolders, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleOpenAssetsOrFoldersCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().PreviewAssets, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandlePreviewAssetsCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().CreateNewFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleCreateNewFolderCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().DirectoryUp, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleDirectoryUpCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().SaveSelectedAsset, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleSaveAssetCommand),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleSaveAssetCommandCanExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().SaveAllCurrentFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleSaveAllCurrentFolderCommand)
	));

	Commands->MapAction(FContentBrowserCommands::Get().ResaveAllCurrentFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleResaveAllCurrentFolderCommand)
	));

	Commands->MapAction(FGlobalEditorCommonCommands::Get().ViewReferences, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleViewReferencesCommand),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleViewReferencesCanExecute)
	));
}

EVisibility SContentBrowser::GetCollectionViewVisibility() const
{
	return bAlwaysShowCollections ? EVisibility::Visible : ( GetDefault<UContentBrowserSettings>()->GetDisplayCollections() ? EVisibility::Visible : EVisibility::Collapsed );
}

FText SContentBrowser::GetHighlightedText() const
{
	return TextFilter->GetRawFilterText();
}

void SContentBrowser::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	AssetViewPtr->CreateNewAsset(DefaultAssetName, PackagePath, AssetClass, Factory);
}

bool SContentBrowser::IsImportEnabled() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	return SourcesData.PackagePaths.Num() == 1 && !ContentBrowserUtils::IsClassPath(SourcesData.PackagePaths[0].ToString());
}

FText SContentBrowser::GetImportTooltipText() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	if ( SourcesData.PackagePaths.Num() == 1 )
	{
		const FString CurrentPath = SourcesData.PackagePaths[0].ToString();
		if ( ContentBrowserUtils::IsClassPath( CurrentPath ) )
		{
			return LOCTEXT( "ImportAssetToolTip_InvalidClassPath", "Cannot import assets to class paths." );
		}
		else
		{
			return FText::Format( LOCTEXT( "ImportAssetToolTip", "Import to {0}..." ), FText::FromString( CurrentPath ) );
		}
	}
	else if ( SourcesData.PackagePaths.Num() > 1 )
	{
		return LOCTEXT( "ImportAssetToolTip_MultiplePaths", "Cannot import assets to multiple paths." );
	}
	
	return LOCTEXT( "ImportAssetToolTip_NoPath", "No path is selected as an import target." );
}

FReply SContentBrowser::HandleImportClicked()
{
	ImportAsset( GetCurrentPath() );
	return FReply::Handled();
}

void SContentBrowser::ImportAsset( const FString& InPath )
{
	if ( ensure( !InPath.IsEmpty() ) )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
		AssetToolsModule.Get().ImportAssetsWithDialog( InPath );
	}
}

void SContentBrowser::PrepareToSync( const TArray<FAssetData>& AssetDataList, const TArray<FString>& FolderPaths, const bool bDisableFiltersThatHideAssets )
{
	// Check to see if any of the assets require certain folders to be visible
	bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
	bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	bool bDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
	bool bDisplayLocalized = GetDefault<UContentBrowserSettings>()->GetDisplayL10NFolder();
	if ( !bDisplayDev || !bDisplayEngine || !bDisplayPlugins || !bDisplayLocalized )
	{
		TSet<FString> PackagePaths = TSet<FString>(FolderPaths);
		for (const FAssetData& AssetData : AssetDataList)
		{
			FString PackagePath;
			if (AssetData.AssetClass == NAME_Class)
			{
				// Classes are found in the /Classes_ roots
				TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
				NativeClassHierarchy->GetClassPath(Cast<UClass>(AssetData.GetAsset()), PackagePath, false/*bIncludeClassName*/);
			}
			else
			{
				// All other assets are found by their package path
				PackagePath = AssetData.PackagePath.ToString();
			}

			PackagePaths.Add(PackagePath);
		}

		bool bRepopulate = false;
		for (const FString& PackagePath : PackagePaths)
		{
			const ContentBrowserUtils::ECBFolderCategory FolderCategory = ContentBrowserUtils::GetFolderCategory( PackagePath );
			if ( !bDisplayDev && FolderCategory == ContentBrowserUtils::ECBFolderCategory::DeveloperContent )
			{
				bDisplayDev = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder(true, true);
				bRepopulate = true;
			}
			else if ( !bDisplayEngine && (FolderCategory == ContentBrowserUtils::ECBFolderCategory::EngineContent || FolderCategory == ContentBrowserUtils::ECBFolderCategory::EngineClasses) )
			{
				bDisplayEngine = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder(true, true);
				bRepopulate = true;
			}
			else if ( !bDisplayPlugins && (FolderCategory == ContentBrowserUtils::ECBFolderCategory::PluginContent || FolderCategory == ContentBrowserUtils::ECBFolderCategory::PluginClasses) )
			{
				bDisplayPlugins = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders(true, true);
				bRepopulate = true;
			}

			if (!bDisplayLocalized && ContentBrowserUtils::IsLocalizationFolder(PackagePath))
			{
				bDisplayLocalized = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayL10NFolder(true);
				bRepopulate = true;
			}

			if (bDisplayDev && bDisplayEngine && bDisplayPlugins && bDisplayLocalized)
			{
				break;
			}
		}

		// If we have auto-enabled any flags, force a refresh
		if ( bRepopulate )
		{
			PathViewPtr->Populate();
		}
	}

	if ( bDisableFiltersThatHideAssets )
	{
		// Disable the filter categories
		FilterListPtr->DisableFiltersThatHideAssets(AssetDataList);
	}

	// Disable the filter search (reset the filter, then clear the search text)
	// Note: we have to remove the filter immediately, we can't wait for OnSearchBoxChanged to hit
	SetSearchBoxText(FText::GetEmpty());
	SearchBoxPtr->SetText(FText::GetEmpty());
	SearchBoxPtr->SetError(FText::GetEmpty());
}

void SContentBrowser::SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets )
{
	PrepareToSync(AssetDataList, TArray<FString>(), bDisableFiltersThatHideAssets);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncToAssets(AssetDataList, bAllowImplicitSync);
	AssetViewPtr->SyncToAssets(AssetDataList);
}

void SContentBrowser::SyncToFolders( const TArray<FString>& FolderList, const bool bAllowImplicitSync )
{
	PrepareToSync(TArray<FAssetData>(), FolderList, false);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncToFolders(FolderList, bAllowImplicitSync);
	AssetViewPtr->SyncToFolders(FolderList);
}

void SContentBrowser::SyncTo( const FContentBrowserSelection& ItemSelection, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets )
{
	PrepareToSync(ItemSelection.SelectedAssets, ItemSelection.SelectedFolders, bDisableFiltersThatHideAssets);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncTo(ItemSelection, bAllowImplicitSync);
	AssetViewPtr->SyncTo(ItemSelection);
}

void SContentBrowser::SetIsPrimaryContentBrowser(bool NewIsPrimary)
{
	bIsPrimaryBrowser = NewIsPrimary;

	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}
	else
	{
		USelection* EditorSelection = GEditor->GetSelectedObjects();
		if ( ensure( EditorSelection != NULL ) )
		{
			EditorSelection->DeselectAll();
		}
	}
}

TSharedPtr<FTabManager> SContentBrowser::GetTabManager() const
{
	if ( ContainingTab.IsValid() )
	{
		return ContainingTab.Pin()->GetTabManager();
	}

	return NULL;
}

void SContentBrowser::LoadSelectedObjectsIfNeeded()
{
	// Get the selected assets in the asset view
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	// Load every asset that isn't already in memory
	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		const FAssetData& AssetData = *AssetIt;
		const bool bShowProgressDialog = (!AssetData.IsAssetLoaded() && FEditorFileUtils::IsMapPackageAsset(AssetData.ObjectPath.ToString()));
		GWarn->BeginSlowTask(LOCTEXT("LoadingObjects", "Loading Objects..."), bShowProgressDialog);

		(*AssetIt).GetAsset();

		GWarn->EndSlowTask();
	}

	// Sync the global selection set if we are the primary browser
	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}
}

void SContentBrowser::GetSelectedAssets(TArray<FAssetData>& SelectedAssets)
{
	// Make sure the asset data is up to date
	AssetViewPtr->ProcessRecentlyLoadedOrChangedAssets();

	SelectedAssets = AssetViewPtr->GetSelectedAssets();
}

void SContentBrowser::SaveSettings() const
{
	const FString& SettingsString = InstanceName.ToString();

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), bSourcesViewExpanded, GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".Locked")), bIsLocked, GEditorPerProjectIni);

	for(int32 SlotIndex = 0; SlotIndex < PathAssetSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->SetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".VerticalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
	}
	
	for(int32 SlotIndex = 0; SlotIndex < PathCollectionSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathCollectionSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->SetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".HorizontalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
	}

	// Save all our data using the settings string as a key in the user settings ini
	FilterListPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	PathViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	CollectionViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	AssetViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
}

const FName SContentBrowser::GetInstanceName() const
{
	return InstanceName;
}

bool SContentBrowser::IsLocked() const
{
	return bIsLocked;
}

void SContentBrowser::SetKeyboardFocusOnSearch() const
{
	// Focus on the search box
	FSlateApplication::Get().SetKeyboardFocus( SearchBoxPtr, EFocusCause::SetDirectly );
}

FReply SContentBrowser::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if( Commands->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SContentBrowser::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Clicking in a content browser will shift it to be the primary browser
	FContentBrowserSingleton::Get().SetPrimaryContentBrowser(SharedThis(this));

	return FReply::Unhandled();
}

FReply SContentBrowser::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		HistoryManager.GoBack();
		return FReply::Handled();
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		HistoryManager.GoForward();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SContentBrowser::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		HistoryManager.GoBack();
		return FReply::Handled();
	}
	else if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		HistoryManager.GoForward();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SContentBrowser::OnContainingTabSavingVisualState() const
{
	SaveSettings();
}

void SContentBrowser::OnContainingTabClosed(TSharedRef<SDockTab> DockTab)
{
	FContentBrowserSingleton::Get().ContentBrowserClosed( SharedThis(this) );
}

void SContentBrowser::OnContainingTabActivated(TSharedRef<SDockTab> DockTab, ETabActivationCause InActivationCause)
{
	if(InActivationCause == ETabActivationCause::UserClickedOnTab)
	{
		FContentBrowserSingleton::Get().SetPrimaryContentBrowser(SharedThis(this));
	}
}

void SContentBrowser::LoadSettings(const FName& InInstanceName)
{
	FString SettingsString = InInstanceName.ToString();

	// Test to see if we should load legacy settings from a previous instance name
	// First make sure there aren't any existing settings with the given instance name
	bool TestBool;
	if ( !GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), TestBool, GEditorPerProjectIni) )
	{
		// If there were not any settings and we are Content Browser 1, see if we have any settings under the legacy name "LevelEditorContentBrowser"
		if ( InInstanceName.ToString() == TEXT("ContentBrowserTab1") && GConfig->GetBool(*SettingsIniSection, TEXT("LevelEditorContentBrowser.SourcesExpanded"), TestBool, GEditorPerProjectIni) )
		{
			// We have found some legacy settings with the old ID, use them. These settings will be saved out to the new id later
			SettingsString = TEXT("LevelEditorContentBrowser");
		}
		// else see if we are Content Browser 2, and see if we have any settings under the legacy name "MajorContentBrowserTab"
		else if ( InInstanceName.ToString() == TEXT("ContentBrowserTab2") && GConfig->GetBool(*SettingsIniSection, TEXT("MajorContentBrowserTab.SourcesExpanded"), TestBool, GEditorPerProjectIni) )
		{
			// We have found some legacy settings with the old ID, use them. These settings will be saved out to the new id later
			SettingsString = TEXT("MajorContentBrowserTab");
		}
	}

	// Now that we have determined the appropriate settings string, actually load the settings
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), bSourcesViewExpanded, GEditorPerProjectIni);
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".Locked")), bIsLocked, GEditorPerProjectIni);

	for(int32 SlotIndex = 0; SlotIndex < PathAssetSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->GetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".VerticalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
		PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue = SplitterSize;
	}
	
	for(int32 SlotIndex = 0; SlotIndex < PathCollectionSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathCollectionSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->GetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".HorizontalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
		PathCollectionSplitterPtr->SlotAt(SlotIndex).SizeValue = SplitterSize;
	}

	// Save all our data using the settings string as a key in the user settings ini
	FilterListPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	PathViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	CollectionViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	AssetViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
}

void SContentBrowser::SourcesChanged(const TArray<FString>& SelectedPaths, const TArray<FCollectionNameType>& SelectedCollections)
{
	FString NewSource = SelectedPaths.Num() > 0 ? SelectedPaths[0] : (SelectedCollections.Num() > 0 ? SelectedCollections[0].Name.ToString() : TEXT("None"));
	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("The content browser source was changed by the sources view to '%s'"), *NewSource);

	FSourcesData SourcesData;
	{
		TArray<FName> SelectedPathNames;
		SelectedPathNames.Reserve(SelectedPaths.Num());
		for (const FString& SelectedPath : SelectedPaths)
		{
			SelectedPathNames.Add(FName(*SelectedPath));
		}
		SourcesData = FSourcesData(MoveTemp(SelectedPathNames), SelectedCollections);
	}

	// A dynamic collection should apply its search query to the CB search, so we need to stash the current search so that we can restore it again later
	if (SourcesData.IsDynamicCollection())
	{
		// Only stash the user search term once in case we're switching between dynamic collections
		if (!StashedSearchBoxText.IsSet())
		{
			StashedSearchBoxText = TextFilter->GetRawFilterText();
		}

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		const FCollectionNameType& DynamicCollection = SourcesData.Collections[0];

		FString DynamicQueryString;
		CollectionManagerModule.Get().GetDynamicQueryText(DynamicCollection.Name, DynamicCollection.Type, DynamicQueryString);

		const FText DynamicQueryText = FText::FromString(DynamicQueryString);
		SetSearchBoxText(DynamicQueryText);
		SearchBoxPtr->SetText(DynamicQueryText);
	}
	else if (StashedSearchBoxText.IsSet())
	{
		// Restore the stashed search term
		const FText StashedText = StashedSearchBoxText.GetValue();
		StashedSearchBoxText.Reset();

		SetSearchBoxText(StashedText);
		SearchBoxPtr->SetText(StashedText);
	}

	if (!AssetViewPtr->GetSourcesData().IsEmpty())
	{
		// Update the current history data to preserve selection if there is a valid SourcesData
		HistoryManager.UpdateHistoryData();
	}

	// Change the filter for the asset view
	AssetViewPtr->SetSourcesData(SourcesData);

	// Add a new history data now that the source has changed
	HistoryManager.AddHistoryData();

	// Update the breadcrumb trail path
	UpdatePath();
}

void SContentBrowser::FolderEntered(const FString& FolderPath)
{
	// Have we entered a sub-collection folder?
	FName CollectionName;
	ECollectionShareType::Type CollectionFolderShareType = ECollectionShareType::CST_All;
	if (ContentBrowserUtils::IsCollectionPath(FolderPath, &CollectionName, &CollectionFolderShareType))
	{
		const FCollectionNameType SelectedCollection(CollectionName, CollectionFolderShareType);

		TArray<FCollectionNameType> Collections;
		Collections.Add(SelectedCollection);
		CollectionViewPtr->SetSelectedCollections(Collections);

		CollectionSelected(SelectedCollection);
	}
	else
	{
		// set the path view to the incoming path
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(FolderPath);
		PathViewPtr->SetSelectedPaths(SelectedPaths);

		PathSelected(SelectedPaths[0]);
	}
}

void SContentBrowser::PathSelected(const FString& FolderPath)
{
	// You may not select both collections and paths
	CollectionViewPtr->ClearSelection();

	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	TArray<FCollectionNameType> SelectedCollections;
	SourcesChanged(SelectedPaths, SelectedCollections);

	// Notify 'asset path changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnAssetPathChanged& PathChangedDelegate = ContentBrowserModule.GetOnAssetPathChanged();
	if(PathChangedDelegate.IsBound())
	{
		PathChangedDelegate.Broadcast(FolderPath);
	}

	// Update the context menu's selected paths list
	PathContextMenu->SetSelectedPaths(SelectedPaths);
}

TSharedRef<FExtender> SContentBrowser::GetPathContextMenuExtender(const TArray<FString>& InSelectedPaths) const
{
	return PathContextMenu->MakePathViewContextMenuExtender(InSelectedPaths);
}

void SContentBrowser::CollectionSelected(const FCollectionNameType& SelectedCollection)
{
	// You may not select both collections and paths
	PathViewPtr->ClearSelection();

	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();
	TArray<FString> SelectedPaths;

	if( SelectedCollections.Num() == 0  )
	{
		// just select the game folder
		SelectedPaths.Add(TEXT("/Game"));
		SourcesChanged(SelectedPaths, SelectedCollections);
	}
	else
	{
		SourcesChanged(SelectedPaths, SelectedCollections);
	}

}

void SContentBrowser::PathPickerPathSelected(const FString& FolderPath)
{
	PathPickerButton->SetIsOpen(false);

	if ( !FolderPath.IsEmpty() )
	{
		TArray<FString> Paths;
		Paths.Add(FolderPath);
		PathViewPtr->SetSelectedPaths(Paths);
	}

	PathSelected(FolderPath);
}

void SContentBrowser::SetSelectedPaths(const TArray<FString>& FolderPaths, bool bNeedsRefresh/* = false */)
{
	if (FolderPaths.Num() > 0)
	{
		if (bNeedsRefresh)
		{
			PathViewPtr->Populate();
		}

		PathViewPtr->SetSelectedPaths(FolderPaths);

		PathSelected(FolderPaths[0]);
	}
}

void SContentBrowser::ForceShowPluginContent(bool bEnginePlugin)
{
	if (AssetViewPtr.IsValid())
	{
		AssetViewPtr->ForceShowPluginFolder(bEnginePlugin);
	}
}

void SContentBrowser::PathPickerCollectionSelected(const FCollectionNameType& SelectedCollection)
{
	PathPickerButton->SetIsOpen(false);

	TArray<FCollectionNameType> Collections;
	Collections.Add(SelectedCollection);
	CollectionViewPtr->SetSelectedCollections(Collections);

	CollectionSelected(SelectedCollection);
}

void SContentBrowser::OnApplyHistoryData( const FHistoryData& History )
{
	PathViewPtr->ApplyHistoryData(History);
	CollectionViewPtr->ApplyHistoryData(History);
	AssetViewPtr->ApplyHistoryData(History);

	// Update the breadcrumb trail path
	UpdatePath();

	if (History.SourcesData.HasPackagePaths())
	{
		// Notify 'asset path changed' delegate
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		FContentBrowserModule::FOnAssetPathChanged& PathChangedDelegate = ContentBrowserModule.GetOnAssetPathChanged();
		if (PathChangedDelegate.IsBound())
		{
			PathChangedDelegate.Broadcast(History.SourcesData.PackagePaths[0].ToString());
		}
	}
}

void SContentBrowser::OnUpdateHistoryData(FHistoryData& HistoryData) const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	const FText NewSource = SourcesData.HasPackagePaths() ? FText::FromName(SourcesData.PackagePaths[0]) : (SourcesData.HasCollections() ? FText::FromName(SourcesData.Collections[0].Name) : LOCTEXT("AllAssets", "All Assets"));

	HistoryData.HistoryDesc = NewSource;
	HistoryData.SourcesData = SourcesData;

	HistoryData.SelectionData.Reset();
	HistoryData.SelectionData.SelectedFolders = TSet<FString>(AssetViewPtr->GetSelectedFolders());
	for (const FAssetData& SelectedAsset : SelectedAssets)
	{
		HistoryData.SelectionData.SelectedAssets.Add(SelectedAsset.ObjectPath);
	}
}

void SContentBrowser::NewAssetRequested(const FString& SelectedPath, TWeakObjectPtr<UClass> FactoryClass)
{
	if ( ensure(SelectedPath.Len() > 0) && ensure(FactoryClass.IsValid()) )
	{
		UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), FactoryClass.Get());
		FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(NewFactory);
		if ( NewFactory->ConfigureProperties() )
		{
			FString DefaultAssetName;
			FString PackageNameToUse;

			static FName AssetToolsModuleName = FName("AssetTools");
			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
			AssetToolsModule.Get().CreateUniqueAssetName(SelectedPath + TEXT("/") + NewFactory->GetDefaultNewAssetName(), TEXT(""), PackageNameToUse, DefaultAssetName);
			CreateNewAsset(DefaultAssetName, SelectedPath, NewFactory->GetSupportedClass(), NewFactory);
		}
	}
}

void SContentBrowser::NewClassRequested(const FString& SelectedPath)
{
	// Parse out the on disk location for the currently selected path, this will then be used as the default location for the new class (if a valid project module location)
	FString ExistingFolderPath;
	if (!SelectedPath.IsEmpty())
	{
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
		NativeClassHierarchy->GetFileSystemPath(SelectedPath, ExistingFolderPath);
	}

	FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog(
		FAddToProjectConfig()
		.InitialPath(ExistingFolderPath)
		.ParentWindow(FGlobalTabmanager::Get()->GetRootWindow())
	);
}

void SContentBrowser::NewFolderRequested(const FString& SelectedPath)
{
	if( ensure(SelectedPath.Len() > 0) && AssetViewPtr.IsValid() )
	{
		CreateNewFolder(SelectedPath, FOnCreateNewFolder::CreateSP(AssetViewPtr.Get(), &SAssetView::OnCreateNewFolder));
	}
}

void SContentBrowser::SetSearchBoxText(const FText& InSearchText)
{
	// Has anything changed? (need to test case as the operators are case-sensitive)
	if (!InSearchText.ToString().Equals(TextFilter->GetRawFilterText().ToString(), ESearchCase::CaseSensitive))
	{
		TextFilter->SetRawFilterText( InSearchText );
		SearchBoxPtr->SetError( TextFilter->GetFilterErrorText() );
		if(InSearchText.IsEmpty())
		{
			FrontendFilters->Remove(TextFilter);
			AssetViewPtr->SetUserSearching(false);
		}
		else
		{
			FrontendFilters->Add(TextFilter);
			AssetViewPtr->SetUserSearching(true);
		}
	}
}

void SContentBrowser::OnSearchBoxChanged(const FText& InSearchText)
{
	SetSearchBoxText(InSearchText);

	// Broadcast 'search box changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	ContentBrowserModule.GetOnSearchBoxChanged().Broadcast(InSearchText, bIsPrimaryBrowser);	
}

void SContentBrowser::OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo)
{
	SetSearchBoxText(InSearchText);
}

bool SContentBrowser::IsSaveSearchButtonEnabled() const
{
	return !TextFilter->GetRawFilterText().IsEmptyOrWhitespace();
}

FReply SContentBrowser::OnSaveSearchButtonClicked()
{
	// Need to make sure we can see the collections view
	if (!bSourcesViewExpanded)
	{
		SourcesViewExpandClicked();
	}

	// We want to add any currently selected paths to the final saved query so that you get back roughly the same list of objects as what you're currently seeing
	FString SelectedPathsQuery;
	{
		const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
		for (int32 SelectedPathIndex = 0; SelectedPathIndex < SourcesData.PackagePaths.Num(); ++SelectedPathIndex)
		{
			SelectedPathsQuery.Append(TEXT("Path:'"));
			SelectedPathsQuery.Append(SourcesData.PackagePaths[SelectedPathIndex].ToString());
			SelectedPathsQuery.Append(TEXT("'..."));

			if (SelectedPathIndex + 1 < SourcesData.PackagePaths.Num())
			{
				SelectedPathsQuery.Append(TEXT(" OR "));
			}
		}
	}

	// todo: should we automatically append any type filters too?

	// Produce the final query
	FText FinalQueryText;
	if (SelectedPathsQuery.IsEmpty())
	{
		FinalQueryText = TextFilter->GetRawFilterText();
	}
	else
	{
		FinalQueryText = FText::FromString(FString::Printf(TEXT("(%s) AND (%s)"), *TextFilter->GetRawFilterText().ToString(), *SelectedPathsQuery));
	}

	CollectionViewPtr->MakeSaveDynamicCollectionMenu(FinalQueryText);
	return FReply::Handled();
}

void SContentBrowser::OnPathClicked( const FString& CrumbData )
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	if ( SourcesData.HasCollections() )
	{
		// Collection crumb was clicked. See if we've clicked on a different collection in the hierarchy, and change the path if required.
		TOptional<FCollectionNameType> CollectionClicked;
		{
			FString CollectionName;
			FString CollectionTypeString;
			if (CrumbData.Split(TEXT("?"), &CollectionName, &CollectionTypeString))
			{
				const int32 CollectionType = FCString::Atoi(*CollectionTypeString);
				if (CollectionType >= 0 && CollectionType < ECollectionShareType::CST_All)
				{
					CollectionClicked = FCollectionNameType(FName(*CollectionName), ECollectionShareType::Type(CollectionType));
				}
			}
		}

		if ( CollectionClicked.IsSet() && SourcesData.Collections[0] != CollectionClicked.GetValue() )
		{
			TArray<FCollectionNameType> Collections;
			Collections.Add(CollectionClicked.GetValue());
			CollectionViewPtr->SetSelectedCollections(Collections);

			CollectionSelected(CollectionClicked.GetValue());
		}
	}
	else if ( !SourcesData.HasPackagePaths() )
	{
		// No collections or paths are selected. This is "All Assets". Don't change the path when this is clicked.
	}
	else if ( SourcesData.PackagePaths.Num() > 1 || SourcesData.PackagePaths[0].ToString() != CrumbData )
	{
		// More than one path is selected or the crumb that was clicked is not the same path as the current one. Change the path.
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(CrumbData);
		PathViewPtr->SetSelectedPaths(SelectedPaths);

		PathSelected(SelectedPaths[0]);
	}
}

void SContentBrowser::OnPathMenuItemClicked(FString ClickedPath)
{
	OnPathClicked( ClickedPath );
}

TSharedPtr<SWidget> SContentBrowser::OnGetCrumbDelimiterContent(const FString& CrumbData) const
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
	TSharedPtr<SWidget> MenuWidget;

	if( SourcesData.HasCollections() )
	{
		TOptional<FCollectionNameType> CollectionClicked;
		{
			FString CollectionName;
			FString CollectionTypeString;
			if (CrumbData.Split(TEXT("?"), &CollectionName, &CollectionTypeString))
			{
				const int32 CollectionType = FCString::Atoi(*CollectionTypeString);
				if (CollectionType >= 0 && CollectionType < ECollectionShareType::CST_All)
				{
					CollectionClicked = FCollectionNameType(FName(*CollectionName), ECollectionShareType::Type(CollectionType));
				}
			}
		}

		if( CollectionClicked.IsSet() )
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			TArray<FCollectionNameType> ChildCollections;
			CollectionManagerModule.Get().GetChildCollections(CollectionClicked->Name, CollectionClicked->Type, ChildCollections);

			if( ChildCollections.Num() > 0 )
			{
				FMenuBuilder MenuBuilder( true, nullptr );

				for( const FCollectionNameType& ChildCollection : ChildCollections )
				{
					const FString ChildCollectionCrumbData = FString::Printf(TEXT("%s?%s"), *ChildCollection.Name.ToString(), *FString::FromInt(ChildCollection.Type));

					MenuBuilder.AddMenuEntry(
						FText::FromName(ChildCollection.Name),
						FText::GetEmpty(),
						FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ChildCollection.Type)),
						FUIAction(FExecuteAction::CreateSP(this, &SContentBrowser::OnPathMenuItemClicked, ChildCollectionCrumbData))
						);
				}

				MenuWidget = MenuBuilder.MakeWidget();
			}
		}
	}
	else if( SourcesData.HasPackagePaths() )
	{
		TArray<FString> SubPaths;
		const bool bRecurse = false;
		if( ContentBrowserUtils::IsClassPath(CrumbData) )
		{
			TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();

			FNativeClassHierarchyFilter ClassFilter;
			ClassFilter.ClassPaths.Add(FName(*CrumbData));
			ClassFilter.bRecursivePaths = bRecurse;

			NativeClassHierarchy->GetMatchingFolders(ClassFilter, SubPaths);
		}
		else
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			AssetRegistry.GetSubPaths( CrumbData, SubPaths, bRecurse );
		}

		if( SubPaths.Num() > 0 )
		{
			FMenuBuilder MenuBuilder( true, nullptr );

			for( int32 PathIndex = 0; PathIndex < SubPaths.Num(); ++PathIndex )
			{
				const FString& SubPath = SubPaths[PathIndex];

				// For displaying in the menu cut off the parent path since it is redundant
				FString PathWithoutParent = SubPath.RightChop( CrumbData.Len() + 1 );
				MenuBuilder.AddMenuEntry(
					FText::FromString(PathWithoutParent),
					FText::GetEmpty(),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.BreadcrumbPathPickerFolder"),
					FUIAction(FExecuteAction::CreateSP(this, &SContentBrowser::OnPathMenuItemClicked, SubPath))
					);
			}

			MenuWidget = MenuBuilder.MakeWidget();
		}
	}

	if( MenuWidget.IsValid() )
	{
		// Do not allow the menu to become too large if there are many directories
		Widget =
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.MaxHeight( 400.0f )
			[
				MenuWidget.ToSharedRef()
			];
	}

	return Widget;
}

TSharedRef<SWidget> SContentBrowser::GetPathPickerContent()
{
	FPathPickerConfig PathPickerConfig;

	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();
	if ( SourcesData.HasPackagePaths() )
	{
		PathPickerConfig.DefaultPath = SourcesData.PackagePaths[0].ToString();
	}
	
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SContentBrowser::PathPickerPathSelected);
	PathPickerConfig.bAllowContextMenu = false;
	PathPickerConfig.bAllowClassesFolder = true;

	return SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(500)
		.Padding(4)
		[
			SNew(SVerticalBox)

			// Path Picker
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				FContentBrowserSingleton::Get().CreatePathPicker(PathPickerConfig)
			]

			// Collection View
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 6, 0, 0)
			[
				SNew(SCollectionView)
				.AllowCollectionButtons(false)
				.OnCollectionSelected(this, &SContentBrowser::PathPickerCollectionSelected)
				.AllowContextMenu(false)
			]
		];
}

FString SContentBrowser::GetCurrentPath() const
{
	FString CurrentPath;
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	if ( SourcesData.HasPackagePaths() && SourcesData.PackagePaths[0] != NAME_None )
	{
		CurrentPath = SourcesData.PackagePaths[0].ToString();
	}

	return CurrentPath;
}

TSharedRef<SWidget> SContentBrowser::MakeAddNewContextMenu(bool bShowGetContent, bool bShowImport)
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SourcesData.PackagePaths, NumAssetPaths, NumClassPaths);

	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender_SelectedPaths> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetContextMenuExtenders();
	
	// Delegate wants paths as FStrings
	TArray<FString> SelectPaths;
	for (FName PathName: SourcesData.PackagePaths)
	{
		SelectPaths.Add(PathName.ToString());
	}

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(SelectPaths));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL, MenuExtender);

	// Only add "New Folder" item if we do not have a collection selected
	FNewAssetOrClassContextMenu::FOnNewFolderRequested OnNewFolderRequested;
	if (CollectionViewPtr->GetSelectedCollections().Num() == 0)
	{
		OnNewFolderRequested = FNewAssetOrClassContextMenu::FOnNewFolderRequested::CreateSP(this, &SContentBrowser::NewFolderRequested);
	}


	// New feature packs don't depend on the current paths, so we always add this item if it was requested
	FNewAssetOrClassContextMenu::FOnGetContentRequested OnGetContentRequested;
	if(bShowGetContent)
	{
		OnGetContentRequested = FNewAssetOrClassContextMenu::FOnGetContentRequested::CreateSP(this, &SContentBrowser::OnAddContentRequested);
	}

	// Only the asset items if we have an asset path selected
	FNewAssetOrClassContextMenu::FOnNewAssetRequested OnNewAssetRequested;
	FNewAssetOrClassContextMenu::FOnImportAssetRequested OnImportAssetRequested;
	if(NumAssetPaths > 0)
	{
		OnNewAssetRequested = FNewAssetOrClassContextMenu::FOnNewAssetRequested::CreateSP(this, &SContentBrowser::NewAssetRequested);
		if(bShowImport)
		{
			OnImportAssetRequested = FNewAssetOrClassContextMenu::FOnImportAssetRequested::CreateSP(this, &SContentBrowser::ImportAsset);
		}
	}

	// This menu always lets you create classes, but uses your default project source folder if the selected path is invalid for creating classes
	FNewAssetOrClassContextMenu::FOnNewClassRequested OnNewClassRequested = FNewAssetOrClassContextMenu::FOnNewClassRequested::CreateSP(this, &SContentBrowser::NewClassRequested);

	FNewAssetOrClassContextMenu::MakeContextMenu(
		MenuBuilder, 
		SourcesData.PackagePaths, 
		OnNewAssetRequested,
		OnNewClassRequested,
		OnNewFolderRequested,
		OnImportAssetRequested,
		OnGetContentRequested
		);

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	return 
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.MaxHeight(DisplaySize.Y * 0.9)
		[
			MenuBuilder.MakeWidget()
		];
}

bool SContentBrowser::IsAddNewEnabled() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	return SourcesData.PackagePaths.Num() == 1;
}

FText SContentBrowser::GetAddNewToolTipText() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	if ( SourcesData.PackagePaths.Num() == 1 )
	{
		const FString CurrentPath = SourcesData.PackagePaths[0].ToString();
		if ( ContentBrowserUtils::IsClassPath( CurrentPath ) )
		{
			return FText::Format( LOCTEXT("AddNewToolTip_AddNewClass", "Create a new class in {0}..."), FText::FromString(CurrentPath) );
		}
		else
		{
			return FText::Format( LOCTEXT("AddNewToolTip_AddNewAsset", "Create a new asset in {0}..."), FText::FromString(CurrentPath) );
		}
	}
	else if ( SourcesData.PackagePaths.Num() > 1 )
	{
		return LOCTEXT( "AddNewToolTip_MultiplePaths", "Cannot add assets or classes to multiple paths." );
	}
	
	return LOCTEXT( "AddNewToolTip_NoPath", "No path is selected as an add target." );
}

TSharedRef<SWidget> SContentBrowser::MakeAddFilterMenu()
{
	return FilterListPtr->ExternalMakeAddFilterMenu();
}

TSharedPtr<SWidget> SContentBrowser::GetFilterContextMenu()
{
	return FilterListPtr->ExternalMakeAddFilterMenu();
}

FReply SContentBrowser::OnSaveClicked()
{
	ContentBrowserUtils::SaveDirtyPackages();
	return FReply::Handled();
}

void SContentBrowser::OnAddContentRequested()
{
	IAddContentDialogModule& AddContentDialogModule = FModuleManager::LoadModuleChecked<IAddContentDialogModule>("AddContentDialog");
	FWidgetPath WidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetChecked(AsShared(), WidgetPath);
	AddContentDialogModule.ShowDialog(WidgetPath.GetWindow());
}

void SContentBrowser::OnAssetSelectionChanged(const FAssetData& SelectedAsset)
{
	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}

	// Notify 'asset selection changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnAssetSelectionChanged& AssetSelectionChangedDelegate = ContentBrowserModule.GetOnAssetSelectionChanged();
	
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();
	AssetContextMenu->SetSelectedAssets(SelectedAssets);
	CollectionViewPtr->SetSelectedAssets(SelectedAssets);
	if(AssetSelectionChangedDelegate.IsBound())
	{
		AssetSelectionChangedDelegate.Broadcast(SelectedAssets, bIsPrimaryBrowser);
	}
}

void SContentBrowser::OnAssetsActivated(const TArray<FAssetData>& ActivatedAssets, EAssetTypeActivationMethod::Type ActivationMethod)
{
	TMap< TSharedRef<IAssetTypeActions>, TArray<UObject*> > TypeActionsToObjects;
	TArray<UObject*> ObjectsWithoutTypeActions;

	const FText LoadingTemplate = LOCTEXT("LoadingAssetName", "Loading {0}...");
	const FText DefaultText = ActivatedAssets.Num() == 1 ? FText::Format(LoadingTemplate, FText::FromName(ActivatedAssets[0].AssetName)) : LOCTEXT("LoadingObjects", "Loading Objects...");
	FScopedSlowTask SlowTask(100, DefaultText);

	// Iterate over all activated assets to map them to AssetTypeActions.
	// This way individual asset type actions will get a batched list of assets to operate on
	for ( auto AssetIt = ActivatedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		const FAssetData& AssetData = *AssetIt;
		if (!AssetData.IsAssetLoaded() && FEditorFileUtils::IsMapPackageAsset(AssetData.ObjectPath.ToString()))
		{
			SlowTask.MakeDialog();
		}

		SlowTask.EnterProgressFrame(75.f/ActivatedAssets.Num(), FText::Format(LoadingTemplate, FText::FromName(AssetData.AssetName)));

		UObject* Asset = (*AssetIt).GetAsset();

		if ( Asset != NULL )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Asset->GetClass());
			if ( AssetTypeActions.IsValid() )
			{
				// Add this asset to the list associated with the asset type action object
				TArray<UObject*>& ObjList = TypeActionsToObjects.FindOrAdd(AssetTypeActions.Pin().ToSharedRef());
				ObjList.AddUnique(Asset);
			}
			else
			{
				ObjectsWithoutTypeActions.AddUnique(Asset);
			}
		}
	}

	// Now that we have created our map, activate all the lists of objects for each asset type action.
	for ( auto TypeActionsIt = TypeActionsToObjects.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt )
	{
		SlowTask.EnterProgressFrame(25.f/TypeActionsToObjects.Num());

		const TSharedRef<IAssetTypeActions>& TypeActions = TypeActionsIt.Key();
		const TArray<UObject*>& ObjList = TypeActionsIt.Value();

		TypeActions->AssetsActivated(ObjList, ActivationMethod);
	}

	// Finally, open a simple asset editor for all assets which do not have asset type actions if activating with enter or double click
	if ( ActivationMethod == EAssetTypeActivationMethod::DoubleClicked || ActivationMethod == EAssetTypeActivationMethod::Opened )
	{
		ContentBrowserUtils::OpenEditorForAsset(ObjectsWithoutTypeActions);
	}
}

TSharedPtr<SWidget> SContentBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	if ( SelectedAssets.Num() == 0 )
	{
		return MakeAddNewContextMenu( false, true );
	}
	else
	{
		return AssetContextMenu->MakeContextMenu(SelectedAssets, AssetViewPtr->GetSourcesData(), Commands);
	}
}

FReply SContentBrowser::ToggleLockClicked()
{
	bIsLocked = !bIsLocked;

	return FReply::Handled();
}

const FSlateBrush* SContentBrowser::GetToggleLockImage() const
{
	if ( bIsLocked )
	{
		return FEditorStyle::GetBrush("ContentBrowser.LockButton_Locked");
	}
	else
	{
		return FEditorStyle::GetBrush("ContentBrowser.LockButton_Unlocked");
	}
}

EVisibility SContentBrowser::GetSourcesViewVisibility() const
{
	return bSourcesViewExpanded ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SContentBrowser::GetSourcesToggleImage() const
{
	if ( bSourcesViewExpanded )
	{
		return FEditorStyle::GetBrush("ContentBrowser.HideSourcesView");
	}
	else
	{
		return FEditorStyle::GetBrush("ContentBrowser.ShowSourcesView");
	}
}

FReply SContentBrowser::SourcesViewExpandClicked()
{
	bSourcesViewExpanded = !bSourcesViewExpanded;

	// Notify 'Sources View Expanded' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnSourcesViewChanged& SourcesViewChangedDelegate = ContentBrowserModule.GetOnSourcesViewChanged();
	if(SourcesViewChangedDelegate.IsBound())
	{
		SourcesViewChangedDelegate.Broadcast(bSourcesViewExpanded);
	}

	return FReply::Handled();
}

EVisibility SContentBrowser::GetPathExpanderVisibility() const
{
	return bSourcesViewExpanded ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SContentBrowser::BackClicked()
{
	HistoryManager.GoBack();

	return FReply::Handled();
}

FReply SContentBrowser::ForwardClicked()
{
	HistoryManager.GoForward();

	return FReply::Handled();
}

bool SContentBrowser::HandleRenameCommandCanExecute() const
{
	const TArray<TSharedPtr<FAssetViewItem>>& SelectedItems = AssetViewPtr->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return AssetContextMenu->CanExecuteRename();
	}
	else
	{
		const TArray<FString>& SelectedPaths = PathViewPtr->GetSelectedPaths();
		if (SelectedPaths.Num() > 0)
		{
			return PathContextMenu->CanExecuteRename();
		}
	}
	return false;
}

bool SContentBrowser::HandleSaveAssetCommandCanExecute() const
{
	const TArray<TSharedPtr<FAssetViewItem>>& SelectedItems = AssetViewPtr->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return AssetContextMenu->CanExecuteSaveAsset();
	}

	return false;
}

void SContentBrowser::HandleSaveAllCurrentFolderCommand() const
{
	PathContextMenu->ExecuteSaveFolder();
}

void SContentBrowser::HandleResaveAllCurrentFolderCommand() const
{
	PathContextMenu->ExecuteResaveFolder();
}

void SContentBrowser::HandleRenameCommand()
{
	const TArray<TSharedPtr<FAssetViewItem>>& SelectedItems = AssetViewPtr->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		AssetContextMenu->ExecuteRename();
	}
	else
	{
		const TArray<FString>& SelectedPaths = PathViewPtr->GetSelectedPaths();
		if (SelectedPaths.Num() > 0)
		{
			PathContextMenu->ExecuteRename();
		}
	}
}

void SContentBrowser::HandleSaveAssetCommand()
{
	const TArray<TSharedPtr<FAssetViewItem>>& SelectedItems = AssetViewPtr->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		AssetContextMenu->ExecuteSaveAsset();
	}
}

bool SContentBrowser::HandleDeleteCommandCanExecute() const
{
	const TArray<TSharedPtr<FAssetViewItem>>& SelectedItems = AssetViewPtr->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return AssetContextMenu->CanExecuteDelete();
	}
	else
	{
		const TArray<FString>& SelectedPaths = PathViewPtr->GetSelectedPaths();
		if (SelectedPaths.Num() > 0)
		{
			return PathContextMenu->CanExecuteDelete();
		}
	}
	return false;
}

void SContentBrowser::HandleDeleteCommandExecute()
{
	const TArray<TSharedPtr<FAssetViewItem>>& SelectedItems = AssetViewPtr->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		AssetContextMenu->ExecuteDelete();
	}
	else
	{
		const TArray<FString>& SelectedPaths = PathViewPtr->GetSelectedPaths();
		if (SelectedPaths.Num() > 0)
		{
			PathContextMenu->ExecuteDelete();
		}
	}
}

void SContentBrowser::HandleOpenAssetsOrFoldersCommandExecute()
{
	AssetViewPtr->OnOpenAssetsOrFolders();
}

void SContentBrowser::HandlePreviewAssetsCommandExecute()
{
	AssetViewPtr->OnPreviewAssets();
}

void SContentBrowser::HandleCreateNewFolderCommandExecute()
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();

	// only create folders when a single path is selected
	const bool bCanCreateNewFolder = (SelectedPaths.Num() == 1) && ContentBrowserUtils::IsValidPathToCreateNewFolder(SelectedPaths[0]);

	if (bCanCreateNewFolder)
	{
		CreateNewFolder(
			SelectedPaths.Num() > 0
			? SelectedPaths[0]
			: FString(),
			FOnCreateNewFolder::CreateSP(AssetViewPtr.Get(), &SAssetView::OnCreateNewFolder));
	}
}

void SContentBrowser::HandleDirectoryUpCommandExecute()
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	if(SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsRootDir(SelectedPaths[0]))
	{
		FString ParentDir = SelectedPaths[0] / TEXT("..");
		FPaths::CollapseRelativeDirectories(ParentDir);
		FolderEntered(ParentDir);
	}
}

void SContentBrowser::HandleViewReferencesCommand()
{
	TArray<FName> ViewableAssets;

	TArray<FAssetData> SelectedAssets;
	TArray<FString> SelectedPaths;

	// Get the list of selected assets and paths from the view that actually has focus

	if (AssetViewPtr->HasAnyUserFocusOrFocusedDescendants())
	{
		SelectedAssets = AssetViewPtr->GetSelectedAssets();
		SelectedPaths = AssetViewPtr->GetSelectedFolders();
	}
	else if (PathViewPtr->HasAnyUserFocusOrFocusedDescendants())
	{
		SelectedPaths = PathViewPtr->GetSelectedPaths();
	}

	// For any selected assets, just get the package name from the asset data
	for (const FAssetData& Asset : SelectedAssets)
	{
		ViewableAssets.Add(Asset.PackageName);
	}

	// For any selected paths, get all assets that exist within that path
	if (SelectedPaths.Num() > 0)
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;

		for (const FString& Path : SelectedPaths)
		{
			new (Filter.PackagePaths) FName(*Path);
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		
		TArray<FAssetData> AssetsInPaths;
		AssetRegistryModule.Get().GetAssets(Filter, AssetsInPaths);

		for (const FAssetData& Asset : AssetsInPaths)
		{
			ViewableAssets.Add(Asset.PackageName);
		}
	}

	if (ViewableAssets.Num() > 0)
	{
		IReferenceViewerModule::Get().InvokeReferenceViewerTab(ViewableAssets);
	}
}

bool SContentBrowser::HandleViewReferencesCanExecute()
{
	bool bCanViewReferences = IReferenceViewerModule::IsAvailable();

	if (bCanViewReferences)
	{
		// The reference viewer should be called if either the asset view or the path view have focus and have
		// at least one item selected

		if (AssetViewPtr.IsValid() && AssetViewPtr->HasAnyUserFocusOrFocusedDescendants())
		{
			bCanViewReferences = AssetViewPtr->GetSelectedItems().Num() > 0;
		}
		else if (PathViewPtr.IsValid() && PathViewPtr->HasAnyUserFocusOrFocusedDescendants())
		{
			bCanViewReferences = PathViewPtr->GetSelectedPaths().Num() > 0;
		}
	}

	return bCanViewReferences;
}

bool SContentBrowser::IsBackEnabled() const
{
	return HistoryManager.CanGoBack();
}

bool SContentBrowser::IsForwardEnabled() const
{
	return HistoryManager.CanGoForward();
}

bool SContentBrowser::CanExecuteDirectoryUp() const
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	return (SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsRootDir(SelectedPaths[0]));
}

FText SContentBrowser::GetHistoryBackTooltip() const
{
	if ( HistoryManager.CanGoBack() )
	{
		return FText::Format( LOCTEXT("HistoryBackTooltipFmt", "Back to {0}"), HistoryManager.GetBackDesc() );
	}
	return FText::GetEmpty();
}

FText SContentBrowser::GetHistoryForwardTooltip() const
{
	if ( HistoryManager.CanGoForward() )
	{
		return FText::Format( LOCTEXT("HistoryForwardTooltipFmt", "Forward to {0}"), HistoryManager.GetForwardDesc() );
	}
	return FText::GetEmpty();
}

FText SContentBrowser::GetDirectoryUpTooltip() const
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	if(SelectedPaths.Num() == 1 && !ContentBrowserUtils::IsRootDir(SelectedPaths[0]))
	{
		FString ParentDir = SelectedPaths[0] / TEXT("..");
		FPaths::CollapseRelativeDirectories(ParentDir);
		return FText::Format(LOCTEXT("DirectoryUpTooltip", "Up to {0}"), FText::FromString(ParentDir) );
	}

	return FText();
}

EVisibility SContentBrowser::GetDirectoryUpToolTipVisibility() const
{
	EVisibility ToolTipVisibility = EVisibility::Collapsed;

	// if we have text in our tooltip, make it visible. 
	if(GetDirectoryUpTooltip().IsEmpty() == false)
	{
		ToolTipVisibility = EVisibility::Visible;
	}

	return ToolTipVisibility;
}

void SContentBrowser::SyncGlobalSelectionSet()
{
	USelection* EditorSelection = GEditor->GetSelectedObjects();
	if ( !ensure( EditorSelection != NULL ) )
	{
		return;
	}

	// Get the selected assets in the asset view
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	EditorSelection->BeginBatchSelectOperation();
	{
		TSet< UObject* > SelectedObjects;
		// Lets see what the user has selected and add any new selected objects to the global selection set
		for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			// Grab the object if it is loaded
			if ( (*AssetIt).IsAssetLoaded() )
			{
				UObject* FoundObject = (*AssetIt).GetAsset();
				if( FoundObject != NULL && FoundObject->GetClass() != UObjectRedirector::StaticClass() )
				{
					SelectedObjects.Add( FoundObject );

					// Select this object!
					EditorSelection->Select( FoundObject );
				}
			}
		}


		// Now we'll build a list of objects that need to be removed from the global selection set
		for( int32 CurEditorObjectIndex = 0; CurEditorObjectIndex < EditorSelection->Num(); ++CurEditorObjectIndex )
		{
			UObject* CurEditorObject = EditorSelection->GetSelectedObject( CurEditorObjectIndex );
			if( CurEditorObject != NULL ) 
			{
				if( !SelectedObjects.Contains( CurEditorObject ) )
				{
					EditorSelection->Deselect( CurEditorObject );
				}
			}
		}
	}
	EditorSelection->EndBatchSelectOperation();
}

void SContentBrowser::UpdatePath()
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	PathBreadcrumbTrail->ClearCrumbs();

	if ( SourcesData.HasPackagePaths() )
	{
		TArray<FString> Crumbs;
		SourcesData.PackagePaths[0].ToString().ParseIntoArray(Crumbs, TEXT("/"), true);

		FString CrumbPath = TEXT("/");
		for ( auto CrumbIt = Crumbs.CreateConstIterator(); CrumbIt; ++CrumbIt )
		{
			// If this is the root part of the path, try and get the localized display name to stay in sync with what we see in SPathView
			const FText DisplayName = (CrumbIt.GetIndex() == 0) ? ContentBrowserUtils::GetRootDirDisplayName(*CrumbIt) : FText::FromString(*CrumbIt);

			CrumbPath += *CrumbIt;
			PathBreadcrumbTrail->PushCrumb(DisplayName, CrumbPath);
			CrumbPath += TEXT("/");
		}
	}
	else if ( SourcesData.HasCollections() )
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		TArray<FCollectionNameType> CollectionPathItems;

		// Walk up the parents of this collection so that we can generate a complete path (this loop also adds the child collection to the array)
		for (TOptional<FCollectionNameType> CurrentCollection = SourcesData.Collections[0]; 
			CurrentCollection.IsSet(); 
			CurrentCollection = CollectionManagerModule.Get().GetParentCollection(CurrentCollection->Name, CurrentCollection->Type)
			)
		{
			CollectionPathItems.Insert(CurrentCollection.GetValue(), 0);
		}

		// Now add each part of the path to the breadcrumb trail
		for (const FCollectionNameType& CollectionPathItem : CollectionPathItems)
		{
			const FString CrumbData = FString::Printf(TEXT("%s?%s"), *CollectionPathItem.Name.ToString(), *FString::FromInt(CollectionPathItem.Type));

			FFormatNamedArguments Args;
			Args.Add(TEXT("CollectionName"), FText::FromName(CollectionPathItem.Name));
			const FText DisplayName = FText::Format(LOCTEXT("CollectionPathIndicator", "{CollectionName} (Collection)"), Args);

			PathBreadcrumbTrail->PushCrumb(DisplayName, CrumbData);
		}
	}
	else
	{
		PathBreadcrumbTrail->PushCrumb(LOCTEXT("AllAssets", "All Assets"), TEXT(""));
	}
}

void SContentBrowser::OnFilterChanged()
{
	FARFilter Filter = FilterListPtr->GetCombinedBackendFilter();
	AssetViewPtr->SetBackendFilter( Filter );

	// Notify 'filter changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	ContentBrowserModule.GetOnFilterChanged().Broadcast(Filter, bIsPrimaryBrowser);
}

FString SContentBrowser::GetPathText() const
{
	FString PathLabelText;

	if ( IsFilteredBySource() )
	{
		const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

		// At least one source is selected
		const int32 NumSources = SourcesData.PackagePaths.Num() + SourcesData.Collections.Num();

		if (NumSources > 0)
		{
			PathLabelText = SourcesData.HasPackagePaths() ? SourcesData.PackagePaths[0].ToString() : SourcesData.Collections[0].Name.ToString();

			if (NumSources > 1)
			{
				PathLabelText += FString::Printf(*LOCTEXT("MultipleSourcesSuffix", " and %d others...").ToString(), NumSources - 1);
			}
		}
	}
	else
	{
		PathLabelText = LOCTEXT("AllAssets", "All Assets").ToString();
	}

	return PathLabelText;
}

bool SContentBrowser::IsFilteredBySource() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	return !SourcesData.IsEmpty();
}

void SContentBrowser::OnAssetRenameCommitted(const TArray<FAssetData>& Assets)
{
	// After a rename is committed we allow an implicit sync so as not to
	// disorientate the user if they are looking at a parent folder

	const bool bAllowImplicitSync = true;
	const bool bDisableFiltersThatHideAssets = false;
	SyncToAssets(Assets, bAllowImplicitSync, bDisableFiltersThatHideAssets);
}

void SContentBrowser::OnFindInAssetTreeRequested(const TArray<FAssetData>& AssetsToFind)
{
	SyncToAssets(AssetsToFind);
}

void SContentBrowser::OnRenameRequested(const FAssetData& AssetData)
{
	AssetViewPtr->RenameAsset(AssetData);
}

void SContentBrowser::OnRenameFolderRequested(const FString& FolderToRename)
{
	const TArray<FString>& SelectedFolders = AssetViewPtr->GetSelectedFolders();
	if (SelectedFolders.Num() > 0)
	{
		AssetViewPtr->RenameFolder(FolderToRename);
	}
	else
	{
		const TArray<FString>& SelectedPaths = PathViewPtr->GetSelectedPaths();
		if (SelectedPaths.Num() > 0)
		{
			PathViewPtr->RenameFolder(FolderToRename);
		}
	}
}

void SContentBrowser::OnOpenedFolderDeleted()
{
	// Since the contents of the asset view have just been deleted, set the selected path to the default "/Game"
	TArray<FString> DefaultSelectedPaths;
	DefaultSelectedPaths.Add(TEXT("/Game"));
	PathViewPtr->SetSelectedPaths(DefaultSelectedPaths);

	FSourcesData DefaultSourcesData(FName("/Game"));
	AssetViewPtr->SetSourcesData(DefaultSourcesData);

	UpdatePath();
}

void SContentBrowser::OnDuplicateRequested(const TWeakObjectPtr<UObject>& OriginalObject)
{
	UObject* Object = OriginalObject.Get();

	if ( Object )
	{
		AssetViewPtr->DuplicateAsset(FPackageName::GetLongPackagePath(Object->GetOutermost()->GetName()), OriginalObject);
	}
}

void SContentBrowser::OnAssetViewRefreshRequested()
{
	AssetViewPtr->RequestSlowFullListRefresh();
}

void SContentBrowser::HandleCollectionRemoved(const FCollectionNameType& Collection)
{
	AssetViewPtr->SetSourcesData(FSourcesData());

	auto RemoveHistoryDelegate = [&](const FHistoryData& HistoryData)
	{
		return (HistoryData.SourcesData.Collections.Num() == 1 &&
				HistoryData.SourcesData.PackagePaths.Num() == 0 &&
				HistoryData.SourcesData.Collections.Contains(Collection));
	};

	HistoryManager.RemoveHistoryData(RemoveHistoryDelegate);
}

void SContentBrowser::HandleCollectionRenamed(const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection)
{
	return HandleCollectionRemoved(OriginalCollection);
}

void SContentBrowser::HandleCollectionUpdated(const FCollectionNameType& Collection)
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	// If we're currently viewing the dynamic collection that was updated, make sure our active filter text is up-to-date
	if (SourcesData.IsDynamicCollection() && SourcesData.Collections[0] == Collection)
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		const FCollectionNameType& DynamicCollection = SourcesData.Collections[0];

		FString DynamicQueryString;
		CollectionManagerModule.Get().GetDynamicQueryText(DynamicCollection.Name, DynamicCollection.Type, DynamicQueryString);

		const FText DynamicQueryText = FText::FromString(DynamicQueryString);
		SetSearchBoxText(DynamicQueryText);
		SearchBoxPtr->SetText(DynamicQueryText);
	}
}

void SContentBrowser::HandlePathRemoved(const FString& Path)
{
	const FName PathName(*Path);

	auto RemoveHistoryDelegate = [&](const FHistoryData& HistoryData)
	{
		return (HistoryData.SourcesData.PackagePaths.Num() == 1 &&
				HistoryData.SourcesData.Collections.Num() == 0 &&
				HistoryData.SourcesData.PackagePaths.Contains(PathName));
	};

	HistoryManager.RemoveHistoryData(RemoveHistoryDelegate);
}

FText SContentBrowser::GetSearchAssetsHintText() const
{
	if (PathViewPtr.IsValid())
	{
		TArray<FString> Paths = PathViewPtr->GetSelectedPaths();
		if (Paths.Num() != 0)
		{
			FString SearchHint = NSLOCTEXT( "ContentBrowser", "SearchBoxPartialHint", "Search" ).ToString();
			SearchHint += TEXT(" ");
			for(int32 i = 0; i < Paths.Num(); i++)
			{
				const FString& Path = Paths[i];
				if (ContentBrowserUtils::IsRootDir(Path))
				{
					SearchHint += ContentBrowserUtils::GetRootDirDisplayName(Path).ToString();
				}
				else
				{
					SearchHint += FPaths::GetCleanFilename(Path);
				}

				if (i + 1 < Paths.Num())
				{
					SearchHint += ", ";
				}
			}

			return FText::FromString(SearchHint);
		}
	}
	
	return NSLOCTEXT( "ContentBrowser", "SearchBoxHint", "Search Assets" );
}

TArray<FString> SContentBrowser::GetAssetSearchSuggestions() const
{
	TArray<FString> AllSuggestions;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	TArray< TWeakPtr<IAssetTypeActions> > AssetTypeActionsList;
	AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);

	for ( auto TypeActionsIt = AssetTypeActionsList.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt )
	{
		if ( (*TypeActionsIt).IsValid() )
		{
			const TSharedPtr<IAssetTypeActions> TypeActions = (*TypeActionsIt).Pin();
			AllSuggestions.Add( TypeActions->GetSupportedClass()->GetName() );
		}
	}

	return AllSuggestions;
}

TSharedPtr<SWidget> SContentBrowser::GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder, bool bPathView)
{
	// Clear any selection in the asset view, as it'll conflict with other view info
	// This is important for determining which context menu may be open based on the asset selection for rename/delete operations
	if (bPathView)
	{
		AssetViewPtr->ClearSelection();
	}
	
	// Ensure the path context menu has the up-to-date list of paths being worked on
	PathContextMenu->SetSelectedPaths(SelectedPaths);

	TSharedPtr<FExtender> Extender;
	if(InMenuExtender.IsBound())
	{
		Extender = InMenuExtender.Execute(SelectedPaths);
	}

	const bool bInShouldCloseWindowAfterSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterSelection, Commands, Extender, true);
	
	// We can only create folders when we have a single path selected
	const bool bCanCreateNewFolder = SelectedPaths.Num() == 1 && ContentBrowserUtils::IsValidPathToCreateNewFolder(SelectedPaths[0]);

	FText NewFolderToolTip;
	if(SelectedPaths.Num() == 1)
	{
		if(bCanCreateNewFolder)
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_CreateIn", "Create a new folder in {0}."), FText::FromString(SelectedPaths[0]));
		}
		else
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_InvalidPath", "Cannot create new folders in {0}."), FText::FromString(SelectedPaths[0]));
		}
	}
	else
	{
		NewFolderToolTip = LOCTEXT("NewFolderTooltip_InvalidNumberOfPaths", "Can only create folders when there is a single path selected.");
	}

	// New Folder
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewFolder", "New Folder"),
		NewFolderToolTip,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"),
		FUIAction(
			FExecuteAction::CreateSP( this, &SContentBrowser::CreateNewFolder, SelectedPaths.Num() > 0 ? SelectedPaths[0] : FString(), InOnCreateNewFolder ),
			FCanExecuteAction::CreateLambda( [bCanCreateNewFolder] { return bCanCreateNewFolder; } )
			),
		"NewFolder"
		);

	return MenuBuilder.MakeWidget();
}

void SContentBrowser::CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder)
{
	// Create a valid base name for this folder
	FText DefaultFolderBaseName = LOCTEXT("DefaultFolderName", "NewFolder");
	FText DefaultFolderName = DefaultFolderBaseName;
	int32 NewFolderPostfix = 1;
	while(ContentBrowserUtils::DoesFolderExist(FolderPath / DefaultFolderName.ToString()))
	{
		DefaultFolderName = FText::Format(LOCTEXT("DefaultFolderNamePattern", "{0}{1}"), DefaultFolderBaseName, FText::AsNumber(NewFolderPostfix));
		NewFolderPostfix++;
	}

	InOnCreateNewFolder.ExecuteIfBound(DefaultFolderName.ToString(), FolderPath);
}

#undef LOCTEXT_NAMESPACE
