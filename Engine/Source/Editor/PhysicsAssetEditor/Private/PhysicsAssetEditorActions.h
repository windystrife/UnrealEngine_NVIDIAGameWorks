// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

/*-----------------------------------------------------------------------------
   FPhysicsAssetEditorCommands
-----------------------------------------------------------------------------*/

class FPhysicsAssetEditorCommands : public TCommands<FPhysicsAssetEditorCommands>
{
public:
	/** Constructor */
	FPhysicsAssetEditorCommands() 
		: TCommands<FPhysicsAssetEditorCommands>("PhysicsAssetEditor", NSLOCTEXT("Contexts", "PhysicsAssetEditor", "PhysicsAssetEditor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}
	
	/** See tooltips in cpp for documentation */
	
	TSharedPtr<FUICommandInfo> RegenerateBodies;
	TSharedPtr<FUICommandInfo> AddBodies;
	TSharedPtr<FUICommandInfo> ApplyPhysicalMaterial;
	TSharedPtr<FUICommandInfo> Snap;
	TSharedPtr<FUICommandInfo> CopyProperties;
	TSharedPtr<FUICommandInfo> PasteProperties;
	TSharedPtr<FUICommandInfo> RepeatLastSimulation;
	TSharedPtr<FUICommandInfo> SimulationNoGravity;
	TSharedPtr<FUICommandInfo> SelectedSimulation;
	TSharedPtr<FUICommandInfo> SimulationAll;
	TSharedPtr<FUICommandInfo> MeshRenderingMode_Solid;
	TSharedPtr<FUICommandInfo> MeshRenderingMode_Wireframe;
	TSharedPtr<FUICommandInfo> MeshRenderingMode_None;
	TSharedPtr<FUICommandInfo> CollisionRenderingMode_Solid;
	TSharedPtr<FUICommandInfo> CollisionRenderingMode_Wireframe;
	TSharedPtr<FUICommandInfo> CollisionRenderingMode_None;
	TSharedPtr<FUICommandInfo> ConstraintRenderingMode_None;
	TSharedPtr<FUICommandInfo> ConstraintRenderingMode_AllPositions;
	TSharedPtr<FUICommandInfo> ConstraintRenderingMode_AllLimits;
	TSharedPtr<FUICommandInfo> MeshRenderingMode_Simulation_Solid;
	TSharedPtr<FUICommandInfo> MeshRenderingMode_Simulation_Wireframe;
	TSharedPtr<FUICommandInfo> MeshRenderingMode_Simulation_None;
	TSharedPtr<FUICommandInfo> CollisionRenderingMode_Simulation_Solid;
	TSharedPtr<FUICommandInfo> CollisionRenderingMode_Simulation_Wireframe;
	TSharedPtr<FUICommandInfo> CollisionRenderingMode_Simulation_None;
	TSharedPtr<FUICommandInfo> ConstraintRenderingMode_Simulation_None;
	TSharedPtr<FUICommandInfo> ConstraintRenderingMode_Simulation_AllPositions;
	TSharedPtr<FUICommandInfo> ConstraintRenderingMode_Simulation_AllLimits;
	TSharedPtr<FUICommandInfo> RenderOnlySelectedSolid;
	TSharedPtr<FUICommandInfo> DrawConstraintsAsPoints;
	TSharedPtr<FUICommandInfo> ToggleMassProperties;
	TSharedPtr<FUICommandInfo> DisableCollision;
	TSharedPtr<FUICommandInfo> DisableCollisionAll;
	TSharedPtr<FUICommandInfo> EnableCollision;
	TSharedPtr<FUICommandInfo> EnableCollisionAll;
	TSharedPtr<FUICommandInfo> WeldToBody;
	TSharedPtr<FUICommandInfo> AddSphere;
	TSharedPtr<FUICommandInfo> AddSphyl;
	TSharedPtr<FUICommandInfo> AddBox;
	TSharedPtr<FUICommandInfo> DeletePrimitive;
	TSharedPtr<FUICommandInfo> DuplicatePrimitive;
	TSharedPtr<FUICommandInfo> ResetConstraint;
	TSharedPtr<FUICommandInfo> SnapConstraint;
	TSharedPtr<FUICommandInfo> ConvertToBallAndSocket;
	TSharedPtr<FUICommandInfo> ConvertToHinge;
	TSharedPtr<FUICommandInfo> ConvertToPrismatic;
	TSharedPtr<FUICommandInfo> ConvertToSkeletal;
	TSharedPtr<FUICommandInfo> DeleteConstraint;
	TSharedPtr<FUICommandInfo> ShowSkeleton;
	TSharedPtr<FUICommandInfo> MakeBodyKinematic;
	TSharedPtr<FUICommandInfo> MakeBodySimulated;
	TSharedPtr<FUICommandInfo> MakeBodyDefault;
	TSharedPtr<FUICommandInfo> KinematicAllBodiesBelow;
	TSharedPtr<FUICommandInfo> SimulatedAllBodiesBelow;
	TSharedPtr<FUICommandInfo> MakeAllBodiesBelowDefault;
	TSharedPtr<FUICommandInfo> DeleteBody;
	TSharedPtr<FUICommandInfo> DeleteAllBodiesBelow;
	TSharedPtr<FUICommandInfo> SelectAllBodies;
	TSharedPtr<FUICommandInfo> SelectAllConstraints;
	TSharedPtr<FUICommandInfo> DeselectAll;
	TSharedPtr<FUICommandInfo> Mirror;
	TSharedPtr<FUICommandInfo> NewPhysicalAnimationProfile;
	TSharedPtr<FUICommandInfo> DuplicatePhysicalAnimationProfile;
	TSharedPtr<FUICommandInfo> DeleteCurrentPhysicalAnimationProfile;
	TSharedPtr<FUICommandInfo> AddBodyToPhysicalAnimationProfile;
	TSharedPtr<FUICommandInfo> RemoveBodyFromPhysicalAnimationProfile;
	TSharedPtr<FUICommandInfo> NewConstraintProfile;
	TSharedPtr<FUICommandInfo> DuplicateConstraintProfile;
	TSharedPtr<FUICommandInfo> DeleteCurrentConstraintProfile;
	TSharedPtr<FUICommandInfo> AddConstraintToCurrentConstraintProfile;
	TSharedPtr<FUICommandInfo> RemoveConstraintFromCurrentConstraintProfile;
	TSharedPtr<FUICommandInfo> ShowBodies;
	TSharedPtr<FUICommandInfo> ShowConstraints;
	TSharedPtr<FUICommandInfo> ShowPrimitives;

	/** Hotkey only commands */
	TSharedPtr<FUICommandInfo> DeleteSelected;
	TSharedPtr<FUICommandInfo> CycleConstraintOrientation;
	TSharedPtr<FUICommandInfo> CycleConstraintActive;
	TSharedPtr<FUICommandInfo> ToggleSwing1;
	TSharedPtr<FUICommandInfo> ToggleSwing2;
	TSharedPtr<FUICommandInfo> ToggleTwist;
	TSharedPtr<FUICommandInfo> FocusOnSelection;
	

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
