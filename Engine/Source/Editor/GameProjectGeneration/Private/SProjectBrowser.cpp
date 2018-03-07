// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SProjectBrowser.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FeedbackContext.h"
#include "UObject/UnrealType.h"
#include "Misc/EngineVersion.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "SlateOptMacros.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Framework/Layout/Overscroll.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "EditorDirectories.h"
#include "ProjectDescriptor.h"
#include "Interfaces/IProjectManager.h"
#include "GameProjectUtils.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "SVerbChoiceDialog.h"
#include "Misc/UProjectInfo.h"
#include "SourceCodeNavigation.h"
#include "PlatformInfo.h"
#include "Widgets/Input/SSearchBox.h"
#include "Settings/EditorSettings.h"
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Framework/Application/SlateApplication.h"
#include "ILauncherPlatform.h"
#include "LauncherPlatformModule.h"
#include "IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "ProjectBrowser"


/**
 * Structure for project items.
 */
struct FProjectItem
{
	FText Name;
	FText Description;
	FString EngineIdentifier;
	bool bUpToDate;
	FString ProjectFile;
	TSharedPtr<FSlateBrush> ProjectThumbnail;
	bool bIsNewProjectItem;
	TArray<FName> TargetPlatforms;
	bool bSupportsAllPlatforms;

	FProjectItem(const FText& InName, const FText& InDescription, const FString& InEngineIdentifier, bool InUpToDate, const TSharedPtr<FSlateBrush>& InProjectThumbnail, const FString& InProjectFile, bool InIsNewProjectItem, TArray<FName> InTargetPlatforms, bool InSupportsAllPlatforms)
		: Name(InName)
		, Description(InDescription)
		, EngineIdentifier(InEngineIdentifier)
		, bUpToDate(InUpToDate)
		, ProjectFile(InProjectFile)
		, ProjectThumbnail(InProjectThumbnail)
		, bIsNewProjectItem(InIsNewProjectItem)
		, TargetPlatforms(InTargetPlatforms)
		, bSupportsAllPlatforms(InSupportsAllPlatforms)
	{ }

	/** Check if this project is up to date */
	bool IsUpToDate() const
	{
		return bUpToDate;
	}

	/** Gets the engine label for this project */
	FString GetEngineLabel() const
	{
		if(bUpToDate)
		{
			return FString();
		}
		else if(FDesktopPlatformModule::Get()->IsStockEngineRelease(EngineIdentifier))
		{
			return EngineIdentifier;
		}
		else
		{
			return FString(TEXT("?"));
		}
	}
};


/**
 * Structure for project categories.
 */
struct FProjectCategory
{
	FText CategoryName;
	TSharedPtr< STileView< TSharedPtr<FProjectItem> > > ProjectTileView;
	TArray< TSharedPtr<FProjectItem> > ProjectItemsSource;
	TArray< TSharedPtr<FProjectItem> > FilteredProjectItemsSource;
};


void ProjectItemToString(const TSharedPtr<FProjectItem> InItem, TArray<FString>& OutFilterStrings)
{
	OutFilterStrings.Add(InItem->Name.ToString());
}


/* SCompoundWidget interface
 *****************************************************************************/

