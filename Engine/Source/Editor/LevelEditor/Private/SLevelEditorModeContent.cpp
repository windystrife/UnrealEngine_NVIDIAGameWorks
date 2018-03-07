// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SLevelEditorModeContent.h"
#include "EdMode.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "EditorModeManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SLevelEditorModeContent"

SLevelEditorModeContent::~SLevelEditorModeContent()
{
	GLevelEditorModeTools().OnEditorModeChanged().RemoveAll( this );
	GetMutableDefault<UEditorPerProjectUserSettings>()->OnUserSettingChanged().RemoveAll( this );
}

void SLevelEditorModeContent::Construct( const FArguments& InArgs, const TSharedRef< class ILevelEditor >& InOwningLevelEditor, const TSharedRef< class SDockTab >& InOwningDocTab, FEdMode* InEditorMode )
{
	LevelEditor = InOwningLevelEditor;
	DocTab = InOwningDocTab;
	EditorMode = InEditorMode;

	InOwningDocTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateSP(this, &SLevelEditorModeContent::HandleParentClosed ) );
	GLevelEditorModeTools().OnEditorModeChanged().AddSP( this, &SLevelEditorModeContent::HandleEditorModeChanged );
	GetMutableDefault<UEditorPerProjectUserSettings>()->OnUserSettingChanged().AddSP( this, &SLevelEditorModeContent::HandleUserSettingsChange );

	ChildSlot
	[
		SNew( SHorizontalBox )

		// The Current Creation Tool
		+ SHorizontalBox::Slot()
		.FillWidth( 1.0 )
		.Padding( 2, 0, 0, 0 )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			[
				SAssignNew(InlineContentHolder, SBorder)
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				.Padding(0.f)
				.Visibility( this, &SLevelEditorModeContent::GetInlineContentHolderVisibility )
			]
		]
	];

	UpdateModeToolBar();
}

void SLevelEditorModeContent::HandleEditorModeChanged( FEdMode* Mode, bool IsEnabled )
{
	if ( Mode == EditorMode && !IsEnabled )
	{
		DocTab.Pin()->SetOnTabClosed( SDockTab::FOnTabClosedCallback() );
		DocTab.Pin()->RequestCloseTab();
	}
}

void SLevelEditorModeContent::HandleUserSettingsChange( FName PropertyName )
{
	UpdateModeToolBar();
}

void SLevelEditorModeContent::UpdateModeToolBar()
{
	const TArray< TSharedPtr< IToolkit > >& HostedToolkits = LevelEditor.Pin()->GetHostedToolkits();
	for( auto HostedToolkitIt = HostedToolkits.CreateConstIterator(); HostedToolkitIt; ++HostedToolkitIt )
	{
		UpdateInlineContent( ( *HostedToolkitIt )->GetInlineContent() );
		break;
	}
}

EVisibility SLevelEditorModeContent::GetInlineContentHolderVisibility() const
{
	return InlineContentHolder->GetContent() == SNullWidget::NullWidget ? EVisibility::Collapsed : EVisibility::Visible;
}

void SLevelEditorModeContent::UpdateInlineContent(TSharedPtr<SWidget> InlineContent) const
{
	if (InlineContent.IsValid() && InlineContentHolder.IsValid())
	{
		InlineContentHolder->SetContent(InlineContent.ToSharedRef());
	}
}

void SLevelEditorModeContent::OnToolkitHostingStarted( const TSharedRef< class IToolkit >& Toolkit )
{
	UpdateInlineContent( Toolkit->GetInlineContent() );
}

void SLevelEditorModeContent::OnToolkitHostingFinished( const TSharedRef< class IToolkit >& Toolkit )
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

void SLevelEditorModeContent::HandleParentClosed( TSharedRef<SDockTab> TabBeingClosed )
{
	if ( GLevelEditorModeTools().IsModeActive(EditorMode->GetID()) )
	{
		GLevelEditorModeTools().DeactivateMode(EditorMode->GetID());
	}
}

#undef LOCTEXT_NAMESPACE
