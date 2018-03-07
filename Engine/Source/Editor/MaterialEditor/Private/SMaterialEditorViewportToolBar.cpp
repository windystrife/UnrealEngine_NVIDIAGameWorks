// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMaterialEditorViewportToolBar.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "MaterialEditorActions.h"

#define LOCTEXT_NAMESPACE "MaterialEditorViewportToolBar"

///////////////////////////////////////////////////////////
// SMaterialEditorViewportPreviewShapeToolBar

void SMaterialEditorViewportPreviewShapeToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SMaterialEditor3DPreviewViewport> InViewport)
{
	// Force this toolbar to have small icons, as the preview panel is only small so we have limited space
	const bool bForceSmallIcons = true;
	FToolBarBuilder ToolbarBuilder(InViewport->GetCommandList(), FMultiBoxCustomization::None, nullptr, Orient_Horizontal, bForceSmallIcons);

	// Use a custom style
	ToolbarBuilder.SetStyle(&FEditorStyle::Get(), "ViewportMenu");
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetIsFocusable(false);
	
	ToolbarBuilder.BeginSection("Preview");
	{
		ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().SetCylinderPreview);
		ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().SetSpherePreview);
		ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().SetPlanePreview);
		ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().SetCubePreview);
		ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().SetPreviewMeshFromSelection);
	}
	ToolbarBuilder.EndSection();

	static const FName DefaultForegroundName("DefaultForeground");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity(this, &SViewportToolBar::OnGetColorAndOpacity)
		.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
		.HAlign(HAlign_Right)
		[
			ToolbarBuilder.MakeWidget()
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

///////////////////////////////////////////////////////////
// SMaterialEditorViewportToolBar

void SMaterialEditorViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SMaterialEditor3DPreviewViewport> InViewport)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InViewport);
}

TSharedRef<SWidget> SMaterialEditorViewportToolBar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		auto Commands = FMaterialEditorCommands::Get();

		ShowMenuBuilder.AddMenuEntry(Commands.ToggleMaterialStats);
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleMobileStats);

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(Commands.TogglePreviewGrid);
		ShowMenuBuilder.AddMenuEntry(Commands.TogglePreviewBackground);
	}

	return ShowMenuBuilder.MakeWidget();
}

bool SMaterialEditorViewportToolBar::IsViewModeSupported(EViewModeIndex ViewModeIndex) const 
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
