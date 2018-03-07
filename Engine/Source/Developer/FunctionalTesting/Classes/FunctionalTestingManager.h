// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "FunctionalTest.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FunctionalTestingManager.generated.h"

class IMessageLogListing;

namespace FFunctionalTesting
{
	extern const TCHAR* ReproStringTestSeparator;
	extern const TCHAR* ReproStringParamsSeparator;
}

UCLASS(BlueprintType, MinimalAPI)
class UFunctionalTestingManager : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(Transient)
	TArray<class AFunctionalTest*> TestsLeft;

	UPROPERTY(Transient)
	TArray<class AFunctionalTest*> AllTests;

	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnSetupTests;

	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnTestsComplete;

	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnTestsBegin;
	
	/**
	 * Triggers in sequence all functional tests found on the level.
	 * @return true if any tests have been triggered
	 */
	UFUNCTION(BlueprintCallable, Category="FunctionalTesting", meta=(WorldContext="WorldContextObject", CallableWithoutWorldContext ) )
	static bool RunAllFunctionalTests(UObject* WorldContextObject, bool bNewLog = true, bool bRunLooped = false, bool bWaitForNavigationBuildFinish = true, FString FailedTestsReproString = TEXT(""));
		
	bool IsRunning() const { return bIsRunning; }
	bool IsFinished() const { return bFinished; }
	bool IsLooped() const { return bLooped; }
	void SetLooped(const bool bNewLooped) { bLooped = bNewLooped; }

	void TickMe(float DeltaTime);

private:
	void LogMessage(const FString& MessageString, TSharedPtr<IMessageLogListing> LogListing = NULL);
	
protected:
	static UFunctionalTestingManager* GetManager(UObject* WorldContext);

	void TriggerFirstValidTest();
	void SetUpTests();

	void OnTestDone(class AFunctionalTest* FTest);

	bool RunFirstValidTest();

	void NotifyTestDone(class AFunctionalTest* FTest);

	void SetReproString(FString ReproString);
	void AllTestsDone();

	void OnWorldCleanedUp(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	virtual UWorld* GetWorld() const override;
	
	bool bIsRunning;
	bool bFinished;
	bool bLooped;
	bool bInitialDelayApplied;
	uint32 CurrentIteration;

	FFunctionalTestDoneSignature TestFinishedObserver;
	
	//FString GatheredFailedTestsReproString;
	FString StartingReproString;
	TArray<FString> TestReproStrings;

private:
	FTimerHandle TriggerFirstValidTestTimerHandle;
};

UCLASS(abstract, Blueprintable, MinimalAPI)
class APhasedAutomationActorBase : public AActor
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent, Category = "Automation")
	void OnFunctionalTestingComplete();

	UFUNCTION(BlueprintImplementableEvent, Category = "Automation")
	void OnFunctionalTestingBegin();
};
