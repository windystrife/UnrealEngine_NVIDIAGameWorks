// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "Styling/SlateColor.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "OnlineSubsystemTypes.h"
#define SUPPRESS_MONOLITHIC_HEADER_WARNINGS 1 // This is necessary because a game project may contain an older revision of the OnlineSubsystem plugin, which includes monolithic headers.
#include "OnlineSubsystem.h"
#undef SUPPRESS_MONOLITHIC_HEADER_WARNINGS

class FExtender;
class FMenuBuilder;
class FSurvey;
class FToolBarBuilder;
class FUICommandList;
class SWidget;
class SWindow;

DECLARE_DELEGATE_OneParam( FOnBrushLoaded, const TSharedPtr< FSlateDynamicImageBrush >& /*Brush*/ );

namespace EContentInitializationState
{
	enum Type
	{
		NotStarted = 0,
		Working = 1,
		Success = 2,
		Failure = 4
	};
}

enum class ECultureSpecification
{
	Full = 0,
	LanguageOnly = 1,
	None = 2
};

class FSurvey;
class FSurveyBranch;

class FEpicSurvey : public TSharedFromThis< FEpicSurvey > 
{
public:
	~FEpicSurvey();

	static TSharedRef< FEpicSurvey > Create();

	const TArray< TSharedRef< FSurvey > >& GetSurveys() { return Surveys; }

	void OpenEpicSurveyWindow();

	void LoadCloudFileAsBrush( const FString& Filename, FOnBrushLoaded Callback );
	void LoadSurveys();

	bool PromptSurvey( const FGuid& SurveyName );

	void SetActiveSurvey( const TSharedPtr< FSurvey >& Survey, bool AutoPrompted = true );
	bool ShouldPulseSurveyIcon( const TSharedRef< FSurvey >& Survey ) const;
	void SubmitSurvey( const TSharedRef< FSurvey >& Survey );

	/** Adds a new branch from which to track points */
	void AddBranch( const FString& Branch );

	/** Adds Points to the BranchName, if BranchName exists */
	void AddToBranchPoints( const FString& BranchName, const int32 Points );

	/** returns the branch points for the BranchName if exists, else zero */
	const int32 GetBranchPoints( const FString& BranchName ) const;

	/** Gets (or loads) the branch survey by Filename */
	TSharedPtr< FSurvey > GetBranchSurvey( const FString& Filename );
	
private:
	FEpicSurvey( const TSharedRef< FUICommandList >& InActionList );
	void Initialize();
	void OnEpicSurveyWindowClosed(const TSharedRef<SWindow>& InWindow);

	void OnEnumerateFilesComplete(bool bSuccess, const FString& ErrorString);
	void OnReadFileComplete(bool bSuccess, const FString& Filename);
	void LoadSurveyIndexFile();

	void AddEpicContentMenus( FMenuBuilder& MenuBuilder );
	void AddEpicSurveyNotifier( FToolBarBuilder& ToolBarBuilder );

	FSlateColor GetInvertedForegroundIfHovered( const TWeakPtr< SWidget > WidgetPtr ) const;

	void HandledMainFrameLoad( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow );

	TSharedPtr< FSlateDynamicImageBrush > LoadRawDataAsBrush( FName ResourceName, const TArray< uint8 >& RawData ) const;

	void DisplayToolbarNotification();
	void DisplayNotification();
	void StartPulseSurveyIcon();
	void EndPulseSurveyIcon();

	void AcceptSurveyNotification();
	void CancelSurveyNotification();

	void InitializeTitleCloud();

private:
	/** ID name for the plugins editor major tab */
	EContentInitializationState::Type InitializationState;

	TSharedPtr< FExtender > MenuExtender;
	TSharedPtr< FExtender > NotificationBarExtender;

	TSharedRef< FUICommandList > ActionList;

	TSharedPtr< FSurvey > ActiveSurvey;

	/** All surveys loaded from the survey_index.json file */
	TArray< TSharedRef< FSurvey > > Surveys;

	/** All surveys loaded as branches by being referred to by other surveys  */
	TMap< FString, TSharedPtr< FSurvey > > BranchSurveys;

	IOnlineTitleFilePtr TitleCloud;

	static const FString SurveyIndexFilename;
	FCloudFileHeader SurveyIndexCloudFile;

	TMultiMap< FString, FOnBrushLoaded > FilenameToLoadCallbacks;
	TWeakPtr< SWindow > SurveyWindow;
	TWeakPtr< SWindow > RootWindow;

	/** The delay time before the user is notified that there is a survey available */
	int32 SurveyNotificationDelayTime;

	/** The time to display the survey notification */
	float SurveyNotificationDuration;

	/** The time between pulsing the icon to alert the user after the notification has occurred */
	int32 SurveyPulseTimeInterval;

	/** The time the icon should be pulsed */
	int32 SurveyPulseDuration;

	/** The survey notification */
	TWeakPtr<SNotificationItem> DisplaySurveyNotification;

	/** The delegate for the notification callback */
	FTimerDelegate NotificationDelegate;

	/** The delegate for starting the icon pulsing */
	FTimerDelegate StartPulseSurveyIconDelegate;

	/** The delegate for ending the icon pulsing */
	FTimerDelegate EndPulseSurveyIconDelegate;

	/** Whether the Survey icon should be pulsing */
	bool bSurveyIconPulsing;

	/** Number of points associated with the branch name */
	TMap<FString, int32> BranchPoints;

	/** Are we showing the toolbar notification icon */
	bool bIsShowingToolbarNotification;

	/** The specification of the culture we are using to display surveys */
	ECultureSpecification CurrentCulture;

private:
	FTimerHandle DisplayNotificationTimerHandle;
};
