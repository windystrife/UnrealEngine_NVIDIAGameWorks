// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLoggerAutomationTests.h"
#include "Misc/AutomationTest.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
	UWorld* GetSimpleEngineAutomationTestWorld(const int32 TestFlags)
	{
		// Accessing the game world is only valid for game-only 
		if (((TestFlags & EAutomationTestFlags::EditorContext) || (TestFlags & EAutomationTestFlags::ClientContext)) == false)
		{
			return nullptr;
		}
		if (GEngine->GetWorldContexts().Num() == 0)
		{
			return nullptr;
		}
		if (GEngine->GetWorldContexts()[0].WorldType != EWorldType::Game && GEngine->GetWorldContexts()[0].WorldType != EWorldType::Editor)
		{
			return nullptr;
		}

		return GEngine->GetWorldContexts()[0].World();
	}
}

UVisualLoggerAutomationTests::UVisualLoggerAutomationTests(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

#if ENABLE_VISUAL_LOG

class FVisualLoggerTestDevice : public FVisualLogDevice
{
public:
	FVisualLoggerTestDevice();
	virtual void Cleanup(bool bReleaseMemory = false) override;
	virtual void Serialize(const class UObject* LogOwner, FName OwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) override;

	class UObject* LastObject;
	FVisualLogEntry LastEntry;
};

FVisualLoggerTestDevice::FVisualLoggerTestDevice()
{
	Cleanup();
}

void FVisualLoggerTestDevice::Cleanup(bool bReleaseMemory)
{
	LastObject = nullptr;
	LastEntry.Reset();
}

void FVisualLoggerTestDevice::Serialize(const UObject* LogOwner, FName OwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry)
{
	LastObject = const_cast<class UObject*>(LogOwner);
	LastEntry = LogEntry;
}

#define CHECK_SUCCESS(__Test__) \
if (!(__Test__)) \
{ \
	TestTrue(FString::Printf( TEXT("%s (%s:%d)"), TEXT(#__Test__), TEXT(__FILE__), __LINE__ ), __Test__); \
	return false; \
}
#define CHECK_FAIL(__Test__) \
if ((__Test__)) \
{ \
	TestFalse(FString::Printf( TEXT("%s (%s:%d)"), TEXT(#__Test__), TEXT(__FILE__), __LINE__ ), __Test__); \
	return false; \
}

template<typename TYPE = FVisualLoggerTestDevice>
struct FTestDeviceContext
{
	FTestDeviceContext()
	{
		Device.Cleanup();
		FVisualLogger::Get().SetIsRecording(false);
		FVisualLogger::Get().Cleanup(nullptr);
		FVisualLogger::Get().AddDevice(&Device);

		EngineDisableAILoggingFlag = GEngine->bDisableAILogging;
		GEngine->bDisableAILogging = false;

	}

	~FTestDeviceContext()
	{
		FVisualLogger::Get().SetIsRecording(false);
		FVisualLogger::Get().RemoveDevice(&Device);
		FVisualLogger::Get().Cleanup(nullptr);
		Device.Cleanup();

		GEngine->bDisableAILogging = EngineDisableAILoggingFlag;
	}

	TYPE Device;
	uint32 EngineDisableAILoggingFlag : 1;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisualLogTest, "System.Engine.VisualLogger.Logging simple text", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
/**
*
*
* @param Parameters - Unused for this test
* @return	TRUE if the test was successful, FALSE otherwise
*/

bool FVisualLogTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetSimpleEngineAutomationTestWorld(GetTestFlags());
	CHECK_SUCCESS(World != nullptr);

	FTestDeviceContext<FVisualLoggerTestDevice> Context;

	FVisualLogger::Get().SetIsRecording(false);
	CHECK_FAIL(FVisualLogger::Get().IsRecording());

	UE_VLOG(World, LogVisual, Log, TEXT("Simple text line to test vlog"));
	CHECK_SUCCESS(Context.Device.LastObject == nullptr);
	CHECK_SUCCESS(Context.Device.LastEntry.TimeStamp == -1);

	FVisualLogger::Get().SetIsRecording(true);
	CHECK_SUCCESS(FVisualLogger::Get().IsRecording());

	{
		const FString TextToLog = TEXT("Simple text line to test if UE_VLOG_UELOG works fine");
		float CurrentTimestamp = World->TimeSeconds;
		UE_VLOG_UELOG(World, LogVisual, Log, TEXT("%s"), *TextToLog);
		CHECK_SUCCESS(Context.Device.LastObject != World);
		CHECK_SUCCESS(Context.Device.LastEntry.TimeStamp == -1);
		FVisualLogEntry* CurrentEntry = FVisualLogger::Get().GetEntryToWrite(World, World->TimeSeconds, ECreateIfNeeded::DontCreate);
		
		{
			CHECK_SUCCESS(CurrentEntry != nullptr);
			CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
			CHECK_SUCCESS(CurrentEntry->LogLines.Num() == 1);
			CHECK_SUCCESS(CurrentEntry->LogLines[0].Category == LogVisual.GetCategoryName());
			CHECK_SUCCESS(CurrentEntry->LogLines[0].Line == TextToLog);

			const float NewTimestamp = CurrentTimestamp + 0.1;
			FVisualLogEntry* NewEntry = FVisualLogger::Get().GetEntryToWrite(World, NewTimestamp); //generate new entry and serialize old one
			CurrentEntry = &Context.Device.LastEntry;
			CHECK_SUCCESS(CurrentEntry != nullptr);
			CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
			CHECK_SUCCESS(CurrentEntry->LogLines.Num() == 1);
			CHECK_SUCCESS(CurrentEntry->LogLines[0].Category == LogVisual.GetCategoryName());
			CHECK_SUCCESS(CurrentEntry->LogLines[0].Line == TextToLog);

			CHECK_SUCCESS(NewEntry != nullptr);
			CHECK_SUCCESS(NewEntry->TimeStamp - NewTimestamp <= SMALL_NUMBER);
			CHECK_SUCCESS(NewEntry->LogLines.Num() == 0);
		}
	}

	FVisualLogger::Get().SetIsRecording(false);
	CHECK_FAIL(FVisualLogger::Get().IsRecording());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisualLogSegmentsTest, "System.Engine.VisualLogger.Logging segment shape", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVisualLogSegmentsTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetSimpleEngineAutomationTestWorld(GetTestFlags());
	CHECK_SUCCESS(World != nullptr);

	FTestDeviceContext<FVisualLoggerTestDevice> Context;

	FVisualLogger::Get().SetIsRecording(false);
	CHECK_FAIL(FVisualLogger::Get().IsRecording());

	UE_VLOG_SEGMENT(World, LogVisual, Log, FVector(0, 0, 0), FVector(1, 0, 0), FColor::Red, TEXT("Simple segment log to test vlog"));
	CHECK_SUCCESS(Context.Device.LastObject == nullptr);
	CHECK_SUCCESS(Context.Device.LastEntry.TimeStamp == -1);

	FVisualLogger::Get().SetIsRecording(true);
	CHECK_SUCCESS(FVisualLogger::Get().IsRecording());
	{
		const FVector StartPoint(0, 0, 0);
		const FVector EndPoint(1, 0, 0);
		UE_VLOG_SEGMENT(World, LogVisual, Log, StartPoint, EndPoint, FColor::Red, TEXT("Simple segment log to test vlog"));
		CHECK_SUCCESS(Context.Device.LastObject == nullptr);
		CHECK_SUCCESS(Context.Device.LastEntry.TimeStamp == -1);
		FVisualLogEntry* CurrentEntry = FVisualLogger::Get().GetEntryToWrite(World, World->TimeSeconds, ECreateIfNeeded::DontCreate);

		float CurrentTimestamp = World->TimeSeconds;
		{
			CHECK_SUCCESS(CurrentEntry != nullptr);
			CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw.Num() == 1);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].GetType() == EVisualLoggerShapeElement::Segment);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].Points.Num() == 2);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].Points[0] == StartPoint);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].Points[1] == EndPoint);

			const float NewTimestamp = CurrentTimestamp + 0.1;
			FVisualLogEntry* NewEntry = FVisualLogger::Get().GetEntryToWrite(World, NewTimestamp); //generate new entry and serialize old one
			CurrentEntry = &Context.Device.LastEntry;
			CHECK_SUCCESS(CurrentEntry != nullptr);
			CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw.Num() == 1);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].GetType() == EVisualLoggerShapeElement::Segment);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].Points.Num() == 2);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].Points[0] == StartPoint);
			CHECK_SUCCESS(CurrentEntry->ElementsToDraw[0].Points[1] == EndPoint);

			CHECK_SUCCESS(NewEntry != nullptr);
			CHECK_SUCCESS(NewEntry->TimeStamp - NewTimestamp <= SMALL_NUMBER);
			CHECK_SUCCESS(NewEntry->ElementsToDraw.Num() == 0);
		}
	}
	return true;
}

