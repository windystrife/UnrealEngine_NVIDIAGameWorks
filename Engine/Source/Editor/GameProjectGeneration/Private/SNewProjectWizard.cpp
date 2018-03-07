// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SNewProjectWizard.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/SOverlay.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"
#include "GameProjectGenerationLog.h"
#include "GameProjectGenerationModule.h"
#include "HardwareTargetingModule.h"
#include "TemplateProjectDefs.h"
#include "GameProjectUtils.h"
#include "SGetSuggestedIDEWidget.h"
#include "DesktopPlatformModule.h"
#include "SourceCodeNavigation.h"
#include "TemplateCategory.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Workflow/SWizard.h"
#include "SDecoratedEnumCombo.h"
#include "IDocumentation.h"
#include "Internationalization/BreakIterator.h"
#include "Dialogs/SOutputLogDialog.h"
#include "TemplateItem.h"
#include "Settings/EditorSettings.h"

#define LOCTEXT_NAMESPACE "NewProjectWizard"

FName SNewProjectWizard::TemplatePageName = TEXT("Template");
FName SNewProjectWizard::NameAndLocationPageName = TEXT("NameAndLocation");


namespace NewProjectWizardDefs
{
	const float ThumbnailSize = 64.f, ThumbnailPadding = 5.f;
	const float ItemWidth = ThumbnailSize + 2*ThumbnailPadding;
	const float ItemHeight = ItemWidth + 30;
}

/**
 * Simple widget used to display a folder path, and a name of a file:
 * __________________________  ____________________
 * | C:\Users\Joe.Bloggs    |  | SomeFile.txt     |
 * |-------- Folder --------|  |------ Name ------|
 */
class SFilepath : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SFilepath )
		: _LabelBackgroundColor(FLinearColor::Black)
		, _LabelBackgroundBrush(FEditorStyle::GetBrush("WhiteBrush"))
	{}
		/** Attribute specifying the text to display in the folder input */
		SLATE_ATTRIBUTE(FText, FolderPath)

		/** Attribute specifying the text to display in the name input */
		SLATE_ATTRIBUTE(FText, Name)

		/** Background label tint for the folder/name labels */
		SLATE_ATTRIBUTE(FSlateColor, LabelBackgroundColor)

		/** Background label brush for the folder/name labels */
		SLATE_ATTRIBUTE(const FSlateBrush*, LabelBackgroundBrush)

		/** Event that is triggered when the browser for folder button is clicked */
		SLATE_EVENT(FOnClicked, OnBrowseForFolder)

		/** Events for when the name field is manipulated */
		SLATE_EVENT(FOnTextChanged, OnNameChanged)
		SLATE_EVENT(FOnTextCommitted, OnNameCommitted)
		
		/** Events for when the folder field is manipulated */
		SLATE_EVENT(FOnTextChanged, OnFolderChanged)
		SLATE_EVENT(FOnTextCommitted, OnFolderCommitted)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		ChildSlot
		[
			SNew(SGridPanel)
			.FillColumn(0, 2.f)
			.FillColumn(1, 1.f)

			// Folder input
			+ SGridPanel::Slot(0, 0)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SEditableTextBox)
					.Text(InArgs._FolderPath)
					// Large right hand padding to make room for the browse button
					.Padding(FMargin(5.f, 3.f, 25.f, 3.f))
					.OnTextChanged(InArgs._OnFolderChanged)
					.OnTextCommitted(InArgs._OnFolderCommitted)
				]
					
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FilePath.FolderButton")
					.ContentPadding(FMargin(4.f, 0.f))
					.OnClicked(InArgs._OnBrowseForFolder)
					.ToolTipText(LOCTEXT("BrowseForFolder", "Browse for a folder"))
					.Text(LOCTEXT("...", "..."))
				]
			]

			// Folder label
			+ SGridPanel::Slot(0, 1)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(3)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("FilePath.GroupIndicator"))
						.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.5f))
						.Padding(FMargin(150.f, 0.f))
					]
				]
					
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(5.f)
					.BorderImage(InArgs._LabelBackgroundBrush)
					.BorderBackgroundColor(InArgs._LabelBackgroundColor)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Folder", "Folder"))
					]
				]
			]

			// Name input
			+ SGridPanel::Slot(1, 0)
			.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
			.VAlign(VAlign_Center)
			[
				SNew(SEditableTextBox)
				.Text(InArgs._Name)
				.Padding(FMargin(5.f, 3.f))
				.OnTextChanged(InArgs._OnNameChanged)
				.OnTextCommitted(InArgs._OnNameCommitted)
			]

			// Name label
			+ SGridPanel::Slot(1, 1)
			.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(3)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("FilePath.GroupIndicator"))
						.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.5f))
						.Padding(FMargin(75.f, 0.f))
					]
				]
					
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(5.f)
					.BorderImage(InArgs._LabelBackgroundBrush)
					.BorderBackgroundColor(InArgs._LabelBackgroundColor)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Name", "Name"))
					]
				]
			]
		];
	}
};

/** Slate tile widget for template projects */
class STemplateTile : public STableRow<TSharedPtr<FTemplateItem>>
{
public:
	SLATE_BEGIN_ARGS( STemplateTile ){}
		SLATE_ARGUMENT( TSharedPtr<FTemplateItem>, Item )
	SLATE_END_ARGS()

private:
	TWeakPtr<FTemplateItem> Item;

public:
	/** Static build function */
	static TSharedRef<ITableRow> BuildTile(TSharedPtr<FTemplateItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if (!ensure(Item.IsValid()))
		{
			return SNew(STableRow<TSharedPtr<FTemplateItem>>, OwnerTable);
		}

		return SNew(STemplateTile, OwnerTable).Item(Item);
	}

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable )
	{
		check(InArgs._Item.IsValid())
		Item = InArgs._Item;

		STableRow::Construct(
			STableRow::FArguments()
			.Style(FEditorStyle::Get(), "GameProjectDialog.TemplateListView.TableRow")
			.Content()
			[
				SNew(SVerticalBox)

				// Thumbnail
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(NewProjectWizardDefs::ThumbnailPadding)
				[
					SNew(SBox)
					.WidthOverride( NewProjectWizardDefs::ThumbnailSize )
					.HeightOverride( NewProjectWizardDefs::ThumbnailSize )
					[
						SNew(SImage)
						.Image(this, &STemplateTile::GetThumbnail)
					]
				]

				// Name
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				.Padding(FMargin(NewProjectWizardDefs::ThumbnailPadding, 0))
				[
					SNew(STextBlock)
					.WrapTextAt( NewProjectWizardDefs::ThumbnailSize )
					.Justification(ETextJustify::Center)
					.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
					//.HighlightText(this, &SNewProjectWizard::GetItemHighlightText)
					.Text(InArgs._Item->Name)
				]
			],
			OwnerTable
		);
	}

