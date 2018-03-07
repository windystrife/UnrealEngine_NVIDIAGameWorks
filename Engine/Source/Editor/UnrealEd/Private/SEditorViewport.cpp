// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SEditorViewport.h"
#include "Misc/Paths.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SViewport.h"
#include "Misc/App.h"
#include "Widgets/Layout/SBorder.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "EngineGlobals.h"
#include "Engine/TextureStreamingTypes.h"
#include "EditorModeManager.h"
#include "Slate/SceneViewport.h"
#include "EditorViewportCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Settings/EditorProjectSettings.h"
#include "Kismet2/DebuggerCommands.h"

#define LOCTEXT_NAMESPACE "EditorViewport"

SEditorViewport::SEditorViewport()
	: LastTickTime(0)
	, bInvalidated(false)
{
}

SEditorViewport::~SEditorViewport()
{
	// Close viewport
	if( Client.IsValid() )
	{
		Client->Viewport = NULL;
	}

	// Release our reference to the viewport client
	Client.Reset();

	check( SceneViewport.IsUnique() );
}

void SEditorViewport::Construct( const FArguments& InArgs )
{
	

	ChildSlot
	[
		SNew(SGlobalPlayWorldActions)
		[
			SAssignNew(ViewportWidget, SViewport)
			.ShowEffectWhenDisabled(false)
			.EnableGammaCorrection(false) // Scene rendering handles this
			.AddMetaData(InArgs.MetaData.Num() > 0 ? InArgs.MetaData[0] : MakeShareable(new FTagMetaData(TEXT("LevelEditorViewport"))))
			[
				SAssignNew(ViewportOverlay, SOverlay)
				+ SOverlay::Slot()
				[
				SNew(SBorder)
				.BorderImage(this, &SEditorViewport::OnGetViewportBorderBrush)
				.BorderBackgroundColor(this, &SEditorViewport::OnGetViewportBorderColorAndOpacity)
				.Visibility(this, &SEditorViewport::OnGetViewportContentVisibility)
				.Padding(0.0f)
				.ShowEffectWhenDisabled(false)
				]
			]
		]
	];

	TSharedRef<FEditorViewportClient> ViewportClient = MakeEditorViewportClient();

	if (!ViewportClient->VisibilityDelegate.IsBound())
	{
		ViewportClient->VisibilityDelegate.BindSP(this, &SEditorViewport::IsVisible);
	}

	SceneViewport = MakeShareable( new FSceneViewport( &ViewportClient.Get(), ViewportWidget ) );
	ViewportClient->Viewport = SceneViewport.Get();
	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());
	Client = ViewportClient;

	if ( Client->IsRealtime() )
	{
		ActiveTimerHandle = RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SEditorViewport::EnsureTick ) );
	}

	CommandList = MakeShareable( new FUICommandList );
	// Ensure the commands are registered
	FEditorViewportCommands::Register();
	BindCommands();

	TSharedPtr<SWidget> ViewportToolbar = MakeViewportToolbar();

	if( ViewportToolbar.IsValid() )
	{
		ViewportOverlay->AddSlot()
			.VAlign(VAlign_Top)
			[
				ViewportToolbar.ToSharedRef()
			];
	}

	PopulateViewportOverlays(ViewportOverlay.ToSharedRef());
}

FReply SEditorViewport::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	FReply Reply = FReply::Unhandled();
	if( CommandList->ProcessCommandBindings( InKeyEvent ) )
	{
		Reply = FReply::Handled();
		Client->Invalidate();
	}

	return Reply;
		
}

bool SEditorViewport::SupportsKeyboardFocus() const 
{
	return true;
}

FReply SEditorViewport::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	// forward focus to the viewport
	return FReply::Handled().SetUserFocus(ViewportWidget.ToSharedRef(), InFocusEvent.GetCause());
}

void SEditorViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	LastTickTime = FPlatformTime::Seconds();
}

