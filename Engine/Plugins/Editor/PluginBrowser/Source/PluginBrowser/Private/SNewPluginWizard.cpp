// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SNewPluginWizard.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "ModuleDescriptor.h"
#include "PluginDescriptor.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STileView.h"
#include "PluginStyle.h"
#include "PluginHelpers.h"
#include "DesktopPlatformModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "GameProjectGenerationModule.h"
#include "GameProjectUtils.h"
#include "PluginBrowserModule.h"
#include "SFilePathBlock.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"
#include "IMainFrameModule.h"
#include "IProjectManager.h"
#include "GameProjectGenerationModule.h"
#include "DefaultPluginWizardDefinition.h"
#include "UnrealEdMisc.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SourceCodeNavigation.h"

DEFINE_LOG_CATEGORY(LogPluginWizard);

#define LOCTEXT_NAMESPACE "NewPluginWizard"

static bool IsContentOnlyProject()
{
	const FProjectDescriptor* CurrentProject = IProjectManager::Get().GetCurrentProject();
	return CurrentProject == nullptr || CurrentProject->Modules.Num() == 0 || !FGameProjectGenerationModule::Get().ProjectHasCodeFiles();
}

SNewPluginWizard::SNewPluginWizard()
	: bIsPluginPathValid(false)
	, bIsPluginNameValid(false)
	, bIsEnginePlugin(false)
{
	AbsoluteGamePluginPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::ProjectPluginsDir());
	FPaths::MakePlatformFilename(AbsoluteGamePluginPath);
	AbsoluteEnginePluginPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::EnginePluginsDir());
	FPaths::MakePlatformFilename(AbsoluteEnginePluginPath);
}