private:

	/** Get this item's thumbnail or return the default */
	const FSlateBrush* GetThumbnail() const
	{
		auto ItemPtr = Item.Pin();
		if (ItemPtr.IsValid() && ItemPtr->Thumbnail.IsValid())
		{
			return ItemPtr->Thumbnail.Get();
		}
		return FEditorStyle::GetBrush("GameProjectDialog.DefaultGameThumbnail.Small");
	}
	
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SNewProjectWizard::Construct( const FArguments& InArgs )
{
	LastValidityCheckTime = 0;
	ValidityCheckFrequency = 4;
	bLastGlobalValidityCheckSuccessful = true;
	bLastNameAndLocationValidityCheckSuccessful = true;
	bPreventPeriodicValidityChecksUntilNextChange = false;
	bCopyStarterContent = GEditor ? GetDefault<UEditorSettings>()->bCopyStarterContentPreference : true;

	IHardwareTargetingModule& HardwareTargeting = IHardwareTargetingModule::Get();

	SelectedHardwareClassTarget = EHardwareClass::Desktop;
	SelectedGraphicsPreset = EGraphicsPreset::Maximum;

	// Find all template projects
	FindTemplateProjects();
	SetDefaultProjectLocation();

	SAssignNew(TemplateListView, STileView< TSharedPtr<FTemplateItem> >)
	.ListItemsSource(&FilteredTemplateList)
	.SelectionMode(ESelectionMode::Single)
	.ClearSelectionOnClick(false)
	.OnGenerateTile_Static(&STemplateTile::BuildTile)
	.ItemHeight( NewProjectWizardDefs::ItemHeight )
	.ItemWidth( NewProjectWizardDefs::ItemWidth )
	.OnMouseButtonDoubleClick(this, &SNewProjectWizard::HandleTemplateListViewDoubleClick)
	.OnSelectionChanged(this, &SNewProjectWizard::HandleTemplateListViewSelectionChanged);

	const EVisibility StarterContentVisiblity = GameProjectUtils::IsStarterContentAvailableForNewProjects() ? EVisibility::Visible : EVisibility::Collapsed;

	TSharedRef<SSeparator> Separator = SNew(SSeparator).Orientation(EOrientation::Orient_Vertical);
	Separator->SetBorderBackgroundColor(FLinearColor::White.CopyWithNewOpacity(0.25f));

	TSharedPtr<SWidget> StartContentCombo;
	{
		TArray<SDecoratedEnumCombo<int32>::FComboOption> StarterContentInfo;
		StarterContentInfo.Add(SDecoratedEnumCombo<int32>::FComboOption(
			0, FSlateIcon(FEditorStyle::GetStyleSetName(), "GameProjectDialog.NoStarterContent"), LOCTEXT("NoStarterContent", "No Starter Content")));

		// Only add the option to add starter content if its there to add !
		bool bIsStarterAvailable = GameProjectUtils::IsStarterContentAvailableForNewProjects();
		StarterContentInfo.Add(SDecoratedEnumCombo<int32>::FComboOption(
			1, FSlateIcon(FEditorStyle::GetStyleSetName(), "GameProjectDialog.IncludeStarterContent"), LOCTEXT("IncludeStarterContent", "With Starter Content"),bIsStarterAvailable));
		StartContentCombo = SNew(SDecoratedEnumCombo<int32>, MoveTemp(StarterContentInfo))
			.SelectedEnum(this, &SNewProjectWizard::GetCopyStarterContentIndex)
			.OnEnumChanged(this, &SNewProjectWizard::OnSetCopyStarterContent)
			.ToolTipText(LOCTEXT("CopyStarterContent_ToolTip", "Enable to include an additional content pack containing simple placeable meshes with basic materials and textures.\nYou can opt out of including this to create a project that only has the bare essentials for the selected project template."));
	}

	const float UniformPadding = 16.f;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		[
			SNew(SOverlay)

			// Wizard
			+SOverlay::Slot()
			.Padding(UniformPadding / 2.0f)
			[
				SAssignNew(MainWizard, SWizard)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
				.CancelButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
				.FinishButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
				.ButtonTextStyle(FEditorStyle::Get(), "LargeText")
				.ForegroundColor(FEditorStyle::Get().GetSlateColor("WhiteBrush"))
				.ShowPageList(false)
				.ShowCancelButton(false)
				.CanFinish(this, &SNewProjectWizard::HandleCreateProjectWizardCanFinish)
				.FinishButtonText(LOCTEXT("FinishButtonText", "Create Project"))
				.FinishButtonToolTip(LOCTEXT("FinishButtonToolTip", "Creates your new project in the specified location with the specified template and name.") )
				.OnFinished(this, &SNewProjectWizard::HandleCreateProjectWizardFinished)
				.OnFirstPageBackClicked(InArgs._OnBackRequested)

				// Choose Template
				+SWizard::Page()
				.OnEnter(this, &SNewProjectWizard::OnPageVisited, TemplatePageName)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.Padding(FMargin(UniformPadding / 2.0f, UniformPadding / 2.0f, UniformPadding / 2.0f, 0.0f))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.Padding(FMargin(0, 0, 0, 15))
						.AutoHeight()
						[
							SNew(SRichTextBlock)
							.Text(LOCTEXT("ProjectTemplateDescription", "Choose a <RichTextBlock.BoldHighlight>template</> to use as a starting point for your new project.  Any of these features can be added later by clicking <RichTextBlock.BoldHighlight>Add Feature or Content Pack</> in <RichTextBlock.BoldHighlight>Content Browser</>."))
							.AutoWrapText(true)
							.DecoratorStyleSet(&FEditorStyle::Get())
							.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("TemplateChoiceTooltip", "A template consists of a little bit of player control logic (either as a Blueprint or in C++), input bindings, and appropriate prototyping assets."), NULL, TEXT("Shared/Editor/NewProjectWizard"), TEXT("TemplateChoice")))
						]

						+ SVerticalBox::Slot()
						[
							// Template category tabs
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							.Padding(FMargin(8.f, 0.f))
							.AutoHeight()
							[
								BuildCategoryTabs()
							]

							// Templates list
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SBorder)
								.Padding(UniformPadding)
								.BorderImage(FEditorStyle::GetBrush("GameProjectDialog.TabBackground"))
								[
									SNew(SHorizontalBox)

									+ SHorizontalBox::Slot()
									[
										SNew(SScrollBorder, TemplateListView.ToSharedRef())
										[
											TemplateListView.ToSharedRef()
										]
									]
										
									+ SHorizontalBox::Slot()
									.Padding(UniformPadding, 0.0f)
									.AutoWidth()
									[
										Separator
									]

									// Selected template details
									+ SHorizontalBox::Slot()
									[
										SNew(SScrollBox)
										+ SScrollBox::Slot()
										.Padding(UniformPadding, 0.0f)
										[
											SNew(SVerticalBox)

											// Preview image
											+ SVerticalBox::Slot()
											.AutoHeight()
											.HAlign(HAlign_Center)
											.Padding(FMargin(0.0f, 0.0f, 0.0f, 15.f))
											[
												SNew(SBox)
												.Visibility(this, &SNewProjectWizard::GetSelectedTemplatePreviewVisibility)
												.WidthOverride(400)
												.HeightOverride(200)
												[
													SNew(SOverlay)
							
													+ SOverlay::Slot()
													[
														SNew(SBorder)
														.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.f))
														.BorderImage(FEditorStyle::GetBrush("ContentBrowser.ThumbnailShadow"))
														[
															SNew(SImage)
															.Image(this, &SNewProjectWizard::GetSelectedTemplatePreviewImage)
														]
													]

													+ SOverlay::Slot()
													.HAlign(HAlign_Right)
													.VAlign(VAlign_Top)
													.Padding(10.0f)
													[
														SNew(SBox)
														.WidthOverride(48.0f)
														.HeightOverride(48.0f)
														[
															SNew(SImage)
															.Image(this, &SNewProjectWizard::GetSelectedTemplateTypeImage)
														]
													]
												]
											]

											// Template Name
											+ SVerticalBox::Slot()
											.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
											.AutoHeight()
											[
												SNew(STextBlock)
												.AutoWrapText(true)
												.TextStyle(FEditorStyle::Get(), "GameProjectDialog.FeatureText")
												.Text(this, &SNewProjectWizard::GetSelectedTemplateProperty<FText>, &FTemplateItem::Name)
											]
						
											// Template Description
											+ SVerticalBox::Slot()
											[
												SNew(STextBlock)
												.AutoWrapText(true)
												.Text(this, &SNewProjectWizard::GetSelectedTemplateProperty<FText>, &FTemplateItem::Description)
											]
											
											// Asset types
											+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
											[
												SNew(SBox)
												.Visibility(this, &SNewProjectWizard::GetSelectedTemplateAssetVisibility)
												[
													SNew(SVerticalBox)
													+ SVerticalBox::Slot()
													[
														SNew(STextBlock)
														.TextStyle(FEditorStyle::Get(), "GameProjectDialog.FeatureText")
														.Text(LOCTEXT("ProjectTemplateAssetTypes", "Asset Type References:"))
													]
													+ SVerticalBox::Slot()
													.AutoHeight()
													[
														SNew(STextBlock)
														.AutoWrapText(true)
														.Text(this, &SNewProjectWizard::GetSelectedTemplateAssetTypes)
													]
												]
											]
											// Class types
											+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
											[
												SNew(SBox)
												.Visibility(this, &SNewProjectWizard::GetSelectedTemplateClassVisibility)
												[
													SNew(SVerticalBox)
													+ SVerticalBox::Slot()
													[
														SNew(STextBlock)
														.TextStyle(FEditorStyle::Get(), "GameProjectDialog.FeatureText")
														.Text(LOCTEXT("ProjectTemplateClassTypes", "Class Type References:"))
													]
													+ SVerticalBox::Slot()
														.AutoHeight()
														[
															SNew(STextBlock)
															.AutoWrapText(true)
															.Text(this, &SNewProjectWizard::GetSelectedTemplateClassTypes)
														]
												]

											]
										]
									]
								]
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 15.0f, 0.0f, 0.0f))
						[
							SNew(SScrollBox)

							+ SScrollBox::Slot()
							[
								
								SNew(SVerticalBox)
									
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0, 0, 0, 15.f))
								[
									SNew(SRichTextBlock)
									.Text(LOCTEXT("ProjectSettingsDescription", "Choose some <RichTextBlock.BoldHighlight>settings</> for your project.  Don't worry, you can change these later in the <RichTextBlock.BoldHighlight>Target Hardware</> section of <RichTextBlock.BoldHighlight>Project Settings</>.  You can also add the <RichTextBlock.BoldHighlight>Starter Content</> to your project later using <RichTextBlock.BoldHighlight>Content Browser</>."))
									.AutoWrapText(true)
									.DecoratorStyleSet(&FEditorStyle::Get())
									.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("HardwareTargetTooltip", "These settings will choose good defaults for a number of other settings in the project such as post-processing flags and touch input emulation using the mouse."), NULL, TEXT("Shared/Editor/NewProjectWizard"), TEXT("TargetHardware")))
								]

								+ SVerticalBox::Slot()
								.HAlign(HAlign_Center)
								.AutoHeight()
								[
									SNew(SBox)
									.WidthOverride(650)
									[
										SNew(SVerticalBox)

										+ SVerticalBox::Slot()
										.AutoHeight()
										.HAlign(HAlign_Center)
										.Padding(FMargin(0, 0, 0, 25.f))
										[
											SNew(SHorizontalBox)

											+ SHorizontalBox::Slot()
											.AutoWidth()
											[
												HardwareTargeting.MakeHardwareClassTargetCombo(
													FOnHardwareClassChanged::CreateSP(this, &SNewProjectWizard::SetHardwareClassTarget),
													TAttribute<EHardwareClass::Type>(this, &SNewProjectWizard::GetHardwareClassTarget)
													)
											]
								
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(FMargin(30, 0))
											[
												HardwareTargeting.MakeGraphicsPresetTargetCombo(
													FOnGraphicsPresetChanged::CreateSP(this, &SNewProjectWizard::SetGraphicsPreset),
													TAttribute<EGraphicsPreset::Type>(this, &SNewProjectWizard::GetGraphicsPreset)
													)
											]
								
											+ SHorizontalBox::Slot()
											.AutoWidth()
											[
												SNew(SOverlay)
												+SOverlay::Slot()
												[
													StartContentCombo.ToSharedRef()
												]

												// Warning when enabled for mobile, since the current starter content is bad for mobile
												+SOverlay::Slot()
												//.Visibility(EVisibility::SelfHitTestInvisible)
												.HAlign(HAlign_Right)
												.VAlign(VAlign_Top)
												.Padding(4)
												[
													SNew(SImage)
													.Image(FEditorStyle::GetBrush("Icons.Warning"))
													.ToolTipText(this, &SNewProjectWizard::GetStarterContentWarningTooltip)
													.Visibility(this, &SNewProjectWizard::GetStarterContentWarningVisibility)
												]
											]
										]
									]
								]

								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0, 0, 0, 15.f))
								[
									SNew(SRichTextBlock)
									.Text(LOCTEXT("ProjectPathDescription", "Select a <RichTextBlock.BoldHighlight>location</> for your project to be stored."))
									.AutoWrapText(true)
									.DecoratorStyleSet(&FEditorStyle::Get())
									.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("ProjectPathDescriptionTooltip", "All of your project content and code will be stored here."), NULL, TEXT("Shared/Editor/NewProjectWizard"), TEXT("ProjectPath")))
								]

								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Center)
								[
									// File path widget
									SNew(SFilepath)
									.OnBrowseForFolder(this, &SNewProjectWizard::HandleBrowseButtonClicked)
									.LabelBackgroundBrush(FEditorStyle::GetBrush("ProjectBrowser.Background"))
									.LabelBackgroundColor(FLinearColor::White)
									.FolderPath(this, &SNewProjectWizard::GetCurrentProjectFilePath)
									.Name(this, &SNewProjectWizard::GetCurrentProjectFileName)
									.OnFolderChanged(this, &SNewProjectWizard::OnCurrentProjectFilePathChanged)
									.OnNameChanged(this, &SNewProjectWizard::OnCurrentProjectFileNameChanged)
								]
							]
						]
					]
				]
			]

			// Global Error label
			+SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding( UniformPadding / 2 )
			[
				SNew(SBorder)
				.Visibility(this, &SNewProjectWizard::GetGlobalErrorLabelVisibility)
				.BorderImage(FEditorStyle::GetBrush("GameProjectDialog.ErrorLabelBorder"))
				.Padding( UniformPadding / 2 )
				[
					SNew(SHorizontalBox)
						
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(2.f)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("MessageLog.Warning"))
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(this, &SNewProjectWizard::GetGlobalErrorLabelText)
						.TextStyle(FEditorStyle::Get(), TEXT("GameProjectDialog.ErrorLabelFont"))
					]

					// Button/link to the suggested IDE
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.AutoWidth()
					.Padding(5.f, 0.f)
					[
						SNew(SGetSuggestedIDEWidget)
					]
									
					// A button to close the persistent global error text
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.ContentPadding(0.0f) 
						.OnClicked(this, &SNewProjectWizard::OnCloseGlobalErrorLabelClicked)
						.Visibility(this, &SNewProjectWizard::GetGlobalErrorLabelCloseButtonVisibility)
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("GameProjectDialog.ErrorLabelCloseButton"))
						]
					]
				]
			]

			// Project filename error
			+SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding( UniformPadding / 2 )
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("GameProjectDialog.ErrorLabelBorder"))
				.Visibility(this, &SNewProjectWizard::GetNameAndLocationErrorLabelVisibility)
				.Padding(UniformPadding / 2)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(2.f)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("MessageLog.Warning"))
					]

					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(this, &SNewProjectWizard::GetNameAndLocationErrorLabelText)
						.TextStyle(FEditorStyle::Get(), "GameProjectDialog.ErrorLabelFont")
					]
				]
			]
		]
	];

	// Initialize the current page name. Assuming the template page.
	CurrentPageName = TemplatePageName;

	HandleCategoryChanged(ECheckBoxState::Checked, ActiveCategory);

	UpdateProjectFileValidity();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


