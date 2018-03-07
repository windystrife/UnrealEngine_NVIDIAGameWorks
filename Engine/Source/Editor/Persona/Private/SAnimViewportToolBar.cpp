// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimViewportToolBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineGlobals.h"
#include "AssetData.h"
#include "Engine/Engine.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Preferences/PersonaOptions.h"
#include "EditorViewportCommands.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportLODCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "SAnimPlusMinusSlider.h"
#include "SEditorViewportToolBarMenu.h"
#include "STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "AssetViewerSettings.h"
#include "PersonaPreviewSceneDescription.h"
#include "Engine/PreviewMeshCollection.h"
#include "PreviewSceneCustomizations.h"
#include "ClothingSimulation.h"
#include "SimulationEditorExtender.h"
#include "ClothingSimulationFactoryInterface.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "ISlateMetaData.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "AnimViewportToolBar"

//Class definition which represents widget to modify strength of wind for clothing
class SClothWindSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SClothWindSettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget from its declaration */
	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = SNew( STextBlock )
			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthLabel)
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) );

		this->ChildSlot
			[
				SNew(SAnimPlusMinusSlider)
				.IsEnabled( this, &SClothWindSettings::IsWindEnabled ) 
				.Label( LOCTEXT("WindStrength", "Wind Strength:") )
				.OnMinusClicked( this, &SClothWindSettings::OnDecreaseWindStrength )
				.MinusTooltip( LOCTEXT("DecreaseWindStrength_ToolTip", "Decrease Wind Strength") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthSliderValue)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetWindStrength)
				.SliderTooltip( LOCTEXT("WindStrength_ToolTip", "Change wind strength") )
				.OnPlusClicked( this, &SClothWindSettings::OnIncreaseWindStrength )
				.PlusTooltip( LOCTEXT("IncreasetWindStrength_ToolTip", "Increase Wind Strength") )
				.ExtraWidget( ExtraWidget )
			];
	}

protected:
	/** Callback function for decreasing size */
	FReply OnDecreaseWindStrength()
	{
		const float DeltaValue = 0.1f;
		AnimViewportPtr.Pin()->SetWindStrength( AnimViewportPtr.Pin()->GetWindStrengthSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	/** Callback function for increasing size */
	FReply OnIncreaseWindStrength()
	{
		const float DeltaValue = 0.1f;
		AnimViewportPtr.Pin()->SetWindStrength( AnimViewportPtr.Pin()->GetWindStrengthSliderValue() + DeltaValue );
		return FReply::Handled();
	}

	/** Callback function which determines whether this widget is enabled */
	bool IsWindEnabled() const
	{
		return AnimViewportPtr.Pin()->IsApplyingClothWind();
	}

protected:
	/** The viewport hosting this widget */
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify gravity for preview
class SGravitySettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGravitySettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = SNew( STextBlock )
			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleLabel)
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) );

		this->ChildSlot
			[
				SNew(SAnimPlusMinusSlider)
				.Label( LOCTEXT("Gravity Scale", "Gravity Scale Preview:")  )
				.OnMinusClicked( this, &SGravitySettings::OnDecreaseGravityScale )
				.MinusTooltip( LOCTEXT("DecreaseGravitySize_ToolTip", "Decrease Gravity Scale") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleSliderValue)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetGravityScale)
				.SliderTooltip( LOCTEXT("GravityScale_ToolTip", "Change Gravity Scale") )
				.OnPlusClicked( this, &SGravitySettings::OnIncreaseGravityScale )
				.PlusTooltip( LOCTEXT("IncreaseGravityScale_ToolTip", "Increase Gravity Scale") )
				.ExtraWidget( ExtraWidget )
			];
	}

protected:
	FReply OnDecreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	FReply OnIncreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() + DeltaValue );
		return FReply::Handled();
	}

protected:
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

///////////////////////////////////////////////////////////
// SAnimViewportToolBar

void SAnimViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SAnimationEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport)
{
	bShowShowMenu = InArgs._ShowShowMenu;
	bShowLODMenu = InArgs._ShowLODMenu;
	bShowPlaySpeedMenu = InArgs._ShowPlaySpeedMenu;
	bShowFloorOptions = InArgs._ShowFloorOptions;
	bShowTurnTable = InArgs._ShowTurnTable;
	bShowPhysicsMenu = InArgs._ShowPhysicsMenu;

	CommandList = InRealViewport->GetCommandList();
	Extenders = InArgs._Extenders;

	// If we have no extender, make an empty one
	if (Extenders.Num() == 0)
	{
		Extenders.Add(MakeShared<FExtender>());
	}

	const FMargin ToolbarSlotPadding(2.0f, 2.0f);
	const FMargin ToolbarButtonPadding(2.0f, 0.0f);

	static const FName DefaultForegroundName("DefaultForeground");


	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Image("EditorViewportToolBar.MenuDropdown")
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewMenu)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(this, &SAnimViewportToolBar::GetCameraMenuLabel)
			.LabelIcon(this, &SAnimViewportToolBar::GetCameraMenuLabelIcon)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.CameraMenu")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewportTypeMenu)
		]
		// View menu (lit, unlit, etc...)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(LOCTEXT("ShowMenu", "Show"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewMenuButton")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateShowMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Label(LOCTEXT("Physics", "Physics"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GeneratePhysicsMenu)
			.Visibility(bShowPhysicsMenu ? EVisibility::Visible : EVisibility::Collapsed)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			//LOD
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Label(this, &SAnimViewportToolBar::GetLODMenuLabel)
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateLODMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Label(this, &SAnimViewportToolBar::GetPlaybackMenuLabel)
			.LabelIcon(FEditorStyle::GetBrush("AnimViewportMenu.PlayBackSpeed"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GeneratePlaybackMenu)
		]
		+ SHorizontalBox::Slot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Right)
		[
			SNew(STransformViewportToolBar)
			.Viewport(InRealViewport)
			.CommandList(InRealViewport->GetCommandList())
			.Visibility(this, &SAnimViewportToolBar::GetTransformToolbarVisibility)
		];
			
	
	//@TODO: Need clipping horizontal box: LeftToolbar->AddWrapButton();

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity( this, &SViewportToolBar::OnGetColorAndOpacity )
		.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				LeftToolbar
			]
			+SVerticalBox::Slot()
			.Padding(FMargin(4.0f, 3.0f, 0.0f, 0.0f))
			[
				// Display text (e.g., item being previewed)
				SNew(STextBlock)
				.Text(InViewport.Get(), &SAnimationEditorViewportTabBody::GetDisplayString)
				.Font(FEditorStyle::GetFontStyle(TEXT("AnimViewport.MessageFont")))
				.ShadowOffset(FVector2D(0.5f, 0.5f))
				.ShadowColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				.ColorAndOpacity(this, &SAnimViewportToolBar::GetFontColor)
			]
		]
	];
	
	SViewportToolBar::Construct(SViewportToolBar::FArguments());

	// We assign the viewport pointer her rather that initially, as SViewportToolbar::Construct 
	// ends up calling through and attempting to perform operations on the not-yet-full-constructed viewport
	Viewport = InViewport;
}

