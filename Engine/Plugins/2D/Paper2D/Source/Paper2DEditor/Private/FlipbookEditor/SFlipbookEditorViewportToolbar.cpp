// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FlipbookEditor/SFlipbookEditorViewportToolbar.h"
#include "SEditorViewport.h"
#include "FlipbookEditor/FlipbookEditorCommands.h"

#define LOCTEXT_NAMESPACE "SFlipbookEditorViewportToolbar"

///////////////////////////////////////////////////////////
// SFlipbookEditorViewportToolbar

void SFlipbookEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

TSharedRef<SWidget> SFlipbookEditorViewportToolbar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		ShowMenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().SetShowSockets);
		ShowMenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().SetShowPivot);

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().SetShowGrid);
		ShowMenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().SetShowBounds);

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(FFlipbookEditorCommands::Get().SetShowCollision);
	}

	return ShowMenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