DEFINE_VLOG_EVENT(EventTest, All, "Simple event for vlog tests")

DEFINE_VLOG_EVENT(EventTest2, All, "Second simple event for vlog tests")

DEFINE_VLOG_EVENT(EventTest3, All, "Third simple event for vlog tests")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisualLogEventsTest, "System.Engine.VisualLogger.Logging events", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVisualLogEventsTest::RunTest(const FString& Parameters)
{
	UWorld* World = GetSimpleEngineAutomationTestWorld(GetTestFlags());
	CHECK_SUCCESS(World != nullptr);

	FTestDeviceContext<FVisualLoggerTestDevice> Context;
	FVisualLogger::Get().SetIsRecording(true);

	CHECK_SUCCESS(EventTest.Name == TEXT("EventTest"));
	CHECK_SUCCESS(EventTest.FriendlyDesc == TEXT("Simple event for vlog tests"));

	CHECK_SUCCESS(EventTest2.Name == TEXT("EventTest2"));
	CHECK_SUCCESS(EventTest2.FriendlyDesc == TEXT("Second simple event for vlog tests"));

	CHECK_SUCCESS(EventTest3.Name == TEXT("EventTest3"));
	CHECK_SUCCESS(EventTest3.FriendlyDesc == TEXT("Third simple event for vlog tests"));

	FVisualLogEntry* CurrentEntry = FVisualLogger::Get().GetEntryToWrite(World, World->TimeSeconds, ECreateIfNeeded::DontCreate);
	float CurrentTimestamp = World->TimeSeconds;
	UE_VLOG_EVENTS(World, NAME_None, EventTest);
	CHECK_SUCCESS(CurrentEntry != nullptr);
	CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
	CHECK_SUCCESS(CurrentEntry->Events.Num() == 1);
	CHECK_SUCCESS(CurrentEntry->Events[0].Name == TEXT("EventTest"));

	UE_VLOG_EVENTS(World, NAME_None, EventTest, EventTest2);
	CHECK_SUCCESS(CurrentEntry != nullptr);
	CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
	CHECK_SUCCESS(CurrentEntry->Events.Num() == 2);
	CHECK_SUCCESS(CurrentEntry->Events[0].Counter == 2);
	CHECK_SUCCESS(CurrentEntry->Events[0].Name == TEXT("EventTest"));
	CHECK_SUCCESS(CurrentEntry->Events[1].Counter == 1);
	CHECK_SUCCESS(CurrentEntry->Events[1].Name == TEXT("EventTest2"));

	CurrentTimestamp = World->TimeSeconds;
	UE_VLOG_EVENTS(World, NAME_None, EventTest, EventTest2, EventTest3);

	{
		CHECK_SUCCESS(CurrentEntry != nullptr);
		CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
		CHECK_SUCCESS(CurrentEntry->Events.Num() == 3);
		CHECK_SUCCESS(CurrentEntry->Events[0].Counter == 3);
		CHECK_SUCCESS(CurrentEntry->Events[0].Name == TEXT("EventTest"));
		CHECK_SUCCESS(CurrentEntry->Events[1].Counter == 2);
		CHECK_SUCCESS(CurrentEntry->Events[1].Name == TEXT("EventTest2"));
		CHECK_SUCCESS(CurrentEntry->Events[2].Counter == 1);
		CHECK_SUCCESS(CurrentEntry->Events[2].Name == TEXT("EventTest3"));

		CHECK_SUCCESS(CurrentEntry->Events[0].UserFriendlyDesc == TEXT("Simple event for vlog tests"));
		CHECK_SUCCESS(CurrentEntry->Events[1].UserFriendlyDesc == TEXT("Second simple event for vlog tests"));
		CHECK_SUCCESS(CurrentEntry->Events[2].UserFriendlyDesc == TEXT("Third simple event for vlog tests"));

		const float NewTimestamp = CurrentTimestamp + 0.1;
		FVisualLogEntry* NewEntry = FVisualLogger::Get().GetEntryToWrite(World, NewTimestamp); //generate new entry and serialize old one
		CurrentEntry = &Context.Device.LastEntry;
		CHECK_SUCCESS(CurrentEntry != nullptr);
		CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
		CHECK_SUCCESS(CurrentEntry->Events.Num() == 3);
		CHECK_SUCCESS(CurrentEntry->Events[0].Counter == 3);
		CHECK_SUCCESS(CurrentEntry->Events[0].Name == TEXT("EventTest"));
		CHECK_SUCCESS(CurrentEntry->Events[1].Counter == 2);
		CHECK_SUCCESS(CurrentEntry->Events[1].Name == TEXT("EventTest2"));
		CHECK_SUCCESS(CurrentEntry->Events[2].Counter == 1);
		CHECK_SUCCESS(CurrentEntry->Events[2].Name == TEXT("EventTest3"));

		CHECK_SUCCESS(CurrentEntry->Events[0].UserFriendlyDesc == TEXT("Simple event for vlog tests"));
		CHECK_SUCCESS(CurrentEntry->Events[1].UserFriendlyDesc == TEXT("Second simple event for vlog tests"));
		CHECK_SUCCESS(CurrentEntry->Events[2].UserFriendlyDesc == TEXT("Third simple event for vlog tests"));

		CHECK_SUCCESS(NewEntry != nullptr);
		CHECK_SUCCESS(NewEntry->TimeStamp - NewTimestamp <= SMALL_NUMBER);
		CHECK_SUCCESS(NewEntry->Events.Num() == 0);
	}

	const FName EventTag1 = TEXT("ATLAS_C_0");
	const FName EventTag2 = TEXT("ATLAS_C_1");
	const FName EventTag3 = TEXT("ATLAS_C_2");

	CurrentTimestamp = World->TimeSeconds + 0.2;
	CurrentEntry = FVisualLogger::Get().GetEntryToWrite(World, CurrentTimestamp); //generate new entry and serialize old one
	UE_VLOG_EVENT_WITH_DATA(World, EventTest, EventTag1);
	CHECK_SUCCESS(CurrentEntry != nullptr);
	CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
	CHECK_SUCCESS(CurrentEntry->Events.Num() == 1);
	CHECK_SUCCESS(CurrentEntry->Events[0].Name == TEXT("EventTest"));
	CHECK_SUCCESS(CurrentEntry->Events[0].EventTags.Num() == 1);
	CHECK_SUCCESS(CurrentEntry->Events[0].EventTags[EventTag1] == 1);

	CurrentTimestamp = World->TimeSeconds + 0.3;
	CurrentEntry = FVisualLogger::Get().GetEntryToWrite(World, CurrentTimestamp); //generate new entry and serialize old one
	UE_VLOG_EVENT_WITH_DATA(World, EventTest, EventTag1, EventTag2, EventTag3);
	UE_VLOG_EVENT_WITH_DATA(World, EventTest, EventTag3);
	CHECK_SUCCESS(CurrentEntry != nullptr);
	CHECK_SUCCESS(CurrentEntry->TimeStamp == CurrentTimestamp);
	CHECK_SUCCESS(CurrentEntry->Events.Num() == 1);
	CHECK_SUCCESS(CurrentEntry->Events[0].Name == TEXT("EventTest"));
	CHECK_SUCCESS(CurrentEntry->Events[0].EventTags.Num() == 3);
	CHECK_SUCCESS(CurrentEntry->Events[0].EventTags[EventTag1] == 1);
	CHECK_SUCCESS(CurrentEntry->Events[0].EventTags[EventTag2] == 1);
	CHECK_SUCCESS(CurrentEntry->Events[0].EventTags[EventTag3] == 2);


	return true;
}

#undef CHECK_SUCCESS
#undef CHECK_FAIL

#endif //ENABLE_VISUAL_LOG