EVisibility SAnimViewportToolBar::GetTransformToolbarVisibility() const
{
	return Viewport.Pin()->CanUseGizmos() ? EVisibility::Visible : EVisibility::Hidden;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewMenu() const
{
	const FAnimViewportMenuCommands& Actions = FAnimViewportMenuCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	InMenuBuilder.BeginSection("AnimViewportSceneSetup", LOCTEXT("ViewMenu_SceneSetupLabel", "Scene Setup"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().PreviewSceneSettings);
		InMenuBuilder.PopCommandList();

		if(bShowFloorOptions)
		{
			TSharedPtr<SWidget> FloorOffsetWidget = 
				SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.WidthOverride(100.0f)
					[
						SNew(SNumericEntryBox<float>)
						.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.AllowSpin(true)
						.MinSliderValue(-100.0f)
						.MaxSliderValue(100.0f)
						.Value(this, &SAnimViewportToolBar::OnGetFloorOffset)
						.OnValueChanged(this, &SAnimViewportToolBar::OnFloorOffsetChanged)
						.ToolTipText(LOCTEXT("FloorOffsetToolTip", "Height offset for the floor mesh (stored per-mesh)"))
					]
				];

			InMenuBuilder.AddWidget(FloorOffsetWidget.ToSharedRef(), LOCTEXT("FloorHeightOffset", "Floor Height Offset"));

			InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
			InMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().AutoAlignFloorToMesh);
			InMenuBuilder.PopCommandList();
		}
			
		if (bShowTurnTable)
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("TurnTableLabel", "Turn Table"),
				LOCTEXT("TurnTableTooltip", "Set up auto-rotation of preview."),
				FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::GenerateTurnTableMenu),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "AnimViewportMenu.TurnTableSpeed")
				);
		}
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("AnimViewportCamera", LOCTEXT("ViewMenu_CameraLabel", "Camera"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().CameraFollow);
		InMenuBuilder.PopCommandList();
		InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().FocusViewportToSelection);

		const float FOVMin = 5.f;
		const float FOVMax = 170.f;

		TSharedPtr<SWidget> FOVWidget = 
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
					.Value(this, &SAnimViewportToolBar::OnGetFOVValue)
					.OnValueChanged(this, &SAnimViewportToolBar::OnFOVValueChanged)
					.OnValueCommitted(this, &SAnimViewportToolBar::OnFOVValueCommitted)
				]
			];

		InMenuBuilder.AddWidget(FOVWidget.ToSharedRef(), LOCTEXT("Viewport_FOVLabel", "Field Of View"));
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("AnimViewportDefaultCamera", LOCTEXT("ViewMenu_DefaultCameraLabel", "Default Camera"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().JumpToDefaultCamera);
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SaveCameraAsDefault);
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().ClearDefaultCamera);
		InMenuBuilder.PopCommandList();
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePhysicsMenu() const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());
	{
		InMenuBuilder.BeginSection("AnimViewportShowMenu", LOCTEXT("ViewMenu_AnimViewportShowMenu", "Anim Viewport Show Menu"));
		InMenuBuilder.EndSection();
	}
	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();
	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateShowMenu() const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	{
		InMenuBuilder.BeginSection("AnimViewportGeneralShowFlags", LOCTEXT("ViewMenu_GeneralShowFlags", "General Show Flags"));
		{
			InMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ToggleGrid);
		}
		InMenuBuilder.EndSection();

		InMenuBuilder.BeginSection("AnimViewportSceneElements", LOCTEXT("ViewMenu_SceneElements", "Scene Elements"));
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("AnimViewportMeshSubMenu", "Mesh"),
				LOCTEXT("AnimViewportMeshSubMenuToolTip", "Mesh-related options"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenuBuilder)
				{
					SubMenuBuilder.BeginSection("AnimViewportMesh", LOCTEXT("ShowMenu_Actions_Mesh", "Mesh"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowRetargetBasePose );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBound );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().UseInGameBound );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowPreviewMesh );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowMorphTargets );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowVertexColors );
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportMeshInfo", LOCTEXT("ShowMenu_Actions_MeshInfo", "Mesh Info"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoBasic);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoDetailed);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoSkelControls);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().HideDisplayInfo);
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportPreviewOverlayDraw", LOCTEXT("ShowMenu_Actions_Overlay", "Mesh Overlay Drawing"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowOverlayNone);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneWeight);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowMorphTargetVerts);
					}
					SubMenuBuilder.EndSection();
				})
			);

			InMenuBuilder.AddSubMenu(
				LOCTEXT("AnimViewportAnimationSubMenu", "Animation"),
				LOCTEXT("AnimViewportAnimationSubMenuToolTip", "Animation-related options"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenuBuilder)
				{
					SubMenuBuilder.BeginSection("AnimViewportRootMotion", LOCTEXT("Viewport_RootMotionLabel", "Root Motion"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ProcessRootMotion);
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportAnimation", LOCTEXT("ShowMenu_Actions_AnimationAsset", "Animation"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowRawAnimation);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowNonRetargetedAnimation);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowAdditiveBaseBones);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowSourceRawAnimation);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBakedAnimation);
					}
					SubMenuBuilder.EndSection();
				})
			);

			InMenuBuilder.AddSubMenu(
				LOCTEXT("AnimViewportBoneDrawSubMenu", "Bones"),
				LOCTEXT("AnimViewportBoneDrawSubMenuToolTip", "Bone Drawing Options"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenuBuilder)
				{
					SubMenuBuilder.BeginSection("BonesAndSockets", LOCTEXT("Viewport_BonesAndSocketsLabel", "Bones & Sockets"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowSockets);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneNames);
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportPreviewHierarchyBoneDraw", LOCTEXT("ShowMenu_Actions_BoneDrawing", "Bone Drawing"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawAll);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelected);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndParents);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawNone);
					}
					SubMenuBuilder.EndSection();
				})
				);