TSharedRef<SWidget> SNewProjectWizard::BuildCategoryTabs()
{
	TSharedRef<SHorizontalBox> TabStrip = SNew(SHorizontalBox);

	TArray<FName> Categories;
	Templates.GenerateKeyArray(Categories);

	for (const FName& CategoryName : Categories)
	{
		TSharedPtr<const FTemplateCategory> Category = FGameProjectGenerationModule::Get().GetCategory(CategoryName);

		TSharedPtr<SHorizontalBox> HorizontalBox;

		TabStrip->AddSlot().AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,0,2.f,0))
		[
			SNew(SBox)
			// Constrain the height to 32px (for the image) plus 5px padding vertically
			.HeightOverride(32.f + 5.f*2)
			[
				SNew( SCheckBox )
				.Style( FEditorStyle::Get(), "GameProjectDialog.Tab" )
				.OnCheckStateChanged(this, &SNewProjectWizard::HandleCategoryChanged, CategoryName)
				.IsChecked(this, &SNewProjectWizard::GetCategoryTabCheckState, CategoryName)
				.ToolTipText(Category.IsValid() ? Category->Description : FText())
				.Padding(FMargin(5.f))
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
				]
			]
		];


		if (Category.IsValid())
		{
			HorizontalBox->AddSlot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(32)
				.HeightOverride(32)
				[
					SNew(SImage)
					.Image(Category->Icon)
				]
			];
		}

		HorizontalBox->AddSlot()
		.Padding(5.f, 0.f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "GameProjectDialog.FeatureText")
			.Text(Category.IsValid() ? Category->Name : FText::FromString(CategoryName.ToString()))
		];
	}
	return TabStrip;
}