SProjectBrowser::SProjectBrowser()
	: ProjectItemFilter( ProjectItemTextFilter::FItemToStringArray::CreateStatic( ProjectItemToString ))
	, NumFilteredProjects(0)
{

}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectBrowser::Construct( const FArguments& InArgs )
{
	bPreventSelectionChangeEvent = false;
	ThumbnailBorderPadding = 4;
	ThumbnailSize = 128.0f;

	// Prepare the categories box
	CategoriesBox = SNew(SVerticalBox);

	// Find all projects
	FindProjects();

	CategoriesBox->AddSlot()
	.HAlign(HAlign_Center)
	.Padding(FMargin(0.f, 25.f))
	[
		SNew(STextBlock)
		.Visibility(this, &SProjectBrowser::GetNoProjectsErrorVisibility)
		.Text(LOCTEXT("NoProjects", "You don't have any projects yet :("))
	];

	CategoriesBox->AddSlot()
	.HAlign(HAlign_Center)
	.Padding(FMargin(0.f, 25.f))
	[
		SNew(STextBlock)
		.Visibility(this, &SProjectBrowser::GetNoProjectsAfterFilterErrorVisibility)
		.Text(LOCTEXT("NoProjectsAfterFilter", "There are no projects that match the specified filter"))
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(SVerticalBox)

			// Categories
			+ SVerticalBox::Slot()
			.Padding(8.f)
			.FillHeight(1.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.Padding(FMargin(0, 0, 5.f, 0))
					.VAlign(VAlign_Center)
					[
						SNew(SOverlay)

						+SOverlay::Slot()
						[
							SAssignNew(SearchBoxPtr, SSearchBox)
							.HintText(LOCTEXT("FilterHint", "Filter Projects..."))
							.OnTextChanged(this, &SProjectBrowser::OnFilterTextChanged)
						]

						+SOverlay::Slot()
						[
							SNew(SBorder)
							.Visibility(this, &SProjectBrowser::GetFilterActiveOverlayVisibility)
							.BorderImage(FEditorStyle::Get().GetBrush("SearchBox.ActiveBorder"))
						]
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(0, 0, 5.f, 0))
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SProjectBrowser::FindProjects)
						.ForegroundColor(FSlateColor::UseForeground())
						.ToolTipText(LOCTEXT("RefreshProjectList", "Refresh the project list"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.Padding(2.0f)
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("Icons.Refresh")) 
							]

							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(2.0f)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ProjectBrowser.Toolbar.Text")
								.Text(LOCTEXT("RefreshProjectsText", "Refresh"))
							]
						]
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Visibility( FLauncherPlatformModule::Get()->CanOpenLauncher(true) ? EVisibility::Visible : EVisibility::Collapsed )
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SProjectBrowser::HandleMarketplaceTabButtonClicked)
						.ForegroundColor(FSlateColor::UseForeground())
						.ToolTipText(LOCTEXT("MarketplaceToolTip", "Check out the Marketplace to find new projects!"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.Padding(2.0f)
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LevelEditor.OpenMarketplace.Small")) 
							]

							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(2.0f)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ProjectBrowser.Toolbar.Text")
								.Text(LOCTEXT("Marketplace", "Marketplace"))
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				.Padding(FMargin(0, 5.f))
				[
					SNew(SScrollBox)

					+ SScrollBox::Slot()
					[
						CategoriesBox.ToSharedRef()
					]
				]
			]

			+ SVerticalBox::Slot()
			.Padding( 0, 40, 0, 0 )	// Lots of vertical padding before the dialog buttons at the bottom
			.AutoHeight()
			[
				SNew( SHorizontalBox )

				// Auto-load project
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)			
					.IsChecked(GetDefault<UEditorSettings>()->bLoadTheMostRecentlyLoadedProjectAtStartup ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SProjectBrowser::OnAutoloadLastProjectChanged)
					.Content()
					[
						SNew(STextBlock).Text(LOCTEXT("AutoloadOnStartupCheckbox", "Always load last project on startup"))
					]
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]
				// Browse Button
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.f, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseProjectButton", "Browse..."))
					.OnClicked(this, &SProjectBrowser::OnBrowseToProjectClicked)
					.ContentPadding( FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding") )
				]

				// Open Button
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenProjectButton", "Open"))
					.OnClicked(this, &SProjectBrowser::HandleOpenProjectButtonClicked)
					.IsEnabled(this, &SProjectBrowser::HandleOpenProjectButtonIsEnabled)
					.ContentPadding( FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding") )
				]
			]
		]
	];

	// Select the first item in the first category
	if (ProjectCategories.Num() > 0)
	{
		const TSharedRef<FProjectCategory> Category = ProjectCategories[0];
		if ( ensure(Category->ProjectItemsSource.Num() > 0) && ensure(Category->ProjectTileView.IsValid()))
		{
			Category->ProjectTileView->SetSelection(Category->ProjectItemsSource[0], ESelectInfo::Direct);
		}
	}

	bHasProjectFiles = false;
	for (auto CategoryIt = ProjectCategories.CreateConstIterator(); CategoryIt; ++CategoryIt)
	{
		const TSharedRef<FProjectCategory>& Category = *CategoryIt;

		if (Category->ProjectItemsSource.Num() > 0)
		{
			bHasProjectFiles = true;
			break;
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProjectBrowser::ConstructCategory( const TSharedRef<SVerticalBox>& InCategoriesBox, const TSharedRef<FProjectCategory>& Category ) const
{
	// Title
	InCategoriesBox->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Visibility(this, &SProjectBrowser::GetProjectCategoryVisibility, Category)
		.TextStyle(FEditorStyle::Get(), "GameProjectDialog.ProjectNamePathLabels")
		.Text(Category->CategoryName)
	];

	// Separator
	InCategoriesBox->AddSlot()
	.AutoHeight()
	.Padding(0, 2.0f, 0, 8.0f)
	[
		SNew(SSeparator)
		.Visibility(this, &SProjectBrowser::GetProjectCategoryVisibility, Category)
	];

	// Project tile view
	InCategoriesBox->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.0f, 0.f, 40.0f)
	[
		SAssignNew(Category->ProjectTileView, STileView<TSharedPtr<FProjectItem> >)
		.Visibility(this, &SProjectBrowser::GetProjectCategoryVisibility, Category)
		.ListItemsSource(&Category->FilteredProjectItemsSource)
		.SelectionMode(ESelectionMode::Single)
		.ClearSelectionOnClick(false)
		.AllowOverscroll(EAllowOverscroll::No)
		.OnGenerateTile(this, &SProjectBrowser::MakeProjectViewWidget)
		.OnContextMenuOpening(this, &SProjectBrowser::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SProjectBrowser::HandleProjectItemDoubleClick)
		.OnSelectionChanged(this, &SProjectBrowser::HandleProjectViewSelectionChanged, Category->CategoryName)
		.ItemHeight(ThumbnailSize + ThumbnailBorderPadding + 32)
		.ItemWidth(ThumbnailSize + ThumbnailBorderPadding)
	];
}

bool SProjectBrowser::HasProjects() const
{
	return bHasProjectFiles;
}


/* SProjectBrowser implementation
 *****************************************************************************/

TSharedRef<ITableRow> SProjectBrowser::MakeProjectViewWidget(TSharedPtr<FProjectItem> ProjectItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(ProjectItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FProjectItem>>, OwnerTable );
	}

	TSharedPtr<SWidget> Thumbnail;
	if ( ProjectItem->bIsNewProjectItem )
	{
		Thumbnail = SNew(SBox)
			.Padding(ThumbnailBorderPadding)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FEditorStyle::GetBrush("MarqueeSelection"))
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle( FEditorStyle::Get(), "GameProjectDialog.NewProjectTitle" )
						.Text( LOCTEXT("NewProjectThumbnailText", "NEW") )
					]
				]
			];
	}
	else
	{
		const FLinearColor Tint = ProjectItem->IsUpToDate() ? FLinearColor::White : FLinearColor::White.CopyWithNewOpacity(0.5);

		// Drop shadow border
		Thumbnail =	SNew(SBorder)
			.Padding(ThumbnailBorderPadding)
			.BorderImage( FEditorStyle::GetBrush("ContentBrowser.ThumbnailShadow") )
			.ColorAndOpacity(Tint)
			.BorderBackgroundColor(Tint)
			[
				SNew(SImage).Image(this, &SProjectBrowser::GetProjectItemImage, TWeakPtr<FProjectItem>(ProjectItem))
			];
	}

	TSharedRef<ITableRow> TableRow = SNew( STableRow<TSharedPtr<FProjectItem>>, OwnerTable )
	.Style(FEditorStyle::Get(), "GameProjectDialog.TemplateListView.TableRow")
	[
		SNew(SBox)
		.HeightOverride(ThumbnailSize+ThumbnailBorderPadding+5)
		[
			SNew(SVerticalBox)

			// Thumbnail
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.WidthOverride(ThumbnailSize + ThumbnailBorderPadding * 2)
				.HeightOverride(ThumbnailSize + ThumbnailBorderPadding * 2)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						Thumbnail.ToSharedRef()
					]

					// Show the out of date engine version for this project file
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(10)
					[
						SNew(STextBlock)
						.Text(FText::FromString(ProjectItem->GetEngineLabel()))
						.TextStyle(FEditorStyle::Get(), "ProjectBrowser.VersionOverlayText")
						.ColorAndOpacity(FLinearColor::White.CopyWithNewOpacity(0.5f))
						.Visibility(ProjectItem->IsUpToDate() ? EVisibility::Collapsed : EVisibility::Visible)
					]
				]
			]

			// Name
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.HighlightText(this, &SProjectBrowser::GetItemHighlightText)
				.Text(ProjectItem->Name)
			]
		]
	];

	TableRow->AsWidget()->SetToolTip(MakeProjectToolTip(ProjectItem));

	return TableRow;
}