#if WITH_APEX_CLOTHING
			UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent();

			if(PreviewComp)
			{
				InMenuBuilder.AddSubMenu(
					LOCTEXT("AnimViewportClothingSubMenu", "Clothing"),
					LOCTEXT("AnimViewportClothingSubMenuToolTip", "Options relating to clothing"),
					FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::FillShowClothingMenu));
			}
#endif // #if WITH_APEX_CLOTHING
		}

		InMenuBuilder.AddSubMenu(
			LOCTEXT("AnimViewportAdvancedSubMenu", "Advanced"),
			LOCTEXT("AnimViewportAdvancedSubMenuToolTip", "Advanced options"),
			FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::FillShowAdvancedMenu));

		InMenuBuilder.EndSection();

		InMenuBuilder.BeginSection("AnimViewportOtherFlags", LOCTEXT("ViewMenu_OtherFlags", "Other Flags"));
		{
			InMenuBuilder.AddMenuEntry(Actions.MuteAudio);
			InMenuBuilder.AddMenuEntry(Actions.UseAudioAttenuation);
		}
		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

void SAnimViewportToolBar::FillShowAdvancedMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	// Draw UVs
	MenuBuilder.BeginSection("UVVisualization", LOCTEXT("UVVisualization_Label", "UV Visualization"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().AnimSetDrawUVs);
		MenuBuilder.AddWidget(Viewport.Pin()->UVChannelCombo.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Skinning", LOCTEXT("Skinning_Label", "Skinning"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetCPUSkinning);
	}
	MenuBuilder.EndSection();
	

	MenuBuilder.BeginSection("ShowVertex", LOCTEXT("ShowVertex_Label", "Vertex Normal Visualization"));
	{
		// Vertex debug flags
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowNormals);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowTangents);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowBinormals);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyLocalAxes", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes") );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesAll );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesSelected );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesNone );
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillShowClothingMenu(FMenuBuilder& MenuBuilder)
{
#if WITH_APEX_CLOTHING
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("ClothPreview", LOCTEXT("ClothPreview_Label", "Preview"));
	{
		MenuBuilder.AddMenuEntry(Actions.DisableClothSimulation);
		MenuBuilder.AddMenuEntry(Actions.ApplyClothWind);
		TSharedPtr<SWidget> WindWidget = SNew(SClothWindSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(WindWidget.ToSharedRef(), FText());
		TSharedPtr<SWidget> GravityWidget = SNew(SGravitySettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(GravityWidget.ToSharedRef(), FText());
		MenuBuilder.AddMenuEntry(Actions.EnableCollisionWithAttachedClothChildren);
		MenuBuilder.AddMenuEntry(Actions.PauseClothWithAnim);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothAdditionalVisualization", LOCTEXT("ClothAdditionalVisualization_Label", "Sections Display Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowAllSections);
		MenuBuilder.AddMenuEntry(Actions.ShowOnlyClothSections);
		MenuBuilder.AddMenuEntry(Actions.HideOnlyClothSections);
	}
	MenuBuilder.EndSection();

	// Call into the clothing editor module to customize the menu (this is mainly for debug visualizations and sim-specific options)
	TSharedPtr<SAnimationEditorViewportTabBody> SharedViewport = Viewport.Pin();
	if(SharedViewport.IsValid())
	{
		TSharedRef<IPersonaPreviewScene> PreviewScene = SharedViewport->GetAnimationViewportClient()->GetPreviewScene();
		if(UDebugSkelMeshComponent* PreviewComponent = PreviewScene->GetPreviewMeshComponent())
		{
			FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));

			if(ISimulationEditorExtender* Extender = ClothingEditorModule.GetSimulationEditorExtender(PreviewComponent->ClothingSimulationFactory->GetFName()))
			{
				Extender->ExtendViewportShowMenu(MenuBuilder, PreviewScene);
			}
		}
	}