void SNewPluginWizard::Construct(const FArguments& Args, TSharedPtr<SDockTab> InOwnerTab, TSharedPtr<IPluginWizardDefinition> InPluginWizardDefinition)
{
	OwnerTab = InOwnerTab;

	PluginWizardDefinition = InPluginWizardDefinition;

	// Prepare to create the descriptor data field
	DescriptorData = NewObject<UNewPluginDescriptorData>();
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowMultipleTopLevelObjects = false;
		DetailsViewArgs.bAllowFavoriteSystem = false;
		DetailsViewArgs.bShowActorLabel = false;
		DetailsViewArgs.bHideSelectionTip = true;
	}
	TSharedPtr<IDetailsView> DescriptorDetailView = EditModule.CreateDetailView(DetailsViewArgs);

	if ( !PluginWizardDefinition.IsValid() )
	{
		PluginWizardDefinition = MakeShared<FDefaultPluginWizardDefinition>(IsContentOnlyProject());
	}
	check(PluginWizardDefinition.IsValid());

	// Ensure that nothing is selected in the plugin wizard definition
	PluginWizardDefinition->ClearTemplateSelection();

	IPluginWizardDefinition* WizardDef = PluginWizardDefinition.Get();

	LastBrowsePath = AbsoluteGamePluginPath;
	PluginFolderPath = AbsoluteGamePluginPath;
	bIsPluginPathValid = true;

	const float PaddingAmount = FPluginStyle::Get()->GetFloat("PluginCreator.Padding");

	// Create the list view and ensure that it exists
	GenerateListViewWidget();
	check(ListView.IsValid());

	TSharedPtr<SWidget> HeaderWidget = PluginWizardDefinition->GetCustomHeaderWidget();
	FText PluginNameTextHint = PluginWizardDefinition->IsMod() ? LOCTEXT("ModNameTextHint", "Mod Name") : LOCTEXT("PluginNameTextHint", "Plugin Name");

	TSharedRef<SVerticalBox> MainContent = SNew(SVerticalBox)
	+SVerticalBox::Slot()
	.Padding(PaddingAmount)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		
		// Custom header widget display
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(PaddingAmount)
		[
			HeaderWidget.IsValid() ? HeaderWidget.ToSharedRef() : SNullWidget::NullWidget
		]
		
		// Instructions
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(PaddingAmount)
		.HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(PaddingAmount)
			.VAlign(VAlign_Center)
			.FillHeight(1.0f)
			[
				SNew(STextBlock)
				.Text(WizardDef, &IPluginWizardDefinition::GetInstructions)
				.AutoWrapText(true)
			]
		]
	]
	+SVerticalBox::Slot()
	.Padding(PaddingAmount)
	[
		// main list of plugins
		ListView.ToSharedRef()
	]
	+SVerticalBox::Slot()
	.AutoHeight()
	.Padding(PaddingAmount)
	.HAlign(HAlign_Center)
	[
		SAssignNew(FilePathBlock, SFilePathBlock)
		.OnBrowseForFolder(this, &SNewPluginWizard::OnBrowseButtonClicked)
		.LabelBackgroundBrush(FPluginStyle::Get()->GetBrush("PluginCreator.Background"))
		.LabelBackgroundColor(FLinearColor::White)
		.FolderPath(this, &SNewPluginWizard::GetPluginDestinationPath)
		.Name(this, &SNewPluginWizard::GetCurrentPluginName)
		.NameHint(PluginNameTextHint)
		.OnFolderChanged(this, &SNewPluginWizard::OnFolderPathTextChanged)
		.OnNameChanged(this, &SNewPluginWizard::OnPluginNameTextChanged)
        .ReadOnlyFolderPath( !PluginWizardDefinition->AllowsEnginePlugins() )	// only allow the user to select the folder if they can create engine plugins
	];

	// Add the descriptor data object if it exists
	if (DescriptorData.IsValid() && DescriptorDetailView.IsValid())
	{
		DescriptorDetailView->SetObject(DescriptorData.Get());

		MainContent->AddSlot()
		.AutoHeight()
		.Padding(PaddingAmount)
		[
			DescriptorDetailView.ToSharedRef()
		];
	}

	if (PluginWizardDefinition->AllowsEnginePlugins())
	{
		MainContent->AddSlot()
		.AutoHeight()
		.Padding(PaddingAmount)
		[
			SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SNewPluginWizard::OnEnginePluginCheckboxChanged)
				.IsChecked(this, &SNewPluginWizard::IsEnginePlugin)
				.ToolTipText(LOCTEXT("EnginePluginButtonToolTip", "Toggles whether this plugin will be created in the current project or the engine directory."))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnginePluginCheckbox", "Is Engine Plugin"))
				]
			]
		];
	}

	if (PluginWizardDefinition->CanShowOnStartup())
	{
		MainContent->AddSlot()
		.AutoHeight()
		.Padding(PaddingAmount)
		[
			SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(WizardDef, &IPluginWizardDefinition::OnShowOnStartupCheckboxChanged)
				.IsChecked(WizardDef, &IPluginWizardDefinition::GetShowOnStartupCheckBoxState)
				.ToolTipText(LOCTEXT("ShowOnStartupToolTip", "Toggles whether this wizard will show when the editor is launched."))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowOnStartupCheckbox", "Show on Startup"))
				]
			]
		];
	}

	// Checkbox to show the plugin's content directory when the plugin is created
	MainContent->AddSlot()
	.AutoHeight()
	.Padding(PaddingAmount)
	[
		SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ShowPluginContentDirectoryCheckBox, SCheckBox)
			.IsChecked(ECheckBoxState::Checked)
			.Visibility(this, &SNewPluginWizard::GetShowPluginContentDirectoryVisibility)
			.ToolTipText(LOCTEXT("ShowPluginContentDirectoryToolTip", "Shows the content directory after creation."))
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShowPluginContentDirectoryText", "Show Content Directory"))
			]
		]
	];

	FText CreateButtonLabel = PluginWizardDefinition->IsMod() ? LOCTEXT("CreateModButtonLabel", "Create Mod") : LOCTEXT("CreatePluginButtonLabel", "Create Plugin");

	MainContent->AddSlot()
	.AutoHeight()
	.Padding(5)
	.HAlign(HAlign_Right)
	[
		SNew(SButton)
		.ContentPadding(5)
		.TextStyle(FEditorStyle::Get(), "LargeText")
		.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
		.IsEnabled(this, &SNewPluginWizard::CanCreatePlugin)
		.HAlign(HAlign_Center)
		.Text(CreateButtonLabel)
		.OnClicked(this, &SNewPluginWizard::OnCreatePluginClicked)
	];

	ChildSlot
	[
		MainContent
	];
}