void SEditorViewport::BindCommands()
{
	FUICommandList& CommandListRef = *CommandList;

	const FEditorViewportCommands& Commands = FEditorViewportCommands::Get();

	TSharedRef<FEditorViewportClient> ClientRef = Client.ToSharedRef();

	CommandListRef.MapAction( 
		Commands.ToggleRealTime,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnToggleRealtime ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SEditorViewport::IsRealtime));

	CommandListRef.MapAction( 
		Commands.ToggleStats,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnToggleStats ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::ShouldShowStats));

	CommandListRef.MapAction( 
		Commands.ToggleFPS,
		FExecuteAction::CreateSP(this, &SEditorViewport::ToggleStatCommand, FString("FPS")),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SEditorViewport::IsStatCommandVisible, FString("FPS")));

	CommandListRef.MapAction(
		Commands.IncrementPositionGridSize,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnIncrementPositionGridSize )
		);

	CommandListRef.MapAction(
		Commands.DecrementPositionGridSize,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnDecrementPositionGridSize )
		);

	CommandListRef.MapAction(
		Commands.IncrementRotationGridSize,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnIncrementRotationGridSize )
		);

	CommandListRef.MapAction(
		Commands.DecrementRotationGridSize,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnDecrementRotationGridSize )
		);

	CommandListRef.MapAction( 
		Commands.Perspective,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetViewportType, LVT_Perspective ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_Perspective));

	CommandListRef.MapAction( 
		Commands.Front,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetViewportType, LVT_OrthoXZ ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_OrthoXZ));

	CommandListRef.MapAction( 
		Commands.Left,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetViewportType, LVT_OrthoYZ ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_OrthoYZ));

	CommandListRef.MapAction( 
		Commands.Top,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetViewportType, LVT_OrthoXY ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_OrthoXY));

	CommandListRef.MapAction(
		Commands.Back,
		FExecuteAction::CreateSP(ClientRef, &FEditorViewportClient::SetViewportType, LVT_OrthoNegativeXZ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_OrthoNegativeXZ));

	CommandListRef.MapAction(
		Commands.Right,
		FExecuteAction::CreateSP(ClientRef, &FEditorViewportClient::SetViewportType, LVT_OrthoNegativeYZ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_OrthoNegativeYZ));

	CommandListRef.MapAction(
		Commands.Bottom,
		FExecuteAction::CreateSP(ClientRef, &FEditorViewportClient::SetViewportType, LVT_OrthoNegativeXY),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportType, LVT_OrthoNegativeXY));

	CommandListRef.MapAction(
		Commands.Next,
		FExecuteAction::CreateSP(ClientRef, &FEditorViewportClient::RotateViewportType),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ClientRef, &FEditorViewportClient::IsActiveViewportTypeInRotation));

	CommandListRef.MapAction(
		Commands.ScreenCapture,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnScreenCapture ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SEditorViewport::DoesAllowScreenCapture)
		);

	CommandListRef.MapAction(
		Commands.ScreenCaptureForProjectThumbnail,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnScreenCaptureForProjectThumbnail ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SEditorViewport::DoesAllowScreenCapture)
		);

	
	CommandListRef.MapAction(
		Commands.TranslateMode,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetWidgetMode, FWidget::WM_Translate ),
		FCanExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::CanSetWidgetMode, FWidget::WM_Translate ),
		FIsActionChecked::CreateSP( this, &SEditorViewport::IsWidgetModeActive, FWidget::WM_Translate ) 
		);

	CommandListRef.MapAction( 
		Commands.RotateMode,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetWidgetMode, FWidget::WM_Rotate ),
		FCanExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::CanSetWidgetMode, FWidget::WM_Rotate ),
		FIsActionChecked::CreateSP( this, &SEditorViewport::IsWidgetModeActive, FWidget::WM_Rotate )
		);
		

	CommandListRef.MapAction( 
		Commands.ScaleMode,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetWidgetMode, FWidget::WM_Scale ),
		FCanExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::CanSetWidgetMode, FWidget::WM_Scale ),
		FIsActionChecked::CreateSP( this, &SEditorViewport::IsWidgetModeActive, FWidget::WM_Scale )
		);

	CommandListRef.MapAction( 
		Commands.TranslateRotateMode,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetWidgetMode, FWidget::WM_TranslateRotateZ ),
		FCanExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::CanSetWidgetMode, FWidget::WM_TranslateRotateZ ),
		FIsActionChecked::CreateSP( this, &SEditorViewport::IsWidgetModeActive, FWidget::WM_TranslateRotateZ ),
		FIsActionButtonVisible::CreateSP( this, &SEditorViewport::IsTranslateRotateModeVisible )
		);

	CommandListRef.MapAction(
		Commands.TranslateRotate2DMode,
		FExecuteAction::CreateSP(ClientRef, &FEditorViewportClient::SetWidgetMode, FWidget::WM_2D),
		FCanExecuteAction::CreateSP(ClientRef, &FEditorViewportClient::CanSetWidgetMode, FWidget::WM_2D),
		FIsActionChecked::CreateSP(this, &SEditorViewport::IsWidgetModeActive, FWidget::WM_2D),
		FIsActionButtonVisible::CreateSP(this, &SEditorViewport::Is2DModeVisible)
		);

	CommandListRef.MapAction(
		Commands.ShrinkTransformWidget,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::AdjustTransformWidgetSize, -1 )
		);

	CommandListRef.MapAction( 
		Commands.ExpandTransformWidget,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::AdjustTransformWidgetSize, 1 )
		);

	CommandListRef.MapAction( 
		Commands.RelativeCoordinateSystem_World,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetWidgetCoordSystemSpace, COORD_World ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SEditorViewport::IsCoordSystemActive, COORD_World )
		);

	CommandListRef.MapAction( 
		Commands.RelativeCoordinateSystem_Local,
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetWidgetCoordSystemSpace, COORD_Local ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SEditorViewport::IsCoordSystemActive, COORD_Local )
		);

	CommandListRef.MapAction(
		Commands.CycleTransformGizmos,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnCycleWidgetMode ),
		FCanExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::CanCycleWidgetMode )
		);

	CommandListRef.MapAction(
		Commands.CycleTransformGizmoCoordSystem,
		FExecuteAction::CreateSP( this, &SEditorViewport::OnCycleCoordinateSystem )
		);

							
	CommandListRef.MapAction( 
		Commands.FocusViewportToSelection, 
		FExecuteAction::CreateSP( this, &SEditorViewport::OnFocusViewportToSelection )
		//FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY") ) )
		);

	CommandListRef.MapAction(
		Commands.SurfaceSnapping,
		FExecuteAction::CreateStatic( &SEditorViewport::OnToggleSurfaceSnap ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &SEditorViewport::OnIsSurfaceSnapEnabled ) 
		);

		// Simple macro for binding many Exposure Setting commands
