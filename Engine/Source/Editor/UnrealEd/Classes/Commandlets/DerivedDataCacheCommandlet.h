// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DerivedDataCacheCommandlet.cpp: Commandlet for DDC maintenence
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "DerivedDataCacheCommandlet.generated.h"

UCLASS()
class UDerivedDataCacheCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	// We hook this up to a delegate to avoid reloading textures and whatnot
	TSet<FName> ProcessedPackages;

	void MaybeMarkPackageAsAlreadyLoaded(UPackage *Package);
};


