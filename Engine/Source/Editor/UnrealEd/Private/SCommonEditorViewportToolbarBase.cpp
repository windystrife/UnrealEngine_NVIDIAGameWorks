// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCommonEditorViewportToolbarBase.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"


#include "STransformViewportToolbar.h"
#include "EditorShowFlags.h"
#include "SEditorViewport.h"
#include "EditorViewportCommands.h"
#include "SEditorViewportToolBarMenu.h"
#include "SEditorViewportViewMenu.h"

#define LOCTEXT_NAMESPACE "LevelViewportToolBar"

//////////////////////////////////////////////////////////////////////////
// SCommonEditorViewportToolbarBase

void SCommonEditorViewportToolbarBase::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	InfoProviderPtr = InInfoProvider;
 	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const FMargin ToolbarSlotPadding( 2.0f, 2.0f );
	const FMargin ToolbarButtonPadding( 2.0f, 0.0f );

	static const FName DefaultForegroundName("DefaultForeground");

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
				SNew( SHorizontalBox )

				// Options menu
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Image( "EditorViewportToolBar.MenuDropdown" )
					.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateOptionsMenu)
				]

				// Camera mode menu
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SCommonEditorViewportToolbarBase::GetCameraMenuLabel)
					.LabelIcon(this, &SCommonEditorViewportToolbarBase::GetCameraMenuLabelIcon)
					.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateCameraMenu)
				]

				// View menu
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					MakeViewMenu()
				]

				// Show menu
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[
					SNew( SEditorViewportToolbarMenu )
					.Label( LOCTEXT("ShowMenuTitle", "Show") )
					.Cursor( EMouseCursor::Default )
					.ParentToolBar( SharedThis( this ) )
					.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateShowMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( ToolbarSlotPadding )
				[

					SNew( SEditorViewportToolbarMenu )
					.Label( LOCTEXT("ViewParamMenuTitle", "View Mode Options") )
					.Cursor( EMouseCursor::Default )
					.ParentToolBar( SharedThis( this ) )
					.Visibility( this, &SCommonEditorViewportToolbarBase::GetViewModeOptionsVisibility )
					.OnGetMenuContent( this, &SCommonEditorViewportToolbarBase::GenerateViewModeOptionsMenu ) 
				]

				// Transform toolbar
				+ SHorizontalBox::Slot()
				.Padding( ToolbarSlotPadding )
				.HAlign( HAlign_Right )
				[
					SNew(STransformViewportToolBar)
					.Viewport(ViewportRef)
					.CommandList(ViewportRef->GetCommandList())
					.Extenders(GetInfoProvider().GetExtenders())
					.Visibility(ViewportRef, &SEditorViewport::GetTransformToolbarVisibility)
				]
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

FText SCommonEditorViewportToolbarBase::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("CameraMenuTitle_Default", "Camera");

	switch (GetViewportClient().GetViewportType())
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
			Label = LOCTEXT("CameraMenuTitle_OrthoFreelook", "Ortho");
			break;
	}

	return Label;
}

const FSlateBrush* SCommonEditorViewportToolbarBase::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;

	switch (GetViewportClient().GetViewportType())
	{
		case LVT_Perspective:
			Icon = FName( "EditorViewport.Perspective" );
			break;

		case LVT_OrthoXY:
			Icon = FName( "EditorViewport.Top" );
			break;

		case LVT_OrthoYZ:
			Icon = FName( "EditorViewport.Left" );
			break;

		case LVT_OrthoXZ:
			Icon = FName( "EditorViewport.Front" );
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

	return FEditorStyle::GetBrush( Icon );
}

EVisibility SCommonEditorViewportToolbarBase::GetViewModeOptionsVisibility() const
{
	const FEditorViewportClient& ViewClient = GetViewportClient();
	if (ViewClient.GetViewMode() == VMI_MeshUVDensityAccuracy || ViewClient.GetViewMode() == VMI_MaterialTextureScaleAccuracy || ViewClient.GetViewMode() == VMI_RequiredTextureResolution)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateViewModeOptionsMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();
	FEditorViewportClient& ViewClient = GetViewportClient();
	const UWorld* World = ViewClient.GetWorld();
	return BuildViewModeOptionsMenu(ViewportRef->GetCommandList(), ViewClient.GetViewMode(), World ? World->FeatureLevel : GMaxRHIFeatureLevel, ViewClient.GetViewModeParamNameMap());
}


TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateOptionsMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bIsPerspective = GetViewportClient().GetViewportType() == LVT_Perspective;
	
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		OptionsMenuBuilder.BeginSection("LevelViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options") );
		{
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleRealTime );
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleStats );
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleFPS );

			if (bIsPerspective)
			{
				OptionsMenuBuilder.AddWidget( GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)") );
				OptionsMenuBuilder.AddWidget( GenerateFarViewPlaneMenu(), LOCTEXT("FarViewPlane", "Far View Plane") );
			}
		}
		OptionsMenuBuilder.EndSection();
	}

	return OptionsMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateCameraMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder( bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList() );

	// Camera types
	CameraMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().Perspective );

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic") );
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
		CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

#if 0
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

	const TArray<FShowFlagData>& ShowFlagData = GetShowFlagMenuItems();

	TArray<CommonEditorViewportUtils::FShowMenuCommand> ShowMenu[SFG_Max];

	// Get each show flag command and put them in their corresponding groups
	for( int32 ShowFlag = 0; ShowFlag < ShowFlagData.Num(); ++ShowFlag )
	{
		const FShowFlagData& SFData = ShowFlagData[ShowFlag];
		
		ShowMenu[SFData.Group].Add( Actions.ShowFlagCommands[ ShowFlag ] );
	}
#endif

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{

#if 0
		ShowMenuBuilder.AddMenuEntry( Actions.UseDefaultShowFlags );
		
		if( ShowMenu[SFG_Normal].Num() > 0 )
		{
			// Generate entries for the standard show flags
			ShowMenuBuilder.BeginSection("LevelViewportShowFlagsCommon", LOCTEXT("CommonShowFlagHeader", "Common Show Flags") );
			{
				for( int32 EntryIndex = 0; EntryIndex < ShowMenu[SFG_Normal].Num(); ++EntryIndex )
				{
					ShowMenuBuilder.AddMenuEntry( ShowMenu[SFG_Normal][ EntryIndex ].ShowMenuItem, NAME_None, ShowMenu[SFG_Normal][ EntryIndex ].LabelOverride );
				}
			}
			ShowMenuBuilder.EndSection();
		}

		// Generate entries for the different show flags groups
		ShowMenuBuilder.BeginSection("LevelViewportShowFlags");
		{
			ShowMenuBuilder.AddSubMenu( LOCTEXT("PostProcessShowFlagsMenu", "Post Processing"), LOCTEXT("PostProcessShowFlagsMenu_ToolTip", "Post process show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_PostProcess], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightTypesShowFlagsMenu", "Light Types"), LOCTEXT("LightTypesShowFlagsMenu_ToolTip", "Light Types show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_LightTypes], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightingComponentsShowFlagsMenu", "Lighting Components"), LOCTEXT("LightingComponentsShowFlagsMenu_ToolTip", "Lighting Components show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_LightingComponents], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightingFeaturesShowFlagsMenu", "Lighting Features"), LOCTEXT("LightingFeaturesShowFlagsMenu_ToolTip", "Lighting Features show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_LightingFeatures], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("DeveloperShowFlagsMenu", "Developer"), LOCTEXT("DeveloperShowFlagsMenu_ToolTip", "Developer show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Developer], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("VisualizeShowFlagsMenu", "Visualize"), LOCTEXT("VisualizeShowFlagsMenu_ToolTip", "Visualize show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Visualize], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("AdvancedShowFlagsMenu", "Advanced"), LOCTEXT("AdvancedShowFlagsMenu_ToolTip", "Advanced show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Advanced], 0));
		}
		ShowMenuBuilder.EndSection();


		FText ShowAllLabel = LOCTEXT("ShowAllLabel", "Show All");
		FText HideAllLabel = LOCTEXT("HideAllLabel", "Hide All");

		// Show Volumes sub-menu
		{
			TArray< FLevelViewportCommands::FShowMenuCommand > ShowVolumesMenu;

			// 'Show All' and 'Hide All' buttons
			ShowVolumesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.ShowAllVolumes, ShowAllLabel ) );
			ShowVolumesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.HideAllVolumes, HideAllLabel ) );

			// Get each show flag command and put them in their corresponding groups
			ShowVolumesMenu += Actions.ShowVolumeCommands;

			ShowMenuBuilder.AddSubMenu( LOCTEXT("ShowVolumesMenu", "Volumes"), LOCTEXT("ShowVolumesMenu_ToolTip", "Show volumes flags"),
				FNewMenuDelegate::CreateStatic( &FillShowMenu, ShowVolumesMenu, 2 ) );
		}
