// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/**
 * Optimizes selected objects by attempting to merge their polygons back together.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GeomModifier_Triangulate.h"
#include "GeomModifier_Optimize.generated.h"

UCLASS()
class UGeomModifier_Optimize : public UGeomModifier_Triangulate
{
	GENERATED_UCLASS_BODY()


	//~ Begin UGeomModifier Interface
protected:
	virtual bool OnApply() override;
	//~ End UGeomModifier Interface
};