TSharedRef<SToolTip> SProjectBrowser::MakeProjectToolTip( TSharedPtr<FProjectItem> ProjectItem ) const
{
	// Create a box to hold every line of info in the body of the tooltip
	TSharedRef<SVerticalBox> InfoBox = SNew(SVerticalBox);

	if(!ProjectItem->Description.IsEmpty())
	{
		AddToToolTipInfoBox( InfoBox, LOCTEXT("ProjectTileTooltipDescription", "Description"), ProjectItem->Description );
	}

	{
		const FString ProjectPath = FPaths::GetPath(ProjectItem->ProjectFile);
		AddToToolTipInfoBox( InfoBox, LOCTEXT("ProjectTileTooltipPath", "Path"), FText::FromString(ProjectPath) );
	}

	if(!ProjectItem->IsUpToDate())
	{
		FText Description;
		if(FDesktopPlatformModule::Get()->IsStockEngineRelease(ProjectItem->EngineIdentifier))
		{
			Description = FText::FromString(ProjectItem->EngineIdentifier);
		}
		else
		{
			FString RootDir;
			if(FDesktopPlatformModule::Get()->GetEngineRootDirFromIdentifier(ProjectItem->EngineIdentifier, RootDir))
			{
				FString PlatformRootDir = RootDir;
				FPaths::MakePlatformFilename(PlatformRootDir);
				Description = FText::FromString(PlatformRootDir);
			}
			else
			{
				Description = LOCTEXT("UnknownEngineVersion", "Unknown engine version");
			}
		}
		AddToToolTipInfoBox(InfoBox, LOCTEXT("EngineVersion", "Engine"), Description);
	}

	// Create the target platform icons
	TSharedRef<SHorizontalBox> TargetPlatformIconsBox = SNew(SHorizontalBox);
	for(const FName& PlatformName : ProjectItem->TargetPlatforms)
	{
		const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(PlatformName);
		check(PlatformInfo);

		TargetPlatformIconsBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0, 0, 1, 0))
		[
			SNew(SBox)
			.WidthOverride(20)
			.HeightOverride(20)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(PlatformInfo->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)))
			]
		];
	}

	TSharedRef<SToolTip> Tooltip = SNew(SToolTip)
	.TextMargin(1)
	.BorderImage( FEditorStyle::GetBrush("ProjectBrowser.TileViewTooltip.ToolTipBorder") )
	[
		SNew(SBorder)
		.Padding(6)
		.BorderImage( FEditorStyle::GetBrush("ProjectBrowser.TileViewTooltip.NonContentBorder") )
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SBorder)
				.Padding(6)
				.BorderImage( FEditorStyle::GetBrush("ProjectBrowser.TileViewTooltip.ContentBorder") )
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( ProjectItem->Name )
						.Font( FEditorStyle::GetFontStyle("ProjectBrowser.TileViewTooltip.NameFont") )
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					.Padding(0, 2, 0, 0)
					[
						TargetPlatformIconsBox
					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(6)
				.BorderImage( FEditorStyle::GetBrush("ProjectBrowser.TileViewTooltip.ContentBorder") )
				[
					InfoBox
				]
			]
		]
	];

	return Tooltip;
}


void SProjectBrowser::AddToToolTipInfoBox(const TSharedRef<SVerticalBox>& InfoBox, const FText& Key, const FText& Value) const
{
	InfoBox->AddSlot()
	.AutoHeight()
	.Padding(0, 1)
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(STextBlock) .Text( FText::Format(LOCTEXT("ProjectBrowserTooltipFormat", "{0}:"), Key ) )
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock) .Text( Value )
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
}


TSharedPtr<SWidget> SProjectBrowser::OnGetContextMenuContent() const
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	TSharedPtr<FProjectItem> SelectedProjectItem = GetSelectedProjectItem();
	const FText ProjectContextActionsText = (SelectedProjectItem.IsValid()) ? SelectedProjectItem->Name : LOCTEXT("ProjectActionsMenuHeading", "Project Actions");
	MenuBuilder.BeginSection("ProjectContextActions", ProjectContextActionsText);

	FFormatNamedArguments Args;
	Args.Add(TEXT("FileManagerName"), FPlatformMisc::GetFileManagerName());
	const FText ExploreToText = FText::Format(NSLOCTEXT("GenericPlatform", "ShowInFileManager", "Show in {FileManagerName}"), Args);

	MenuBuilder.AddMenuEntry(
		ExploreToText,
		LOCTEXT("FindInExplorerTooltip", "Finds this project on disk"),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP( this, &SProjectBrowser::ExecuteFindInExplorer ),
		FCanExecuteAction::CreateSP( this, &SProjectBrowser::CanExecuteFindInExplorer )
		)
		);

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void SProjectBrowser::ExecuteFindInExplorer() const
{
	TSharedPtr<FProjectItem> SelectedProjectItem = GetSelectedProjectItem();
	check(SelectedProjectItem.IsValid());
	FPlatformProcess::ExploreFolder(*SelectedProjectItem->ProjectFile);
}