#endif
	}

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateFOVMenu() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew( SBox )
		.HAlign( HAlign_Right )
		[
			SNew( SBox )
			.Padding( FMargin(4.0f, 0.0f, 0.0f, 0.0f) )
			.WidthOverride( 100.0f )
			[
				SNew(SSpinBox<float>)
				.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.Value(this, &SCommonEditorViewportToolbarBase::OnGetFOVValue)
				.OnValueChanged(this, &SCommonEditorViewportToolbarBase::OnFOVValueChanged)
			]
		];
}

float SCommonEditorViewportToolbarBase::OnGetFOVValue() const
{
	return GetViewportClient().ViewFOV;
}

void SCommonEditorViewportToolbarBase::OnFOVValueChanged(float NewValue) const
{
	FEditorViewportClient& ViewportClient = GetViewportClient();
	ViewportClient.FOVAngle = NewValue;
	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateFarViewPlaneMenu() const
{
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SSpinBox<float>)
				.ToolTipText(LOCTEXT("FarViewPlaneTooltip", "Distance to use as the far view plane, or zero to enable an infinite far view plane"))
				.MinValue(0.0f)
				.MaxValue(100000.0f)
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.Value(this, &SCommonEditorViewportToolbarBase::OnGetFarViewPlaneValue)
				.OnValueChanged(this, &SCommonEditorViewportToolbarBase::OnFarViewPlaneValueChanged)
			]
		];
}

float SCommonEditorViewportToolbarBase::OnGetFarViewPlaneValue() const
{
	return GetViewportClient().GetFarClipPlaneOverride();
}

void SCommonEditorViewportToolbarBase::OnFarViewPlaneValueChanged(float NewValue)
{
	GetViewportClient().OverrideFarClipPlane(NewValue);
}

TSharedPtr<FExtender> SCommonEditorViewportToolbarBase::GetCombinedExtenderList(TSharedRef<FExtender> MenuExtender) const
{
	TSharedPtr<FExtender> HostEditorExtenders = GetInfoProvider().GetExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	Extenders.Reserve(2);
	Extenders.Add(HostEditorExtenders);
	Extenders.Add(MenuExtender);

	return FExtender::Combine(Extenders);
}

TSharedPtr<FExtender> SCommonEditorViewportToolbarBase::GetViewMenuExtender() const
{
	TSharedRef<FExtender> ViewModeExtender(new FExtender());
	ViewModeExtender->AddMenuExtension(
		TEXT("ViewMode"),
		EExtensionHook::After,
		GetInfoProvider().GetViewportWidget()->GetCommandList(),
		FMenuExtensionDelegate::CreateSP(this, &SCommonEditorViewportToolbarBase::CreateViewMenuExtensions));

	return GetCombinedExtenderList(ViewModeExtender);
}

void SCommonEditorViewportToolbarBase::CreateViewMenuExtensions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LevelViewportDeferredRendering", LOCTEXT("DeferredRenderingHeader", "Deferred Rendering") );
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportCollision", LOCTEXT("CollisionViewModeHeader", "Collision") );
	{
		MenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().CollisionPawn, NAME_None, LOCTEXT("CollisionPawnViewModeDisplayName", "Player Collision") );
		MenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().CollisionVisibility, NAME_None, LOCTEXT("CollisionVisibilityViewModeDisplayName", "Visibility Collision") );
	}
	MenuBuilder.EndSection();

//FINDME
// 	MenuBuilder.BeginSection("LevelViewportLandscape", LOCTEXT("LandscapeHeader", "Landscape") );
// 	{
// 		MenuBuilder.AddSubMenu(LOCTEXT("LandscapeLODDisplayName", "LOD"), LOCTEXT("LandscapeLODMenu_ToolTip", "Override Landscape LOD in this viewport"), FNewMenuDelegate::CreateStatic(&Local::BuildLandscapeLODMenu, this), /*Default*/false, FSlateIcon());
// 	}
// 	MenuBuilder.EndSection();
}

ICommonEditorViewportToolbarInfoProvider& SCommonEditorViewportToolbarBase::GetInfoProvider() const
{
	return *InfoProviderPtr.Pin().Get();
}

FEditorViewportClient& SCommonEditorViewportToolbarBase::GetViewportClient() const
{
	return *GetInfoProvider().GetViewportWidget()->GetViewportClient().Get();
}

TSharedRef<SEditorViewportViewMenu> SCommonEditorViewportToolbarBase::MakeViewMenu()
{
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	return SNew(SEditorViewportViewMenu, ViewportRef, SharedThis(this))
		.Cursor(EMouseCursor::Default)
		.MenuExtenders(GetViewMenuExtender());
}

#undef LOCTEXT_NAMESPACE
