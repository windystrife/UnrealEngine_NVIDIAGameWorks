// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Preferences/CascadeOptions.h"
#include "Preferences/CurveEdOptions.h"
#include "Preferences/MaterialEditorOptions.h"
#include "Preferences/PersonaOptions.h"
#include "Preferences/PhysicsAssetEditorOptions.h"

// @todo find a better place for all of this, preferably in the appropriate modules
// though this would require the classes to be relocated as well

//
// UCascadeOptions
// 
UCascadeOptions::UCascadeOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// UPhysicsAssetEditorOptions /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
UPhysicsAssetEditorOptions::UPhysicsAssetEditorOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PhysicsBlend = 1.0f;
	bUpdateJointsFromAnimation = false;
	MaxFPS = -1;

	// These should duplicate defaults from UPhysicsHandleComponent
	HandleLinearDamping = 200.0f;
	HandleLinearStiffness = 750.0f;
	HandleAngularDamping = 500.0f;
	HandleAngularStiffness = 1500.0f;
	InterpolationSpeed = 50.f;

	bShowConstraintsAsPoints = false;
	ConstraintDrawSize = 1.0f;

	// view options
	MeshViewMode = EPhysicsAssetEditorRenderMode::Solid;
	CollisionViewMode = EPhysicsAssetEditorRenderMode::Solid;
	ConstraintViewMode = EPhysicsAssetEditorConstraintViewMode::AllLimits;
	SimulationMeshViewMode = EPhysicsAssetEditorRenderMode::Solid;
	SimulationCollisionViewMode = EPhysicsAssetEditorRenderMode::Solid;
	SimulationConstraintViewMode = EPhysicsAssetEditorConstraintViewMode::None;

	CollisionOpacity = 0.3f;
	bSolidRenderingForSelectedOnly = false;
}

UMaterialEditorOptions::UMaterialEditorOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


UCurveEdOptions::UCurveEdOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPersonaOptions::UPersonaOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultLocalAxesSelection(2)
	, DefaultBoneDrawSelection(1)
	, bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons(true)
{
	ViewModeIndex = VMI_Lit;

	SectionTimingNodeColor = FLinearColor(0.0f, 1.0f, 0.0f);
	NotifyTimingNodeColor = FLinearColor(1.0f, 0.0f, 0.0f);
	BranchingPointTimingNodeColor = FLinearColor(0.5f, 1.0f, 1.0f);

	bAutoAlignFloorToMesh = true;

	NumFolderFiltersInAssetBrowser = 2;

	bUseAudioAttenuation = true;
}

void UPersonaOptions::SetShowGrid( bool bInShowGrid )
{
	bShowGrid = bInShowGrid;
	SaveConfig();
}

void UPersonaOptions::SetHighlightOrigin( bool bInHighlightOrigin )
{
	bHighlightOrigin = bInHighlightOrigin;
	SaveConfig();
}

void UPersonaOptions::SetGridSize( int32 InGridSize )
{
	GridSize = InGridSize;
	SaveConfig();
}

void UPersonaOptions::SetViewModeIndex( EViewModeIndex InViewModeIndex )
{
	ViewModeIndex = InViewModeIndex;
	SaveConfig();
}

void UPersonaOptions::SetShowFloor( bool bInShowFloor )
{
	bShowFloor = bInShowFloor;
	SaveConfig();
}

void UPersonaOptions::SetAutoAlignFloorToMesh(bool bInAutoAlignFloorToMesh)
{
	bAutoAlignFloorToMesh = bInAutoAlignFloorToMesh;
	SaveConfig();
}

void UPersonaOptions::SetShowSky( bool bInShowSky )
{
	bShowSky = bInShowSky;
	SaveConfig();
}

void UPersonaOptions::SetMuteAudio( bool bInMuteAudio )
{
	bMuteAudio = bInMuteAudio;
	SaveConfig();
}

void UPersonaOptions::SetUseAudioAttenuation( bool bInUseAudioAttenuation )
{
	bUseAudioAttenuation = bInUseAudioAttenuation;
	SaveConfig();
}

void UPersonaOptions::SetViewFOV( float InViewFOV )
{
	ViewFOV = InViewFOV;
	SaveConfig();
}

void UPersonaOptions::SetDefaultLocalAxesSelection( uint32 InDefaultLocalAxesSelection )
{
	DefaultLocalAxesSelection = InDefaultLocalAxesSelection;
	SaveConfig();
}

void UPersonaOptions::SetDefaultBoneDrawSelection(uint32 InDefaultBoneDrawSelection)
{
	DefaultBoneDrawSelection = InDefaultBoneDrawSelection;
	SaveConfig();
}

void UPersonaOptions::SetShowMeshStats( int32 InShowMeshStats )
{
	ShowMeshStats = InShowMeshStats;
	SaveConfig();
}


void UPersonaOptions::SetSectionTimingNodeColor(const FLinearColor& InColor)
{
	SectionTimingNodeColor = InColor;
	SaveConfig();
}

void UPersonaOptions::SetNotifyTimingNodeColor(const FLinearColor& InColor)
{
	NotifyTimingNodeColor = InColor;
	SaveConfig();
}

void UPersonaOptions::SetBranchingPointTimingNodeColor(const FLinearColor& InColor)
{
	BranchingPointTimingNodeColor = InColor;
	SaveConfig();
}