void SNewPluginWizard::GenerateListViewWidget()
{
	// for now, just determine what view to create based on the selection mode of the wizard definition
	ESelectionMode::Type SelectionMode = PluginWizardDefinition->GetSelectionMode();

	// Get the source of the templates to use for the list view
	const TArray<TSharedRef<FPluginTemplateDescription>>& TemplateSource = PluginWizardDefinition->GetTemplatesSource();

	switch (SelectionMode)
	{
		case ESelectionMode::Multi:
		{
			ListView = SNew(STileView<TSharedRef<FPluginTemplateDescription>>)
				.SelectionMode(SelectionMode)
				.ListItemsSource(&TemplateSource)
				.OnGenerateTile(this, &SNewPluginWizard::OnGenerateTemplateTile)
				.OnSelectionChanged(this, &SNewPluginWizard::OnTemplateSelectionChanged)
				.ItemHeight(180.0f);
		}
		break;

		case ESelectionMode::Single:
		case ESelectionMode::SingleToggle:
		{
			ListView = SNew(SListView<TSharedRef<FPluginTemplateDescription>>)
				.SelectionMode(SelectionMode)
				.ListItemsSource(&TemplateSource)
				.OnGenerateRow(this, &SNewPluginWizard::OnGenerateTemplateRow)
				.OnSelectionChanged(this, &SNewPluginWizard::OnTemplateSelectionChanged);
		}
		break;

		case ESelectionMode::None:
		default:
		{
			// This isn't a valid selection mode for this widget
			check(false);
		}
		break;
	}		
}

void SNewPluginWizard::GeneratePluginTemplateDynamicBrush(TSharedRef<FPluginTemplateDescription> InItem)
{
	if ( !InItem->PluginIconDynamicImageBrush.IsValid() )
	{
		// Plugin thumbnail image
		FString Icon128FilePath;
		PluginWizardDefinition->GetTemplateIconPath(InItem, Icon128FilePath);

		const FName BrushName(*Icon128FilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if ((Size.X > 0) && (Size.Y > 0))
		{
			InItem->PluginIconDynamicImageBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(Size.X, Size.Y)));
		}
	}
}

TSharedRef<ITableRow> SNewPluginWizard::OnGenerateTemplateTile(TSharedRef<FPluginTemplateDescription> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const float PaddingAmount = FPluginStyle::Get()->GetFloat("PluginTile.Padding");
	const float ThumbnailImageSize = FPluginStyle::Get()->GetFloat("PluginTile.ThumbnailImageSize");

	GeneratePluginTemplateDynamicBrush(InItem);

	return SNew(STableRow< TSharedRef<FPluginTemplateDescription> >, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(PaddingAmount)
			.ToolTipText(InItem->Description)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(PaddingAmount)
				[
					SNew(SVerticalBox)
					
					// Template thumbnail image
					+ SVerticalBox::Slot()
					.Padding(PaddingAmount)
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(ThumbnailImageSize)
						.HeightOverride(ThumbnailImageSize)
						[
							SNew(SImage)
							.Image(InItem->PluginIconDynamicImageBrush.IsValid() ? InItem->PluginIconDynamicImageBrush.Get() : nullptr)
						]
					]

					// Template name
					+ SVerticalBox::Slot()
					.Padding(PaddingAmount)
					.FillHeight(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.Padding(PaddingAmount)
						.HAlign(HAlign_Center)
						.FillWidth(1.0)
						[
							SNew(STextBlock)
							.Text(InItem->Name)
							.TextStyle(FPluginStyle::Get(), "PluginTile.DescriptionText")
							.AutoWrapText(true)
							.Justification(ETextJustify::Center)
						]
					]
				]
			]
		];
}

TSharedRef<ITableRow> SNewPluginWizard::OnGenerateTemplateRow(TSharedRef<FPluginTemplateDescription> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const float PaddingAmount = FPluginStyle::Get()->GetFloat("PluginTile.Padding");
	const float ThumbnailImageSize = FPluginStyle::Get()->GetFloat("PluginTile.ThumbnailImageSize");

	GeneratePluginTemplateDynamicBrush(InItem);

	return SNew(STableRow< TSharedRef<FPluginTemplateDescription> >, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(PaddingAmount)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(PaddingAmount)
				[
					SNew(SHorizontalBox)
					
					// Template thumbnail image
					+ SHorizontalBox::Slot()
					.Padding(PaddingAmount)
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(ThumbnailImageSize)
						.HeightOverride(ThumbnailImageSize)
						[
							SNew(SImage)
							.Image(InItem->PluginIconDynamicImageBrush.IsValid() ? InItem->PluginIconDynamicImageBrush.Get() : nullptr)
						]
					]

					// Template name and description
					+ SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(PaddingAmount)
						[
							SNew(STextBlock)
							.Text(InItem->Name)
							.TextStyle(FPluginStyle::Get(), "PluginTile.NameText")
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(PaddingAmount)
						[
							SNew(SRichTextBlock)
							.Text(InItem->Description)
							.TextStyle(FPluginStyle::Get(), "PluginTile.DescriptionText")
							.AutoWrapText(true)
						]
					]
				]
			]
		];
}


void SNewPluginWizard::OnTemplateSelectionChanged(TSharedPtr<FPluginTemplateDescription> InItem, ESelectInfo::Type SelectInfo)
{
	// Forward the set of selected items to the plugin wizard definition
	TArray<TSharedRef<FPluginTemplateDescription>> SelectedItems;

	if (ListView.IsValid())
	{
		SelectedItems = ListView->GetSelectedItems();
	}

	if (PluginWizardDefinition.IsValid())
	{
		PluginWizardDefinition->OnTemplateSelectionChanged(SelectedItems, SelectInfo);
	}
}

void SNewPluginWizard::OnFolderPathTextChanged(const FText& InText)
{
	PluginFolderPath = InText.ToString();
	FPaths::MakePlatformFilename(PluginFolderPath);
	ValidateFullPluginPath();
}

void SNewPluginWizard::OnPluginNameTextChanged(const FText& InText)
{
	PluginNameText = InText;
	ValidateFullPluginPath();
}

FReply SNewPluginWizard::OnBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		FString FolderName;
		const FString Title = LOCTEXT("NewPluginBrowseTitle", "Choose a plugin location").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			Title,
			LastBrowsePath,
			FolderName
			);

		if (bFolderSelected)
		{
			LastBrowsePath = FolderName;
			OnFolderPathTextChanged(FText::FromString(FolderName));
		}
	}

	return FReply::Handled();
}

void SNewPluginWizard::ValidateFullPluginPath()
{
	// Check for issues with path
	bIsPluginPathValid = false;
	bool bIsNewPathValid = true;
	FText FolderPathError;

	if (!FPaths::ValidatePath(GetPluginDestinationPath().ToString(), &FolderPathError))
	{
		bIsNewPathValid = false;
	}

	if (bIsNewPathValid)
	{
		bool bFoundValidPath = false;
		FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GetPluginDestinationPath().ToString());
		FPaths::MakePlatformFilename(AbsolutePath);

		if (AbsolutePath.StartsWith(AbsoluteGamePluginPath))
		{
			bFoundValidPath = true;
			bIsEnginePlugin = false;
		}
		else if (!bFoundValidPath && !FApp::IsEngineInstalled())
		{
			if (AbsolutePath.StartsWith(AbsoluteEnginePluginPath))
			{
				bFoundValidPath = true;
				bIsEnginePlugin = true;
			}
		}
		else
		{
			// This path will be added to the additional plugin directories for the project when created
		}
	}

	bIsPluginPathValid = bIsNewPathValid;
	FilePathBlock->SetFolderPathError(FolderPathError);

	// Check for issues with name
	bIsPluginNameValid = false;
	bool bIsNewNameValid = true;
	FText PluginNameError;

	// Fail silently if text is empty
	if (GetCurrentPluginName().IsEmpty())
	{
		bIsNewNameValid = false;
	}

	// Don't allow commas, dots, etc...
	FString IllegalCharacters;
	if (bIsNewNameValid && !GameProjectUtils::NameContainsOnlyLegalCharacters(GetCurrentPluginName().ToString(), IllegalCharacters))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("IllegalCharacters"), FText::FromString(IllegalCharacters));
		PluginNameError = FText::Format(LOCTEXT("WrongPluginNameErrorText", "Plugin name cannot contain illegal characters like: \"{IllegalCharacters}\""), Args);
		bIsNewNameValid = false;
	}

	// Fail if name doesn't begin with alphabetic character.
	if (bIsNewNameValid && !FChar::IsAlpha(GetCurrentPluginName().ToString()[0]))
	{
		PluginNameError = LOCTEXT("PluginNameMustBeginWithACharacter", "Plugin names must begin with an alphabetic character.");
		bIsNewNameValid = false;
	}

	if (bIsNewNameValid)
	{
		const FString& TestPluginName = GetCurrentPluginName().ToString();

		// Check to see if a a compiled plugin with this name exists (at any path)
		const TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetDiscoveredPlugins();
		for (const TSharedRef<IPlugin>& Plugin : Plugins)
		{
			if (Plugin->GetName() == TestPluginName)
			{
				PluginNameError = LOCTEXT("PluginNameExistsErrorText", "A plugin with this name already exists!");
				bIsNewNameValid = false;
				break;
			}
		}
	}

	// Check to see if a .uplugin exists at this path (in case there is an uncompiled or disabled plugin)
	if (bIsNewNameValid)
	{
		const FString TestPluginPath = GetPluginFilenameWithPath();
		if (!TestPluginPath.IsEmpty())
		{
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*TestPluginPath))
			{
				PluginNameError = LOCTEXT("PluginPathExistsErrorText", "A plugin already exists at this path!");
				bIsNewNameValid = false;
			}
		}
	}

	bIsPluginNameValid = bIsNewNameValid;
	FilePathBlock->SetNameError(PluginNameError);
}