#define MAP_EXPOSURE_ACTION( Command, ID ) \
	CommandListRef.MapAction( \
	Command, \
	FExecuteAction::CreateSP( this, &SEditorViewport::ChangeExposureSetting, ID ), \
	FCanExecuteAction(), \
	FIsActionChecked::CreateSP( this, &SEditorViewport::IsExposureSettingSelected, ID ) ) 

	MAP_EXPOSURE_ACTION( Commands.ToggleAutoExposure, FEditorViewportCommands::AutoExposureRadioID );
	MAP_EXPOSURE_ACTION( Commands.FixedExposure4m, -4);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure3m, -3);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure2m, -2);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure1m, -1);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure0, 0);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure1p, 1);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure2p, 2);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure3p, 3);
	MAP_EXPOSURE_ACTION( Commands.FixedExposure4p, 4);

	// Simple macro for binding many view mode UI commands

#define MAP_VIEWMODEPARAM_ACTION( ViewModeCommand, ViewModeParam ) \
	CommandListRef.MapAction( \
		ViewModeCommand, \
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetViewModeParam, ViewModeParam ), \
		FCanExecuteAction(), \
		FIsActionChecked::CreateSP( ClientRef, &FEditorViewportClient::IsViewModeParam, ViewModeParam ) ) 

