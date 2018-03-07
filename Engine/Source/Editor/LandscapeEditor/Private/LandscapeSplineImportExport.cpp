// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeSplineImportExport.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineControlPoint.h"

#define LOCTEXT_NAMESPACE "Landscape"

FLandscapeSplineTextObjectFactory::FLandscapeSplineTextObjectFactory(FFeedbackContext* InWarningContext /*= GWarn*/)
	: FCustomizableTextObjectFactory(InWarningContext)
{
}

TArray<UObject*> FLandscapeSplineTextObjectFactory::ImportSplines(UObject* InParent, const TCHAR* TextBuffer)
{
	if (FParse::Command(&TextBuffer, TEXT("BEGIN SPLINES")))
	{
		ProcessBuffer(InParent, RF_Transactional, TextBuffer);

		//FParse::Command(&TextBuffer, TEXT("END SPLINES"));
	}

	return OutObjects;
}

void FLandscapeSplineTextObjectFactory::ProcessConstructedObject(UObject* CreatedObject)
{
	OutObjects.Add(CreatedObject);

	CreatedObject->PostEditImport();
}

bool FLandscapeSplineTextObjectFactory::CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const
{
	if (ObjectClass == ULandscapeSplineControlPoint::StaticClass() ||
		ObjectClass == ULandscapeSplineSegment::StaticClass())
	{
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