bool SProjectBrowser::CanExecuteFindInExplorer() const
{
	TSharedPtr<FProjectItem> SelectedProjectItem = GetSelectedProjectItem();
	return SelectedProjectItem.IsValid();
}


const FSlateBrush* SProjectBrowser::GetProjectItemImage(TWeakPtr<FProjectItem> ProjectItem) const
{
	if ( ProjectItem.IsValid() )
	{
		TSharedPtr<FProjectItem> Item = ProjectItem.Pin();
		if ( Item->ProjectThumbnail.IsValid() )
		{
			return Item->ProjectThumbnail.Get();
		}
	}
	
	return FEditorStyle::GetBrush("GameProjectDialog.DefaultGameThumbnail");
}


TSharedPtr<FProjectItem> SProjectBrowser::GetSelectedProjectItem() const
{
	for ( auto CategoryIt = ProjectCategories.CreateConstIterator(); CategoryIt; ++CategoryIt )
	{
		const TSharedRef<FProjectCategory>& Category = *CategoryIt;
		TArray< TSharedPtr<FProjectItem> > SelectedItems = Category->ProjectTileView->GetSelectedItems();
		if ( SelectedItems.Num() > 0 )
		{
			return SelectedItems[0];
		}
	}
	
	return NULL;
}


FText SProjectBrowser::GetSelectedProjectName() const
{
	TSharedPtr<FProjectItem> SelectedItem = GetSelectedProjectItem();
	if ( SelectedItem.IsValid() )
	{
		return SelectedItem->Name;
	}

	return FText::GetEmpty();
}

