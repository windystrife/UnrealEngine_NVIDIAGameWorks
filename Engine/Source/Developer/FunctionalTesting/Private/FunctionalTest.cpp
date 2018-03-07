// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTest.h"
#include "FunctionalTestingModule.h"
#include "Misc/Paths.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LatentActionManager.h"
#include "Components/BillboardComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "UObject/ConstructorHelpers.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Selection.h"
#include "FuncTestRenderingComponent.h"
#include "ObjectEditorUtils.h"
#include "VisualLogger/VisualLogger.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "DelayForFramesLatentAction.h"
#include "Engine/DebugCameraController.h"
#include "TraceQueryTestResults.h"
#include "Misc/RuntimeErrors.h"

namespace
{
	template <typename T>
	bool PerformComparison(const T& lhs, const T& rhs, EComparisonMethod comparison)
	{
		switch (comparison)
		{
		case EComparisonMethod::Equal_To:
			return lhs == rhs;

		case EComparisonMethod::Not_Equal_To:
			return lhs != rhs;

		case EComparisonMethod::Greater_Than_Or_Equal_To:
			return lhs >= rhs;

		case EComparisonMethod::Less_Than_Or_Equal_To:
			return lhs <= rhs;

		case EComparisonMethod::Greater_Than:
			return lhs > rhs;

		case EComparisonMethod::Less_Than:
			return lhs < rhs;
		}

		UE_LOG(LogFunctionalTest, Error, TEXT("Invalid comparison method"));
		return false;
	}

	FString GetComparisonAsString(EComparisonMethod comparison)
	{
		UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EComparisonMethod"), true);
		return Enum->GetNameStringByValue((uint8)comparison).ToLower().Replace(TEXT("_"), TEXT(" "));
	}

	FString TransformToString(const FTransform &transform)
	{
		const FRotator R(transform.Rotator());
		FVector T(transform.GetTranslation());
		FVector S(transform.GetScale3D());

		return FString::Printf(TEXT("Translation: %f, %f, %f | Rotation: %f, %f, %f | Scale: %f, %f, %f"), T.X, T.Y, T.Z, R.Pitch, R.Yaw, R.Roll, S.X, S.Y, S.Z);
	}

	void DelayForFramesCommon(UObject* WorldContextObject, FLatentActionInfo LatentInfo, int32 NumFrames)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			if (LatentActionManager.FindExistingAction<FDelayForFramesLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayForFramesLatentAction(LatentInfo, NumFrames));
			}
		}
	}
}

AFunctionalTest::AFunctionalTest( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bIsEnabled(true)
	, bWarningsAsErrors(false)
	, Result(EFunctionalTestResult::Invalid)
	, PreparationTimeLimit(15.0f)
	, TimeLimit(60.0f)
	, TimesUpMessage( NSLOCTEXT("FunctionalTest", "DefaultTimesUpMessage", "Time's up!") )
	, TimesUpResult(EFunctionalTestResult::Failed)
	, bIsRunning(false)
	, TotalTime(0.f)
	, RunFrame(0)
	, RunTime(0.0f)
	, StartFrame(0)
	, StartTime(0.0f)
	, bIsReady(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	
	bCanBeDamaged = false;

	SpriteComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->bHiddenInGame = true;
#if WITH_EDITORONLY_DATA

		if (!IsRunningCommandlet())
		{
			struct FConstructorStatics
			{
				ConstructorHelpers::FObjectFinderOptional<UTexture2D> Texture;
				FName ID_FTests;
				FText NAME_FTests;

				FConstructorStatics()
					: Texture(TEXT("/Engine/EditorResources/S_FTest"))
					, ID_FTests(TEXT("FTests"))
					, NAME_FTests(NSLOCTEXT( "SpriteCategory", "FTests", "FTests" ))
				{
				}
			};
			static FConstructorStatics ConstructorStatics;

			SpriteComponent->Sprite = ConstructorStatics.Texture.Get();
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_FTests;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_FTests;
		}

#endif
		RootComponent = SpriteComponent;
	}

#if WITH_EDITORONLY_DATA
	RenderComp = CreateDefaultSubobject<UFuncTestRenderingComponent>(TEXT("RenderComp"));
	RenderComp->PostPhysicsComponentTick.bCanEverTick = false;
	RenderComp->SetupAttachment(RootComponent);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	static bool bSelectionHandlerSetUp = false;
	if (HasAnyFlags(RF_ClassDefaultObject) && !HasAnyFlags(RF_TagGarbageTemp) && bSelectionHandlerSetUp == false)
	{
		USelection::SelectObjectEvent.AddStatic(&AFunctionalTest::OnSelectObject);
		bSelectionHandlerSetUp = true;
	}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	TestName = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("TestName"));
	if ( TestName )
	{
		TestName->bHiddenInGame = true;
		TestName->SetHorizontalAlignment(EHTA_Center);
		TestName->SetRelativeLocation(FVector(0, 0, 80));
		TestName->SetRelativeRotation(FRotator(0, 0, 0));
		TestName->PostPhysicsComponentTick.bCanEverTick = false;
		TestName->SetupAttachment(RootComponent);
	}