void SNewProjectWizard::OnSetCopyStarterContent(int32 InCopyStarterContent)
{
	bCopyStarterContent = InCopyStarterContent != 0;
}

EVisibility SNewProjectWizard::GetStarterContentWarningVisibility() const
{
	return (bCopyStarterContent && (SelectedHardwareClassTarget == EHardwareClass::Mobile)) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNewProjectWizard::GetStarterContentWarningTooltip() const
{
	if (SelectedGraphicsPreset == EGraphicsPreset::Maximum)
	{
		return LOCTEXT("StarterContentMobileWarning_Maximum", "Note: Starter content will be inserted first time the project is opened, and can increase the packaged size significantly, removing the example maps will result in only packaging content that is actually used");
	}
	else
	{
		return LOCTEXT("StarterContentMobileWarning_Scalable", "Warning: Starter content content will be inserted first time the project is opened, and is not optimized for scalable mobile projects");
	}
}

void SNewProjectWizard::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Every few seconds, the project file path is checked for validity in case the disk contents changed and the location is now valid or invalid.
	// After project creation, periodic checks are disabled to prevent a brief message indicating that the project you created already exists.
	// This feature is re-enabled if the user did not restart and began editing parameters again.
	if ( !bPreventPeriodicValidityChecksUntilNextChange && (InCurrentTime > LastValidityCheckTime + ValidityCheckFrequency) )
	{
		UpdateProjectFileValidity();
	}
}


void SNewProjectWizard::HandleTemplateListViewSelectionChanged(TSharedPtr<FTemplateItem> TemplateItem, ESelectInfo::Type SelectInfo)
{
	UpdateProjectFileValidity();
}