FReply SProjectBrowser::FindProjects()
{
	enum class EProjectCategoryType : uint8
	{
		Sample,
		UserDefined,
	};

	ProjectCategories.Empty();
	CategoriesBox->ClearChildren();

	// Create a map of parent project folders to their category
	TMap<FString, EProjectCategoryType> ProjectFilesToCategoryType;

	// Find all the engine installations
	TMap<FString, FString> EngineInstallations;
	FDesktopPlatformModule::Get()->EnumerateEngineInstallations(EngineInstallations);

	// Add projects from every branch that we know about
	FString CurrentEngineIdentifier = FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier();
	for(TMap<FString, FString>::TConstIterator Iter(EngineInstallations); Iter; ++Iter)
	{
		TArray<FString> ProjectFiles;
		if(FDesktopPlatformModule::Get()->EnumerateProjectsKnownByEngine(Iter.Key(), false, ProjectFiles))
		{
			for(int Idx = 0; Idx < ProjectFiles.Num(); Idx++)
			{
				ProjectFilesToCategoryType.Add(ProjectFiles[Idx], EProjectCategoryType::UserDefined);
			}
		}
	}

	// Add all the samples from the launcher
	TArray<FString> LauncherSampleProjects;
	FDesktopPlatformModule::Get()->EnumerateLauncherSampleProjects(LauncherSampleProjects);
	for(int32 Idx = 0; Idx < LauncherSampleProjects.Num(); Idx++)
	{
		ProjectFilesToCategoryType.Add(LauncherSampleProjects[Idx], EProjectCategoryType::Sample);
	}

	// Add all the native project files we can find, and automatically filter them depending on their directory
	FUProjectDictionary& DefaultProjectDictionary = FUProjectDictionary::GetDefault();
	DefaultProjectDictionary.Refresh();
	const TArray<FString> &NativeProjectFiles = DefaultProjectDictionary.GetProjectPaths();
	for(int32 Idx = 0; Idx < NativeProjectFiles.Num(); Idx++)
	{
		if(!NativeProjectFiles[Idx].Contains(TEXT("/Templates/")))
		{
			const EProjectCategoryType ProjectCategoryType = NativeProjectFiles[Idx].Contains(TEXT("/Samples/")) ? EProjectCategoryType::Sample : EProjectCategoryType::UserDefined;
			ProjectFilesToCategoryType.Add(NativeProjectFiles[Idx], ProjectCategoryType);
		}
	}

	// Normalize all the filenames and make sure there are no duplicates
	TMap<FString, EProjectCategoryType> AbsoluteProjectFilesToCategory;
	for(TMap<FString, EProjectCategoryType>::TConstIterator Iter(ProjectFilesToCategoryType); Iter; ++Iter)
	{
		FString AbsoluteFile = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Iter.Key());
		AbsoluteProjectFilesToCategory.Add(AbsoluteFile, Iter.Value());
	}

	const FText MyProjectsCategoryName = LOCTEXT("MyProjectsCategoryName", "My Projects");
	const FText SamplesCategoryName = LOCTEXT("SamplesCategoryName", "Samples");

	// Add all the discovered projects to the list
	const FString EngineIdentifier = FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier();
	for(TMap<FString, EProjectCategoryType>::TConstIterator Iter(AbsoluteProjectFilesToCategory); Iter; ++Iter)
	{
		const FString& ProjectFilename = *Iter.Key();
		const EProjectCategoryType DetectedCategoryType = Iter.Value();
		if ( FPaths::FileExists(ProjectFilename) )
		{
			FProjectStatus ProjectStatus;
			if (IProjectManager::Get().QueryStatusForProject(ProjectFilename, ProjectStatus))
			{
				// @todo localized project name
				const FText ProjectName = FText::FromString(ProjectStatus.Name);
				const FText ProjectDescription = FText::FromString(ProjectStatus.Description);

				TSharedPtr<FSlateDynamicImageBrush> DynamicBrush;
				const FString ThumbnailPNGFile = FPaths::GetBaseFilename(ProjectFilename, false) + TEXT(".png");
				const FString AutoScreenShotPNGFile = FPaths::Combine(*FPaths::GetPath(ProjectFilename), TEXT("Saved"), TEXT("AutoScreenshot.png"));
				FString PNGFileToUse;
				if ( FPaths::FileExists(*ThumbnailPNGFile) )
				{
					PNGFileToUse = ThumbnailPNGFile;
				}
				else if ( FPaths::FileExists(*AutoScreenShotPNGFile) )
				{
					PNGFileToUse = AutoScreenShotPNGFile;
				}

				if ( !PNGFileToUse.IsEmpty() )
				{
					const FName BrushName = FName(*PNGFileToUse);
					DynamicBrush = MakeShareable( new FSlateDynamicImageBrush(BrushName, FVector2D(128,128) ) );
				}

				FText ProjectCategory;
				if ( ProjectStatus.bSignedSampleProject )
				{
					// Signed samples can't override their category name
					ProjectCategory = SamplesCategoryName;
				}
				else
				{
					if(ProjectStatus.Category.IsEmpty())
					{
						// Not category specified, so use the category for the detected project type
						ProjectCategory = (DetectedCategoryType == EProjectCategoryType::Sample) ? SamplesCategoryName : MyProjectsCategoryName;
					}
					else
					{
						// Use the user defined category
						ProjectCategory = FText::FromString(ProjectStatus.Category);
					}
				}

				FString ProjectEngineIdentifier;
				bool bIsUpToDate = FDesktopPlatformModule::Get()->GetEngineIdentifierForProject(ProjectFilename, ProjectEngineIdentifier) && ProjectEngineIdentifier == EngineIdentifier;

				// Work out which platforms this project is targeting
				TArray<FName> TargetPlatforms;
				for(const PlatformInfo::FPlatformInfo& PlatformInfo : PlatformInfo::EnumeratePlatformInfoArray())
				{
					if(PlatformInfo.IsVanilla() && PlatformInfo.PlatformType == PlatformInfo::EPlatformType::Game && ProjectStatus.IsTargetPlatformSupported(PlatformInfo.PlatformInfoName))
					{
						TargetPlatforms.Add(PlatformInfo.PlatformInfoName);
					}
				}
				TargetPlatforms.Sort([](const FName& One, const FName& Two) -> bool
				{
					return One < Two;
				});

				const bool bIsNewProjectItem = false;
				TSharedRef<FProjectItem> NewProjectItem = MakeShareable( new FProjectItem(ProjectName, ProjectDescription, ProjectEngineIdentifier, bIsUpToDate, DynamicBrush, ProjectFilename, bIsNewProjectItem, TargetPlatforms, ProjectStatus.SupportsAllPlatforms() ) );
				AddProjectToCategory(NewProjectItem, ProjectCategory);
			}
		}
	}

	// Make sure the category order is "My Projects", "Samples", then all remaining categories in alphabetical order
	TSharedPtr<FProjectCategory> MyProjectsCategory;
	TSharedPtr<FProjectCategory> SamplesCategory;

	for ( int32 CategoryIdx = ProjectCategories.Num() - 1; CategoryIdx >= 0; --CategoryIdx )
	{
		TSharedRef<FProjectCategory> Category = ProjectCategories[CategoryIdx];
		if ( Category->CategoryName.EqualTo(MyProjectsCategoryName) )
		{
			MyProjectsCategory = Category;
			ProjectCategories.RemoveAt(CategoryIdx);
		}
		else if ( Category->CategoryName.EqualTo(SamplesCategoryName) )
		{
			SamplesCategory = Category;
			ProjectCategories.RemoveAt(CategoryIdx);
		}
	}

	// Sort categories
	struct FCompareCategories
	{
		FORCEINLINE bool operator()( const TSharedRef<FProjectCategory>& A, const TSharedRef<FProjectCategory>& B ) const
		{
			return A->CategoryName.CompareToCaseIgnored(B->CategoryName) < 0;
		}
	};
	ProjectCategories.Sort( FCompareCategories() );

	// Now read the built-in categories (last added is first in the list)
	if ( SamplesCategory.IsValid() )
	{
		ProjectCategories.Insert(SamplesCategory.ToSharedRef(), 0);
	}
	if ( MyProjectsCategory.IsValid() )
	{
		ProjectCategories.Insert(MyProjectsCategory.ToSharedRef(), 0);
	}

	// Sort each individual category
	struct FCompareProjectItems
	{
		FORCEINLINE bool operator()( const TSharedPtr<FProjectItem>& A, const TSharedPtr<FProjectItem>& B ) const
		{
			return A->Name.CompareToCaseIgnored(B->Name) < 0;
		}
	};
	for ( int32 CategoryIdx = 0; CategoryIdx < ProjectCategories.Num(); ++CategoryIdx )
	{
		TSharedRef<FProjectCategory> Category = ProjectCategories[CategoryIdx];
		Category->ProjectItemsSource.Sort( FCompareProjectItems() );
	}

	PopulateFilteredProjectCategories();

	for (auto CategoryIt = ProjectCategories.CreateConstIterator(); CategoryIt; ++CategoryIt)
	{
		const TSharedRef<FProjectCategory>& Category = *CategoryIt;
		ConstructCategory(CategoriesBox.ToSharedRef(), Category);
	}

	return FReply::Handled();
}


void SProjectBrowser::AddProjectToCategory(const TSharedRef<FProjectItem>& ProjectItem, const FText& ProjectCategory)
{
	for ( auto CategoryIt = ProjectCategories.CreateConstIterator(); CategoryIt; ++CategoryIt )
	{
		const TSharedRef<FProjectCategory>& Category = *CategoryIt;
		if ( Category->CategoryName.EqualToCaseIgnored(ProjectCategory) )
		{
			Category->ProjectItemsSource.Add(ProjectItem);
			return;
		}
	}

	TSharedRef<FProjectCategory> NewCategory = MakeShareable( new FProjectCategory );
	NewCategory->CategoryName = ProjectCategory;
	NewCategory->ProjectItemsSource.Add(ProjectItem);
	ProjectCategories.Add(NewCategory);
}


