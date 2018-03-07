// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "IDistCurveEditor.h"

class UInterpCurveEdSetup;

extern const FName DistCurveEditorAppIdentifier;


/*-----------------------------------------------------------------------------
   IDistributionCurveEditorModule
-----------------------------------------------------------------------------*/

class IDistributionCurveEditorModule : public IModuleInterface
{
public:
	/**  */
	virtual TSharedRef<IDistributionCurveEditor> CreateCurveEditorWidget(UInterpCurveEdSetup* EdSetup, FCurveEdNotifyInterface* NotifyObject) = 0;
	virtual TSharedRef<IDistributionCurveEditor> CreateCurveEditorWidget(UInterpCurveEdSetup* EdSetup, FCurveEdNotifyInterface* NotifyObject, IDistributionCurveEditor::FCurveEdOptions Options) = 0;
};