TSharedPtr<FTemplateItem> SNewProjectWizard::GetSelectedTemplateItem() const
{
	TArray< TSharedPtr<FTemplateItem> > SelectedItems = TemplateListView->GetSelectedItems();
	if ( SelectedItems.Num() > 0 )
	{
		return SelectedItems[0];
	}
	
	return NULL;
}

FText SNewProjectWizard::GetSelectedTemplateClassTypes() const
{
	return FText::FromString(GetSelectedTemplateProperty<FString>(&FTemplateItem::ClassTypes));
}

EVisibility SNewProjectWizard::GetSelectedTemplateClassVisibility() const
{
	return GetSelectedTemplateProperty<FString>(&FTemplateItem::ClassTypes).IsEmpty() == false? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNewProjectWizard::GetSelectedTemplateAssetTypes() const
{
	return FText::FromString(GetSelectedTemplateProperty<FString>(&FTemplateItem::AssetTypes));
}

EVisibility SNewProjectWizard::GetSelectedTemplateAssetVisibility() const
{
	return GetSelectedTemplateProperty<FString>(&FTemplateItem::AssetTypes).IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SNewProjectWizard::GetSelectedTemplatePreviewImage() const
{
	auto PreviewImage = GetSelectedTemplateProperty(&FTemplateItem::PreviewImage);
	return PreviewImage.IsValid() ? PreviewImage.Get() : nullptr;
}

EVisibility SNewProjectWizard::GetSelectedTemplatePreviewVisibility() const
{
	auto PreviewImage = GetSelectedTemplateProperty(&FTemplateItem::PreviewImage);
	return PreviewImage.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SNewProjectWizard::GetSelectedTemplateTypeImage() const
{
	TSharedPtr<FTemplateItem> SelectedItem = GetSelectedTemplateItem();
	if (SelectedItem.IsValid())
	{
		auto Category = FGameProjectGenerationModule::Get().GetCategory(SelectedItem->Type);
		if (Category.IsValid())
		{
			return Category->Image;
		}
	}
	return nullptr;
}


FText SNewProjectWizard::GetCurrentProjectFileName() const
{
	return FText::FromString( CurrentProjectFileName );
}


FString SNewProjectWizard::GetCurrentProjectFileNameStringWithExtension() const
{
	return CurrentProjectFileName + TEXT(".") + FProjectDescriptor::GetExtension();
}


void SNewProjectWizard::OnCurrentProjectFileNameChanged(const FText& InValue)
{
	CurrentProjectFileName = InValue.ToString();
	UpdateProjectFileValidity();
}


FText SNewProjectWizard::GetCurrentProjectFilePath() const
{
	return FText::FromString(CurrentProjectFilePath);
}


FString SNewProjectWizard::GetCurrentProjectFileParentFolder() const
{
	if ( CurrentProjectFilePath.EndsWith(TEXT("/")) || CurrentProjectFilePath.EndsWith("\\") )
	{
		return FPaths::GetCleanFilename( CurrentProjectFilePath.LeftChop(1) );
	}
	else
	{
		return FPaths::GetCleanFilename( CurrentProjectFilePath );
	}
}


void SNewProjectWizard::OnCurrentProjectFilePathChanged(const FText& InValue)
{
	CurrentProjectFilePath = InValue.ToString();
	FPaths::MakePlatformFilename(CurrentProjectFilePath);
	UpdateProjectFileValidity();
}


FString SNewProjectWizard::GetProjectFilenameWithPathLabelText() const
{
	return GetProjectFilenameWithPath();
}


FString SNewProjectWizard::GetProjectFilenameWithPath() const
{
	if ( CurrentProjectFilePath.IsEmpty() )
	{
		// Don't even try to assemble the path or else it may be relative to the binaries folder!
		return TEXT("");
	}
	else
	{
		const FString ProjectName = CurrentProjectFileName;
		const FString ProjectPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CurrentProjectFilePath);
		const FString Filename = ProjectName + TEXT(".") + FProjectDescriptor::GetExtension();
		FString ProjectFilename = FPaths::Combine( *ProjectPath, *ProjectName, *Filename );
		FPaths::MakePlatformFilename(ProjectFilename);
		return ProjectFilename;
	}
}


FReply SNewProjectWizard::HandleBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if ( DesktopPlatform )
	{
		FString FolderName;
		const FString Title = LOCTEXT("NewProjectBrowseTitle", "Choose a project location").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			Title,
			LastBrowsePath,
			FolderName
			);

		if ( bFolderSelected )
		{
			if ( !FolderName.EndsWith(TEXT("/")) )
			{
				FolderName += TEXT("/");
			}
			
			FPaths::MakePlatformFilename(FolderName);
			LastBrowsePath = FolderName;
			CurrentProjectFilePath = FolderName;
		}
	}

	return FReply::Handled();
}

void SNewProjectWizard::HandleTemplateListViewDoubleClick( TSharedPtr<FTemplateItem> TemplateItem )
{
	// Advance to the name/location page
	const int32 NamePageIdx = 1;
	if ( MainWizard->CanShowPage(NamePageIdx) )
	{
		MainWizard->ShowPage(NamePageIdx);
	}
}


bool SNewProjectWizard::IsCreateProjectEnabled() const
{
	if ( CurrentPageName == NAME_None )//|| CurrentPageName == TemplatePageName )
	{
		return false;
	}

	return bLastGlobalValidityCheckSuccessful && bLastNameAndLocationValidityCheckSuccessful;
}


bool SNewProjectWizard::HandlePageCanShow( FName PageName ) const
{
	if (PageName == NameAndLocationPageName)
	{
		return bLastGlobalValidityCheckSuccessful;
	}

	return true;
}


void SNewProjectWizard::OnPageVisited(FName NewPageName)
{
	CurrentPageName = NewPageName;
}


EVisibility SNewProjectWizard::GetGlobalErrorLabelVisibility() const
{
	const bool bIsVisible = GetNameAndLocationErrorLabelText().IsEmpty() && !GetGlobalErrorLabelText().IsEmpty();
	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}