bool SNewPluginWizard::CanCreatePlugin() const
{
	return bIsPluginPathValid && bIsPluginNameValid && PluginWizardDefinition->HasValidTemplateSelection();
}

FText SNewPluginWizard::GetPluginDestinationPath() const
{
	return FText::FromString(PluginFolderPath);
}

FText SNewPluginWizard::GetCurrentPluginName() const
{
	return PluginNameText;
}

FString SNewPluginWizard::GetPluginFilenameWithPath() const
{
	if (PluginFolderPath.IsEmpty() || PluginNameText.IsEmpty())
	{
		// Don't even try to assemble the path or else it may be relative to the binaries folder!
		return TEXT("");
	}
	else
	{
		const FString& TestPluginName = PluginNameText.ToString();
		FString TestPluginPath = PluginFolderPath / TestPluginName / (TestPluginName + TEXT(".uplugin"));
		FPaths::MakePlatformFilename(TestPluginPath);
		return TestPluginPath;
	}
}

ECheckBoxState SNewPluginWizard::IsEnginePlugin() const
{
	return bIsEnginePlugin ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewPluginWizard::OnEnginePluginCheckboxChanged(ECheckBoxState NewCheckedState)
{
	bool bNewEnginePluginState = NewCheckedState == ECheckBoxState::Checked;
	if (bIsEnginePlugin != bNewEnginePluginState)
	{
		bIsEnginePlugin = bNewEnginePluginState;
		if (bIsEnginePlugin)
		{
			PluginFolderPath = AbsoluteEnginePluginPath;
		}
		else
		{
			PluginFolderPath = AbsoluteGamePluginPath;
		}
		bIsPluginPathValid = true;
		FilePathBlock->SetFolderPathError(FText::GetEmpty());
	}
}

FReply SNewPluginWizard::OnCreatePluginClicked()
{
	const FString AutoPluginName = PluginNameText.ToString();

	// Plugin thumbnail image
	FString PluginEditorIconPath;
	bool bRequiresDefaultIcon = PluginWizardDefinition->GetPluginIconPath(PluginEditorIconPath);

	TArray<FString> CreatedFiles;

	FText LocalFailReason;

	bool bSucceeded = true;

	const bool bHasModules = PluginWizardDefinition->HasModules();

	// Save descriptor file as .uplugin file
	const FString UPluginFilePath = GetPluginFilenameWithPath();

	// Define additional parameters to write out the plugin descriptor
	FWriteDescriptorParams DescriptorParams;
	{
		DescriptorParams.bCanContainContent = PluginWizardDefinition->CanContainContent();
		DescriptorParams.bHasModules = bHasModules;
		DescriptorParams.ModuleDescriptorType = PluginWizardDefinition->GetPluginModuleDescriptor();
		DescriptorParams.LoadingPhase = PluginWizardDefinition->GetPluginLoadingPhase();
	}

	const FString PluginModuleName = AutoPluginName;
	bSucceeded = bSucceeded && WritePluginDescriptor(PluginModuleName, UPluginFilePath, DescriptorParams);

	// Main plugin dir
	const FString BasePluginFolder = GetPluginDestinationPath().ToString();
	const FString PluginFolder = BasePluginFolder / AutoPluginName;

	// Resource folder
	const FString ResourcesFolder = PluginFolder / TEXT("Resources");

	if (bRequiresDefaultIcon)
	{
		// Copy the icon
		bSucceeded = bSucceeded && CopyFile(ResourcesFolder / TEXT("Icon128.png"), PluginEditorIconPath, /*inout*/ CreatedFiles);
	}

	TArray<FString> TemplateFolders = PluginWizardDefinition->GetFoldersForSelection();
	if (TemplateFolders.Num() == 0)
	{
		PopErrorNotification(LOCTEXT("FailedTemplateCopy_NoFolders", "No templates were selected to create the plugin"));
		bSucceeded = false;
	}

	GWarn->BeginSlowTask(LOCTEXT("CopyingData", "Copying data..."), true, false);
	for (FString TemplateFolderName : TemplateFolders)
	{
		if (bSucceeded && !FPluginHelpers::CopyPluginTemplateFolder(*PluginFolder, *TemplateFolderName, AutoPluginName))
		{
			PopErrorNotification(FText::Format(LOCTEXT("FailedTemplateCopy", "Failed to copy plugin Template: {0}"), FText::FromString(TemplateFolderName)));
			bSucceeded = false;
			break;
		}
	}
	GWarn->EndSlowTask();

	// If it contains code, we need the user to restart to enable it. Otherwise, we can just mount it now.
	if (bSucceeded && bHasModules)
	{
		FString ProjectFileName = FPaths::GetProjectFilePath();
		FString Arguments = FString::Printf(TEXT("%sEditor %s %s -EditorRecompile -Module %s -Project=\"%s\" -Plugin \"%s\" -Progress -NoHotReloadFromIDE"), *FPaths::GetBaseFilename(ProjectFileName), FModuleManager::Get().GetUBTConfiguration(), FPlatformMisc::GetUBTPlatform(), *PluginModuleName, *ProjectFileName, *UPluginFilePath);
		if (!FDesktopPlatformModule::Get()->RunUnrealBuildTool(LOCTEXT("Compiling", "Compiling..."), FPaths::RootDir(), Arguments, GWarn))
		{
			PopErrorNotification(LOCTEXT("FailedToCompile", "Failed to compile source code."));
			bSucceeded = false;
		}

		// Generate project files if we happen to be using a project file.
		if (bSucceeded && !FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
		{
			PopErrorNotification(LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files."));
			bSucceeded = false;
		}
	}

	if (bSucceeded)
	{
		// Notify that a new plugin has been created
		FPluginBrowserModule& PluginBrowserModule = FPluginBrowserModule::Get();
		PluginBrowserModule.BroadcastNewPluginCreated();

		// Enable game plugins immediately
		if (!bIsEnginePlugin)
		{
			// If this path isn't in the Engine/Plugins dir and isn't in Project/Plugins dir,
			// add the directory to the list of ones we additionally scan
			
			// There have been issues with ProjectDir can be relative and BasePluginFolder absolute, causing our
			// tests to fail below. We now normalize on absolute paths prior to performing the check to ensure
			// that we don't add the folder to the additional plugin directory unnecessarily (which can result in build failures).
			FString ProjectDirFull = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			FString BasePluginFolderFull = FPaths::ConvertRelativePathToFull(BasePluginFolder);
			if (!BasePluginFolderFull.StartsWith(ProjectDirFull))
			{
				GameProjectUtils::UpdateAdditionalPluginDirectory(BasePluginFolderFull, true);
			}
		}

		// Update the list of known plugins
		IPluginManager::Get().RefreshPluginsList();

		// Enable this plugin in the project, if necessary
		FText FailReason;
		if (!IProjectManager::Get().SetPluginEnabled(AutoPluginName, true, FailReason))
		{
			PopErrorNotification(FText::Format(LOCTEXT("FailedToEnablePlugin", "Couldn't enable plugin: {0}"), FailReason));
			bSucceeded = false;
		}

		// Mount the plugin
		if (bSucceeded)
		{
			GWarn->BeginSlowTask(LOCTEXT("MountingFiles", "Mounting files..."), true, false);
			IPluginManager::Get().MountNewlyCreatedPlugin(AutoPluginName);
			GWarn->EndSlowTask();
		}
	}

	// Set the content browser to show the plugin's content directory
	if (bSucceeded && PluginWizardDefinition->CanContainContent() && ShowPluginContentDirectoryCheckBox->IsChecked())
	{
		TArray<FString> SelectedDirectories;
		SelectedDirectories.Add(TEXT("/") + AutoPluginName);

		const bool bContentBrowserNeedsRefresh = true;

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

		ContentBrowser.ForceShowPluginContent( bIsEnginePlugin );
		ContentBrowser.SetSelectedPaths(SelectedDirectories, bContentBrowserNeedsRefresh);
	}

	if (bSucceeded && PluginWizardDefinition->CanContainContent())
	{
		GWarn->BeginSlowTask(LOCTEXT("LoadingContent", "Loading Content..."), true, false);
		// Attempt to fix any content that was added by the plugin
		bSucceeded = FPluginHelpers::FixupPluginTemplateAssets(AutoPluginName);
		GWarn->EndSlowTask();
	}

	PluginWizardDefinition->PluginCreated(AutoPluginName, bSucceeded);
	// Trigger the plugin manager to mount the new plugin, or delete the partially created plugin and abort
	if (bSucceeded)
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("PluginCreatedSuccessfully", "'{0}' was created successfully."), FText::FromString(AutoPluginName)));
		Info.bUseThrobber = false;
		Info.ExpireDuration = 8.0f;
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);

		OwnerTab.Pin()->RequestCloseTab();

		if (bHasModules)
		{
			FSourceCodeNavigation::OpenModuleSolution();
		}

		return FReply::Handled();
	}
	else
	{
		DeletePluginDirectory(*PluginFolder);
		return FReply::Unhandled();
	}
}