void SProjectBrowser::PopulateFilteredProjectCategories()
{
	NumFilteredProjects = 0;
	for (auto& Category : ProjectCategories)
	{
		Category->FilteredProjectItemsSource.Empty();

		for (auto& ProjectItem : Category->ProjectItemsSource)
		{
			if (ProjectItemFilter.PassesFilter(ProjectItem))
			{
				Category->FilteredProjectItemsSource.Add(ProjectItem);
				++NumFilteredProjects;
			}
		}

		if (Category->ProjectTileView.IsValid())
		{
			Category->ProjectTileView->RequestListRefresh();
		}
	}
}

FReply SProjectBrowser::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (InKeyEvent.GetKey() == EKeys::F5)
	{
		return FindProjects();
	}

	return FReply::Unhandled();
}

bool SProjectBrowser::OpenProject( const FString& InProjectFile )
{
	FText FailReason;
	FString ProjectFile = InProjectFile;

	// Get the identifier for the project
	FString ProjectIdentifier;
	FDesktopPlatformModule::Get()->GetEngineIdentifierForProject(ProjectFile, ProjectIdentifier);

	// Abort straight away if the project engine version is newer than the current engine version
	FEngineVersion EngineVersion;
	if (FDesktopPlatformModule::Get()->TryParseStockEngineVersion(ProjectIdentifier, EngineVersion))
	{
		if (FEngineVersion::GetNewest(EngineVersion, FEngineVersion::Current(), nullptr) == EVersionComparison::First)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CantLoadNewerProject", "Unable to open this project, as it was made with a newer version of the Unreal Engine."));
			return false;
		}
	}
	
	// Get the identifier for the current engine
	FString CurrentIdentifier = FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier();
	if(ProjectIdentifier != CurrentIdentifier)
	{
		// Get the current project status
		FProjectStatus ProjectStatus;
		if(!IProjectManager::Get().QueryStatusForProject(ProjectFile, ProjectStatus))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CouldNotReadProjectStatus", "Unable to read project status."));
			return false;
		}

		// If it's a code project, verify the user has the needed compiler installed before we continue.
		if ( ProjectStatus.bCodeBasedProject )
		{
			if ( !FSourceCodeNavigation::IsCompilerAvailable() )
			{
				const FText TitleText = LOCTEXT("CompilerNeeded", "Missing Compiler");
				const FText CompilerStillNotInstalled = FText::Format(LOCTEXT("CompilerStillNotInstalledFormatted", "Press OK when you've finished installing {0}."), FSourceCodeNavigation::GetSuggestedSourceCodeIDE());

				if ( FSourceCodeNavigation::GetCanDirectlyInstallSourceCodeIDE() )
				{
					const FText ErrorText = FText::Format(LOCTEXT("WouldYouLikeToDownloadAndInstallCompiler", "To open this project you must first install {0}.\n\nWould you like to download and install it now?"), FSourceCodeNavigation::GetSuggestedSourceCodeIDE());

					EAppReturnType::Type InstallCompilerResult = FMessageDialog::Open(EAppMsgType::YesNo, ErrorText, &TitleText);
					if ( InstallCompilerResult == EAppReturnType::No )
					{
						return false;
					}

					GWarn->BeginSlowTask(LOCTEXT("DownloadingInstalling", "Waiting for Installer to complete."), true, true);

					TOptional<bool> bWasDownloadASuccess;

					FSourceCodeNavigation::DownloadAndInstallSuggestedIDE(FOnIDEInstallerDownloadComplete::CreateLambda([&bWasDownloadASuccess] (bool bSuccessful) {
						bWasDownloadASuccess = bSuccessful;
					}));

					while ( !bWasDownloadASuccess.IsSet() )
					{
						// User canceled the install.
						if ( GWarn->ReceivedUserCancel() )
						{
							GWarn->EndSlowTask();
							return false;
						}

						GWarn->StatusUpdate(1, 1, LOCTEXT("WaitingForDownload", "Waiting for download to complete..."));
						FPlatformProcess::Sleep(0.1f);
					}

					GWarn->EndSlowTask();

					if ( !bWasDownloadASuccess.GetValue() )
					{
						const FText DownloadFailed = LOCTEXT("DownloadFailed", "Failed to download. Please check your internet connection.");
						if ( FMessageDialog::Open(EAppMsgType::OkCancel, DownloadFailed) == EAppReturnType::Cancel )
						{
							// User canceled, fail.
							return false;
						}
					}
				}
				else
				{
					const FText ErrorText = FText::Format(LOCTEXT("WouldYouLikeToInstallCompiler", "To open this project you must first install {0}.\n\nWould you like to install it now?"), FSourceCodeNavigation::GetSuggestedSourceCodeIDE());
					EAppReturnType::Type InstallCompilerResult = FMessageDialog::Open(EAppMsgType::YesNo, ErrorText, &TitleText);
					if ( InstallCompilerResult == EAppReturnType::No )
					{
						return false;
					}

					FString DownloadURL = FSourceCodeNavigation::GetSuggestedSourceCodeIDEDownloadURL();
					FPlatformProcess::LaunchURL(*DownloadURL, nullptr, nullptr);
				}

				// Loop until the users cancels or they complete installation.
				while ( !FSourceCodeNavigation::IsCompilerAvailable() )
				{
					EAppReturnType::Type UserInstalledResult = FMessageDialog::Open(EAppMsgType::OkCancel, CompilerStillNotInstalled);
					if ( UserInstalledResult == EAppReturnType::Cancel )
					{
						return false;
					}

					FSourceCodeNavigation::RefreshCompilerAvailability();
				}
			}
		}

		// Hyperlinks for the upgrade dialog
		TArray<FText> Hyperlinks;
		int32 MoreOptionsHyperlink = Hyperlinks.Add(LOCTEXT("ProjectConvert_MoreOptions", "More Options..."));

		// Button labels for the upgrade dialog
		TArray<FText> Buttons;
		int32 OpenCopyButton = Buttons.Add(LOCTEXT("ProjectConvert_OpenCopy", "Open a copy"));
		int32 CancelButton = Buttons.Add(LOCTEXT("ProjectConvert_Cancel", "Cancel"));
		int32 OpenExistingButton = -1;
		int32 SkipConversionButton = -1;

		// Prompt for upgrading. Different message for code and content projects, since the process is a bit trickier for code.
		FText DialogText;
		if(ProjectStatus.bCodeBasedProject)
		{
			DialogText = LOCTEXT("ConvertCodeProjectPrompt", "This project was made with a different version of the Unreal Engine. Converting to this version will rebuild your code projects.\n\nNew features and improvements sometimes cause API changes, which may require you to modify your code before it compiles. Content saved with newer versions of the editor will not open in older versions.\n\nWe recommend you open a copy of your project to avoid damaging the original.");
		}
		else
		{
			DialogText = LOCTEXT("ConvertContentProjectPrompt", "This project was made with a different version of the Unreal Engine.\n\nOpening it with this version of the editor may prevent it opening with the original editor, and may lose data. We recommend you open a copy to avoid damaging the original.");
		}

		// Show the dialog, and expand to the advanced dialog if the user selects 'More Options...'
		int32 Selection = SVerbChoiceDialog::ShowModal(LOCTEXT("ProjectConversionTitle", "Convert Project"), DialogText, Hyperlinks, Buttons);
		if(~Selection == MoreOptionsHyperlink)
		{
			OpenExistingButton = Buttons.Insert(LOCTEXT("ProjectConvert_ConvertInPlace", "Convert in-place"), 1);
			SkipConversionButton = Buttons.Insert(LOCTEXT("ProjectConvert_SkipConversion", "Skip conversion"), 2);
			CancelButton += 2;
			Selection = SVerbChoiceDialog::ShowModal(LOCTEXT("ProjectConversionTitle", "Convert Project"), DialogText, Buttons);
		}

		// Handle the selection
		if(Selection == CancelButton)
		{
			return false;
		}
		if(Selection == OpenCopyButton)
		{
			FString NewProjectFile;
			GameProjectUtils::EProjectDuplicateResult DuplicateResult = GameProjectUtils::DuplicateProjectForUpgrade(ProjectFile, NewProjectFile);

			if (DuplicateResult == GameProjectUtils::EProjectDuplicateResult::UserCanceled)
			{
				return false;
			}
			else if (DuplicateResult == GameProjectUtils::EProjectDuplicateResult::Failed)
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("ConvertProjectCopyFailed", "Couldn't copy project. Check you have sufficient hard drive space and write access to the project folder.") );
				return false;
			}

			ProjectFile = NewProjectFile;
		}
		if(Selection == OpenExistingButton)
		{
			FString FailPath;
			if(!FDesktopPlatformModule::Get()->CleanGameProject(FPaths::GetPath(ProjectFile), FailPath, GWarn))
			{
				FText FailMessage = FText::Format(LOCTEXT("ConvertProjectCleanFailed", "{0} could not be removed. Try deleting it manually and try again."), FText::FromString(FailPath));
				FMessageDialog::Open(EAppMsgType::Ok, FailMessage);
				return false;
			}
		}
		if(Selection != SkipConversionButton)
		{
			// Update the game project to the latest version. This will prompt to check-out as necessary. We don't need to write the engine identifier directly, because it won't use the right .uprojectdirs logic.
			if(!GameProjectUtils::UpdateGameProject(ProjectFile, CurrentIdentifier, FailReason))
			{
				if(FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ProjectUpgradeFailure", "The project file could not be updated to latest version. Attempt to open anyway?")) != EAppReturnType::Yes)
				{
					return false;
				}
			}

			// If it's a code-based project, generate project files and open visual studio after an upgrade
			if(ProjectStatus.bCodeBasedProject)
			{
				// Try to generate project files
				FStringOutputDevice OutputLog;
				OutputLog.SetAutoEmitLineTerminator(true);
				GLog->AddOutputDevice(&OutputLog);
				bool bHaveProjectFiles = FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), ProjectFile, GWarn);
				GLog->RemoveOutputDevice(&OutputLog);

				// Display any errors
				if(!bHaveProjectFiles)
				{
					FFormatNamedArguments Args;
					Args.Add( TEXT("LogOutput"), FText::FromString(OutputLog) );
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CouldNotGenerateProjectFiles", "Project files could not be generated. Log output:\n\n{LogOutput}"), Args));
					return false;
				}

				// Try to compile the project
				if(!GameProjectUtils::BuildCodeProject(ProjectFile))
				{
					return false;
				}
			}
		}
	}

	// Open the project
	if (!GameProjectUtils::OpenProject(ProjectFile, FailReason))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FailReason);
		return false;
	}

	return true;
}