EVisibility SNewProjectWizard::GetGlobalErrorLabelCloseButtonVisibility() const
{
	return PersistentGlobalErrorLabelText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SNewProjectWizard::GetGlobalErrorLabelText() const
{
	if ( !PersistentGlobalErrorLabelText.IsEmpty() )
	{
		return PersistentGlobalErrorLabelText;
	}

	if ( !bLastGlobalValidityCheckSuccessful )
	{
		return LastGlobalValidityErrorText;
	}

	return FText::GetEmpty();
}


FReply SNewProjectWizard::OnCloseGlobalErrorLabelClicked()
{
	PersistentGlobalErrorLabelText = FText();

	return FReply::Handled();
}


EVisibility SNewProjectWizard::GetNameAndLocationErrorLabelVisibility() const
{
	return GetNameAndLocationErrorLabelText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}


FText SNewProjectWizard::GetNameAndLocationErrorLabelText() const
{
	if ( !bLastNameAndLocationValidityCheckSuccessful )
	{
		return LastNameAndLocationValidityErrorText;
	}

	return FText::GetEmpty();
}

TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& SNewProjectWizard::FindTemplateProjects()
{
	// Default to showing the blueprint category
	ActiveCategory = FTemplateCategory::BlueprintCategoryName;
	
	// Clear the list out first - or we could end up with duplicates
	Templates.Empty();

	// Add some default non-data driven templates
	Templates.FindOrAdd(FTemplateCategory::BlueprintCategoryName).Add(MakeShareable(new FTemplateItem(
		LOCTEXT("BlankProjectName", "Blank"),
		LOCTEXT("BlankProjectDescription", "A clean empty project with no code."),
		false, FTemplateCategory::BlueprintCategoryName,
		TEXT("_1"),			// SortKey
		TEXT(""),			// No filename, this is a generation template
		MakeShareable( new FSlateBrush( *FEditorStyle::GetBrush("GameProjectDialog.BlankProjectThumbnail") ) ),
		MakeShareable( new FSlateBrush( *FEditorStyle::GetBrush("GameProjectDialog.BlankProjectPreview") ) ),
		TEXT(""),		// No class types
		TEXT("")		// No asset types
		)) );

	Templates.FindOrAdd(FTemplateCategory::CodeCategoryName).Add(MakeShareable(new FTemplateItem(
		LOCTEXT("BasicCodeProjectName", "Basic Code"),
		LOCTEXT("BasicCodeProjectDescription", "An empty project with some basic game framework code classes created."),
		true, FTemplateCategory::CodeCategoryName,
		TEXT("_2"),			// SortKey
		TEXT(""),			// No filename, this is a generation template
		MakeShareable( new FSlateBrush( *FEditorStyle::GetBrush("GameProjectDialog.BasicCodeThumbnail") ) ),
		MakeShareable( new FSlateBrush( *FEditorStyle::GetBrush("GameProjectDialog.BlankProjectPreview") ) ),
		TEXT(""),		// No class types
		TEXT("")		// No asset types
		)) );

	// Now discover and all data driven templates
	TArray<FString> TemplateRootFolders;

	// @todo rocket make template folder locations extensible.
	TemplateRootFolders.Add( FPaths::RootDir() + TEXT("Templates") );

	// allow plugins to define templates
	TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPlugins();
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		FString PluginDirectory = Plugin->GetBaseDir();
		if (!PluginDirectory.IsEmpty())
		{
			const FString PluginTemplatesDirectory = FPaths::Combine(*PluginDirectory, TEXT("Templates"));

			if (IFileManager::Get().DirectoryExists(*PluginTemplatesDirectory))
			{
				TemplateRootFolders.Add(PluginTemplatesDirectory);
			}
		}
	}

	// Form a list of all folders that could contain template projects
	TArray<FString> AllTemplateFolders;
	for ( auto TemplateRootFolderIt = TemplateRootFolders.CreateConstIterator(); TemplateRootFolderIt; ++TemplateRootFolderIt )
	{
		const FString Root = *TemplateRootFolderIt;
		const FString SearchString = Root / TEXT("*");
		TArray<FString> TemplateFolders;
		IFileManager::Get().FindFiles(TemplateFolders, *SearchString, /*Files=*/false, /*Directories=*/true);
		for ( auto TemplateFolderIt = TemplateFolders.CreateConstIterator(); TemplateFolderIt; ++TemplateFolderIt )
		{
			AllTemplateFolders.Add( Root / (*TemplateFolderIt) );
		}
	}

	// Add a template item for every discovered project
	for ( auto TemplateFolderIt = AllTemplateFolders.CreateConstIterator(); TemplateFolderIt; ++TemplateFolderIt )
	{
		const FString SearchString = (*TemplateFolderIt) / TEXT("*.") + FProjectDescriptor::GetExtension();
		TArray<FString> FoundProjectFiles;
		IFileManager::Get().FindFiles(FoundProjectFiles, *SearchString, /*Files=*/true, /*Directories=*/false);
		if ( FoundProjectFiles.Num() > 0 )
		{
			if ( ensure(FoundProjectFiles.Num() == 1) )
			{
				// Make sure a TemplateDefs ini file exists
				const FString Root = *TemplateFolderIt;
				UTemplateProjectDefs* TemplateDefs = GameProjectUtils::LoadTemplateDefs(Root);
				if ( TemplateDefs )
				{
					// Ignore any templates whoose definition says we cannot use to create a project
					if( TemplateDefs->bAllowProjectCreation == false )
						continue;
					// Found a template. Add it to the template items list.
					FString ProjectFilename = Root / FoundProjectFiles[0];
					FText TemplateName = TemplateDefs->GetDisplayNameText();
					FText TemplateDescription = TemplateDefs->GetLocalizedDescription();
					FString ClassTypes = TemplateDefs->ClassTypes;
					FString AssetTypes = TemplateDefs->AssetTypes;
					
					// If no template name was specified for the current culture, just use the project name
					if ( TemplateName.IsEmpty() )
					{
						TemplateName = FText::FromString(FPaths::GetBaseFilename(ProjectFilename));
					}

					// Only generate code if the template has a source folder
					const bool bGenerateCode = TemplateDefs->GeneratesCode(Root);

					TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;
					const FString ThumbnailPNGFile = (Root + TEXT("/Media/") + FoundProjectFiles[0]).Replace(TEXT(".uproject"), TEXT(".png"));
					if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*ThumbnailPNGFile) )
					{
						const FName BrushName = FName(*ThumbnailPNGFile);
						ThumbnailBrush = MakeShareable( new FSlateDynamicImageBrush(BrushName , FVector2D(128,128) ) );
					}

					TSharedPtr<FSlateDynamicImageBrush> PreviewBrush;
					const FString PreviewPNGFile = (Root + TEXT("/Media/") + FoundProjectFiles[0]).Replace(TEXT(".uproject"), TEXT("_Preview.png"));
					if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*PreviewPNGFile) )
					{
						const FName BrushName = FName(*PreviewPNGFile);
						PreviewBrush = MakeShareable( new FSlateDynamicImageBrush(BrushName , FVector2D(512,256) ) );
					}

					// Get the sort key
					FString SortKey = TemplateDefs->SortKey;
					if(SortKey.Len() == 0)
					{
						SortKey = FPaths::GetCleanFilename(ProjectFilename);
					}
					if (FPaths::GetCleanFilename(ProjectFilename) == GameProjectUtils::GetDefaultProjectTemplateFilename())
					{
						SortKey = TEXT("_0");
					}

					// Assign the template to the correct category. If the template has no explicit category assigned, we assign it to either C++ or blueprint
					FName Category = TemplateDefs->Category;
					if (Category.IsNone())
					{
						Category = bGenerateCode ? FTemplateCategory::CodeCategoryName : FTemplateCategory::BlueprintCategoryName;
					}

					Templates.FindOrAdd(Category).Add(MakeShareable(new FTemplateItem(
						TemplateName,
						TemplateDescription,
						bGenerateCode,
						Category,
						MoveTemp(SortKey),
						MoveTemp(ProjectFilename),
						ThumbnailBrush,
						PreviewBrush,
						ClassTypes,
						AssetTypes
					)));
				}
			}
			else
			{
				// More than one project file in this template? This is not legal, skip it.
				continue;
			}
		}
	}
	return Templates;
}


