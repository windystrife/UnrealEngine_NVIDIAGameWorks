// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimViewportShowCommands.h"

#define LOCTEXT_NAMESPACE "AnimViewportShowCommands"

void FAnimViewportShowCommands::RegisterCommands()
{
	UI_COMMAND( ToggleGrid, "Grid", "Display Grid", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( AutoAlignFloorToMesh, "Auto Align Floor To Mesh", "Auto aligns floor to mesh bounds", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( MuteAudio, "Mute Audio", "Mutes audio from the preview", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( UseAudioAttenuation, "Use Audio Attenuation", "Use audio attentuation when playing back audio in the preview", EUserInterfaceActionType::ToggleButton, FInputChord() );
	
	UI_COMMAND(ProcessRootMotion, "Process Root Motion", "Moves preview based on animation root motion", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND( ShowRetargetBasePose, "Retarget Base Pose", "Show retarget Base pose on preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowBound, "Bound", "Show bound on preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( UseInGameBound, "In-game Bound", "Use in-game bound on preview mesh when showing bounds. Otherwise bounds will always be calculated from bones alone.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowPreviewMesh, "Mesh", "Show the preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowMorphTargets, "MorphTargets", "Display Applied Morph Targets of the mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( ShowBoneNames, "Bone Names", "Display Bone Names in Viewport", EUserInterfaceActionType::ToggleButton, FInputChord() );

	// below 3 menus are radio button styles
	UI_COMMAND(ShowDisplayInfoBasic, "Basic", "Display Basic Mesh Info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowDisplayInfoDetailed, "Detailed", "Display Detailed Mesh Info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowDisplayInfoSkelControls, "Skeletal Controls", "Display selected skeletal control info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(HideDisplayInfo, "None", "Hide All Display Info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND( ShowOverlayNone, "None", "Clear Overlay Display", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND( ShowBoneWeight, "Selected Bone Weight", "Display color overlay of the weight from Selected Bone in Viewport", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( ShowMorphTargetVerts, "Selected Morphtarget Vertices", "Display color overlay with the chnage of Selected Morphtarget in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(ShowVertexColors, "Vertex Colors", "Display mesh vertex colors.", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND( ShowRawAnimation, "Uncompressed Animation", "Display Skeleton With Uncompressed Animation Data", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowNonRetargetedAnimation, "NonRetargeted Animation", "Display Skeleton With non retargeted Animation Data", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowAdditiveBaseBones, "Additive Base", "Display Skeleton In Additive Base Pose", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowSourceRawAnimation, "Source Animation", "Display Skeleton In Source Raw Animation if you have Track Curves Modified.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowBakedAnimation, "Baked Animation", "Display Skeleton In Baked Raw Animation if you have Track Curves Modified.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowSockets, "Sockets", "Display socket hitpoints", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( ShowBoneDrawNone, "None", "Hides bone selection", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND( ShowBoneDrawSelected, "Selected Only", "Shows only the selected bone", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND( ShowBoneDrawSelectedAndParents, "Selected and Parents", "Shows the selected bone and its parents, to the root", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND( ShowBoneDrawAll, "All Hierarchy", "Shows all hierarchy joints", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND( ShowLocalAxesNone, "None", "Hides all local hierarchy axis", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( ShowLocalAxesSelected, "Selected Only", "Shows only the local bone axis of the selected bones", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( ShowLocalAxesAll, "All", "Shows all local hierarchy axes", EUserInterfaceActionType::RadioButton, FInputChord() );

	UI_COMMAND( DisableClothSimulation, "Disable Cloth Simulation", "Disable Cloth Simulation and Show non-simulated mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ApplyClothWind, "Apply Wind", "Apply Wind to Clothing", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( EnableCollisionWithAttachedClothChildren, "Collide with Cloth Children", "Enables collision detection between collision primitives in the base mesh and clothing on any attachments in the preview scene.", EUserInterfaceActionType::ToggleButton, FInputChord() );	
	UI_COMMAND(PauseClothWithAnim, "Pause with Animation", "If enabled, the clothing simulation will pause when the animation is paused using the scrub panel", EUserInterfaceActionType::ToggleButton, FInputChord());

	// below 3 menus are radio button styles
	UI_COMMAND(ShowAllSections, "Show All Sections", "Display All sections including Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowOnlyClothSections, "Show Only Cloth Sections", "Display Only Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(HideOnlyClothSections, "Hide Only Cloth Sections", "Display All Except Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE