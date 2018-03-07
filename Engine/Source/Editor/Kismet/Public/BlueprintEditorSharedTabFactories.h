// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

#define LOCTEXT_NAMESPACE "BlueprintEditor"

//////////////////////////////////////////////////////////////////////////
// FCompilerResultsSummoner

struct KISMET_API FCompilerResultsSummoner : public FWorkflowTabFactory
{
public:
	FCompilerResultsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return LOCTEXT("CompilerResultsTooltip", "The compiler results tab shows any errors or warnings generated when compiling this Blueprint.");
	}
};

#undef LOCTEXT_NAMESPACE
