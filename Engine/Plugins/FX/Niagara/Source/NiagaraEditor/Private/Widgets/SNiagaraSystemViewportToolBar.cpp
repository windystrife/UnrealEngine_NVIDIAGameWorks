// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSystemViewportToolBar.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "NiagaraEditorCommands.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemViewportToolBar"


///////////////////////////////////////////////////////////
// SNiagaraSystemViewportToolBar

void SNiagaraSystemViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SNiagaraSystemViewport> InViewport)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InViewport);
}

TSharedRef<SWidget> SNiagaraSystemViewportToolBar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		auto Commands = FNiagaraEditorCommands::Get();

		//ShowMenuBuilder.AddMenuEntry(Commands.ToggleMaterialStats);
		//ShowMenuBuilder.AddMenuEntry(Commands.ToggleMobileStats);

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(Commands.TogglePreviewGrid);
		//ShowMenuBuilder.AddMenuEntry(Commands.TogglePreviewBackground);
	}

	return ShowMenuBuilder.MakeWidget();
}

bool SNiagaraSystemViewportToolBar::IsViewModeSupported(EViewModeIndex ViewModeIndex) const 
{
	switch (ViewModeIndex)
	{
	case VMI_PrimitiveDistanceAccuracy:
	case VMI_MeshUVDensityAccuracy:
	case VMI_RequiredTextureResolution:
		return false;
	default:
		return true;
	}
	return true; 
}

#undef LOCTEXT_NAMESPACE