#endif
}

void AFunctionalTest::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if ( TestName )
	{
		if ( bIsEnabled )
		{
			TestName->SetTextRenderColor(FColor(11, 255, 0));
			TestName->SetText(FText::FromString(GetActorLabel()));
		}
		else
		{
			TestName->SetTextRenderColor(FColor(55, 55, 55));
			TestName->SetText(FText::FromString(GetActorLabel() + TEXT("\n") + TEXT("# Disabled #")));
		}
	}
#endif
}

bool AFunctionalTest::RunTest(const TArray<FString>& Params)
{
	FAutomationTestFramework::Get().SetTreatWarningsAsErrors(bWarningsAsErrors);

	//Scalability::FQualityLevels Quality;
	//Quality.SetDefaults();
	//Scalability::SetQualityLevels(Quality);

	FailureMessage = TEXT("");
	
	//Do not collect garbage during the test. We force GC at the end.
	GEngine->DelayGarbageCollection();

	RunFrame = GFrameNumber;
	RunTime = GetWorld()->GetTimeSeconds();

	TotalTime = 0.f;
	if (TimeLimit >= 0)
	{
		SetActorTickEnabled(true);
	}

	bIsReady = false;
	bIsRunning = true;

	GoToObservationPoint();

	PrepareTest();
	OnTestPrepare.Broadcast();

	return true;
}

void AFunctionalTest::PrepareTest()
{
	ReceivePrepareTest();
}

void AFunctionalTest::StartTest()
{
	TotalTime = 0.f;
	StartFrame = GFrameNumber;
	StartTime = GetWorld()->GetTimeSeconds();

	ReceiveStartTest();
	OnTestStart.Broadcast();
}

void AFunctionalTest::OnTimeout()
{
	FinishTest(TimesUpResult, TimesUpMessage.ToString());
}

void AFunctionalTest::Tick(float DeltaSeconds)
{
	// already requested not to tick. 
	if ( bIsRunning == false )
	{
		return;
	}

	//Do not collect garbage during the test. We force GC at the end.
	GEngine->DelayGarbageCollection();

	if ( !bIsReady )
	{
		bIsReady = IsReady();

		// Once we're finally ready to begin the test, then execute the Start event.
		if ( bIsReady )
		{
			StartTest();
		}
	}

	if ( bIsReady )
	{
		TotalTime += DeltaSeconds;
		if ( TimeLimit > 0.f && TotalTime > TimeLimit )
		{
			OnTimeout();
		}
		else
		{
			Super::Tick(DeltaSeconds);
		}
	}
	else
	{
		TotalTime += DeltaSeconds;
		if ( PreparationTimeLimit > 0.f && TotalTime > PreparationTimeLimit )
		{
			OnTimeout();
		}
	}
}

bool AFunctionalTest::IsReady_Implementation()
{
	return true;
}

void AFunctionalTest::FinishTest(EFunctionalTestResult TestResult, const FString& Message)
{
	const static UEnum* FTestResultTypeEnum = FindObject<UEnum>( nullptr, TEXT("FunctionalTesting.EFunctionalTestResult") );
	
	if (bIsRunning == false)
	{
		// ignore
		return;
	}

	//Force GC at the end of every test.
	GEngine->ForceGarbageCollection();

	Result = TestResult;

	bIsRunning = false;
	SetActorTickEnabled(false);

	OnTestFinished.Broadcast();

	AActor** ActorToDestroy = AutoDestroyActors.GetData();

	for (int32 ActorIndex = 0; ActorIndex < AutoDestroyActors.Num(); ++ActorIndex, ++ActorToDestroy)
	{
		if (*ActorToDestroy != NULL)
		{
			// will be removed next frame
			(*ActorToDestroy)->SetLifeSpan( 0.01f );
		}
	}

	const FText ResultText = FTestResultTypeEnum->GetDisplayNameTextByValue( (int64)TestResult );
	const FString OutMessage = FString::Printf(TEXT("%s %s: \"%s\"")
		, *GetName()
		, *ResultText.ToString()
		, Message.IsEmpty() == false ? *Message : TEXT("Test finished") );

	AutoDestroyActors.Reset();
		
	switch (TestResult)
	{
		case EFunctionalTestResult::Invalid:
		case EFunctionalTestResult::Error:
		case EFunctionalTestResult::Failed:
			UE_VLOG(this, LogFunctionalTest, Error, TEXT("%s"), *OutMessage);
			UE_LOG(LogFunctionalTest, Error, TEXT("%s"), *OutMessage);
			break;

		case EFunctionalTestResult::Running:
			UE_VLOG(this, LogFunctionalTest, Warning, TEXT("%s"), *OutMessage);
			UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *OutMessage);
			break;
		
		default:
			UE_VLOG(this, LogFunctionalTest, Log, TEXT("%s"), *OutMessage);
			UE_LOG(LogFunctionalTest, Log, TEXT("%s"), *OutMessage);
			break;
	}
	
	//if (AdditionalDetails.IsEmpty() == false)
	//{
	//	const FString AdditionalDetails = FString::Printf(TEXT("%s %s, time %.2fs"), *GetAdditionalTestFinishedMessage(TestResult), *OnAdditionalTestFinishedMessageRequest(TestResult), TotalTime);
	//	UE_LOG(LogFunctionalTest, Log, TEXT("%s"), *AdditionalDetails);
	//}

	TestFinishedObserver.ExecuteIfBound(this);

	FAutomationTestFramework::Get().SetTreatWarningsAsErrors(TOptional<bool>());
}

void AFunctionalTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TestFinishedObserver.Unbind();

	Super::EndPlay(EndPlayReason);
}

void AFunctionalTest::CleanUp()
{
	FailureMessage = TEXT("");
}

bool AFunctionalTest::IsRunning() const
{
	return bIsRunning;
}

bool AFunctionalTest::IsEnabled() const
{
	return bIsEnabled;
}

//@todo add "warning" level here
void AFunctionalTest::LogMessage(const FString& Message)
{
	UE_LOG(LogFunctionalTest, Log, TEXT("%s"), *Message);
	UE_VLOG(this, LogFunctionalTest, Log
		, TEXT("%s> %s")
		, *GetName(), *Message);
}

void AFunctionalTest::SetTimeLimit(float InTimeLimit, EFunctionalTestResult InResult)
{
	if (InTimeLimit < 0.f)
	{
		UE_VLOG(this, LogFunctionalTest, Warning
			, TEXT("%s> Trying to set TimeLimit to less than 0. Falling back to 0 (infinite).")
			, *GetName());

		InTimeLimit = 0.f;
	}
	TimeLimit = InTimeLimit;

	if (InResult == EFunctionalTestResult::Invalid)
	{
		UE_VLOG(this, LogFunctionalTest, Warning
			, TEXT("%s> Trying to set test Result to \'Invalid\'. Falling back to \'Failed\'")
			, *GetName());

		InResult = EFunctionalTestResult::Failed;
	}
	TimesUpResult = InResult;
}

void AFunctionalTest::GatherRelevantActors(TArray<AActor*>& OutActors) const
{
	if (ObservationPoint)
	{
		OutActors.AddUnique(ObservationPoint);
	}

	for (auto Actor : AutoDestroyActors)
	{
		if (Actor)
		{
			OutActors.AddUnique(Actor);
		}
	}

	OutActors.Append(DebugGatherRelevantActors());
}

void AFunctionalTest::AddRerun(FName Reason)
{
	RerunCauses.Add(Reason);
}

FName AFunctionalTest::GetCurrentRerunReason()const
{
	return CurrentRerunCause;
}

void AFunctionalTest::RegisterAutoDestroyActor(AActor* ActorToAutoDestroy)
{
	AutoDestroyActors.AddUnique(ActorToAutoDestroy);
}

#if WITH_EDITOR

void AFunctionalTest::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_FunctionalTesting = FName(TEXT("FunctionalTesting"));
	static const FName NAME_TimeLimit = FName(TEXT("TimeLimit"));
	static const FName NAME_TimesUpResult = FName(TEXT("TimesUpResult"));

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != NULL)
	{
		if (FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property) == NAME_FunctionalTesting)
		{
			// first validate new data since there are some dependencies
			if (PropertyChangedEvent.Property->GetFName() == NAME_TimeLimit)
			{
				if (TimeLimit < 0.f)
				{
					TimeLimit = 0.f;
				}
			}
			else if (PropertyChangedEvent.Property->GetFName() == NAME_TimesUpResult)
			{
				if (TimesUpResult == EFunctionalTestResult::Invalid)
				{
					TimesUpResult = EFunctionalTestResult::Failed;
				}
			}
		}
	}
}

void AFunctionalTest::OnSelectObject(UObject* NewSelection)
{
	AFunctionalTest* AsFTest = Cast<AFunctionalTest>(NewSelection);
	if (AsFTest)
	{
		AsFTest->MarkComponentsRenderStateDirty();
	}
}

#endif // WITH_EDITOR

void AFunctionalTest::GoToObservationPoint()
{
	if (ObservationPoint == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World && World->GetGameInstance())
	{
		APlayerController* TargetPC = nullptr;
		for (FConstPlayerControllerIterator PCIterator = World->GetPlayerControllerIterator(); PCIterator; ++PCIterator)
		{
			APlayerController* PC = PCIterator->Get();

			// Don't use debug camera player controllers.
			// While it's tempting to teleport the camera if the user is debugging something then moving the camera around will them.
			if (PC && !PC->IsA(ADebugCameraController::StaticClass()))
			{
				TargetPC = PC;
				break;
			}
		}

		if (TargetPC)
		{
			if (TargetPC->GetPawn())
			{
				TargetPC->GetPawn()->TeleportTo(ObservationPoint->GetActorLocation(), ObservationPoint->GetActorRotation(), /*bIsATest=*/false, /*bNoCheck=*/true);
				TargetPC->SetControlRotation(ObservationPoint->GetActorRotation());
			}
			else
			{
				TargetPC->SetViewTarget(ObservationPoint);
			}
		}
	}
}

/** Returns SpriteComponent subobject **/
UBillboardComponent* AFunctionalTest::GetSpriteComponent()
{
	return SpriteComponent;
}

//////////////////////////////////////////////////////////////////////////

bool AFunctionalTest::AssertTrue(bool Condition, FString Message, const UObject* ContextObject)
{
	if ( !Condition )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Assertion failed: '%s' for context '%s'"), *Message, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Assertion passed (%s)"), *Message));
		return true;
	}
}

bool AFunctionalTest::AssertFalse(bool Condition, FString Message, const UObject* ContextObject)
{
	return AssertTrue(!Condition, Message, ContextObject);
}

bool AFunctionalTest::AssertIsValid(UObject* Object, FString Message, const UObject* ContextObject)
{
	if ( !IsValid(Object) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Invalid object: '%s' for context '%s'"), *Message, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Valid object: (%s)"), *Message));
		return true;
	}
}

bool AFunctionalTest::AssertValue_Int(int32 Actual, EComparisonMethod ShouldBe, int32 Expected, const FString& What, const UObject* ContextObject)
{
	if ( !PerformComparison(Actual, Expected, ShouldBe) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("%s: expected {%d} to be %s {%d} for context '%s'"), *What, Actual, *GetComparisonAsString(ShouldBe), Expected, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("%s: expected {%d} to be %s {%d} for context '%s'"), *What, Actual, *GetComparisonAsString(ShouldBe), Expected, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return true;
	}
}

bool AFunctionalTest::AssertValue_Float(float Actual, EComparisonMethod ShouldBe, float Expected, const FString& What, const UObject* ContextObject)
{
	if ( !PerformComparison(Actual, Expected, ShouldBe) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("%s: expected {%f} to be %s {%f} for context '%s'"), *What, Actual, *GetComparisonAsString(ShouldBe), Expected, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("%s: expected {%f} to be %s {%f} for context '%s'"), *What, Actual, *GetComparisonAsString(ShouldBe), Expected, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return true;
	}
}

bool AFunctionalTest::AssertValue_DateTime(FDateTime Actual, EComparisonMethod ShouldBe, FDateTime Expected, const FString& What, const UObject* ContextObject)
{
	if ( !PerformComparison(Actual, Expected, ShouldBe) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("%s: expected {%s} to be %s {%s} for context '%s'"), *What, *Actual.ToString(), *GetComparisonAsString(ShouldBe), *Expected.ToString(), ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("DateTime assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_Float(const float Actual, const float Expected, const FString& What, const float Tolerance, const UObject* ContextObject)
{
	if ( !FMath::IsNearlyEqual(Actual, Expected, Tolerance) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%f}, but it was {%f} within tolerance {%f} for context '%s'"), *What, Expected, Actual, Tolerance, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Float assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_Bool(const bool Actual, const bool Expected, const FString& What, const UObject* ContextObject)
{
	if (Actual != Expected)
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%d}, but it was {%d} for context '%s'"), *What, Expected, Actual, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Bool assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_Int(const int32 Actual, const int32 Expected, const FString& What, const UObject* ContextObject)
{
	if (Actual != Expected)
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%d}, but it was {%d} for context '%s'"), *What, Expected, Actual, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Bool assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_Name(const FName Actual, const FName Expected, const FString& What, const UObject* ContextObject)
{
	if (Actual != Expected)
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%s}, but it was {%s} for context '%s'"), *What, *Expected.ToString(), *Actual.ToString(), ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("FName assertion passed (%s)"), *What));
		return true;
	}
}


bool AFunctionalTest::AssertEqual_Transform(const FTransform& Actual, const FTransform& Expected, const FString& What, const float Tolerance, const UObject* ContextObject)
{
	if ( !Expected.Equals(Actual, Tolerance) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%s}, but it was {%s} within tolerance {%f} for context '%s'"), *What, *TransformToString(Expected), *TransformToString(Actual), Tolerance, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Transform assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertNotEqual_Transform(const FTransform& Actual, const FTransform& NotExpected, const FString& What, const UObject* ContextObject)
{
	if ( NotExpected.Equals(Actual) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' not to be {%s} for context '%s'"), *What, *TransformToString(NotExpected), ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Transform assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_Rotator(const FRotator Actual, const FRotator Expected, const FString& What, const float Tolerance, const UObject* ContextObject)
{
	if ( !Expected.Equals(Actual, Tolerance) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%s} but it was {%s} within tolerance {%f} for context '%s'"), *What, *Expected.ToString(), *Actual.ToString(), Tolerance, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Rotator assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertNotEqual_Rotator(const FRotator Actual, const FRotator NotExpected, const FString& What, const UObject* ContextObject)
{
	if ( NotExpected.Equals(Actual) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' not to be {%s} for context '%s'"), *What, *NotExpected.ToString(), ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Rotator assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_Vector(const FVector Actual, const FVector Expected, const FString& What, const float Tolerance, const UObject* ContextObject)
{
	if ( !Expected.Equals(Actual, Tolerance) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%s} but it was {%s} within tolerance {%f} for context '%s'"), *What, *Expected.ToString(), *Actual.ToString(), Tolerance, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Vector assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertNotEqual_Vector(const FVector Actual, const FVector NotExpected, const FString& What, const UObject* ContextObject)
{
	if ( NotExpected.Equals(Actual) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' not to be {%s} for context '%s'"), *What, *NotExpected.ToString(), ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("Vector assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_String(const FString Actual, const FString Expected, const FString& What, const UObject* ContextObject)
{
	if ( !Expected.Equals(Actual) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' to be {%s} but it was {%s} for context '%s'"), *What, *Expected, *Actual, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("String assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertNotEqual_String(const FString Actual, const FString NotExpected, const FString& What, const UObject* ContextObject)
{
	if ( NotExpected.Equals(Actual) )
	{
		LogStep(ELogVerbosity::Error, FString::Printf(TEXT("Expected '%s' not to be {%s} for context '%s'"), *What, *NotExpected, ContextObject ? *ContextObject->GetName() : TEXT("")));
		return false;
	}
	else
	{
		LogStep(ELogVerbosity::Log, FString::Printf(TEXT("String assertion passed (%s)"), *What));
		return true;
	}
}

bool AFunctionalTest::AssertEqual_TraceQueryResults(const UTraceQueryTestResults* Actual, const UTraceQueryTestResults* Expected, const FString& What, const UObject* ContextObject)
{
	return Actual->AssertEqual(Expected, What, ContextObject, *this);
}

void AFunctionalTest::AddWarning(const FString Message)
{
	LogStep(ELogVerbosity::Warning, Message);
}

void AFunctionalTest::AddError(const FString Message)
{
	LogStep(ELogVerbosity::Error, Message);
}

void AFunctionalTest::LogStep(ELogVerbosity::Type Verbosity, const FString& Message)
{
	FString FullMessage(Message);
	if ( IsInStep() )
	{
		FullMessage.Append(TEXT(" in step: "));
		FString StepName = TEXT("");
		if ( StepName.IsEmpty() )
		{
			StepName = TEXT("<UN-NAMED STEP>");
		}
		FullMessage.Append(StepName);
	}

	switch ( Verbosity )
	{
		case ELogVerbosity::Display:
		case ELogVerbosity::Log:
			UE_VLOG(this, LogFunctionalTest, Display, TEXT("%s"), *FullMessage);
			UE_LOG(LogFunctionalTest, Display, TEXT("%s"), *FullMessage);
		break;
		case ELogVerbosity::Warning:
			UE_VLOG(this, LogFunctionalTest, Warning, TEXT("%s"), *FullMessage);
			UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *FullMessage);
		break;
		case ELogVerbosity::Error:
			UE_VLOG(this, LogFunctionalTest, Error, TEXT("%s"), *FullMessage);
			UE_LOG(LogFunctionalTest, Error, TEXT("%s"), *FullMessage);
		break;
	}
}

FString	AFunctionalTest::GetCurrentStepName() const
{
	return IsInStep() ? Steps.Top() : FString();
}

void AFunctionalTest::StartStep(const FString& StepName)
{
	Steps.Push(StepName);
}

void AFunctionalTest::FinishStep()
{
	if ( Steps.Num() > 0 )
	{
		Steps.Pop();
	}
	else
	{
		AddWarning(TEXT("FinishStep was called when no steps were currently in progress."));
	}
}

bool AFunctionalTest::IsInStep() const
{
	return Steps.Num() > 0;
}

//////////////////////////////////////////////////////////////////////////

FPerfStatsRecord::FPerfStatsRecord(FString InName)
: Name(InName)
, GPUBudget(0.0f)
, RenderThreadBudget(0.0f)
, GameThreadBudget(0.0f)
{
}

void FPerfStatsRecord::SetBudgets(float InGPUBudget, float InRenderThreadBudget, float InGameThreadBudget)
{
	GPUBudget = InGPUBudget;
	RenderThreadBudget = InRenderThreadBudget;
	GameThreadBudget = InGameThreadBudget;
}

FString FPerfStatsRecord::GetReportString() const
{
	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Record.FrameTimeTracker.GetMinValue() - Baseline.FrameTimeTracker.GetMinValue(),
		Record.FrameTimeTracker.GetAvgValue() - Baseline.FrameTimeTracker.GetAvgValue(),
		Record.FrameTimeTracker.GetMaxValue() - Baseline.FrameTimeTracker.GetMaxValue(),
		Record.RenderThreadTimeTracker.GetMinValue() - Baseline.RenderThreadTimeTracker.GetMinValue(),
		Record.RenderThreadTimeTracker.GetAvgValue() - Baseline.RenderThreadTimeTracker.GetAvgValue(),
		Record.RenderThreadTimeTracker.GetMaxValue() - Baseline.RenderThreadTimeTracker.GetMaxValue(),
		Record.GameThreadTimeTracker.GetMinValue() - Baseline.GameThreadTimeTracker.GetMinValue(),
		Record.GameThreadTimeTracker.GetAvgValue() - Baseline.GameThreadTimeTracker.GetAvgValue(),
		Record.GameThreadTimeTracker.GetMaxValue() - Baseline.GameThreadTimeTracker.GetMaxValue(),
		Record.GPUTimeTracker.GetMinValue() - Baseline.GPUTimeTracker.GetMinValue(),
		Record.GPUTimeTracker.GetAvgValue() - Baseline.GPUTimeTracker.GetAvgValue(),
		Record.GPUTimeTracker.GetMaxValue() - Baseline.GPUTimeTracker.GetMaxValue());
}

FString FPerfStatsRecord::GetBaselineString() const
{
	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Baseline.FrameTimeTracker.GetMinValue(),
		Baseline.FrameTimeTracker.GetAvgValue(),
		Baseline.FrameTimeTracker.GetMaxValue(),
		Baseline.RenderThreadTimeTracker.GetMinValue(),
		Baseline.RenderThreadTimeTracker.GetAvgValue(),
		Baseline.RenderThreadTimeTracker.GetMaxValue(),
		Baseline.GameThreadTimeTracker.GetMinValue(),
		Baseline.GameThreadTimeTracker.GetAvgValue(),
		Baseline.GameThreadTimeTracker.GetMaxValue(),
		Baseline.GPUTimeTracker.GetMinValue(),
		Baseline.GPUTimeTracker.GetAvgValue(),
		Baseline.GPUTimeTracker.GetMaxValue());
}

FString FPerfStatsRecord::GetRecordString() const
{
	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Record.FrameTimeTracker.GetMinValue(),
		Record.FrameTimeTracker.GetAvgValue(),
		Record.FrameTimeTracker.GetMaxValue(),
		Record.RenderThreadTimeTracker.GetMinValue(),
		Record.RenderThreadTimeTracker.GetAvgValue(),
		Record.RenderThreadTimeTracker.GetMaxValue(),
		Record.GameThreadTimeTracker.GetMinValue(),
		Record.GameThreadTimeTracker.GetAvgValue(),
		Record.GameThreadTimeTracker.GetMaxValue(),
		Record.GPUTimeTracker.GetMinValue(),
		Record.GPUTimeTracker.GetAvgValue(),
		Record.GPUTimeTracker.GetMaxValue());
}

FString FPerfStatsRecord::GetOverBudgetString() const
{
	double Min, Max, Avg;
	GetRenderThreadTimes(Min, Max, Avg);
	float RTMax = Max;
	float RTBudgetFrac = Max / RenderThreadBudget;
	GetGameThreadTimes(Min, Max, Avg);
	float GTMax = Max;
	float GTBudgetFrac = Max / GameThreadBudget;
	GetGPUTimes(Min, Max, Avg);
	float GPUMax = Max;
	float GPUBudgetFrac = Max / GPUBudget;

	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		RTMax,
		RenderThreadBudget,
		RTBudgetFrac,
		GTMax,
		GameThreadBudget,
		GTBudgetFrac,
		GPUMax,
		GPUBudget,
		GPUBudgetFrac
		);
}

bool FPerfStatsRecord::IsWithinGPUBudget()const
{
	double Min, Max, Avg;
	GetGPUTimes(Min, Max, Avg);
	return Max <= GPUBudget;
}

bool FPerfStatsRecord::IsWithinGameThreadBudget()const
{
	double Min, Max, Avg;
	GetGameThreadTimes(Min, Max, Avg);
	return Max <= GameThreadBudget;
}

bool FPerfStatsRecord::IsWithinRenderThreadBudget()const
{
	double Min, Max, Avg;
	GetRenderThreadTimes(Min, Max, Avg);
	return Max <= RenderThreadBudget;
}

void FPerfStatsRecord::GetGPUTimes(double& OutMin, double& OutMax, double& OutAvg)const
{
	OutMin = Record.GPUTimeTracker.GetMinValue() - Baseline.GPUTimeTracker.GetMinValue();
	OutMax = Record.GPUTimeTracker.GetMaxValue() - Baseline.GPUTimeTracker.GetMaxValue();
	OutAvg = Record.GPUTimeTracker.GetAvgValue() - Baseline.GPUTimeTracker.GetAvgValue();
}

void FPerfStatsRecord::GetGameThreadTimes(double& OutMin, double& OutMax, double& OutAvg)const
{
	OutMin = Record.GameThreadTimeTracker.GetMinValue() - Baseline.GameThreadTimeTracker.GetMinValue();
	OutMax = Record.GameThreadTimeTracker.GetMaxValue() - Baseline.GameThreadTimeTracker.GetMaxValue();
	OutAvg = Record.GameThreadTimeTracker.GetAvgValue() - Baseline.GameThreadTimeTracker.GetAvgValue();
}

void FPerfStatsRecord::GetRenderThreadTimes(double& OutMin, double& OutMax, double& OutAvg)const
{
	OutMin = Record.RenderThreadTimeTracker.GetMinValue() - Baseline.RenderThreadTimeTracker.GetMinValue();
	OutMax = Record.RenderThreadTimeTracker.GetMaxValue() - Baseline.RenderThreadTimeTracker.GetMaxValue();
	OutAvg = Record.RenderThreadTimeTracker.GetAvgValue() - Baseline.RenderThreadTimeTracker.GetAvgValue();
}

void FPerfStatsRecord::Sample(UWorld* World, float DeltaSeconds, bool bBaseline)
{
	check(World);

	const FStatUnitData* StatUnitData = World->GetGameViewport()->GetStatUnitData();
	check(StatUnitData);

	if (bBaseline)
	{
		Baseline.FrameTimeTracker.AddSample(StatUnitData->RawFrameTime);
		Baseline.GameThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGameThreadTime));
		Baseline.RenderThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GRenderThreadTime));
		Baseline.GPUTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGPUFrameTime));
		Baseline.NumFrames++;
		Baseline.SumTimeSeconds += DeltaSeconds;
	}
	else
	{
		Record.FrameTimeTracker.AddSample(StatUnitData->RawFrameTime);
		Record.GameThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGameThreadTime));
		Record.RenderThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GRenderThreadTime));
		Record.GPUTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGPUFrameTime));
		Record.NumFrames++;
		Record.SumTimeSeconds += DeltaSeconds;
	}
}

UAutomationPerformaceHelper::UAutomationPerformaceHelper()
: bRecordingBasicStats(false)
, bRecordingBaselineBasicStats(false)
, bRecordingCPUCapture(false)
, bRecordingStatsFile(false)
, bGPUTraceIfBelowBudget(false)
{
}

UWorld* UAutomationPerformaceHelper::GetWorld() const
{
	UWorld* OuterWorld = GetOuter()->GetWorld();
	ensureAsRuntimeWarning(OuterWorld != nullptr);
	return OuterWorld;
}

void UAutomationPerformaceHelper::BeginRecordingBaseline(FString RecordName)
{
	if (UWorld* World = GetWorld())
	{
		bRecordingBasicStats = true;
		bRecordingBaselineBasicStats = true;
		bGPUTraceIfBelowBudget = false;
		Records.Add(FPerfStatsRecord(RecordName));
		GEngine->SetEngineStat(World, World->GetGameViewport(), TEXT("Unit"), true);
	}
}

void UAutomationPerformaceHelper::EndRecordingBaseline()
{
	bRecordingBaselineBasicStats = false;
	bRecordingBasicStats = false;
}

void UAutomationPerformaceHelper::BeginRecording(FString RecordName, float InGPUBudget, float InRenderThreadBudget, float InGameThreadBudget)
{
	if (UWorld* World = GetWorld())
	{
		//Ensure we're recording engine stats.
		GEngine->SetEngineStat(World, World->GetGameViewport(), TEXT("Unit"), true);
		bRecordingBasicStats = true;
		bRecordingBaselineBasicStats = false;
		bGPUTraceIfBelowBudget = false;

		FPerfStatsRecord* CurrRecord = GetCurrentRecord();
		if (!CurrRecord || CurrRecord->Name != RecordName)
		{
			Records.Add(FPerfStatsRecord(RecordName));
			CurrRecord = GetCurrentRecord();
		}

		check(CurrRecord);
		CurrRecord->SetBudgets(InGPUBudget, InRenderThreadBudget, InGameThreadBudget);
	}
}

void UAutomationPerformaceHelper::EndRecording()
{
	if (const FPerfStatsRecord* Record = GetCurrentRecord())
	{
		UE_LOG(LogFunctionalTest, Log, TEXT("Finished Perf Stats Record:\n%s"), *Record->GetReportString());
	}
	bRecordingBasicStats = false;
}

void UAutomationPerformaceHelper::Tick(float DeltaSeconds)
{
	if (bRecordingBasicStats)
	{
		Sample(DeltaSeconds);
	}

	if (bGPUTraceIfBelowBudget)
	{
		if (!IsCurrentRecordWithinGPUBudget())
		{
			FString PathName = FPaths::ProfilingDir();			
			GGPUTraceFileName = PathName / CreateProfileFilename(GetCurrentRecord()->Name, TEXT(".rtt"), true);
			UE_LOG(LogFunctionalTest, Log, TEXT("Functional Test has fallen below GPU budget. Performing GPU trace."));

			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Performed GPU Thred Trace!"));

			//Only perform one trace per test. 
			bGPUTraceIfBelowBudget = false;
		}
	}

	//Other stats need ticking?
}

void UAutomationPerformaceHelper::Sample(float DeltaSeconds)
{
	if (UWorld* World = GetWorld())
	{
		int32 Index = Records.Num() - 1;
		if (Index >= 0 && bRecordingBasicStats)
		{
			Records[Index].Sample(World, DeltaSeconds, bRecordingBaselineBasicStats);
		}
	}
}