#define MAP_VIEWMODE_ACTION( ViewModeCommand, ViewModeID ) \
	CommandListRef.MapAction( \
		ViewModeCommand, \
		FExecuteAction::CreateSP( ClientRef, &FEditorViewportClient::SetViewMode, ViewModeID ), \
		FCanExecuteAction(), \
		FIsActionChecked::CreateSP( ClientRef, &FEditorViewportClient::IsViewModeEnabled, ViewModeID ) ) 

	// Map each view mode
	MAP_VIEWMODE_ACTION( Commands.WireframeMode, VMI_BrushWireframe );
	MAP_VIEWMODE_ACTION( Commands.UnlitMode, VMI_Unlit );
	MAP_VIEWMODE_ACTION( Commands.LitMode, VMI_Lit );
	MAP_VIEWMODE_ACTION( Commands.DetailLightingMode, VMI_Lit_DetailLighting );
	MAP_VIEWMODE_ACTION( Commands.LightingOnlyMode, VMI_LightingOnly );
	MAP_VIEWMODE_ACTION( Commands.LightComplexityMode, VMI_LightComplexity );
	MAP_VIEWMODE_ACTION( Commands.ShaderComplexityMode, VMI_ShaderComplexity );
	MAP_VIEWMODE_ACTION( Commands.QuadOverdrawMode, VMI_QuadOverdraw);
	MAP_VIEWMODE_ACTION( Commands.ShaderComplexityWithQuadOverdrawMode, VMI_ShaderComplexityWithQuadOverdraw );
	MAP_VIEWMODE_ACTION( Commands.TexStreamAccPrimitiveDistanceMode, VMI_PrimitiveDistanceAccuracy );
	MAP_VIEWMODE_ACTION( Commands.TexStreamAccMeshUVDensityMode, VMI_MeshUVDensityAccuracy);
	MAP_VIEWMODE_ACTION( Commands.TexStreamAccMaterialTextureScaleMode, VMI_MaterialTextureScaleAccuracy );
	MAP_VIEWMODE_ACTION( Commands.RequiredTextureResolutionMode, VMI_RequiredTextureResolution );
	MAP_VIEWMODE_ACTION( Commands.StationaryLightOverlapMode, VMI_StationaryLightOverlap );
	MAP_VIEWMODE_ACTION( Commands.LightmapDensityMode, VMI_LightmapDensity );
	MAP_VIEWMODE_ACTION( Commands.ReflectionOverrideMode, VMI_ReflectionOverride );
	MAP_VIEWMODE_ACTION( Commands.GroupLODColorationMode, VMI_GroupLODColoration);
	MAP_VIEWMODE_ACTION( Commands.LODColorationMode, VMI_LODColoration );
	MAP_VIEWMODE_ACTION( Commands.HLODColorationMode, VMI_HLODColoration);
	MAP_VIEWMODE_ACTION( Commands.VisualizeBufferMode, VMI_VisualizeBuffer );
	MAP_VIEWMODE_ACTION( Commands.CollisionPawn, VMI_CollisionPawn);
	MAP_VIEWMODE_ACTION( Commands.CollisionVisibility, VMI_CollisionVisibility);

	MAP_VIEWMODEPARAM_ACTION( Commands.TexStreamAccMeshUVDensityAll, -1 );
	for (int32 TexCoordIndex = 0; TexCoordIndex < TEXSTREAM_MAX_NUM_UVCHANNELS; ++TexCoordIndex)
	{
		MAP_VIEWMODEPARAM_ACTION( Commands.TexStreamAccMeshUVDensitySingle[TexCoordIndex], TexCoordIndex );
	}

	MAP_VIEWMODEPARAM_ACTION( Commands.TexStreamAccMaterialTextureScaleAll, -1 );
	for (int32 TextureIndex = 0; TextureIndex < TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL; ++TextureIndex)
	{
		MAP_VIEWMODEPARAM_ACTION( Commands.TexStreamAccMaterialTextureScaleSingle[TextureIndex], TextureIndex );
		MAP_VIEWMODEPARAM_ACTION( Commands.RequiredTextureResolutionSingle[TextureIndex], TextureIndex );
	}
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	MAP_VIEWMODE_ACTION(Commands.VxgiOpacityVoxelsMode, VMI_VxgiOpacityVoxels);
	MAP_VIEWMODE_ACTION(Commands.VxgiEmittanceVoxelsMode, VMI_VxgiEmittanceVoxels);
	MAP_VIEWMODE_ACTION(Commands.VxgiIrradianceVoxelsMode, VMI_VxgiIrradianceVoxels);
#endif
	// NVCHANGE_END: Add VXGI
}

EVisibility SEditorViewport::OnGetViewportContentVisibility() const
{
	return GLevelEditorModeTools().IsViewportUIHidden() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
}

