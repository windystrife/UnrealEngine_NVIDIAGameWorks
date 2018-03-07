// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Import data and options used when importing a static mesh from fbx
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FbxExportOption.generated.h"

 // Fbx export compatibility
UENUM()
enum class EFbxExportCompatibility : uint8
{
	FBX_2010,
	FBX_2011,
	FBX_2012,
	FBX_2013,
	FBX_2014,
	FBX_2016,
	FBX_2018
};

UCLASS(config = EditorPerProjectUserSettings, MinimalAPI)
class UFbxExportOption : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	/** This will set the fbx sdk compatibility when exporting to fbx file. The default value is 2013 */
	UPROPERTY(EditAnywhere, config, Category = Exporter)
	EFbxExportCompatibility FbxExportCompatibility;

	/** If enabled, export with X axis as the front axis instead of default -Y */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Exporter)
	uint32 bForceFrontXAxis : 1;

	/** If enable, export vertex color */
	UPROPERTY(EditAnywhere, config, category = Mesh)
	uint32 VertexColor : 1;

	/** If enabled, export the level of detail */
	UPROPERTY(EditAnywhere, config, category = StaticMesh)
	uint32 LevelOfDetail : 1;

	/** If enabled, export collision */
	UPROPERTY(EditAnywhere, config, category = StaticMesh)
	uint32 Collision : 1;

	/** If enable, export welded vertices*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, category = StaticMesh)
	uint32 WeldedVertices : 1;

	/** If enable, Map skeletal actor motion to the root bone of the skeleton. */
	UPROPERTY(EditAnywhere, config, category = Animation)
	uint32 MapSkeletalMotionToRoot : 1;

	/* Set all the UProperty to the CDO value */
	void ResetToDefault();

	/* Save the UProperty to a local ini to retrieve the value the next time we call function LoadOptions() */
	virtual void SaveOptions();
	
	/* Load the UProperty data from a local ini which the value was store by the function SaveOptions() */
	virtual void LoadOptions();
};
