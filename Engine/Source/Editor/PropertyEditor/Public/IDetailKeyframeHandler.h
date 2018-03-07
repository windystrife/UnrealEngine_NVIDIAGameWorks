// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;

class IDetailKeyframeHandler
{
public:
	virtual ~IDetailKeyframeHandler(){}

	virtual bool IsPropertyKeyable(UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const = 0;

	virtual bool IsPropertyKeyingEnabled() const = 0;

	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) = 0;

};