void SNewProjectWizard::SetDefaultProjectLocation( )
{
	FString DefaultProjectFilePath;
	
	// First, try and use the first previously used path that still exists
	for ( const FString& CreatedProjectPath : GetDefault<UEditorSettings>()->CreatedProjectPaths )
	{
		if ( IFileManager::Get().DirectoryExists(*CreatedProjectPath) )
		{
			DefaultProjectFilePath = CreatedProjectPath;
			break;
		}
	}

	if ( DefaultProjectFilePath.IsEmpty() )
	{
		// No previously used path, decide a default path.
		DefaultProjectFilePath = FDesktopPlatformModule::Get()->GetDefaultProjectCreationPath();
		IFileManager::Get().MakeDirectory(*DefaultProjectFilePath, true);
	}

	if ( !DefaultProjectFilePath.IsEmpty() && DefaultProjectFilePath.Right(1) == TEXT("/") )
	{
		DefaultProjectFilePath.LeftChop(1);
	}

	FPaths::NormalizeFilename(DefaultProjectFilePath);
	FPaths::MakePlatformFilename(DefaultProjectFilePath);
	const FString GenericProjectName = LOCTEXT("DefaultProjectName", "MyProject").ToString();
	FString ProjectName = GenericProjectName;

	// Check to make sure the project file doesn't already exist
	FText FailReason;
	if ( !GameProjectUtils::IsValidProjectFileForCreation(DefaultProjectFilePath / ProjectName / ProjectName + TEXT(".") + FProjectDescriptor::GetExtension(), FailReason) )
	{
		// If it exists, find an appropriate numerical suffix
		const int MaxSuffix = 1000;
		int32 Suffix;
		for ( Suffix = 2; Suffix < MaxSuffix; ++Suffix )
		{
			ProjectName = GenericProjectName + FString::Printf(TEXT("%d"), Suffix);
			if ( GameProjectUtils::IsValidProjectFileForCreation(DefaultProjectFilePath / ProjectName / ProjectName + TEXT(".") + FProjectDescriptor::GetExtension(), FailReason) )
			{
				// Found a name that is not taken. Break out.
				break;
			}
		}

		if (Suffix >= MaxSuffix)
		{
			UE_LOG(LogGameProjectGeneration, Warning, TEXT("Failed to find a suffix for the default project name"));
			ProjectName = TEXT("");
		}
	}

	if ( !DefaultProjectFilePath.IsEmpty() )
	{
		CurrentProjectFileName = ProjectName;
		CurrentProjectFilePath = DefaultProjectFilePath;
		FPaths::MakePlatformFilename(CurrentProjectFilePath);
		LastBrowsePath = CurrentProjectFilePath;
	}
}


void SNewProjectWizard::UpdateProjectFileValidity( )
{
	// Global validity
	{
		bLastGlobalValidityCheckSuccessful = true;

		TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();
		if ( !SelectedTemplate.IsValid() )
		{
			bLastGlobalValidityCheckSuccessful = false;
			LastGlobalValidityErrorText = LOCTEXT("NoTemplateSelected", "No Template Selected");
		}
		else
		{
			if (IsCompilerRequired())
			{
				if ( !FSourceCodeNavigation::IsCompilerAvailable() )
				{
					bLastGlobalValidityCheckSuccessful = false;
					LastGlobalValidityErrorText = FText::Format( LOCTEXT("NoCompilerFound", "No compiler was found. In order to use a C++ template, you must first install {0}."), FSourceCodeNavigation::GetSuggestedSourceCodeIDE() );
				}
				else if ( !FDesktopPlatformModule::Get()->IsUnrealBuildToolAvailable() )
				{
					bLastGlobalValidityCheckSuccessful = false;
					LastGlobalValidityErrorText = LOCTEXT("UBTNotFound", "Engine source code was not found. In order to use a C++ template, you must have engine source code in Engine/Source.");
				}
			}
		}
	}

	// Name and Location Validity
	{
		bLastNameAndLocationValidityCheckSuccessful = true;

		if ( !FPlatformMisc::IsValidAbsolutePathFormat(CurrentProjectFilePath) )
		{
			bLastNameAndLocationValidityCheckSuccessful = false;
			LastNameAndLocationValidityErrorText = LOCTEXT( "InvalidFolderPath", "The folder path is invalid" );
		}
		else
		{
			FText FailReason;
			if ( !GameProjectUtils::IsValidProjectFileForCreation(GetProjectFilenameWithPath(), FailReason) )
			{
				bLastNameAndLocationValidityCheckSuccessful = false;
				LastNameAndLocationValidityErrorText = FailReason;
			}
		}

		if ( CurrentProjectFileName.Contains(TEXT("/")) || CurrentProjectFileName.Contains(TEXT("\\")) )
		{
			bLastNameAndLocationValidityCheckSuccessful = false;
			LastNameAndLocationValidityErrorText = LOCTEXT("SlashOrBackslashInProjectName", "The project name may not contain a slash or backslash");
		}
		else
		{
			FText FailReason;
			if ( !GameProjectUtils::IsValidProjectFileForCreation(GetProjectFilenameWithPath(), FailReason) )
			{
				bLastNameAndLocationValidityCheckSuccessful = false;
				LastNameAndLocationValidityErrorText = FailReason;
			}
		}
	}

	LastValidityCheckTime = FSlateApplication::Get().GetCurrentTime();

	// Since this function was invoked, periodic validity checks should be re-enabled if they were disabled.
	bPreventPeriodicValidityChecksUntilNextChange = false;
}


