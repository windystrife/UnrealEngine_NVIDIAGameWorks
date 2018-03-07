// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/**
 * Deletes selected objects.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GeomModifier_Edit.h"
#include "GeomModifier_Delete.generated.h"

UCLASS()
class UGeomModifier_Delete : public UGeomModifier_Edit
{
	GENERATED_UCLASS_BODY()


	//~ Begin UGeomModifier Interface
	virtual bool Supports() override;
protected:
	virtual bool OnApply() override;
	//~ End UGeomModifier Interface
};