void SProjectBrowser::OpenSelectedProject( )
{
	if ( CurrentSelectedProjectPath.IsEmpty() )
	{
		return;
	}

	OpenProject( CurrentSelectedProjectPath.ToString() );
}

/* SProjectBrowser event handlers
 *****************************************************************************/

FReply SProjectBrowser::HandleOpenProjectButtonClicked( )
{
	OpenSelectedProject();

	return FReply::Handled();
}


bool SProjectBrowser::HandleOpenProjectButtonIsEnabled( ) const
{
	return !CurrentSelectedProjectPath.IsEmpty();
}


void SProjectBrowser::HandleProjectItemDoubleClick( TSharedPtr<FProjectItem> TemplateItem )
{
	OpenSelectedProject();
}

FReply SProjectBrowser::OnBrowseToProjectClicked()
{
	const FString ProjectFileDescription = LOCTEXT( "FileTypeDescription", "Unreal Project File" ).ToString();
	const FString ProjectFileExtension = FString::Printf(TEXT("*.%s"), *FProjectDescriptor::GetExtension());
	const FString FileTypes = FString::Printf( TEXT("%s (%s)|%s"), *ProjectFileDescription, *ProjectFileExtension, *ProjectFileExtension );

	// Find the first valid project file to select by default
	FString DefaultFolder = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::PROJECT);
	for ( auto ProjectIt = GetDefault<UEditorSettings>()->RecentlyOpenedProjectFiles.CreateConstIterator(); ProjectIt; ++ProjectIt )
	{
		if ( IFileManager::Get().FileSize(**ProjectIt) > 0 )
		{
			// This is the first uproject file in the recents list that actually exists
			DefaultFolder = FPaths::GetPath(*ProjectIt);
			break;
		}
	}

	// Prompt the user for the filenames
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if ( DesktopPlatform )
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("OpenProjectBrowseTitle", "Open Project").ToString(),
			DefaultFolder,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames
		);
	}

	if ( bOpened && OpenFilenames.Num() > 0 )
	{
		HandleProjectViewSelectionChanged( NULL, ESelectInfo::Direct, FText() );

		FString Path = OpenFilenames[0];
		if ( FPaths::IsRelative( Path ) )
		{
			Path = FPaths::ConvertRelativePathToFull( Path );
		}

		CurrentSelectedProjectPath = FText::FromString( Path );

		OpenSelectedProject();
	}

	return FReply::Handled();
}


