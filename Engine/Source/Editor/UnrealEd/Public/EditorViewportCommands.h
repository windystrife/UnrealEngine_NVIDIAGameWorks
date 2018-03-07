// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once 

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"
#include "Engine/TextureStreamingTypes.h"

class FUICommandList;

/**
 * Class containing commands for editor viewport actions common to all viewports
 */
class UNREALED_API FEditorViewportCommands : public TCommands<FEditorViewportCommands>
{
public:
	FEditorViewportCommands() 
		: TCommands<FEditorViewportCommands>
		(
			TEXT("EditorViewport"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "EditorViewportCommands", "Common Viewport Commands"), // Localized context name for displaying
			TEXT("MainFrame"),
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{
	}
	
	/** Changes the viewport to perspective view */
	TSharedPtr< FUICommandInfo > Perspective;

	/** Changes the viewport to top view */
	TSharedPtr< FUICommandInfo > Top;

	/** Changes the viewport to bottom view */
	TSharedPtr< FUICommandInfo > Bottom;

	/** Changes the viewport to left view */
	TSharedPtr< FUICommandInfo > Left;

	/** Changes the viewport to right view */
	TSharedPtr< FUICommandInfo > Right;

	/** Changes the viewport to front view */
	TSharedPtr< FUICommandInfo > Front;

	/** Changes the viewport to back view */
	TSharedPtr< FUICommandInfo > Back;

	/** Rotate through viewport view options */
	TSharedPtr< FUICommandInfo > Next;

	/** Changes the viewport to wireframe */
	TSharedPtr< FUICommandInfo > WireframeMode;

	/** Changes the viewport to unlit mode */
	TSharedPtr< FUICommandInfo > UnlitMode;

	/** Changes the viewport to lit mode */
	TSharedPtr< FUICommandInfo > LitMode;

	/** Changes the viewport to detail lighting mode */
	TSharedPtr< FUICommandInfo > DetailLightingMode;

	/** Changes the viewport to reflection override mode */
	TSharedPtr< FUICommandInfo > ReflectionOverrideMode;

	/** Changes the viewport to lighting only */
	TSharedPtr< FUICommandInfo > LightingOnlyMode;

	/** Changes the viewport to light complextiy mode */
	TSharedPtr< FUICommandInfo > LightComplexityMode;

	/** Changes the viewport to shader complexity mode */
	TSharedPtr< FUICommandInfo > ShaderComplexityMode;

	/** Changes the viewport to quad overdraw mode */
	TSharedPtr< FUICommandInfo > QuadOverdrawMode;

	/** Changes the viewport to shader complexity with quad overdraw mode */
	TSharedPtr< FUICommandInfo > ShaderComplexityWithQuadOverdrawMode;

	/** Changes the viewport to primitive distance accuracy mode */
	TSharedPtr< FUICommandInfo > TexStreamAccPrimitiveDistanceMode;

	/** Changes the viewport to mesh UV density accuracy view mode */
	TSharedPtr< FUICommandInfo > TexStreamAccMeshUVDensityMode;
	TSharedPtr< FUICommandInfo > TexStreamAccMeshUVDensityAll; // TexStreamAccMeshUVDensityMode, but used in the view mode options menu.
	TSharedPtr< FUICommandInfo > TexStreamAccMeshUVDensitySingle[TEXSTREAM_MAX_NUM_UVCHANNELS];

	/** Changes the viewport to material texture scale accuracy mode */
	TSharedPtr< FUICommandInfo > TexStreamAccMaterialTextureScaleMode;
	TSharedPtr< FUICommandInfo > TexStreamAccMaterialTextureScaleAll; // Similar as TexStreamAccMaterialTextureScaleMode, but used in the view mode options menu.
	TSharedPtr< FUICommandInfo > TexStreamAccMaterialTextureScaleSingle[TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL];

	TSharedPtr< FUICommandInfo > RequiredTextureResolutionMode;
	TSharedPtr< FUICommandInfo > RequiredTextureResolutionSingle[TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL];

	/** Changes the viewport to stationary light overlap mode */
	TSharedPtr< FUICommandInfo > StationaryLightOverlapMode;

	/** Changes the viewport to lightmap density mode */
	TSharedPtr< FUICommandInfo > LightmapDensityMode;

	/** Shows the group menu item for LOD and HLOD coloration modes */
	TSharedPtr< FUICommandInfo > GroupLODColorationMode;

	/** Changes the viewport to LOD index coloration mode */
	TSharedPtr< FUICommandInfo > LODColorationMode;

	/** Changes the viewport to HLOD index coloration mode */
	TSharedPtr< FUICommandInfo > HLODColorationMode;

	/** Changes the viewport to visualize the buffer content */
	TSharedPtr< FUICommandInfo > VisualizeBufferMode;

	/** Collision Draw Mode */
	TSharedPtr< FUICommandInfo > CollisionPawn;
	TSharedPtr< FUICommandInfo > CollisionVisibility;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	TSharedPtr< FUICommandInfo > VxgiOpacityVoxelsMode;
	TSharedPtr< FUICommandInfo > VxgiEmittanceVoxelsMode;
	TSharedPtr< FUICommandInfo > VxgiIrradianceVoxelsMode;
#endif
	// NVCHANGE_END: Add VXGI

	/** Toggles realtime rendering in the viewport */
	TSharedPtr< FUICommandInfo > ToggleRealTime;

	/** Toggles showing stats in the viewport */
	TSharedPtr< FUICommandInfo > ToggleStats;

	/** Toggles showing fps in the viewport */
	TSharedPtr< FUICommandInfo > ToggleFPS;

	/** Allows the grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > IncrementPositionGridSize;

	/** Allows the grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > DecrementPositionGridSize;

	/** Allows the rotation grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > IncrementRotationGridSize;

	/** Allows the rotation grid size setting to by changed by one */
	TSharedPtr< FUICommandInfo > DecrementRotationGridSize;

	/** Command to capture screen */
	TSharedPtr< FUICommandInfo > ScreenCapture;

	/** Captures the viewport and updates the project thumbnail png file */
	TSharedPtr< FUICommandInfo > ScreenCaptureForProjectThumbnail;

	/** Translate Mode */
	TSharedPtr< FUICommandInfo > TranslateMode;

	/** Rotate Mode */
	TSharedPtr< FUICommandInfo > RotateMode;

	/** Scale Mode */
	TSharedPtr< FUICommandInfo > ScaleMode;

	/** TranslateRotate Mode */
	TSharedPtr< FUICommandInfo > TranslateRotateMode;

	/** TranslateRotate Mode */
	TSharedPtr< FUICommandInfo > TranslateRotate2DMode;

	/** Shrink the level editor transform widget */
	TSharedPtr< FUICommandInfo > ShrinkTransformWidget;

	/** Expand the level editor transform widget */
	TSharedPtr< FUICommandInfo > ExpandTransformWidget;

	/** World relative coordinate system */
	TSharedPtr< FUICommandInfo > RelativeCoordinateSystem_World;

	/** Local relative coordinate system */
	TSharedPtr< FUICommandInfo > RelativeCoordinateSystem_Local;

	TSharedPtr< FUICommandInfo > CycleTransformGizmos;
	TSharedPtr< FUICommandInfo > CycleTransformGizmoCoordSystem;

	TSharedPtr< FUICommandInfo > FocusViewportToSelection;

	/** Toggle automatic exposure */
	TSharedPtr< FUICommandInfo > ToggleAutoExposure;

	/** Magic ID to differentiate the auto expose setting from the fixed exposure settings */
	static const int32 AutoExposureRadioID = 999;

	/** Exposure Commands */
	TSharedPtr< FUICommandInfo > FixedExposure4m;
	TSharedPtr< FUICommandInfo > FixedExposure3m;
	TSharedPtr< FUICommandInfo > FixedExposure2m;
	TSharedPtr< FUICommandInfo > FixedExposure1m;
	TSharedPtr< FUICommandInfo > FixedExposure0;
	TSharedPtr< FUICommandInfo > FixedExposure1p;
	TSharedPtr< FUICommandInfo > FixedExposure2p;
	TSharedPtr< FUICommandInfo > FixedExposure3p;
	TSharedPtr< FUICommandInfo > FixedExposure4p;

	/**
	 * Grid commands
	 */

	/** Enables or disables snapping to the grid when dragging objects around */
	TSharedPtr< FUICommandInfo > LocationGridSnap;

	/** Enables or disables snapping to a rotational grid while rotating objects */
	TSharedPtr< FUICommandInfo > RotationGridSnap;

	/** Enables or disables snapping to a 2D layer */
	TSharedPtr< FUICommandInfo > Layer2DSnap;

	/** Enables or disables snapping to a scaling grid while scaling objects */
	TSharedPtr< FUICommandInfo > ScaleGridSnap;

	/** Enables or disables snapping to surfaces when dragging in the viewport */
	TSharedPtr< FUICommandInfo > SurfaceSnapping;
public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};

/**
 * Get the viewmode otions menu label.
 * 
 * @param	ViewModeIndex	The current viewmode, as the options change  per viewmode.

 * @return	The label.
 */
UNREALED_API FText GetViewModeOptionsMenuLabel(EViewModeIndex ViewModeIndex);

/**
 * Build the viewmode otions menu.
 * 
 * @param	CommandList		The command list to be used with the menu builder.
 * @param	ViewModeIndex	The current viewmode, as the options change  per viewmode.
 * @param	FeatureLevel	The feature level to build the option menu for.
 * @param	ParamNameMaps	A map allowing to convert from each menu index to an asset name.

 * @return	The widget containing the viewmode options.
 */
UNREALED_API TSharedRef<SWidget> BuildViewModeOptionsMenu(TSharedPtr<FUICommandList> CommandList, EViewModeIndex ViewModeIndex, ERHIFeatureLevel::Type FeatureLevel, TMap<int32, FName>& ParamNameMap);
