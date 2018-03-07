// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SBlastMeshEditorViewportToolBar.h"
#include "SBlastMeshEditorViewport.h"
#include "BlastMesh.h"
#include "BlastMeshEditorCommands.h"
#include "EditorViewportCommands.h"
#include "STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"

//static const float	AnimationEditorViewport_LightRotSpeed = 0.22f;

#define LOCTEXT_NAMESPACE "BlastMeshEditorViewportToolBar"

///////////////////////////////////////////////////////////
// SBlastVectorViewportToolBar


void SBlastVectorViewportToolBar::Construct(const FArguments& InArgs)
{
	Viewport = InArgs._Viewport;
	CommandList = InArgs._CommandList;
	
	ChildSlot
	[
		MakeTransformToolBar(InArgs._Extenders)
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SBlastVectorViewportToolBar::MakeTransformToolBar(const TSharedPtr<FExtender> InExtenders)
{
	FToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "ViewportMenu";
	ToolbarBuilder.SetStyle(&FEditorStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls cannot be focusable as it fights with the press space to change transform mode feature
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection("BlastVector");
	ToolbarBuilder.BeginBlockGroup();
	{
		// Normal selection Mode
		static FName NormalName = FName(TEXT("NormalSelectionMode"));
		ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().BlastVectorNormal, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), NormalName);

		// Point selection Mode
		static FName PointName = FName(TEXT("PointSelectionMode"));
		ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().BlastVectorPoint, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), PointName);

		// Two Point selection Mode
		static FName TwoPointName = FName(TEXT("TwoPointSelectionMode"));
		ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().BlastVectorTwoPoint, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TwoPointName);

		// Three Point selection Mode
		static FName ThreePointName = FName(TEXT("ThreePointSelectionMode"));
		ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().BlastVectorThreePoint, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ThreePointName);
		
		ToolbarBuilder.AddSeparator();

		// Exit selection Mode
		static FName ExitName = FName(TEXT("ExitSelectionMode"));
		ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().BlastVectorExit, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ExitName);
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}


///////////////////////////////////////////////////////////
// SBlastMeshEditorViewportToolbar

void SBlastMeshEditorViewportToolbar::Construct(const FArguments& InArgs, TWeakPtr<SBlastMeshEditorViewport> InViewport)
{
	Viewport = InViewport;
	auto VP = InViewport.Pin();

	static const FName DefaultForegroundName("DefaultForeground");

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity(this, &SViewportToolBar::OnGetColorAndOpacity)
		.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
		[
			SNew(SHorizontalBox)

			// Options menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Image("EditorViewportToolBar.MenuDropdown")
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
				.OnGetMenuContent(this, &SBlastMeshEditorViewportToolbar::GenerateOptionsMenu)
			]

			// Camera mode menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Label(this, &SBlastMeshEditorViewportToolbar::GetCameraMenuLabel)
				.LabelIcon(this, &SBlastMeshEditorViewportToolbar::GetCameraMenuLabelIcon)
				.OnGetMenuContent(this, &SBlastMeshEditorViewportToolbar::GenerateCameraMenu)
			]

			// View menu (lit, unlit, etc...)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportViewMenu, VP.ToSharedRef(), SharedThis(this))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Label(NSLOCTEXT("PhAT", "ShowMenuTitle_Default", "Show"))
				.OnGetMenuContent(this, &SBlastMeshEditorViewportToolbar::GenerateShowMenu)
			]

			// Transform toolbar
			+SHorizontalBox::Slot()
			.Padding(3.0f, 1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SBlastVectorViewportToolBar)
				.Viewport(VP)
				.CommandList(VP->GetCommandList())
				//.Extenders(GetInfoProvider().GetExtenders())
				.Visibility(VP.ToSharedRef(), &SEditorViewport::GetTransformToolbarVisibility)
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SBlastMeshEditorViewportToolbar::GenerateShowMenu() const
{
	TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		ShowMenuBuilder.AddMenuEntry(FBlastMeshEditorCommands::Get().ToggleFractureVisualization);
		ShowMenuBuilder.AddMenuSeparator();
		ShowMenuBuilder.AddMenuEntry(FBlastMeshEditorCommands::Get().ToggleAABBView);
		ShowMenuBuilder.AddMenuEntry(FBlastMeshEditorCommands::Get().ToggleCollisionMeshView);
		ShowMenuBuilder.AddMenuEntry(FBlastMeshEditorCommands::Get().ToggleVoronoiSitesView);
	}

	return ShowMenuBuilder.MakeWidget();
}

FText SBlastMeshEditorViewportToolbar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("Viewport_Default", "Camera");
	TSharedPtr<SBlastMeshEditorViewport> EditorViewport = Viewport.Pin();
	if (EditorViewport.IsValid())
	{
		switch (EditorViewport->GetViewportClient()->GetViewportType())
		{
		case LVT_Perspective:
			Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
			break;

		case LVT_OrthoXY:
			Label = LOCTEXT("CameraMenuTitle_Top", "Top");
			break;

		case LVT_OrthoYZ:
			Label = LOCTEXT("CameraMenuTitle_Left", "Left");
			break;

		case LVT_OrthoXZ:
			Label = LOCTEXT("CameraMenuTitle_Front", "Front");
			break;

		case LVT_OrthoNegativeXY:
			Label = LOCTEXT("CameraMenuTitle_Bottom", "Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Label = LOCTEXT("CameraMenuTitle_Right", "Right");
			break;

		case LVT_OrthoNegativeXZ:
			Label = LOCTEXT("CameraMenuTitle_Back", "Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return Label;
}

const FSlateBrush* SBlastMeshEditorViewportToolbar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr<SBlastMeshEditorViewport> EditorViewport = Viewport.Pin();
	if (EditorViewport.IsValid())
	{
		switch (EditorViewport->GetViewportClient()->GetViewportType())
		{
		case LVT_Perspective:
			Icon = FName("EditorViewport.Perspective");
			break;

		case LVT_OrthoXY:
			Icon = FName("EditorViewport.Top");
			break;

		case LVT_OrthoYZ:
			Icon = FName("EditorViewport.Left");
			break;

		case LVT_OrthoXZ:
			Icon = FName("EditorViewport.Front");
			break;

		case LVT_OrthoNegativeXY:
			Icon = FName("EditorViewport.Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Icon = FName("EditorViewport.Right");
			break;

		case LVT_OrthoNegativeXZ:
			Icon = FName("EditorViewport.Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return FEditorStyle::GetBrush(Icon);
}

TSharedRef<SWidget> SBlastMeshEditorViewportToolbar::GenerateCameraMenu() const
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());

	// Camera types
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SBlastMeshEditorViewportToolbar::GenerateOptionsMenu() const
{
	const bool bIsPerspective = Viewport.Pin()->GetViewportClient()->IsPerspective();
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
		OptionsMenuBuilder.BeginSection("LevelViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options"));
		{
			if (bIsPerspective)
			{
				OptionsMenuBuilder.AddWidget(GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)"));
			}
		}
		OptionsMenuBuilder.EndSection();
	}

	return OptionsMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SBlastMeshEditorViewportToolbar::GenerateFOVMenu() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SSpinBox<float>)
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.Value(this, &SBlastMeshEditorViewportToolbar::OnGetFOVValue)
				.OnValueChanged(this, &SBlastMeshEditorViewportToolbar::OnFOVValueChanged)
			]
		];
}

float SBlastMeshEditorViewportToolbar::OnGetFOVValue() const
{
	return Viewport.Pin()->GetViewportClient()->ViewFOV;
}

void SBlastMeshEditorViewportToolbar::OnFOVValueChanged(float NewValue)
{
	bool bUpdateStoredFOV = true;
	FEditorViewportClient* ViewportClient = Viewport.Pin()->GetViewportClient().Get();
	ViewportClient->FOVAngle = NewValue;
	ViewportClient->ViewFOV = NewValue;
	ViewportClient->Invalidate();
}

