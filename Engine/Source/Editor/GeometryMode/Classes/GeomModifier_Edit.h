// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/**
 *
 */


//~=============================================================================
// GeomModifier_Edit: Maniupalating selected objects with the widget
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GeomModifier.h"
#include "GeomModifier_Edit.generated.h"

class FViewport;

UCLASS(autoexpandcategories=Settings)
class UGeomModifier_Edit : public UGeomModifier
{
	GENERATED_UCLASS_BODY()



	//~ Begin UGeomModifier Interface
	virtual bool InputDelta(class FEditorViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale) override;	
	//~ End UGeomModifier Interface
};