#endif // #if WITH_APEX_CLOTHING
}

FText SAnimViewportToolBar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");
	if (Viewport.IsValid())
	{
		int32 LODSelectionType = Viewport.Pin()->GetLODSelection();

		if (LODSelectionType > 0)
		{
			FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType - 1);
			Label = FText::FromString(TitleLabel);
		}
	}
	return Label;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateLODMenu() const
{
	const FAnimViewportLODCommands& Actions = FAnimViewportLODCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	{
		// LOD Models
		InMenuBuilder.BeginSection("AnimViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs") );
		{
			InMenuBuilder.AddMenuEntry( Actions.LODAuto );
			InMenuBuilder.AddMenuEntry( Actions.LOD0 );

			int32 LODCount = Viewport.Pin()->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODId);

				FUIAction Action(FExecuteAction::CreateSP(Viewport.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::OnSetLODModel, LODId + 1),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(Viewport.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::IsLODModelSelected, LODId + 1));

				InMenuBuilder.AddMenuEntry(FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
			}
		}
		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewportTypeMenu() const
{

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList.ToSharedRef(), MenuExtender);
	InMenuBuilder.SetStyle(&FEditorStyle::Get(), "Menu");
	InMenuBuilder.PushCommandList(CommandList.ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	// Camera types
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	InMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	InMenuBuilder.EndSection();

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePlaybackMenu() const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());
	{
		// View modes
		{
			InMenuBuilder.BeginSection("AnimViewportPlaybackSpeed", LOCTEXT("PlaybackMenu_SpeedLabel", "Playback Speed") );
			{
				for(int32 PlaybackSpeedIndex = 0; PlaybackSpeedIndex < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++PlaybackSpeedIndex)
				{
					InMenuBuilder.AddMenuEntry( Actions.PlaybackSpeedCommands[PlaybackSpeedIndex] );
				}
			}
			InMenuBuilder.EndSection();
		}
	}
	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

void SAnimViewportToolBar::GenerateTurnTableMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	MenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	MenuBuilder.BeginSection("AnimViewportTurnTableMode", LOCTEXT("TurnTableMenu_ModeLabel", "Turn Table Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePlay);
		MenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePause);
		MenuBuilder.AddMenuEntry(Actions.PersonaTurnTableStop);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportTurnTableSpeed", LOCTEXT("TurnTableMenu_SpeedLabel", "Turn Table Speed"));
	{
		for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			MenuBuilder.AddMenuEntry(Actions.TurnTableSpeeds[i]);
		}
	}
	MenuBuilder.EndSection();
	MenuBuilder.PopCommandList();
}