void SProjectBrowser::HandleProjectViewSelectionChanged(TSharedPtr<FProjectItem> ProjectItem, ESelectInfo::Type SelectInfo, FText CategoryName)
{
	if ( !bPreventSelectionChangeEvent )
	{
		TGuardValue<bool> SelectionEventGuard(bPreventSelectionChangeEvent, true);

		for ( auto CategoryIt = ProjectCategories.CreateConstIterator(); CategoryIt; ++CategoryIt )
		{
			const TSharedRef<FProjectCategory>& Category = *CategoryIt;
			if ( Category->ProjectTileView.IsValid() && !Category->CategoryName.EqualToCaseIgnored(CategoryName) )
			{
				Category->ProjectTileView->ClearSelection();
			}
		}

		const TSharedPtr<FProjectItem> SelectedItem = GetSelectedProjectItem();
		if ( SelectedItem.IsValid() && SelectedItem != CurrentlySelectedItem )
		{
			CurrentSelectedProjectPath = FText::FromString( SelectedItem->ProjectFile );
		}
	}
}


FReply SProjectBrowser::HandleMarketplaceTabButtonClicked()
{
	ILauncherPlatform* LauncherPlatform = FLauncherPlatformModule::Get();

	if (LauncherPlatform != nullptr)
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;

		FOpenLauncherOptions OpenOptions(TEXT("ue/marketplace"));
		if (LauncherPlatform->OpenLauncher(OpenOptions) )
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OpenSucceeded"), TEXT("TRUE")));
		}
		else
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OpenSucceeded"), TEXT("FALSE")));

			if (EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("InstallMarketplacePrompt", "The Marketplace requires the Epic Games Launcher, which does not seem to be installed on your computer. Would you like to install it now?")))
			{
				FOpenLauncherOptions InstallOptions(true, TEXT("ue/marketplace"));
				if (!LauncherPlatform->OpenLauncher(InstallOptions))
				{
					EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InstallSucceeded"), TEXT("FALSE")));
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Sorry, there was a problem installing the Launcher.\nPlease try to install it manually!")));
				}
				else
				{
					EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InstallSucceeded"), TEXT("TRUE")));
				}
			}
		}

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Source"), TEXT("ProjectBrowser")));
		if( FEngineAnalytics::IsAvailable() )
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.OpenMarketplace"), EventAttributes);
		}
	}

	return FReply::Handled();
}

void SProjectBrowser::OnFilterTextChanged(const FText& InText)
{
	ProjectItemFilter.SetRawFilterText(InText);
	SearchBoxPtr->SetError(ProjectItemFilter.GetFilterErrorText());
	PopulateFilteredProjectCategories();
}

void SProjectBrowser::OnAutoloadLastProjectChanged(ECheckBoxState NewState)
{
	UEditorSettings *Settings = GetMutableDefault<UEditorSettings>();
	Settings->bLoadTheMostRecentlyLoadedProjectAtStartup = (NewState == ECheckBoxState::Checked);

	UProperty* AutoloadProjectProperty = FindField<UProperty>(Settings->GetClass(), "bLoadTheMostRecentlyLoadedProjectAtStartup");
	if (AutoloadProjectProperty != NULL)
	{
		FPropertyChangedEvent PropertyUpdateStruct(AutoloadProjectProperty);
		Settings->PostEditChangeProperty(PropertyUpdateStruct);
	}
}

EVisibility SProjectBrowser::GetProjectCategoryVisibility(const TSharedRef<FProjectCategory> InCategory) const
{
	if (NumFilteredProjects == 0)
	{
		return EVisibility::Collapsed;
	}
	return InCategory->FilteredProjectItemsSource.Num() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SProjectBrowser::GetNoProjectsErrorVisibility() const
{
	return bHasProjectFiles ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SProjectBrowser::GetNoProjectsAfterFilterErrorVisibility() const
{
	return (bHasProjectFiles && NumFilteredProjects == 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SProjectBrowser::GetFilterActiveOverlayVisibility() const
{
	return ProjectItemFilter.GetRawFilterText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FText SProjectBrowser:: GetItemHighlightText() const
{
	return ProjectItemFilter.GetRawFilterText();
}

#undef LOCTEXT_NAMESPACE