void SEditorViewport::OnToggleRealtime()
{
	if (Client->IsRealtime())
	{
		Client->SetRealtime( false );
		if ( ActiveTimerHandle.IsValid() )
		{
			UnRegisterActiveTimer( ActiveTimerHandle.Pin().ToSharedRef() );
		}
		
	}
	else
	{
		Client->SetRealtime( true );
		ActiveTimerHandle = RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SEditorViewport::EnsureTick ) );
	}
}


void SEditorViewport::SetRenderDirectlyToWindow( const bool bInRenderDirectlyToWindow )
{
	ViewportWidget->SetRenderDirectlyToWindow( bInRenderDirectlyToWindow );
}


void SEditorViewport::EnableStereoRendering( const bool bInEnableStereoRendering )
{
	ViewportWidget->EnableStereoRendering( bInEnableStereoRendering );
}


void SEditorViewport::OnToggleStats()
{
	bool bIsEnabled =  Client->ShouldShowStats();
	Client->SetShowStats( !bIsEnabled );

	if( !bIsEnabled )
	{
		// We cannot show stats unless realtime rendering is enabled
		if ( !Client->IsRealtime() )
		{
			Client->SetRealtime( true );
			ActiveTimerHandle = RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SEditorViewport::EnsureTick ) );
		}

		 // let the user know how they can enable stats via the console
		 FNotificationInfo Info(LOCTEXT("StatsEnableHint", "Stats display can be toggled via the STAT [type] console command"));
		 Info.ExpireDuration = 3.0f;
		 /* Temporarily remove the link until the page is updated
		 Info.HyperlinkText = LOCTEXT("StatsEnableHyperlink", "Learn more");
		 Info.Hyperlink = FSimpleDelegate::CreateStatic([](){ IDocumentation::Get()->Open(TEXT("Engine/Basics/ConsoleCommands#statisticscommands")); });
		 */
		 FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void SEditorViewport::ToggleStatCommand(FString CommandName)
{
	GEngine->ExecEngineStat(GetWorld(), Client.Get(), *CommandName);

	// Invalidate the client to render once in case the click was on the checkbox itself (which doesn't dismiss the menu)
	Client->Invalidate();
}

bool SEditorViewport::IsStatCommandVisible(FString CommandName) const
{
	// Only if realtime and stats are also enabled should we show the stat as visible
	return Client->IsRealtime() && Client->ShouldShowStats() && Client->IsStatEnabled(CommandName);
}

void SEditorViewport::ToggleShowFlag(uint32 EngineShowFlagIndex)
{
	bool bOldState = Client->EngineShowFlags.GetSingleFlag(EngineShowFlagIndex);
	Client->EngineShowFlags.SetSingleFlag(EngineShowFlagIndex, !bOldState);

	// If changing collision flag, need to do special handling for hidden objects
	if (EngineShowFlagIndex == FEngineShowFlags::EShowFlag::SF_Collision)
	{
		Client->UpdateHiddenCollisionDrawing();
	}

	// Invalidate clients which aren't real-time so we see the changes
	Client->Invalidate();
}

bool SEditorViewport::IsShowFlagEnabled(uint32 EngineShowFlagIndex) const
{
	return Client->EngineShowFlags.GetSingleFlag(EngineShowFlagIndex);
}

void SEditorViewport::ChangeExposureSetting( int32 ID )
{
	Client->ExposureSettings.bFixed = (ID != FEditorViewportCommands::AutoExposureRadioID);
	Client->ExposureSettings.LogOffset = ID;
}

bool SEditorViewport::IsExposureSettingSelected( int32 ID ) const
{
	if (ID == FEditorViewportCommands::AutoExposureRadioID)
	{
		return !Client->ExposureSettings.bFixed;
	}
	else
	{
		return Client->ExposureSettings.bFixed && Client->ExposureSettings.LogOffset == ID;
	}
}

void SEditorViewport::Invalidate()
{
	bInvalidated = true;
	if (!ActiveTimerHandle.IsValid())
	{
		ActiveTimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SEditorViewport::EnsureTick));
	}
}

bool SEditorViewport::IsRealtime() const
{
	return Client->IsRealtime();
}