bool SNewPluginWizard::CopyFile(const FString& DestinationFile, const FString& SourceFile, TArray<FString>& InOutCreatedFiles)
{
	if (IFileManager::Get().Copy(*DestinationFile, *SourceFile, /*bReplaceExisting=*/ false) != COPY_OK)
	{
		const FText ErrorMessage = FText::Format(LOCTEXT("ErrorCopyingFile", "Error: Couldn't copy file '{0}' to '{1}'"), FText::AsCultureInvariant(SourceFile), FText::AsCultureInvariant(DestinationFile));
		PopErrorNotification(ErrorMessage);
		return false;
	}
	else
	{
		InOutCreatedFiles.Add(DestinationFile);
		return true;
	}
}

bool SNewPluginWizard::WritePluginDescriptor(const FString& PluginModuleName, const FString& UPluginFilePath, const FWriteDescriptorParams& Params)
{
	FPluginDescriptor Descriptor;

	Descriptor.FriendlyName = PluginModuleName;
	Descriptor.Version = 1;
	Descriptor.VersionName = TEXT("1.0");
	Descriptor.Category = TEXT("Other");

	if (DescriptorData.IsValid())
	{
		Descriptor.CreatedBy = DescriptorData->CreatedBy;
		Descriptor.CreatedByURL = DescriptorData->CreatedByURL;
		Descriptor.Description = DescriptorData->Description;
		Descriptor.bIsBetaVersion = DescriptorData->bIsBetaVersion;
	}

	if (Params.bHasModules)
	{
		Descriptor.Modules.Add(FModuleDescriptor(*PluginModuleName, Params.ModuleDescriptorType, Params.LoadingPhase));
	}
	Descriptor.bCanContainContent = Params.bCanContainContent;

	// Save the descriptor using JSon
	FText FailReason;
	if (!Descriptor.Save(UPluginFilePath, FailReason))
	{
		PopErrorNotification(FText::Format(LOCTEXT("FailedToWriteDescriptor", "Couldn't save plugin descriptor under %s"), FText::AsCultureInvariant(UPluginFilePath)));
		return false;
	}

	return true;
}

void SNewPluginWizard::PopErrorNotification(const FText& ErrorMessage)
{
	UE_LOG(LogPluginWizard, Log, TEXT("%s"), *ErrorMessage.ToString());

	// Create and display a notification about the failure
	FNotificationInfo Info(ErrorMessage);
	Info.ExpireDuration = 2.0f;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
}

void SNewPluginWizard::DeletePluginDirectory(const FString& InPath)
{
	IFileManager::Get().DeleteDirectory(*InPath, false, true);
}

EVisibility SNewPluginWizard::GetShowPluginContentDirectoryVisibility() const
{
	EVisibility ShowContentVisibility = EVisibility::Collapsed;

	if (PluginWizardDefinition->CanContainContent())
	{
		ShowContentVisibility = EVisibility::Visible;
	}

	return ShowContentVisibility;
}

#undef LOCTEXT_NAMESPACE