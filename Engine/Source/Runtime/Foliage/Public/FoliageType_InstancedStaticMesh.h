// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "FoliageType.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "FoliageType_InstancedStaticMesh.generated.h"

UCLASS(hidecategories=Object, editinlinenew, MinimalAPI)
class UFoliageType_InstancedStaticMesh : public UFoliageType
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Mesh, meta=(DisplayThumbnail="true"))
	UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Mesh, Meta = (ToolTip = "Material overrides for foliage instances."))
	TArray<class UMaterialInterface*> OverrideMaterials;
		
	/** The component class to use for foliage instances. 
	  * You can make a Blueprint subclass of FoliageInstancedStaticMeshComponent to implement custom behavior and assign that class here. */
	UPROPERTY(EditAnywhere, Category = Mesh)
	TSubclassOf<UFoliageInstancedStaticMeshComponent> ComponentClass;

	virtual UStaticMesh* GetStaticMesh() const override
	{
		return Mesh;
	}

	virtual void SetStaticMesh(UStaticMesh* InStaticMesh) override
	{
		Mesh = InStaticMesh;
	}

	virtual UClass* GetComponentClass() const override
	{
		return *ComponentClass;
	}
};