bool SEditorViewport::IsVisible() const
{
	const float VisibilityTimeThreshold = .25f;
	// The viewport is visible if we don't have a parent layout (likely a floating window) or this viewport is visible in the parent layout
	return 
		LastTickTime == 0.0	||	// Never been ticked
		FPlatformTime::Seconds() - LastTickTime <= VisibilityTimeThreshold;	// Ticked recently
}

void SEditorViewport::OnScreenCapture()
{
	Client->TakeScreenshot(Client->Viewport, true);
}

void SEditorViewport::OnScreenCaptureForProjectThumbnail()
{
	if ( FApp::HasProjectName() )
	{
		const FString BaseFilename = FString(FApp::GetProjectName()) + TEXT(".png");
		const FString ScreenshotFilename = FPaths::Combine(*FPaths::ProjectDir(), *BaseFilename);
		UThumbnailManager::CaptureProjectThumbnail(Client->Viewport, ScreenshotFilename, true);
	}
}

EVisibility SEditorViewport::GetTransformToolbarVisibility() const
{
	return (Client->GetWidgetMode() != FWidget::WM_None) ? EVisibility::Visible : EVisibility::Hidden;
}

bool SEditorViewport::IsWidgetModeActive( FWidget::EWidgetMode Mode ) const
{
	return Client->GetWidgetMode() == Mode;
}

bool SEditorViewport::IsTranslateRotateModeVisible() const
{
	return GetDefault<ULevelEditorViewportSettings>()->bAllowTranslateRotateZWidget;
}

bool SEditorViewport::Is2DModeVisible() const
{
	return GetDefault<ULevelEditor2DSettings>()->bEnable2DWidget;
}

bool SEditorViewport::IsCoordSystemActive(ECoordSystem CoordSystem) const
{
	return Client->GetWidgetCoordSystemSpace() == CoordSystem;
}

void SEditorViewport::OnCycleWidgetMode()
{
	FWidget::EWidgetMode WidgetMode = Client->GetWidgetMode();

	int32 WidgetModeAsInt = WidgetMode;

	do
	{
		++WidgetModeAsInt;

		if ((WidgetModeAsInt == FWidget::WM_TranslateRotateZ) && (!GetDefault<ULevelEditorViewportSettings>()->bAllowTranslateRotateZWidget))
		{
			++WidgetModeAsInt;
		}

		if ((WidgetModeAsInt == FWidget::WM_2D) && (!GetDefault<ULevelEditor2DSettings>()->bEnable2DWidget))
		{
			++WidgetModeAsInt;
		}

		if( WidgetModeAsInt == FWidget::WM_Max )
		{
			WidgetModeAsInt -= FWidget::WM_Max;
		}
	}
	while( !Client->CanSetWidgetMode( (FWidget::EWidgetMode)WidgetModeAsInt ) && WidgetModeAsInt != WidgetMode );

	Client->SetWidgetMode( (FWidget::EWidgetMode)WidgetModeAsInt );
}

void SEditorViewport::OnCycleCoordinateSystem()
{
	int32 CoordSystemAsInt = Client->GetWidgetCoordSystemSpace();

	++CoordSystemAsInt;
	if( CoordSystemAsInt == COORD_Max )
	{
		CoordSystemAsInt -= COORD_Max;
	}

	Client->SetWidgetCoordSystemSpace( (ECoordSystem)CoordSystemAsInt );
}

UWorld* SEditorViewport::GetWorld() const
{
	return Client->GetWorld();
}


void SEditorViewport::OnToggleSurfaceSnap()
{
	auto* Settings = GetMutableDefault<ULevelEditorViewportSettings>();
	Settings->SnapToSurface.bEnabled = !Settings->SnapToSurface.bEnabled;
}

bool SEditorViewport::OnIsSurfaceSnapEnabled()
{
	return GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled;
}

EActiveTimerReturnType SEditorViewport::EnsureTick( double InCurrentTime, float InDeltaTime )
{
	// Keep the timer going if we're realtime or were invalidated this frame
	const bool bShouldContinue = Client->IsRealtime() || bInvalidated;
	bInvalidated = false;
	return bShouldContinue ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

#undef LOCTEXT_NAMESPACE
