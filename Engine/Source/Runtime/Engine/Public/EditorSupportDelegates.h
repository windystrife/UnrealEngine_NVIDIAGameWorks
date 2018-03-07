// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/** 
 * FEditorSupportDelegates
 * Delegates that are needed for proper editor functionality, but are accessed or triggered in engine code.
 **/
struct ENGINE_API FEditorSupportDelegates
{
	/** delegate type for force property window rebuild events ( Params: UObject* Object ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnForcePropertyWindowRebuild, UObject*); 
	/** delegate type for material texture setting change events ( Params: UMaterialIterface* Material ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMaterialTextureSettingsChanged, class UMaterialInterface*);	
	/** delegate type for windows messageing events ( Params: FViewport* Viewport, uint32 Message )*/
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWindowsMessage, class FViewport*, uint32);
	/** delegate type for material usage flags change events ( Params: UMaterial* material, int32 FlagThatChanged ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMaterialUsageFlagsChanged, class UMaterial*, int32); 
	/** delegate type for vector parameter default change event */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnVectorParameterDefaultChanged, class UMaterialExpression*, FName, const FLinearColor&); 
	/** delegate type for scalar parameter default change event */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnScalarParameterDefaultChanged, class UMaterialExpression*, FName, float); 

	/** Called when all viewports need to be redrawn */
	static FSimpleMulticastDelegate RedrawAllViewports;
	/** Called when the editor is cleansing of transient references before a map change event */
	static FSimpleMulticastDelegate CleanseEditor;
	/** Called when the world is modified */
	static FSimpleMulticastDelegate WorldChange;
	/** Sent to force a property window rebuild */
	static FOnForcePropertyWindowRebuild ForcePropertyWindowRebuild;
	/** Sent when events happen that affect how the editors UI looks (mode changes, grid size changes, etc) */
	static FSimpleMulticastDelegate UpdateUI;
	/** Called for a material after the user has change a texture's compression settings.
		Needed to notify the material editors that the need to reattach their preview objects */
	static FOnMaterialTextureSettingsChanged MaterialTextureSettingsChanged;
	/** Refresh property windows w/o creating/destroying controls */
	static FSimpleMulticastDelegate RefreshPropertyWindows;
	/** Sent before the given windows message is handled in the given viewport */
	static FOnWindowsMessage PreWindowsMessage;
	/** Sent after the given windows message is handled in the given viewport */
	static FOnWindowsMessage PostWindowsMessage;
	/** Sent after the usages flags on a material have changed*/
	static FOnMaterialUsageFlagsChanged MaterialUsageFlagsChanged;
	/** Sent after vector param default changed */
	static FOnVectorParameterDefaultChanged VectorParameterDefaultChanged;
	/** Sent after scalar param default changed */
	static FOnScalarParameterDefaultChanged ScalarParameterDefaultChanged;
};

#endif // WITH_EDITOR