void UAutomationPerformaceHelper::WriteLogFile(const FString& CaptureDir, const FString& CaptureExtension)
{
	FString PathName = FPaths::ProfilingDir();
	if (!CaptureDir.IsEmpty())
	{
		PathName = PathName + (CaptureDir + TEXT("/"));
		IFileManager::Get().MakeDirectory(*PathName);
	}

	FString Extension = CaptureExtension;
	if (Extension.IsEmpty())
	{
		Extension = TEXT("perf.csv");
	}

	const FString Filename = CreateProfileFilename(CaptureExtension, true);
	const FString FilenameFull = PathName + Filename;
	
	const FString OverBudgetTableHeader = TEXT("TestName, MaxRT, RT Budget, RT Frac, MaxGT, GT Budget, GT Frac, MaxGPU, GPU Budget, GPU Frac\n");
	FString OverbudgetTable;
	const FString DataTableHeader = TEXT("TestName,MinFrameTime,AvgFrameTime,MaxFrameTime,MinRT,AvgRT,MaxRT,MinGT,AvgGT,MaxGT,MinGPU,AvgGPU,MaxGPU\n");
	FString AdjustedTable;
	FString RecordTable;
	FString BaselineTable;
	for (FPerfStatsRecord& Record : Records)
	{
		AdjustedTable += Record.GetReportString() + FString(TEXT("\n"));
		RecordTable += Record.GetRecordString() + FString(TEXT("\n"));
		BaselineTable += Record.GetBaselineString() + FString(TEXT("\n"));

		if (!Record.IsWithinGPUBudget() || !Record.IsWithinRenderThreadBudget() || !Record.IsWithinGameThreadBudget())
		{
			OverbudgetTable += Record.GetOverBudgetString() + FString(TEXT("\n"));
		}
	}

	FString FileContents = FString::Printf(TEXT("Over Budget Tests\n%s%s\nAdjusted Results\n%s%s\nRaw Results\n%s%s\nBaseline Results\n%s%s\n"), 
		*OverBudgetTableHeader, *OverbudgetTable, *DataTableHeader, *AdjustedTable, *DataTableHeader, *RecordTable, *DataTableHeader, *BaselineTable);

	FFileHelper::SaveStringToFile(FileContents, *FilenameFull);

	UE_LOG(LogTemp, Display, TEXT("Finished test, wrote file to %s"), *FilenameFull);

	Records.Empty();
	bRecordingBasicStats = false;
	bRecordingBaselineBasicStats = false;
}

bool UAutomationPerformaceHelper::IsRecording()const 
{
	return bRecordingBasicStats;
}

void UAutomationPerformaceHelper::OnBeginTests()
{
	OutputFileBase = CreateProfileFilename(TEXT(""), true);
	StartOfTestingTime = FDateTime::Now().ToString();
}

void UAutomationPerformaceHelper::OnAllTestsComplete()
{
	if (bRecordingBaselineBasicStats)
	{
		EndRecordingBaseline();
	}

	if (bRecordingBasicStats)
	{
		EndRecording();
	}

	if (bRecordingCPUCapture)
	{
		StopCPUProfiling();
	}

	if (bRecordingStatsFile)
	{
		EndStatsFile();
	}
	
	bGPUTraceIfBelowBudget = false;

	if (Records.Num() > 0)
	{
		WriteLogFile(TEXT(""), TEXT("perf.csv"));
	}
}

bool UAutomationPerformaceHelper::IsCurrentRecordWithinGPUBudget()const
{
	if (const FPerfStatsRecord* Curr = GetCurrentRecord())
	{
		return Curr->IsWithinGPUBudget();
	}
	return true;
}

bool UAutomationPerformaceHelper::IsCurrentRecordWithinGameThreadBudget()const
{
	if (const FPerfStatsRecord* Curr = GetCurrentRecord())
	{
		return Curr->IsWithinGameThreadBudget();
	}
	return true;
}

bool UAutomationPerformaceHelper::IsCurrentRecordWithinRenderThreadBudget()const
{
	if (const FPerfStatsRecord* Curr = GetCurrentRecord())
	{
		return Curr->IsWithinRenderThreadBudget();
	}
	return true;
}

const FPerfStatsRecord* UAutomationPerformaceHelper::GetCurrentRecord()const
{
	int32 Index = Records.Num() - 1;
	if (Index >= 0)
	{
		return &Records[Index];
	}
	return nullptr;
}
FPerfStatsRecord* UAutomationPerformaceHelper::GetCurrentRecord()
{
	int32 Index = Records.Num() - 1;
	if (Index >= 0)
	{
		return &Records[Index];
	}
	return nullptr;
}

void UAutomationPerformaceHelper::StartCPUProfiling()
{
	UE_LOG(LogFunctionalTest, Log, TEXT("START PROFILING..."));
	ExternalProfiler.StartProfiler(false);
}

void UAutomationPerformaceHelper::StopCPUProfiling()
{
	UE_LOG(LogFunctionalTest, Log, TEXT("STOP PROFILING..."));
	ExternalProfiler.StopProfiler();
}

void UAutomationPerformaceHelper::TriggerGPUTraceIfRecordFallsBelowBudget()
{
	bGPUTraceIfBelowBudget = true;
}

void UAutomationPerformaceHelper::BeginStatsFile(const FString& RecordName)
{
	if (UWorld* World = GetWorld())
	{
		FString MapName = World->GetMapName();
		FString Cmd = FString::Printf(TEXT("Stat StartFile %s-%s/%s.ue4stats"), *MapName, *StartOfTestingTime, *RecordName);
		GEngine->Exec(World, *Cmd);
	}
}

void UAutomationPerformaceHelper::EndStatsFile()
{
	if (UWorld* World = GetWorld())
	{
		GEngine->Exec(World, TEXT("Stat StopFile"));
	}
}
