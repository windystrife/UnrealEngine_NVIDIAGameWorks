// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "SurveyBranch.h"
#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"

class FSurveyPage;

struct ESurveyType
{
	enum Type
	{
		Normal,
		Branch
	};
};

class FSurvey : public TSharedFromThis< FSurvey > 
{
public:

	~FSurvey() {};

	static TSharedPtr< FSurvey > Create( const TSharedRef< class FEpicSurvey >& EpicSurvey, const TSharedRef< FJsonObject >& JsonConfig );

public:

	void Initialize();
	EContentInitializationState::Type GetInitializationState();

	FGuid GetIdentifier() const { return Identifier; }
	FText GetDisplayName() const { return DisplayName; }
	FText GetInstructions() const { return Instructions; }
	const FSlateBrush* GetBanner() const { return BannerBrush.Get(); }
	const TArray< TSharedRef< FSurveyPage > >& GetPages() const { return Pages; }

	bool CanAutoPrompt() const { return AutoPrompt; }

	bool IsReadyToSubmit() const;
	void Submit();

	/** returns the type of survey */
	ESurveyType::Type GetSurveyType() const { return SurveyType; }

	/** returns the visibility of the 'Next' button. This takes account of potential branches */
	bool CanPageNext() const;

	/** returns the visibility of the 'Back' button */
	bool CanPageBack() const;

	/** Evaluates the branch conditions, inserts a new branch survey at the current page if needed and evaluates any valid answers into the branch points. */
	void EvaluateBranches();

	/** Called when next page is called */
	void OnPageNext();

	/** Called when back page is called */
	void OnPageBack();

	/** returns the current page index */
	int32 GetCurrentPage() const { return CurrentPageIndex; }

	/** sets the current page index */
	void SetCurrentPage( int32 InCurrentPageIndex ) { CurrentPageIndex=InCurrentPageIndex; }

	/** Set and Get the completed status. This is whether the branch has been taken */
	void SetBranchUsed( bool InBranchUsed ) { BranchUsed = InBranchUsed; }
	bool GetBranchUsed() const { return BranchUsed; }

	/** Evaluates all the answered questions branch points */
	void UpdateAllBranchPoints( bool bAdd ) const;

	void Reset();

private:

	FSurvey( const TSharedRef< class FEpicSurvey >& InEpicSurvey );

	void ApplyBranchResultToSurvey();

	void HandleBannerLoaded( const TSharedPtr< FSlateDynamicImageBrush >& Brush );
	TSharedPtr<FSurveyBranch> TestForBranch() const;

private:
	TSharedRef< class FEpicSurvey > EpicSurvey;
	EContentInitializationState::Type InitializationState; 

	FGuid Identifier;
	FText DisplayName;
	FText Instructions;
	ESurveyType::Type SurveyType;

	/** Whether this survey can be automatically prompted to the user */
	bool AutoPrompt;

	/** The version number of the survey. */
	int32 SurveyVersion;

	/** The maximum engine version supported by this survey */
	FEngineVersion MaxEngineVersion;

	/** The minimum engine version supported by this survey */
	FEngineVersion MinEngineVersion;

	/** The current survey version that the code represents */
	static int32 CurrentSurveyVersion;

	EContentInitializationState::Type BannerState;
	FString BannerBrushPath;
	TSharedPtr< FSlateBrush > BannerBrush;

	/** The pages that the survey has */
	TArray< TSharedRef< FSurveyPage > > Pages;

	/** The branches that the survey has */
	TArray< TSharedRef< FSurveyBranch > > Branches;

	/** The currently displayed page */
	int32 CurrentPageIndex;

	/** Whether this branch survey has been taken already */
	bool BranchUsed;
};
