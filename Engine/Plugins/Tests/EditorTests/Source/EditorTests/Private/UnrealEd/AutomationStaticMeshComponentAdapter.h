// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StaticMeshComponentAdapter.h"
#include "UObject/Package.h"

/** Adapter which supplies the transient package as outer to store the data in, used for automation testing */
class FAutomationStaticMeshComponentAdapter : public FStaticMeshComponentAdapter
{
public:
	FAutomationStaticMeshComponentAdapter(UStaticMeshComponent* InStaticMeshComponent) : FStaticMeshComponentAdapter(InStaticMeshComponent) {}
	virtual UPackage* GetOuter() const override
	{
		return GetTransientPackage();
	}
};