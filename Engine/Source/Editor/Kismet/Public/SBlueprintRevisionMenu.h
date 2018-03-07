// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "ISourceControlProvider.h"

class SVerticalBox;
class UBlueprint;
struct FRevisionInfo;

class KISMET_API SBlueprintRevisionMenu : public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FOnRevisionSelected, FRevisionInfo const&)

public:
	SLATE_BEGIN_ARGS(SBlueprintRevisionMenu)
		: _bIncludeLocalRevision(false)
	{}
		SLATE_ARGUMENT(bool, bIncludeLocalRevision)
		SLATE_EVENT(FOnRevisionSelected, OnRevisionSelected)
	SLATE_END_ARGS()

	~SBlueprintRevisionMenu();

	void Construct(const FArguments& InArgs, UBlueprint const* Blueprint);

private: 
	/** Delegate used to determine the visibility 'in progress' widgets */
	EVisibility GetInProgressVisibility() const;
	/** Delegate used to determine the visibility of the cancel button */
	EVisibility GetCancelButtonVisibility() const;

	/** Delegate used to cancel a source control operation in progress */
	FReply OnCancelButtonClicked() const;
	/** Callback for when the source control operation is complete */
	void OnSourceControlQueryComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/**  */
	bool bIncludeLocalRevision;
	/**  */
	FOnRevisionSelected OnRevisionSelected;
	/** The name of the file we want revision info for */
	FString Filename;
	/** The box we are using to display our menu */
	TSharedPtr<SVerticalBox> MenuBox;
	/** The source control operation in progress */
	TSharedPtr<FUpdateStatus, ESPMode::ThreadSafe> SourceControlQueryOp;
	/** The state of the SCC query */
	uint32 SourceControlQueryState;
};
