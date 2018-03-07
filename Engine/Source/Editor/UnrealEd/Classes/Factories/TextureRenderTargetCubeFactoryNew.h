// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// TextureRenderTargetCubeFactoryNew
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "TextureRenderTargetCubeFactoryNew.generated.h"

UCLASS(MinimalAPI, hidecategories=(Object, Texture))
class UTextureRenderTargetCubeFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** width of new texture */
	UPROPERTY(meta=(ToolTip="Width of the texture render target"))
	int32 Width;

	/** surface format of new texture */
	UPROPERTY(meta=(ToolTip="Pixel format of the texture render target"))
	uint8 Format;


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};
