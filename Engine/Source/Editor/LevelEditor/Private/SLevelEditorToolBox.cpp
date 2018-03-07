// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SLevelEditorToolBox.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/InputBindingManager.h"
#include "EditorModeRegistry.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "LevelEditorModesActions.h"

#define LOCTEXT_NAMESPACE "SLevelEditorToolBox"

SLevelEditorToolBox::~SLevelEditorToolBox()
{
	GetMutableDefault<UEditorPerProjectUserSettings>()->OnUserSettingChanged().RemoveAll( this );
}

void SLevelEditorToolBox::Construct( const FArguments& InArgs, const TSharedRef< class ILevelEditor >& OwningLevelEditor )
{
	LevelEditor = OwningLevelEditor;

	// Important: We use a raw binding here because we are releasing our binding in our destructor (where a weak pointer would be invalid)
	// It's imperative that our delegate is removed in the destructor for the level editor module to play nicely with reloading.

	GetMutableDefault<UEditorPerProjectUserSettings>()->OnUserSettingChanged().AddRaw( this, &SLevelEditorToolBox::HandleUserSettingsChange );

	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign( HAlign_Left )
		.Padding(0, 0, 0, 0)
		[
			SAssignNew( ModeToolBarContainer, SBorder )
			.BorderImage( FEditorStyle::GetBrush( "NoBorder" ) )
			.Padding( FMargin(4, 0, 0, 0) )
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )
		.Padding( 2, 0, 0, 0 )
		[
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			[
				SAssignNew(InlineContentHolder, SBorder)
				.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
				.Padding(0.f)
				.Visibility( this, &SLevelEditorToolBox::GetInlineContentHolderVisibility )
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign( HAlign_Center )
			.Padding( 2.0f, 14.0f, 2.0f, 2.0f )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NoToolSelected", "Select a tool to display its options.") )
				.Visibility( this, &SLevelEditorToolBox::GetNoToolSelectedTextVisibility )
			]
		]
	];

	UpdateModeToolBar();
}

void SLevelEditorToolBox::HandleUserSettingsChange( FName PropertyName )
{
	UpdateModeToolBar();
}

void SLevelEditorToolBox::OnEditorModeCommandsChanged()
{
	UpdateModeToolBar();
}

void SLevelEditorToolBox::UpdateModeToolBar()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor");
	const TSharedPtr< const FUICommandList > CommandList = LevelEditorModule.GetGlobalLevelEditorActions();
	const TSharedPtr<FExtender> ModeBarExtenders = LevelEditorModule.GetModeBarExtensibilityManager()->GetAllExtenders();

	FToolBarBuilder EditorModeTools( CommandList, FMultiBoxCustomization::None, ModeBarExtenders );
	{
		EditorModeTools.SetStyle(&FEditorStyle::Get(), "EditorModesToolbar");
		EditorModeTools.SetLabelVisibility( EVisibility::Collapsed );

		const FLevelEditorModesCommands& Commands = LevelEditorModule.GetLevelEditorModesCommands();

		for ( const FEditorModeInfo& Mode : FEditorModeRegistry::Get().GetSortedModeInfo() )
		{
			// If the mode isn't visible don't create a menu option for it.
			if ( !Mode.bVisible )
			{
				continue;
			}

			FName EditorModeCommandName = FName( *( FString( "EditorMode." ) + Mode.ID.ToString() ) );

			TSharedPtr<FUICommandInfo> EditorModeCommand =
				FInputBindingManager::Get().FindCommandInContext( Commands.GetContextName(), EditorModeCommandName );

			// If a command isn't yet registered for this mode, we need to register one.
			if ( !EditorModeCommand.IsValid() )
			{
				continue;
			}

			const FUIAction* UIAction = EditorModeTools.GetTopCommandList()->GetActionForCommand( EditorModeCommand );
			if ( ensure( UIAction ) )
			{
				EditorModeTools.AddToolBarButton( EditorModeCommand, Mode.ID, Mode.Name, Mode.Name, Mode.IconBrush, Mode.ID );// , EUserInterfaceActionType::ToggleButton );
			}
		}
	}

	ModeToolBarContainer->SetContent( EditorModeTools.MakeWidget() );

	const TArray< TSharedPtr< IToolkit > >& HostedToolkits = LevelEditor.Pin()->GetHostedToolkits();
	for( auto HostedToolkitIt = HostedToolkits.CreateConstIterator(); HostedToolkitIt; ++HostedToolkitIt )
	{
		UpdateInlineContent( ( *HostedToolkitIt )->GetInlineContent() );
		break;
	}
}

EVisibility SLevelEditorToolBox::GetInlineContentHolderVisibility() const
{
	return InlineContentHolder->GetContent() == SNullWidget::NullWidget ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SLevelEditorToolBox::GetNoToolSelectedTextVisibility() const
{
	return InlineContentHolder->GetContent() == SNullWidget::NullWidget ? EVisibility::Visible : EVisibility::Collapsed;
}

void SLevelEditorToolBox::UpdateInlineContent(TSharedPtr<SWidget> InlineContent) const
{
	if (InlineContent.IsValid() && InlineContentHolder.IsValid())
	{
		InlineContentHolder->SetContent(InlineContent.ToSharedRef());
	}
}

void SLevelEditorToolBox::OnToolkitHostingStarted( const TSharedRef< class IToolkit >& Toolkit )
{
	UpdateInlineContent( Toolkit->GetInlineContent() );
}

void SLevelEditorToolBox::OnToolkitHostingFinished( const TSharedRef< class IToolkit >& Toolkit )
{
	bool FoundAnotherToolkit = false;
	const TArray< TSharedPtr< IToolkit > >& HostedToolkits = LevelEditor.Pin()->GetHostedToolkits();
	for( auto HostedToolkitIt = HostedToolkits.CreateConstIterator(); HostedToolkitIt; ++HostedToolkitIt )
	{
		if ( ( *HostedToolkitIt ) != Toolkit )
		{
			UpdateInlineContent( ( *HostedToolkitIt )->GetInlineContent() );
			FoundAnotherToolkit = true;
			break;
		}
	}

	if ( !FoundAnotherToolkit )
	{
		UpdateInlineContent( SNullWidget::NullWidget );
	}
}

FSlateIcon SLevelEditorToolBox::GetEditorModeIcon(TSharedPtr< FUICommandInfo > EditorModeUICommand, FEditorModeID EditorMode )
{
	const FSlateIcon& Icon = EditorModeUICommand->GetIcon();

	FName IconName = Icon.GetStyleName();
	if( FLevelEditorActionCallbacks::IsEditorModeActive(EditorMode) )
	{
		IconName = FEditorStyle::Join(IconName, ".Selected");
	}

	return FSlateIcon(Icon.GetStyleSetName(), IconName);
}

#undef LOCTEXT_NAMESPACE
