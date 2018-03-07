// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories.h"

class FLandscapeSplineTextObjectFactory : protected FCustomizableTextObjectFactory
{
public:
	FLandscapeSplineTextObjectFactory(FFeedbackContext* InWarningContext = GWarn);

	TArray<UObject*> ImportSplines(UObject* InParent, const TCHAR* TextBuffer);

protected:
	TArray<UObject*> OutObjects;

	virtual void ProcessConstructedObject(UObject* CreatedObject) override;
	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override;
};