FSlateColor SAnimViewportToolBar::GetFontColor() const
{
	const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
	const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

	ensureMsgf(Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	FLinearColor FontColor;
	if (Settings->Profiles[ProfileIndex].bShowEnvironment)
	{
		FontColor = FLinearColor::White;
	}
	else
	{
		FLinearColor Color = Settings->Profiles[ProfileIndex].EnvironmentColor * Settings->Profiles[ProfileIndex].EnvironmentIntensity;

		// see if it's dark, if V is less than 0.2
		if (Color.B < 0.3f )
		{
			FontColor = FLinearColor::White;
		}
		else
		{
			FontColor = FLinearColor::Black;
		}
	}

	return FontColor;
}

FText SAnimViewportToolBar::GetPlaybackMenuLabel() const
{
	FText Label = LOCTEXT("PlaybackError", "Error");
	if (Viewport.IsValid())
	{
		for(int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			if (Viewport.Pin()->IsPlaybackSpeedSelected(i))
			{
				Label = FText::FromString(FString::Printf(
					(i == EAnimationPlaybackSpeeds::Quarter) ? TEXT("x%.2f") : TEXT("x%.1f"), 
					EAnimationPlaybackSpeeds::Values[i]
					));
				break;
			}
		}
	}
	return Label;
}

FText SAnimViewportToolBar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("Viewport_Default", "Camera");
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetLevelViewportClient().ViewportType )
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

const FSlateBrush* SAnimViewportToolBar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if (PinnedViewport.IsValid())
	{
		static FName PerspectiveIcon("EditorViewport.Perspective");
		static FName TopIcon("EditorViewport.Top");
		static FName LeftIcon("EditorViewport.Left");
		static FName FrontIcon("EditorViewport.Front");
		static FName BottomIcon("EditorViewport.Bottom");
		static FName RightIcon("EditorViewport.Right");
		static FName BackIcon("EditorViewport.Back");

		switch (PinnedViewport->GetLevelViewportClient().ViewportType)
		{
		case LVT_Perspective:
			Icon = PerspectiveIcon;
			break;

		case LVT_OrthoXY:
			Icon = TopIcon;
			break;

		case LVT_OrthoYZ:
			Icon = LeftIcon;
			break;

		case LVT_OrthoXZ:
			Icon = FrontIcon;
			break;

		case LVT_OrthoNegativeXY:
			Icon = BottomIcon;
			break;

		case LVT_OrthoNegativeYZ:
			Icon = RightIcon;
			break;

		case LVT_OrthoNegativeXZ:
			Icon = BackIcon;
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return FEditorStyle::GetBrush(Icon);
}

float SAnimViewportToolBar::OnGetFOVValue( ) const
{
	return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
}

void SAnimViewportToolBar::OnFOVValueChanged( float NewValue )
{
	bool bUpdateStoredFOV = true;
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	// @todo Viewport Cleanup
/*
	if (ViewportClient.ActorLockedToCamera.IsValid())
	{
		ACameraActor* CameraActor = Cast< ACameraActor >( ViewportClient.ActorLockedToCamera.Get() );
		if( CameraActor != NULL )
		{
			CameraActor->CameraComponent->FieldOfView = NewValue;
			bUpdateStoredFOV = false;
		}
	}*/

	if ( bUpdateStoredFOV )
	{
		ViewportClient.FOVAngle = NewValue;
		// @TODO cleanup - this interface should be in FNewAnimationViewrpotClient in the future
		// update config
		FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
		AnimViewportClient.ConfigOption->SetViewFOV(NewValue);
	}

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

void SAnimViewportToolBar::OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo )
{
	//OnFOVValueChanged will be called... nothing needed here.
}

TOptional<float> SAnimViewportToolBar::OnGetFloorOffset() const
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	return AnimViewportClient.GetFloorOffset();
}

void SAnimViewportToolBar::OnFloorOffsetChanged( float NewValue )
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	AnimViewportClient.SetFloorOffset( NewValue );
}

#undef LOCTEXT_NAMESPACE
