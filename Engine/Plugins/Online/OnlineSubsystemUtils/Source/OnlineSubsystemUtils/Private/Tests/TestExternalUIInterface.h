// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "Interfaces/OnlineExternalUIInterface.h"

class IOnlineSubsystem;

#if WITH_DEV_AUTOMATION_TESTS

/** Enumeration of external UI tests */
namespace ETestExternalUIInterfaceState
{
	enum Type
	{
		Begin,
		ShowLoginUI,
		ShowFriendsUI,
		ShowInviteUI,
		ShowAchievementsUI,
		ShowWebURL,
		ShowProfileUI,
		End,
	};
}

/**
 * Class used to test the external UI interface
 */
 class FTestExternalUIInterface
 {

	/** The subsystem that was requested to be tested or the default if empty */
	const FString SubsystemName;

	/** Booleans that control which external UIs to test */
	bool bTestLoginUI;
	bool bTestFriendsUI;
	bool bTestInviteUI;
	bool bTestAchievementsUI;
	bool bTestWebURL;
	bool bTestProfileUI;

	/** The online interface to use for testing */
	IOnlineSubsystem* OnlineSub;

	/** Convenient access to the external UI interfaces */
	IOnlineExternalUIPtr ExternalUI;

	/** Delegate for external UI opening and closing */
	FOnExternalUIChangeDelegate ExternalUIChangeDelegate;

	/** ExternalUIChange delegate handle */
	FDelegateHandle ExternalUIChangeDelegateHandle;

	/** Current external UI test */
	ETestExternalUIInterfaceState::Type State;

	/** Completes testing and cleans up after itself */
	void FinishTest();

	/** Go to the next test */
	void StartNextTest();

	/** Specific test functions */
	bool TestLoginUI();
	bool TestFriendsUI();
	bool TestInviteUI();
	bool TestAchievementsUI();
	bool TestWebURL();
	bool TestProfileUI();

	/** Delegate called when external UI is opening and closing */
	void OnExternalUIChange(bool bIsOpening);

	/** Delegate executed when the user login UI has been closed. */
	void OnLoginUIClosed(TSharedPtr<const FUniqueNetId> LoggedInUserId, const int LocalUserId);

	/** Delegate executed when the user profile UI has been closed. */
	void OnProfileUIClosed();

	/** Delegate executed when the show web UI has been closed. */
	void OnShowWebUrlClosed(const FString& FinalUrl);

 public:

	/**
	 * Constructor
	 *
	 */
	FTestExternalUIInterface(const FString& InSubsystemName, bool bInTestLoginUI, bool bInTestFriendsUI, bool bInTestInviteUI, bool bInTestAchievementsUI, bool bInTestWebURL, bool bInTestProfileUI)
		:	SubsystemName(InSubsystemName)
		,	bTestLoginUI(bInTestLoginUI)
		,	bTestFriendsUI(bInTestFriendsUI)
		,	bTestInviteUI(bInTestInviteUI)
		,	bTestAchievementsUI(bInTestAchievementsUI)
		,	bTestWebURL(bInTestWebURL)
		,	bTestProfileUI(bInTestProfileUI)
		,	OnlineSub(NULL)
		,	ExternalUI(NULL)
		,	State(ETestExternalUIInterfaceState::Begin)
	{
	}

	/**
	 * Kicks off all of the testing process
	 *
	 */
	void Test();

 };

#endif //WITH_DEV_AUTOMATION_TESTS