bool SNewProjectWizard::IsCompilerRequired( ) const
{
	TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();
	return SelectedTemplate.IsValid() && SelectedTemplate->bGenerateCode;
}


bool SNewProjectWizard::CreateProject( const FString& ProjectFile )
{
	// Get the selected template
	TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();

	if (!ensure(SelectedTemplate.IsValid()))
	{
		// A template must be selected.
		return false;
	}

	FText FailReason, FailLog;

	FProjectInformation ProjectInfo(ProjectFile, SelectedTemplate->bGenerateCode, bCopyStarterContent, SelectedTemplate->ProjectFile);
	ProjectInfo.TargetedHardware = SelectedHardwareClassTarget;
	ProjectInfo.DefaultGraphicsPerformance = SelectedGraphicsPreset;

	const FProjectDescriptor* CurrentProject = IProjectManager::Get().GetCurrentProject();
	if (CurrentProject != nullptr)
	{
		ProjectInfo.bIsEnterpriseProject = CurrentProject->bIsEnterpriseProject;
	}
	else
	{
		// Set the default value for the enterprise flag from the command line for now.
		// This should be temporary until we implement a more generic approach.
		ProjectInfo.bIsEnterpriseProject = FParse::Param(FCommandLine::Get(), TEXT("enterprise"));
	}

	if (!GameProjectUtils::CreateProject(ProjectInfo, FailReason, FailLog))
	{
		SOutputLogDialog::Open(LOCTEXT("CreateProject", "Create Project"), FailReason, FailLog, FText::GetEmpty());
		return false;
	}

	// Successfully created the project. Update the last created location string.
	FString CreatedProjectPath = FPaths::GetPath(FPaths::GetPath(ProjectFile)); 

	// if the original path was the drives root (ie: C:/) the double path call strips the last /
	if (CreatedProjectPath.EndsWith(":"))
	{
		CreatedProjectPath.AppendChar('/');
	}

	auto* Settings = GetMutableDefault<UEditorSettings>();
	Settings->CreatedProjectPaths.Remove(CreatedProjectPath);
	Settings->CreatedProjectPaths.Insert(CreatedProjectPath, 0);
	Settings->bCopyStarterContentPreference = bCopyStarterContent;
	Settings->PostEditChange();

	return true;
}


void SNewProjectWizard::CreateAndOpenProject( )
{
	if( !IsCreateProjectEnabled() )
	{
		return;
	}

	FString ProjectFile = GetProjectFilenameWithPath();
	if ( !CreateProject(ProjectFile) )
	{
		return;
	}

	// Prevent periodic validity checks. This is to prevent a brief error message about the project already existing while you are exiting.
	bPreventPeriodicValidityChecksUntilNextChange = true;

	if( GetSelectedTemplateItem()->bGenerateCode )
	{
	    // If the engine is installed it is already compiled, so we can try to build and open a new project immediately. Non-installed situations might require building
	    // the engine (especially the case when binaries came from P4), so we only open the IDE for that.
		if (FApp::IsEngineInstalled())
		{
			if (GameProjectUtils::BuildCodeProject(ProjectFile))
			{
				OpenCodeIDE( ProjectFile );
				OpenProject( ProjectFile );
			}
			else
			{
				// User will have already been prompted to open the IDE
			}
		}
		else
		{
			OpenCodeIDE( ProjectFile );
		}
	}
	else
	{
		OpenProject( ProjectFile );
	}
}


bool SNewProjectWizard::OpenProject( const FString& ProjectFile )
{
	FText FailReason;
	if ( GameProjectUtils::OpenProject( ProjectFile, FailReason ) )
	{
		// Successfully opened the project, the editor is closing.
		// Close this window in case something prevents the editor from closing (save dialog, quit confirmation, etc)
		CloseWindowIfAppropriate();
		return true;
	}

	DisplayError( FailReason );
	return false;
}


bool SNewProjectWizard::OpenCodeIDE( const FString& ProjectFile )
{
	FText FailReason;

	if ( GameProjectUtils::OpenCodeIDE( ProjectFile, FailReason ) )
	{
		// Successfully opened code editing IDE, the editor is closing
		// Close this window in case something prevents the editor from closing (save dialog, quit confirmation, etc)
		CloseWindowIfAppropriate(true);
		return true;
	}

	DisplayError( FailReason );
	return false;
}


void SNewProjectWizard::CloseWindowIfAppropriate( bool ForceClose )
{
	if ( ForceClose || FApp::HasProjectName() )
	{
		FWidgetPath WidgetPath;
		TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow( AsShared(), WidgetPath);

		if ( ContainingWindow.IsValid() )
		{
			ContainingWindow->RequestDestroyWindow();
		}
	}
}


void SNewProjectWizard::DisplayError( const FText& ErrorText )
{
	FString ErrorString = ErrorText.ToString();
	UE_LOG(LogGameProjectGeneration, Log, TEXT("%s"), *ErrorString);
	if(ErrorString.Contains("\n"))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorText);
	}
	else
	{
		PersistentGlobalErrorLabelText = ErrorText;
	}
}


/* SNewProjectWizard event handlers
 *****************************************************************************/

bool SNewProjectWizard::HandleCreateProjectWizardCanFinish( ) const
{
	return IsCreateProjectEnabled();
}


void SNewProjectWizard::HandleCreateProjectWizardFinished( )
{
	CreateAndOpenProject();
}

ECheckBoxState SNewProjectWizard::GetCategoryTabCheckState(FName Category) const
{
	return Category == ActiveCategory ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewProjectWizard::HandleCategoryChanged(ECheckBoxState CheckState, FName Category)
{
	if (CheckState != ECheckBoxState::Checked)
	{
		return;
	}

	ActiveCategory = Category;
	FilteredTemplateList = Templates.FindRef(Category);

	// Sort the template folders
	FilteredTemplateList.Sort([](const TSharedPtr<FTemplateItem>& A, const TSharedPtr<FTemplateItem>& B){
		return A->SortKey < B->SortKey;
	});

	if (FilteredTemplateList.Num() > 0)
	{
		TemplateListView->SetSelection(FilteredTemplateList[0]);
	}
	TemplateListView->RequestListRefresh();
}

void SNewProjectWizard::SetHardwareClassTarget(EHardwareClass::Type InHardwareClass)
{
	SelectedHardwareClassTarget = InHardwareClass;
}

void SNewProjectWizard::SetGraphicsPreset(EGraphicsPreset::Type InGraphicsPreset)
{
	SelectedGraphicsPreset = InGraphicsPreset;
}

#undef LOCTEXT_NAMESPACE
