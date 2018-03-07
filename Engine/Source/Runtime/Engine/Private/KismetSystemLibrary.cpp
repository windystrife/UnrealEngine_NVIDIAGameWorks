// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/RuntimeErrors.h"
#include "UObject/GCObject.h"
#include "EngineGlobals.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "LatentActions.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameEngine.h"
#include "Engine/Console.h"
#include "DelayAction.h"
#include "InterpolateComponentToAction.h"
#include "Interfaces/IAdvertisingProvider.h"
#include "Advertising.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StreamableManager.h"
#include "Net/OnlineEngineInterface.h"
#include "UserActivityTracking.h"
#include "KismetTraceUtils.h"
#include "Engine/AssetManager.h"
#include "HAL/PlatformApplicationMisc.h"

//////////////////////////////////////////////////////////////////////////
// UKismetSystemLibrary

UKismetSystemLibrary::UKismetSystemLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKismetSystemLibrary::StackTraceImpl(const FFrame& StackFrame)
{
	const FString Trace = StackFrame.GetStackTrace();
	UE_LOG(LogBlueprintUserMessages, Log, TEXT("\n%s"), *Trace);
}

FString UKismetSystemLibrary::GetObjectName(const UObject* Object)
{
	return GetNameSafe(Object);
}

FString UKismetSystemLibrary::GetPathName(const UObject* Object)
{
	return GetPathNameSafe(Object);
}

FString UKismetSystemLibrary::GetDisplayName(const UObject* Object)
{
#if WITH_EDITOR
	if (const AActor* Actor = Cast<const AActor>(Object))
	{
		return Actor->GetActorLabel();
	}
#endif

	if (const UActorComponent* Component = Cast<const UActorComponent>(Object))
	{
		return Component->GetReadableName();
	}

	return Object ? Object->GetName() : FString();
}

FString UKismetSystemLibrary::GetClassDisplayName(UClass* Class)
{
	return Class ? Class->GetName() : FString();
}

FString UKismetSystemLibrary::GetEngineVersion()
{
	return FEngineVersion::Current().ToString();
}

FString UKismetSystemLibrary::GetGameName()
{
	return FString(FApp::GetProjectName());
}

FString UKismetSystemLibrary::GetGameBundleId()
{
	return FString(FPlatformProcess::GetGameBundleId());
}

FString UKismetSystemLibrary::GetPlatformUserName()
{
	return FString(FPlatformProcess::UserName());
}

bool UKismetSystemLibrary::DoesImplementInterface(UObject* TestObject, TSubclassOf<UInterface> Interface)
{
	if (Interface != NULL && TestObject != NULL)
	{
		checkf(Interface->IsChildOf(UInterface::StaticClass()), TEXT("Interface parameter %s is not actually an interface."), *Interface->GetName());
		return TestObject->GetClass()->ImplementsInterface(Interface);
	}

	return false;
}

float UKismetSystemLibrary::GetGameTimeInSeconds(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetTimeSeconds() : 0.f;
}

bool UKismetSystemLibrary::IsServer(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? (World->GetNetMode() != NM_Client) : false;
}

bool UKismetSystemLibrary::IsDedicatedServer(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		return (World->GetNetMode() == NM_DedicatedServer);
	}
	return IsRunningDedicatedServer();
}

bool UKismetSystemLibrary::IsStandalone(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? (World->GetNetMode() == NM_Standalone) : false;
}

bool UKismetSystemLibrary::IsPackagedForDistribution()
{
	return FPlatformMisc::IsPackagedForDistribution();
}

FString UKismetSystemLibrary::GetUniqueDeviceId()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return FPlatformMisc::GetUniqueDeviceId();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FString UKismetSystemLibrary::GetDeviceId()
{
	return FPlatformMisc::GetDeviceId();
}

UObject* UKismetSystemLibrary::Conv_InterfaceToObject(const FScriptInterface& Interface)
{
	return Interface.GetObject();
}

void UKismetSystemLibrary::PrintString(UObject* WorldContextObject, const FString& InString, bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) // Do not Print in Shipping or Test

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	FString Prefix;
	if (World)
	{
		if (World->WorldType == EWorldType::PIE)
		{
			switch(World->GetNetMode())
			{
				case NM_Client:
					Prefix = FString::Printf(TEXT("Client %d: "), GPlayInEditorID - 1);
					break;
				case NM_DedicatedServer:
				case NM_ListenServer:
					Prefix = FString::Printf(TEXT("Server: "));
					break;
				case NM_Standalone:
					break;
			}
		}
	}
	
	const FString FinalDisplayString = Prefix + InString;
	FString FinalLogString = FinalDisplayString;

	static const FBoolConfigValueHelper DisplayPrintStringSource(TEXT("Kismet"), TEXT("bLogPrintStringSource"), GEngineIni);
	if (DisplayPrintStringSource)
	{
		const FString SourceObjectPrefix = FString::Printf(TEXT("[%s] "), *GetNameSafe(WorldContextObject));
		FinalLogString = SourceObjectPrefix + FinalLogString;
	}

	if (bPrintToLog)
	{
		UE_LOG(LogBlueprintUserMessages, Log, TEXT("%s"), *FinalLogString);
		
		APlayerController* PC = (WorldContextObject ? UGameplayStatics::GetPlayerController(WorldContextObject, 0) : NULL);
		ULocalPlayer* LocalPlayer = (PC ? Cast<ULocalPlayer>(PC->Player) : NULL);
		if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->ViewportConsole)
		{
			LocalPlayer->ViewportClient->ViewportConsole->OutputText(FinalDisplayString);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Verbose, TEXT("%s"), *FinalLogString);
	}

	// Also output to the screen, if possible
	if (bPrintToScreen)
	{
		if (GAreScreenMessagesEnabled)
		{
			if (GConfig && Duration < 0)
			{
				GConfig->GetFloat( TEXT("Kismet"), TEXT("PrintStringDuration"), Duration, GEngineIni );
			}
			GEngine->AddOnScreenDebugMessage((uint64)-1, Duration, TextColor.ToFColor(true), FinalDisplayString);
		}
		else
		{
			UE_LOG(LogBlueprint, VeryVerbose, TEXT("Screen messages disabled (!GAreScreenMessagesEnabled).  Cannot print to screen."));
		}
	}
#endif
}

void UKismetSystemLibrary::PrintText(UObject* WorldContextObject, const FText InText, bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) // Do not Print in Shipping or Test
	PrintString(WorldContextObject, InText.ToString(), bPrintToScreen, bPrintToLog, TextColor, Duration);
#endif
}

void UKismetSystemLibrary::PrintWarning(const FString& InString)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) // Do not Print in Shipping or Test
	PrintString(NULL, InString, true, true);
#endif
}

void UKismetSystemLibrary::SetWindowTitle(const FText& Title)
{
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine != nullptr)
	{
		TSharedPtr<SWindow> GameViewportWindow = GameEngine->GameViewportWindow.Pin();
		if (GameViewportWindow.IsValid())
		{
			GameViewportWindow->SetTitle(Title);
		}
	}
}

void UKismetSystemLibrary::ExecuteConsoleCommand(UObject* WorldContextObject, const FString& Command, APlayerController* Player)
{
	// First, try routing through the primary player
	APlayerController* TargetPC = Player ? Player : UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if( TargetPC )
	{
		TargetPC->ConsoleCommand(Command, true);
	}
}

void UKismetSystemLibrary::QuitGame(UObject* WorldContextObject, class APlayerController* SpecificPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference)
{
	APlayerController* TargetPC = SpecificPlayer ? SpecificPlayer : UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if( TargetPC )
	{
		if ( QuitPreference == EQuitPreference::Background)
		{
			TargetPC->ConsoleCommand("quit background");
		}
		else
		{
			TargetPC->ConsoleCommand("quit");
		}
	}
}

bool UKismetSystemLibrary::K2_IsValidTimerHandle(FTimerHandle TimerHandle)
{
	return TimerHandle.IsValid();
}

FTimerHandle UKismetSystemLibrary::K2_InvalidateTimerHandle(FTimerHandle& TimerHandle)
{
	TimerHandle.Invalidate();
	return TimerHandle;
}

FTimerHandle UKismetSystemLibrary::K2_SetTimer(UObject* Object, FString FunctionName, float Time, bool bLooping)
{
	FName const FunctionFName(*FunctionName);

	if (Object)
	{
		UFunction* const Func = Object->FindFunction(FunctionFName);
		if ( Func && (Func->ParmsSize > 0) )
		{
			// User passed in a valid function, but one that takes parameters
			// FTimerDynamicDelegate expects zero parameters and will choke on execution if it tries
			// to execute a mismatched function
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("SetTimer passed a function (%s) that expects parameters."), *FunctionName);
			return FTimerHandle();
		}
	}

	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionFName);
	return K2_SetTimerDelegate(Delegate, Time, bLooping);
}

FTimerHandle UKismetSystemLibrary::K2_SetTimerDelegate(FTimerDynamicDelegate Delegate, float Time, bool bLooping)
{
	FTimerHandle Handle;
	if (Delegate.IsBound())
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.SetTimer(Handle, Delegate, Time, bLooping);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("SetTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}

	return Handle;
}

void UKismetSystemLibrary::K2_ClearTimer(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	K2_ClearTimerDelegate(Delegate);
}

void UKismetSystemLibrary::K2_ClearTimerDelegate(FTimerDynamicDelegate Delegate)
{
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.ClearTimer(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("ClearTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
}

void UKismetSystemLibrary::K2_ClearTimerHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
}

void UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(UObject* WorldContextObject, FTimerHandle& Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
}

void UKismetSystemLibrary::K2_PauseTimer(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	K2_PauseTimerDelegate(Delegate);
}

void UKismetSystemLibrary::K2_PauseTimerDelegate(FTimerDynamicDelegate Delegate)
{
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.PauseTimer(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("PauseTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
}

void UKismetSystemLibrary::K2_PauseTimerHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			World->GetTimerManager().PauseTimer(Handle);
		}
	}
}

void UKismetSystemLibrary::K2_UnPauseTimer(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	K2_UnPauseTimerDelegate(Delegate);
}

void UKismetSystemLibrary::K2_UnPauseTimerDelegate(FTimerDynamicDelegate Delegate)
{
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.UnPauseTimer(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning,
			TEXT("UnPauseTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
}

void UKismetSystemLibrary::K2_UnPauseTimerHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			World->GetTimerManager().UnPauseTimer(Handle);
		}
	}
}

bool UKismetSystemLibrary::K2_IsTimerActive(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_IsTimerActiveDelegate(Delegate);
}

bool UKismetSystemLibrary::K2_IsTimerActiveDelegate(FTimerDynamicDelegate Delegate)
{
	bool bIsActive = false;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			bIsActive = TimerManager.IsTimerActive(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("IsTimerActive passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}

	return bIsActive;
}

bool UKismetSystemLibrary::K2_IsTimerActiveHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	bool bIsActive = false;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			bIsActive = World->GetTimerManager().IsTimerActive(Handle);
		}
	}

	return bIsActive;
}

bool UKismetSystemLibrary::K2_IsTimerPaused(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_IsTimerPausedDelegate(Delegate);
}

bool UKismetSystemLibrary::K2_IsTimerPausedDelegate(FTimerDynamicDelegate Delegate)
{
	bool bIsPaused = false;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			bIsPaused = TimerManager.IsTimerPaused(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("IsTimerPaused passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return bIsPaused;
}

bool UKismetSystemLibrary::K2_IsTimerPausedHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	bool bIsPaused = false;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			bIsPaused = World->GetTimerManager().IsTimerPaused(Handle);
		}
	}

	return bIsPaused;
}

bool UKismetSystemLibrary::K2_TimerExists(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_TimerExistsDelegate(Delegate);
}

bool UKismetSystemLibrary::K2_TimerExistsDelegate(FTimerDynamicDelegate Delegate)
{
	bool bTimerExists = false;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			bTimerExists = TimerManager.TimerExists(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning,
			TEXT("TimerExists passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return bTimerExists;
}

bool UKismetSystemLibrary::K2_TimerExistsHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	bool bTimerExists = false;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			bTimerExists = World->GetTimerManager().TimerExists(Handle);
		}
	}

	return bTimerExists;
}

float UKismetSystemLibrary::K2_GetTimerElapsedTime(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_GetTimerElapsedTimeDelegate(Delegate);
}

float UKismetSystemLibrary::K2_GetTimerElapsedTimeDelegate(FTimerDynamicDelegate Delegate)
{
	float ElapsedTime = 0.f;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			ElapsedTime = TimerManager.GetTimerElapsed(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("GetTimerElapsedTime passed a bad function (%s) or object (%s)"), 
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return ElapsedTime;
}

float UKismetSystemLibrary::K2_GetTimerElapsedTimeHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	float ElapsedTime = 0.f;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			ElapsedTime = World->GetTimerManager().GetTimerElapsed(Handle);
		}
	}

	return ElapsedTime;
}

float UKismetSystemLibrary::K2_GetTimerRemainingTime(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_GetTimerRemainingTimeDelegate(Delegate);
}

float UKismetSystemLibrary::K2_GetTimerRemainingTimeDelegate(FTimerDynamicDelegate Delegate)
{
	float RemainingTime = 0.f;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			RemainingTime = TimerManager.GetTimerRemaining(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("GetTimerRemainingTime passed a bad function (%s) or object (%s)"), 
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return RemainingTime;
}

float UKismetSystemLibrary::K2_GetTimerRemainingTimeHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	float RemainingTime = 0.f;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			RemainingTime = World->GetTimerManager().GetTimerRemaining(Handle);
		}
	}

	return RemainingTime;
}

void UKismetSystemLibrary::SetIntPropertyByName(UObject* Object, FName PropertyName, int32 Value)
{
	if(Object != NULL)
	{
		UIntProperty* IntProp = FindField<UIntProperty>(Object->GetClass(), PropertyName);
		if(IntProp != NULL)
		{
			IntProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetBytePropertyByName(UObject* Object, FName PropertyName, uint8 Value)
{
	if(Object != NULL)
	{
		if(UByteProperty* ByteProp = FindField<UByteProperty>(Object->GetClass(), PropertyName))
		{
			ByteProp->SetPropertyValue_InContainer(Object, Value);
		}
		else if(UEnumProperty* EnumProp = FindField<UEnumProperty>(Object->GetClass(), PropertyName))
		{
			void* PropAddr = EnumProp->ContainerPtrToValuePtr<void>(Object);
			UNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			UnderlyingProp->SetIntPropertyValue(PropAddr, (int64)Value);
		}
	}
}

void UKismetSystemLibrary::SetFloatPropertyByName(UObject* Object, FName PropertyName, float Value)
{
	if(Object != NULL)
	{
		UFloatProperty* FloatProp = FindField<UFloatProperty>(Object->GetClass(), PropertyName);
		if(FloatProp != NULL)
		{
			FloatProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetBoolPropertyByName(UObject* Object, FName PropertyName, bool Value)
{
	if(Object != NULL)
	{
		UBoolProperty* BoolProp = FindField<UBoolProperty>(Object->GetClass(), PropertyName);
		if(BoolProp != NULL)
		{
			BoolProp->SetPropertyValue_InContainer(Object, Value );
		}
	}
}

void UKismetSystemLibrary::SetObjectPropertyByName(UObject* Object, FName PropertyName, UObject* Value)
{
	if(Object != NULL && Value != NULL)
	{
		UObjectPropertyBase* ObjectProp = FindField<UObjectPropertyBase>(Object->GetClass(), PropertyName);
		if(ObjectProp != NULL && Value->IsA(ObjectProp->PropertyClass)) // check it's the right type
		{
			ObjectProp->SetObjectPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetClassPropertyByName(UObject* Object, FName PropertyName, TSubclassOf<UObject> Value)
{
	if (Object && *Value)
	{
		UClassProperty* ClassProp = FindField<UClassProperty>(Object->GetClass(), PropertyName);
		if (ClassProp != NULL && Value->IsChildOf(ClassProp->MetaClass)) // check it's the right type
		{
			ClassProp->SetObjectPropertyValue_InContainer(Object, *Value);
		}
	}
}

void UKismetSystemLibrary::SetInterfacePropertyByName(UObject* Object, FName PropertyName, const FScriptInterface& Value)
{
	if (Object)
	{
		UInterfaceProperty* InterfaceProp = FindField<UInterfaceProperty>(Object->GetClass(), PropertyName);
		if (InterfaceProp != NULL && Value.GetObject()->GetClass()->ImplementsInterface(InterfaceProp->InterfaceClass)) // check it's the right type
		{
			InterfaceProp->SetPropertyValue_InContainer(Object, Value);
		}
	}
}

void UKismetSystemLibrary::SetStringPropertyByName(UObject* Object, FName PropertyName, const FString& Value)
{
	if(Object != NULL)
	{
		UStrProperty* StringProp = FindField<UStrProperty>(Object->GetClass(), PropertyName);
		if(StringProp != NULL)
		{
			StringProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetNamePropertyByName(UObject* Object, FName PropertyName, const FName& Value)
{
	if(Object != NULL)
	{
		UNameProperty* NameProp = FindField<UNameProperty>(Object->GetClass(), PropertyName);
		if(NameProp != NULL)
		{
			NameProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetSoftObjectPropertyByName(UObject* Object, FName PropertyName, const TSoftObjectPtr<UObject>& Value)
{
	if (Object != NULL)
	{
		USoftObjectProperty* ObjectProp = FindField<USoftObjectProperty>(Object->GetClass(), PropertyName);
		const FSoftObjectPtr* SoftObjectPtr = (const FSoftObjectPtr*)(&Value);
		ObjectProp->SetPropertyValue_InContainer(Object, *SoftObjectPtr);
	}
}

void UKismetSystemLibrary::SetSoftClassPropertyByName(UObject* Object, FName PropertyName, const TSoftClassPtr<UObject>& Value)
{
	if (Object != NULL)
	{
		USoftClassProperty* ObjectProp = FindField<USoftClassProperty>(Object->GetClass(), PropertyName);
		const FSoftObjectPtr* SoftObjectPtr = (const FSoftObjectPtr*)(&Value);
		ObjectProp->SetPropertyValue_InContainer(Object, *SoftObjectPtr);
	}
}

FSoftObjectPath UKismetSystemLibrary::MakeSoftObjectPath(const FString& PathString)
{
	FSoftObjectPath SoftObjectPath(PathString);
	if (!PathString.IsEmpty() && !SoftObjectPath.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PathString"), FText::AsCultureInvariant(PathString));
		LogRuntimeError(FText::Format(NSLOCTEXT("KismetSystemLibrary", "PathStringInvalid", "Object path {PathString} not valid for MakeSoftObjectPath."), Args));
	}

	return SoftObjectPath;
}

void UKismetSystemLibrary::BreakSoftObjectPath(FSoftObjectPath InSoftObjectPath, FString& PathString)
{
	PathString = InSoftObjectPath.ToString();
}

bool UKismetSystemLibrary::IsValidSoftObjectReference(const TSoftObjectPtr<UObject>& SoftObjectReference)
{
	return !SoftObjectReference.IsNull();
}

FString UKismetSystemLibrary::Conv_SoftObjectReferenceToString(const TSoftObjectPtr<UObject>& SoftObjectReference)
{
	return SoftObjectReference.ToString();
}

bool UKismetSystemLibrary::EqualEqual_SoftObjectReference(const TSoftObjectPtr<UObject>& A, const TSoftObjectPtr<UObject>& B)
{
	return A == B;
}

bool UKismetSystemLibrary::NotEqual_SoftObjectReference(const TSoftObjectPtr<UObject>& A, const TSoftObjectPtr<UObject>& B)
{
	return A != B;
}

bool UKismetSystemLibrary::IsValidSoftClassReference(const TSoftClassPtr<UObject>& SoftClassReference)
{
	return !SoftClassReference.IsNull();
}

FString UKismetSystemLibrary::Conv_SoftClassReferenceToString(const TSoftClassPtr<UObject>& SoftClassReference)
{
	return SoftClassReference.ToString();
}

bool UKismetSystemLibrary::EqualEqual_SoftClassReference(const TSoftClassPtr<UObject>& A, const TSoftClassPtr<UObject>& B)
{
	return A == B;
}

bool UKismetSystemLibrary::NotEqual_SoftClassReference(const TSoftClassPtr<UObject>& A, const TSoftClassPtr<UObject>& B)
{
	return A != B;
}

UObject* UKismetSystemLibrary::Conv_SoftObjectReferenceToObject(const TSoftObjectPtr<UObject>& SoftObject)
{
	return SoftObject.Get();
}

TSubclassOf<UObject> UKismetSystemLibrary::Conv_SoftClassReferenceToClass(const TSoftClassPtr<UObject>& SoftClass)
{
	return SoftClass.Get();
}

TSoftObjectPtr<UObject> UKismetSystemLibrary::Conv_ObjectToSoftObjectReference(UObject *Object)
{
	return TSoftObjectPtr<UObject>(Object);
}

TSoftClassPtr<UObject> UKismetSystemLibrary::Conv_ClassToSoftClassReference(const TSubclassOf<UObject>& Class)
{
	return TSoftClassPtr<UObject>(*Class);
}

void UKismetSystemLibrary::SetTextPropertyByName(UObject* Object, FName PropertyName, const FText& Value)
{
	if(Object != NULL)
	{
		UTextProperty* TextProp = FindField<UTextProperty>(Object->GetClass(), PropertyName);
		if(TextProp != NULL)
		{
			TextProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetVectorPropertyByName(UObject* Object, FName PropertyName, const FVector& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
		UStructProperty* VectorProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(VectorProp != NULL && VectorProp->Struct == VectorStruct)
		{
			*VectorProp->ContainerPtrToValuePtr<FVector>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetRotatorPropertyByName(UObject* Object, FName PropertyName, const FRotator& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();
		UStructProperty* RotatorProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(RotatorProp != NULL && RotatorProp->Struct == RotatorStruct)
		{
			*RotatorProp->ContainerPtrToValuePtr<FRotator>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetLinearColorPropertyByName(UObject* Object, FName PropertyName, const FLinearColor& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* ColorStruct = TBaseStructure<FLinearColor>::Get();
		UStructProperty* ColorProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(ColorProp != NULL && ColorProp->Struct == ColorStruct)
		{
			*ColorProp->ContainerPtrToValuePtr<FLinearColor>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetTransformPropertyByName(UObject* Object, FName PropertyName, const FTransform& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();
		UStructProperty* TransformProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(TransformProp != NULL && TransformProp->Struct == TransformStruct)
		{
			*TransformProp->ContainerPtrToValuePtr<FTransform>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetCollisionProfileNameProperty(UObject* Object, FName PropertyName, const FCollisionProfileName& Value)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
}

void UKismetSystemLibrary::Generic_SetStructurePropertyByName(UObject* OwnerObject, FName StructPropertyName, const void* SrcStructAddr)
{
	if (OwnerObject != NULL)
	{
		UStructProperty* StructProp = FindField<UStructProperty>(OwnerObject->GetClass(), StructPropertyName);
		if (StructProp != NULL)
		{
			void* Dest = StructProp->ContainerPtrToValuePtr<void>(OwnerObject);
			StructProp->CopyValuesInternal(Dest, SrcStructAddr, 1);
		}
	}
}

void UKismetSystemLibrary::GetActorListFromComponentList(const TArray<UPrimitiveComponent*>& ComponentList, UClass* ActorClassFilter, TArray<class AActor*>& OutActorList)
{
	OutActorList.Empty();
	for (int32 CompIdx=0; CompIdx<ComponentList.Num(); ++CompIdx)
	{
		UPrimitiveComponent* const C = ComponentList[CompIdx];
		if (C)
		{
			AActor* const Owner = C->GetOwner();
			if (Owner)
			{
				if ( !ActorClassFilter || Owner->IsA(ActorClassFilter) )
				{
					OutActorList.AddUnique(Owner);
				}
			}
		}
	}
}



bool UKismetSystemLibrary::SphereOverlapActors(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = SphereOverlapComponents(WorldContextObject, SpherePos, SphereRadius, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::SphereOverlapComponents(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SphereOverlapComponents), false);
	Params.AddIgnoredActors(ActorsToIgnore);
	Params.bTraceAsyncScene = true;
	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}


	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if(World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, SpherePos, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(SphereRadius), Params);
	}

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}



bool UKismetSystemLibrary::BoxOverlapActors(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = BoxOverlapComponents(WorldContextObject, BoxPos, BoxExtent, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::BoxOverlapComponents(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoxOverlapComponents), false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, BoxPos, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(BoxExtent), Params);
	}

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::CapsuleOverlapActors(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = CapsuleOverlapComponents(WorldContextObject, CapsulePos, Radius, HalfHeight, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::CapsuleOverlapComponents(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CapsuleOverlapComponents), false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, CapsulePos, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	}

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::ComponentOverlapActors(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = ComponentOverlapComponents(Component, ComponentTransform, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::ComponentOverlapComponents(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	if(Component != nullptr)
	{
		FComponentQueryParams Params(SCENE_QUERY_STAT(ComponentOverlapComponents));
		Params.bTraceAsyncScene = true;
		Params.AddIgnoredActors(ActorsToIgnore);

		TArray<FOverlapResult> Overlaps;

		FCollisionObjectQueryParams ObjectParams;
		for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
		{
			const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
			ObjectParams.AddObjectTypesToQuery(Channel);
		}

		check( Component->GetWorld());
		Component->GetWorld()->ComponentOverlapMulti(Overlaps, Component, ComponentTransform.GetTranslation(), ComponentTransform.GetRotation(), Params, ObjectParams);

		for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
		{
			FOverlapResult const& O = Overlaps[OverlapIdx];
			if (O.Component.IsValid())
			{ 
				if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
				{
					OutComponents.Add(O.Component.Get());
				}
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::LineTraceSingle(UObject* WorldContextObject, const FVector Start, const FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName LineTraceSingleName(TEXT("LineTraceSingle"));
	FCollisionQueryParams Params = ConfigureCollisionParams(LineTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->LineTraceSingleByChannel(OutHit, Start, End, CollisionChannel, Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugLineTraceSingle(World, Start, End, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::LineTraceMulti(UObject* WorldContextObject, const FVector Start, const FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName LineTraceMultiName(TEXT("LineTraceMulti"));
	FCollisionQueryParams Params = ConfigureCollisionParams(LineTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->LineTraceMultiByChannel(OutHits, Start, End, CollisionChannel, Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugLineTraceMulti(World, Start, End, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceSingle(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName BoxTraceSingleName(TEXT("BoxTraceSingle"));
	FCollisionQueryParams Params = ConfigureCollisionParams(BoxTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByChannel(OutHit, Start, End, Orientation.Quaternion(), UEngineTypes::ConvertToCollisionChannel(TraceChannel), FCollisionShape::MakeBox(HalfSize), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugBoxTraceSingle(World, Start, End, HalfSize, Orientation, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceMulti(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName BoxTraceMultiName(TEXT("BoxTraceMulti"));
	FCollisionQueryParams Params = ConfigureCollisionParams(BoxTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByChannel(OutHits, Start, End, Orientation.Quaternion(), UEngineTypes::ConvertToCollisionChannel(TraceChannel), FCollisionShape::MakeBox(HalfSize), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugBoxTraceMulti(World, Start, End, HalfSize, Orientation, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceSingle(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName SphereTraceSingleName(TEXT("SphereTraceSingle"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(Radius), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceSingle(World, Start, End, Radius, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceMulti(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName SphereTraceMultiName(TEXT("SphereTraceMulti"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByChannel(OutHits, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(Radius), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceMulti(World, Start, End, Radius, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceSingle(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName CapsuleTraceSingleName(TEXT("CapsuleTraceSingle"));
	FCollisionQueryParams Params = ConfigureCollisionParams(CapsuleTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceSingle(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}


bool UKismetSystemLibrary::CapsuleTraceMulti(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName CapsuleTraceMultiName(TEXT("CapsuleTraceMulti"));
	FCollisionQueryParams Params = ConfigureCollisionParams(CapsuleTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByChannel(OutHits, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceMulti(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;	
}

/** Object Query functions **/
bool UKismetSystemLibrary::LineTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceSingleName(TEXT("LineTraceSingleForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(LineTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugLineTraceSingle(World, Start, End, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::LineTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceMultiName(TEXT("LineTraceMultiForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(LineTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->LineTraceMultiByObjectType(OutHits, Start, End, ObjectParams, Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugLineTraceMulti(World, Start, End, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName SphereTraceSingleName(TEXT("SphereTraceSingleForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceSingle(World, Start, End, Radius, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName SphereTraceMultiName(TEXT("SphereTraceMultiForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByObjectType(OutHits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceMulti(World, Start, End, Radius, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName BoxTraceSingleName(TEXT("BoxTraceSingleForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(BoxTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByObjectType(OutHit, Start, End, Orientation.Quaternion(), ObjectParams, FCollisionShape::MakeBox(HalfSize), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugBoxTraceSingle(World, Start, End, HalfSize, Orientation, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, const TArray<TEnumAsByte<	EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName BoxTraceMultiName(TEXT("BoxTraceMultiForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(BoxTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByObjectType(OutHits, Start, End, Orientation.Quaternion(), ObjectParams, FCollisionShape::MakeBox(HalfSize), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugBoxTraceMulti(World, Start, End, HalfSize, Orientation, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName CapsuleTraceSingleName(TEXT("CapsuleTraceSingleForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(CapsuleTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceSingle(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName CapsuleTraceMultiName(TEXT("CapsuleTraceMultiForObjects"));
	FCollisionQueryParams Params = ConfigureCollisionParams(CapsuleTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	FCollisionObjectQueryParams ObjectParams = ConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByObjectType(OutHits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceMulti(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}


bool UKismetSystemLibrary::LineTraceSingleByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceSingleName(TEXT("LineTraceSingleByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(LineTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->LineTraceSingleByProfile(OutHit, Start, End, ProfileName, Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugLineTraceSingle(World, Start, End, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::LineTraceMultiByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceMultiName(TEXT("LineTraceMultiByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(LineTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->LineTraceMultiByProfile(OutHits, Start, End, ProfileName, Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugLineTraceMulti(World, Start, End, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceSingleByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName BoxTraceSingleName(TEXT("BoxTraceSingleByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(BoxTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	bool const bHit = World ? World->SweepSingleByProfile(OutHit, Start, End, Orientation.Quaternion(), ProfileName, FCollisionShape::MakeBox(HalfSize), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugBoxTraceSingle(World, Start, End, HalfSize, Orientation, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceMultiByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName BoxTraceMultiName(TEXT("BoxTraceMultiByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(BoxTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByProfile(OutHits, Start, End, Orientation.Quaternion(), ProfileName, FCollisionShape::MakeBox(HalfSize), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugBoxTraceMulti(World, Start, End, HalfSize, Orientation, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceSingleByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName SphereTraceSingleName(TEXT("SphereTraceSingleByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByProfile(OutHit, Start, End, FQuat::Identity, ProfileName, FCollisionShape::MakeSphere(Radius), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceSingle(World, Start, End, Radius, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceMultiByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName SphereTraceMultiName(TEXT("SphereTraceMultiByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByProfile(OutHits, Start, End, FQuat::Identity, ProfileName, FCollisionShape::MakeSphere(Radius), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceMulti(World, Start, End, Radius, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceSingleByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName CapsuleTraceSingleName(TEXT("CapsuleTraceSingleByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(CapsuleTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByProfile(OutHit, Start, End, FQuat::Identity, ProfileName, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceSingle(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}


bool UKismetSystemLibrary::CapsuleTraceMultiByProfile(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, FName ProfileName, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName CapsuleTraceMultiName(TEXT("CapsuleTraceMultiByProfile"));
	FCollisionQueryParams Params = ConfigureCollisionParams(CapsuleTraceMultiName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepMultiByProfile(OutHits, Start, End, FQuat::Identity, ProfileName, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceMulti(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, DrawTime);
#endif

	return bHit;
}


/** Draw a debug line */
void UKismetSystemLibrary::DrawDebugLine(UObject* WorldContextObject, FVector const LineStart, FVector const LineEnd, FLinearColor Color, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugLine(World, LineStart, LineEnd, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug circle */
void UKismetSystemLibrary::DrawDebugCircle(UObject* WorldContextObject,FVector Center, float Radius, int32 NumSegments, FLinearColor LineColor, float LifeTime, float Thickness, FVector YAxis, FVector ZAxis, bool bDrawAxis)
{ 
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugCircle(World, Center, Radius, NumSegments, LineColor.ToFColor(true), false, LifeTime, SDPG_World, Thickness, YAxis, ZAxis, bDrawAxis);
	}
#endif
}

/** Draw a debug point */
void UKismetSystemLibrary::DrawDebugPoint(UObject* WorldContextObject, FVector const Position, float Size, FLinearColor PointColor, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugPoint(World, Position, Size, PointColor.ToFColor(true), false, LifeTime, SDPG_World);
	}
#endif
}

/** Draw directional arrow, pointing from LineStart to LineEnd. */
void UKismetSystemLibrary::DrawDebugArrow(UObject* WorldContextObject, FVector const LineStart, FVector const LineEnd, float ArrowSize, FLinearColor Color, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		::DrawDebugDirectionalArrow(World, LineStart, LineEnd, ArrowSize, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug box */
void UKismetSystemLibrary::DrawDebugBox(UObject* WorldContextObject, FVector const Center, FVector Extent, FLinearColor Color, const FRotator Rotation, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (Rotation == FRotator::ZeroRotator)
		{
			::DrawDebugBox(World, Center, Extent, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
		}
		else
		{
			::DrawDebugBox(World, Center, Extent, Rotation.Quaternion(), Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
		}
	}
#endif
}

/** Draw a debug coordinate system. */
void UKismetSystemLibrary::DrawDebugCoordinateSystem(UObject* WorldContextObject, FVector const AxisLoc, FRotator const AxisRot, float Scale, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugCoordinateSystem(World, AxisLoc, AxisRot, Scale, false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug sphere */
void UKismetSystemLibrary::DrawDebugSphere(UObject* WorldContextObject, FVector const Center, float Radius, int32 Segments, FLinearColor Color, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugSphere(World, Center, Radius, Segments, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug cylinder */
void UKismetSystemLibrary::DrawDebugCylinder(UObject* WorldContextObject, FVector const Start, FVector const End, float Radius, int32 Segments, FLinearColor Color, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugCylinder(World, Start, End, Radius, Segments, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug cone */
void UKismetSystemLibrary::DrawDebugCone(UObject* WorldContextObject, FVector const Origin, FVector const Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FLinearColor Color, float Duration, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugCone(World, Origin, Direction, Length, AngleWidth, AngleHeight, NumSides, Color.ToFColor(true), false, Duration, SDPG_World, Thickness);
	}
#endif
}

void UKismetSystemLibrary::DrawDebugConeInDegrees(UObject* WorldContextObject, FVector const Origin, FVector const Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FLinearColor Color, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugCone(World, Origin, Direction, Length, FMath::DegreesToRadians(AngleWidth), FMath::DegreesToRadians(AngleHeight), NumSides, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug capsule */
void UKismetSystemLibrary::DrawDebugCapsule(UObject* WorldContextObject, FVector const Center, float HalfHeight, float Radius, const FRotator Rotation, FLinearColor Color, float LifeTime, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugCapsule(World, Center, HalfHeight, Radius, Rotation.Quaternion(), Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug string at a 3d world location. */
void UKismetSystemLibrary::DrawDebugString(UObject* WorldContextObject, FVector const TextLocation, const FString& Text, class AActor* TestBaseActor, FLinearColor TextColor, float Duration)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugString(World, TextLocation, Text, TestBaseActor, TextColor.ToFColor(true), Duration);
	}
#endif
}

/** Removes all debug strings. */
void UKismetSystemLibrary::FlushDebugStrings( UObject* WorldContextObject )
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::FlushDebugStrings( World );
	}
#endif
}

/** Draws a debug plane. */
void UKismetSystemLibrary::DrawDebugPlane(UObject* WorldContextObject, FPlane const& P, FVector const Loc, float Size, FLinearColor Color, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugSolidPlane(World, P, Loc, Size, Color.ToFColor(true), false, LifeTime, SDPG_World);
	}
#endif
}

/** Flush all persistent debug lines and shapes */
void UKismetSystemLibrary::FlushPersistentDebugLines(UObject* WorldContextObject)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::FlushPersistentDebugLines( World );
	}
#endif
}

/** Draws a debug frustum. */
void UKismetSystemLibrary::DrawDebugFrustum(UObject* WorldContextObject, const FTransform& FrustumTransform, FLinearColor FrustumColor, float Duration, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr && FrustumTransform.IsRotationNormalized())
	{
		FMatrix FrustumToWorld =  FrustumTransform.ToMatrixWithScale();
		::DrawDebugFrustum(World, FrustumToWorld, FrustumColor.ToFColor(true), false, Duration, SDPG_World, Thickness);
	}
#endif
}

/** Draw a debug camera shape. */
void UKismetSystemLibrary::DrawDebugCamera(const ACameraActor* CameraActor, FLinearColor CameraColor, float Duration)
{
#if ENABLE_DRAW_DEBUG
	if(CameraActor)
	{
		FVector CamLoc = CameraActor->GetActorLocation();
		FRotator CamRot = CameraActor->GetActorRotation();
		::DrawDebugCamera(CameraActor->GetWorld(), CameraActor->GetActorLocation(), CameraActor->GetActorRotation(), CameraActor->GetCameraComponent()->FieldOfView, 1.0f, CameraColor.ToFColor(true), false, Duration, SDPG_World);
	}
#endif
}

void UKismetSystemLibrary::DrawDebugFloatHistoryTransform(UObject* WorldContextObject, const FDebugFloatHistory& FloatHistory, const FTransform& DrawTransform, FVector2D DrawSize, FLinearColor DrawColor, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugFloatHistory(*World, FloatHistory, DrawTransform, DrawSize, DrawColor.ToFColor(true), false, LifeTime);
	}
#endif
}

void UKismetSystemLibrary::DrawDebugFloatHistoryLocation(UObject* WorldContextObject, const FDebugFloatHistory& FloatHistory, FVector DrawLocation, FVector2D DrawSize, FLinearColor DrawColor, float LifeTime)
{
#if ENABLE_DRAW_DEBUG
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		::DrawDebugFloatHistory(*World, FloatHistory, DrawLocation, DrawSize, DrawColor.ToFColor(true), false, LifeTime);
	}
#endif
}

FDebugFloatHistory UKismetSystemLibrary::AddFloatHistorySample(float Value, const FDebugFloatHistory& FloatHistory)
{
	FDebugFloatHistory* const MutableFloatHistory = const_cast<FDebugFloatHistory*>(&FloatHistory);
	MutableFloatHistory->AddSample(Value);
	return FloatHistory;
}

/** Mark as modified. */
void UKismetSystemLibrary::CreateCopyForUndoBuffer(UObject* ObjectToModify)
{
	if (ObjectToModify != NULL)
	{
		ObjectToModify->Modify();
	}
}

void UKismetSystemLibrary::GetComponentBounds(const USceneComponent* Component, FVector& Origin, FVector& BoxExtent, float& SphereRadius)
{
	if (Component != NULL)
	{
		Origin = Component->Bounds.Origin;
		BoxExtent = Component->Bounds.BoxExtent;
		SphereRadius = Component->Bounds.SphereRadius;
	}
	else
	{
		Origin = FVector::ZeroVector;
		BoxExtent = FVector::ZeroVector;
		SphereRadius = 0.0f;
		UE_LOG(LogBlueprintUserMessages, Verbose, TEXT("GetComponentBounds passed a bad component"));
	}
}

void UKismetSystemLibrary::GetActorBounds(const AActor* Actor, FVector& Origin, FVector& BoxExtent)
{
	if (Actor != NULL)
	{
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);

		// To keep consistency with the other GetBounds functions, transform our result into an origin / extent formatting
		Bounds.GetCenterAndExtents(Origin, BoxExtent);
	}
	else
	{
		Origin = FVector::ZeroVector;
		BoxExtent = FVector::ZeroVector;
		UE_LOG(LogBlueprintUserMessages, Verbose, TEXT("GetActorBounds passed a bad actor"));
	}
}


void UKismetSystemLibrary::Delay(UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo )
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == NULL)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(Duration, LatentInfo));
		}
	}
}

// Delay execution by Duration seconds; Calling again before the delay has expired will reset the countdown to Duration.
void UKismetSystemLibrary::RetriggerableDelay(UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FDelayAction* Action = LatentActionManager.FindExistingAction<FDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		if (Action == NULL)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(Duration, LatentInfo));
		}
		else
		{
			// Reset the existing delay to the new duration
			Action->TimeRemaining = Duration;
		}
	}
}

void UKismetSystemLibrary::MoveComponentTo(USceneComponent* Component, FVector TargetRelativeLocation, FRotator TargetRelativeRotation, bool bEaseOut, bool bEaseIn, float OverTime, bool bForceShortestRotationPath, TEnumAsByte<EMoveComponentAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(Component, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FInterpolateComponentToAction* Action = LatentActionManager.FindExistingAction<FInterpolateComponentToAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);

		const FVector ComponentLocation = (Component != NULL) ? Component->RelativeLocation : FVector::ZeroVector;
		const FRotator ComponentRotation = (Component != NULL) ? Component->RelativeRotation : FRotator::ZeroRotator;

		// If not currently running
		if (Action == NULL)
		{
			if (MoveAction == EMoveComponentAction::Move)
			{
				// Only act on a 'move' input if not running
				Action = new FInterpolateComponentToAction(OverTime, LatentInfo, Component, bEaseOut, bEaseIn, bForceShortestRotationPath);

				Action->TargetLocation = TargetRelativeLocation;
				Action->TargetRotation = TargetRelativeRotation;

				Action->InitialLocation = ComponentLocation;
				Action->InitialRotation = ComponentRotation;

				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
			}
		}
		else
		{
			if (MoveAction == EMoveComponentAction::Move)
			{
				// A 'Move' action while moving restarts interpolation
				Action->TotalTime = OverTime;
				Action->TimeElapsed = 0.f;

				Action->TargetLocation = TargetRelativeLocation;
				Action->TargetRotation = TargetRelativeRotation;

				Action->InitialLocation = ComponentLocation;
				Action->InitialRotation = ComponentRotation;
			}
			else if (MoveAction == EMoveComponentAction::Stop)
			{
				// 'Stop' just stops the interpolation where it is
				Action->bInterpolating = false;
			}
			else if (MoveAction == EMoveComponentAction::Return)
			{
				// Return moves back to the beginning
				Action->TotalTime = Action->TimeElapsed;
				Action->TimeElapsed = 0.f;

				// Set our target to be our initial, and set the new initial to be the current position
				Action->TargetLocation = Action->InitialLocation;
				Action->TargetRotation = Action->InitialRotation;

				Action->InitialLocation = ComponentLocation;
				Action->InitialRotation = ComponentRotation;
			}
		}
	}
}

int32 UKismetSystemLibrary::GetRenderingDetailMode()
{
	static const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DetailMode"));

	// clamp range
	int32 Ret = FMath::Clamp(CVar->GetInt(), 0, 2);

	return Ret;
}

int32 UKismetSystemLibrary::GetRenderingMaterialQualityLevel()
{
	static const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MaterialQualityLevel"));

	// clamp range
	int32 Ret = FMath::Clamp(CVar->GetInt(), 0, (int32)EMaterialQualityLevel::Num - 1);

	return Ret;
}

bool UKismetSystemLibrary::GetSupportedFullscreenResolutions(TArray<FIntPoint>& Resolutions)
{
	uint32 MinYResolution = UKismetSystemLibrary::GetMinYResolutionForUI();

	FScreenResolutionArray SupportedResolutions;
	if ( RHIGetAvailableResolutions(SupportedResolutions, true) )
	{
		uint32 LargestY = 0;
		for ( const FScreenResolutionRHI& SupportedResolution : SupportedResolutions )
		{
			LargestY = FMath::Max(LargestY, SupportedResolution.Height);
			if(SupportedResolution.Height >= MinYResolution)
			{
				FIntPoint Resolution;
				Resolution.X = SupportedResolution.Width;
				Resolution.Y = SupportedResolution.Height;

				Resolutions.Add(Resolution);
			}
		}
		if(!Resolutions.Num())
		{
			// if we don't have any resolution, we take the largest one(s)
			for ( const FScreenResolutionRHI& SupportedResolution : SupportedResolutions )
			{
				if(SupportedResolution.Height == LargestY)
				{
					FIntPoint Resolution;
					Resolution.X = SupportedResolution.Width;
					Resolution.Y = SupportedResolution.Height;

					Resolutions.Add(Resolution);
				}
			}
		}

		return true;
	}

	return false;
}

bool UKismetSystemLibrary::GetConvenientWindowedResolutions(TArray<FIntPoint>& Resolutions)
{
	FDisplayMetrics DisplayMetrics;
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
	}
	else
	{
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
	}

	extern void GenerateConvenientWindowedResolutions(const struct FDisplayMetrics& InDisplayMetrics, TArray<FIntPoint>& OutResolutions);
	GenerateConvenientWindowedResolutions(DisplayMetrics, Resolutions);

	return true;
}

static TAutoConsoleVariable<int32> CVarMinYResolutionForUI(
	TEXT("r.MinYResolutionForUI"),
	720,
	TEXT("Defines the smallest Y resolution we want to support in the UI (default is 720)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarMinYResolutionFor3DView(
	TEXT("r.MinYResolutionFor3DView"),
	360,
	TEXT("Defines the smallest Y resolution we want to support in the 3D view"),
	ECVF_RenderThreadSafe
	);

int32 UKismetSystemLibrary::GetMinYResolutionForUI()
{
	int32 Value = FMath::Clamp(CVarMinYResolutionForUI.GetValueOnGameThread(), 200, 8 * 1024);

	return Value;
}

int32 UKismetSystemLibrary::GetMinYResolutionFor3DView()
{
	int32 Value = FMath::Clamp(CVarMinYResolutionFor3DView.GetValueOnGameThread(), 200, 8 * 1024);

	return Value;
}

void UKismetSystemLibrary::LaunchURL(const FString& URL)
{
	if (!URL.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
	}
}

bool UKismetSystemLibrary::CanLaunchURL(const FString& URL)
{
	if (!URL.IsEmpty())
	{
		return FPlatformProcess::CanLaunchURL(*URL);
	}

	return false;
}

void UKismetSystemLibrary::CollectGarbage()
{
	GEngine->ForceGarbageCollection(true);
}

void UKismetSystemLibrary::ShowAdBanner(int32 AdIdIndex, bool bShowOnBottomOfScreen)
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->ShowAdBanner(bShowOnBottomOfScreen, AdIdIndex);
	}
}

int32 UKismetSystemLibrary::GetAdIDCount()
{
	uint32 AdIDCount = 0;
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		AdIDCount = Provider->GetAdIDCount();
	}

	return AdIDCount;
}

void UKismetSystemLibrary::HideAdBanner()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->HideAdBanner();
	}
}

void UKismetSystemLibrary::ForceCloseAdBanner()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->CloseAdBanner();
	}
}

void UKismetSystemLibrary::LoadInterstitialAd(int32 AdIdIndex)
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->LoadInterstitialAd(AdIdIndex);
	}
}

bool UKismetSystemLibrary::IsInterstitialAdAvailable()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		return Provider->IsInterstitialAdAvailable();
	}
	return false;
}

bool UKismetSystemLibrary::IsInterstitialAdRequested()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		return Provider->IsInterstitialAdRequested();
	}
	return false;
}

void UKismetSystemLibrary::ShowInterstitialAd()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->ShowInterstitialAd();
	}
}

void UKismetSystemLibrary::ShowPlatformSpecificLeaderboardScreen(const FString& CategoryName)
{
	// not PIE safe, doesn't have world context
	UOnlineEngineInterface::Get()->ShowLeaderboardUI(nullptr, CategoryName);
}

void UKismetSystemLibrary::ShowPlatformSpecificAchievementsScreen(APlayerController* SpecificPlayer)
{
	UWorld* World = SpecificPlayer ? SpecificPlayer->GetWorld() : nullptr;

	// Get the controller id from the player
	int LocalUserNum = 0;
	if (SpecificPlayer)
	{
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(SpecificPlayer->Player);
		if (LocalPlayer)
		{
			LocalUserNum = LocalPlayer->GetControllerId();
		}
	}

	UOnlineEngineInterface::Get()->ShowAchievementsUI(World, LocalUserNum);
}

bool UKismetSystemLibrary::IsLoggedIn(APlayerController* SpecificPlayer)
{
	UWorld* World = SpecificPlayer ? SpecificPlayer->GetWorld() : nullptr;

	int LocalUserNum = 0;
	if (SpecificPlayer != nullptr)
	{
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(SpecificPlayer->Player);
		if(LocalPlayer)
		{
			LocalUserNum = LocalPlayer->GetControllerId();
		}
	}

	return UOnlineEngineInterface::Get()->IsLoggedIn(World, LocalUserNum);
}

void UKismetSystemLibrary::SetStructurePropertyByName(UObject* Object, FName PropertyName, const FGenericStruct& Value)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
}

void UKismetSystemLibrary::ControlScreensaver(bool bAllowScreenSaver)
{
	FPlatformApplicationMisc::ControlScreensaver(bAllowScreenSaver ? FPlatformApplicationMisc::EScreenSaverAction::Enable : FPlatformApplicationMisc::EScreenSaverAction::Disable);
}

void UKismetSystemLibrary::SetVolumeButtonsHandledBySystem(bool bEnabled)
{
	FPlatformMisc::SetVolumeButtonsHandledBySystem(bEnabled);
}

bool UKismetSystemLibrary::GetVolumeButtonsHandledBySystem()
{
	return FPlatformMisc::GetVolumeButtonsHandledBySystem();
}

void UKismetSystemLibrary::ResetGamepadAssignments()
{
	FPlatformApplicationMisc::ResetGamepadAssignments();
}

void UKismetSystemLibrary::ResetGamepadAssignmentToController(int32 ControllerId)
{
	FPlatformApplicationMisc::ResetGamepadAssignmentToController(ControllerId);
}

bool UKismetSystemLibrary::IsControllerAssignedToGamepad(int32 ControllerId)
{
	return FPlatformApplicationMisc::IsControllerAssignedToGamepad(ControllerId);
}

void UKismetSystemLibrary::SetSuppressViewportTransitionMessage(UObject* WorldContextObject, bool bState)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World && World->GetFirstLocalPlayerFromController() != nullptr && World->GetFirstLocalPlayerFromController()->ViewportClient != nullptr )
	{
		World->GetFirstLocalPlayerFromController()->ViewportClient->SetSuppressTransitionMessage(bState);
	}
}

TArray<FString> UKismetSystemLibrary::GetPreferredLanguages()
{
	return FPlatformMisc::GetPreferredLanguages();
}

FString UKismetSystemLibrary::GetDefaultLanguage()
{
	return FPlatformMisc::GetDefaultLanguage();
}

FString UKismetSystemLibrary::GetDefaultLocale()
{
	return FPlatformMisc::GetDefaultLocale();
}

FString UKismetSystemLibrary::GetLocalCurrencyCode()
{
	return FPlatformMisc::GetLocalCurrencyCode();
}

FString UKismetSystemLibrary::GetLocalCurrencySymbol()
{
	return FPlatformMisc::GetLocalCurrencySymbol();
}

struct FLoadAssetActionBase : public FPendingLatentAction
{
	// @TODO: it would be good to have static/global manager? 

public:
	FSoftObjectPath SoftObjectPath;
	FStreamableManager StreamableManager;
	TSharedPtr<FStreamableHandle> Handle;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	virtual void OnLoaded() PURE_VIRTUAL(FLoadAssetActionBase::OnLoaded, );

	FLoadAssetActionBase(const FSoftObjectPath& InSoftObjectPath, const FLatentActionInfo& InLatentInfo)
		: SoftObjectPath(InSoftObjectPath)
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{
		Handle = StreamableManager.RequestAsyncLoad(SoftObjectPath);
	}

	virtual ~FLoadAssetActionBase()
	{
		if (Handle.IsValid())
		{
			Handle->ReleaseHandle();
		}
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		const bool bLoaded = !Handle.IsValid() || Handle->HasLoadCompleted() || Handle->WasCanceled();
		if (bLoaded)
		{
			OnLoaded();
		}
		Response.FinishAndTriggerIf(bLoaded, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return FString::Printf(TEXT("Load Asset Action Base: %s"), *SoftObjectPath.ToString());
	}
#endif
};

void UKismetSystemLibrary::LoadAsset(UObject* WorldContextObject, TSoftObjectPtr<UObject> Asset, UKismetSystemLibrary::FOnAssetLoaded OnLoaded, FLatentActionInfo LatentInfo)
{
	struct FLoadAssetAction : public FLoadAssetActionBase
	{
	public:
		UKismetSystemLibrary::FOnAssetLoaded OnLoadedCallback;

		FLoadAssetAction(const FSoftObjectPath& InSoftObjectPath, UKismetSystemLibrary::FOnAssetLoaded InOnLoadedCallback, const FLatentActionInfo& InLatentInfo)
			: FLoadAssetActionBase(InSoftObjectPath, InLatentInfo)
			, OnLoadedCallback(InOnLoadedCallback)
		{}

		virtual void OnLoaded() override
		{
			UObject* LoadedObject = SoftObjectPath.ResolveObject();
			OnLoadedCallback.ExecuteIfBound(LoadedObject);
		}
	};

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FLoadAssetAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FLoadAssetAction* NewAction = new FLoadAssetAction(Asset.ToSoftObjectPath(), OnLoaded, LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UKismetSystemLibrary::LoadAssetClass(UObject* WorldContextObject, TSoftClassPtr<UObject> AssetClass, UKismetSystemLibrary::FOnAssetClassLoaded OnLoaded, FLatentActionInfo LatentInfo)
{
	struct FLoadAssetClassAction : public FLoadAssetActionBase
	{
	public:
		UKismetSystemLibrary::FOnAssetClassLoaded OnLoadedCallback;

		FLoadAssetClassAction(const FSoftObjectPath& InSoftObjectPath, UKismetSystemLibrary::FOnAssetClassLoaded InOnLoadedCallback, const FLatentActionInfo& InLatentInfo)
			: FLoadAssetActionBase(InSoftObjectPath, InLatentInfo)
			, OnLoadedCallback(InOnLoadedCallback)
		{}

		virtual void OnLoaded() override
		{
			UClass* LoadedObject = Cast<UClass>(SoftObjectPath.ResolveObject());
			OnLoadedCallback.ExecuteIfBound(LoadedObject);
		}
	};

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FLoadAssetClassAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FLoadAssetClassAction* NewAction = new FLoadAssetClassAction(AssetClass.ToSoftObjectPath(), OnLoaded, LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UKismetSystemLibrary::RegisterForRemoteNotifications()
{
	FPlatformMisc::RegisterForRemoteNotifications();
}

void UKismetSystemLibrary::UnregisterForRemoteNotifications()
{
	FPlatformMisc::UnregisterForRemoteNotifications();
}

void UKismetSystemLibrary::SetUserActivity(const FUserActivity& UserActivity)
{
	FUserActivityTracking::SetActivity(UserActivity);
}

FString UKismetSystemLibrary::GetCommandLine()
{
	return FString(FCommandLine::Get());
}

UObject* UKismetSystemLibrary::GetObjectFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		FPrimaryAssetTypeInfo Info;
		if (Manager->GetPrimaryAssetTypeInfo(PrimaryAssetId.PrimaryAssetType, Info) && !Info.bHasBlueprintClasses)
		{
			return Manager->GetPrimaryAssetObject(PrimaryAssetId);
		}
	}
	return nullptr;
}

TSubclassOf<UObject> UKismetSystemLibrary::GetClassFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		FPrimaryAssetTypeInfo Info;
		if (Manager->GetPrimaryAssetTypeInfo(PrimaryAssetId.PrimaryAssetType, Info) && Info.bHasBlueprintClasses)
		{
			return Manager->GetPrimaryAssetObjectClass<UObject>(PrimaryAssetId);
		}
	}
	return nullptr;
}

TSoftObjectPtr<UObject> UKismetSystemLibrary::GetSoftObjectReferenceFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		FPrimaryAssetTypeInfo Info;
		if (Manager->GetPrimaryAssetTypeInfo(PrimaryAssetId.PrimaryAssetType, Info) && !Info.bHasBlueprintClasses)
		{
			return TSoftObjectPtr<UObject>(Manager->GetPrimaryAssetPath(PrimaryAssetId));
		}
	}
	return nullptr;
}

TSoftClassPtr<UObject> UKismetSystemLibrary::GetSoftClassReferenceFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		FPrimaryAssetTypeInfo Info;
		if (Manager->GetPrimaryAssetTypeInfo(PrimaryAssetId.PrimaryAssetType, Info) && Info.bHasBlueprintClasses)
		{
			return TSoftClassPtr<UObject>(Manager->GetPrimaryAssetPath(PrimaryAssetId));
		}
	}
	return nullptr;
}

FPrimaryAssetId UKismetSystemLibrary::GetPrimaryAssetIdFromObject(UObject* Object)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		if (Object)
		{
			return Manager->GetPrimaryAssetIdForObject(Object);
		}
	}
	return FPrimaryAssetId();
}

FPrimaryAssetId UKismetSystemLibrary::GetPrimaryAssetIdFromClass(TSubclassOf<UObject> Class)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		if (Class)
		{
			return Manager->GetPrimaryAssetIdForObject(*Class);
		}
	}
	return FPrimaryAssetId();
}

FPrimaryAssetId UKismetSystemLibrary::GetPrimaryAssetIdFromSoftObjectReference(TSoftObjectPtr<UObject> SoftObjectReference)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		return Manager->GetPrimaryAssetIdForPath(SoftObjectReference.ToSoftObjectPath());
	}
	return FPrimaryAssetId();
}

FPrimaryAssetId UKismetSystemLibrary::GetPrimaryAssetIdFromSoftClassReference(TSoftClassPtr<UObject> SoftClassReference)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		return Manager->GetPrimaryAssetIdForPath(SoftClassReference.ToSoftObjectPath());
	}
	return FPrimaryAssetId();
}

void UKismetSystemLibrary::GetPrimaryAssetIdList(FPrimaryAssetType PrimaryAssetType, TArray<FPrimaryAssetId>& OutPrimaryAssetIdList)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		Manager->GetPrimaryAssetIdList(PrimaryAssetType, OutPrimaryAssetIdList);
	}
}

bool UKismetSystemLibrary::IsValidPrimaryAssetId(FPrimaryAssetId PrimaryAssetId)
{
	return PrimaryAssetId.IsValid();
}

FString UKismetSystemLibrary::Conv_PrimaryAssetIdToString(FPrimaryAssetId PrimaryAssetId)
{
	return PrimaryAssetId.ToString();
}

bool UKismetSystemLibrary::EqualEqual_PrimaryAssetId(FPrimaryAssetId A, FPrimaryAssetId B)
{
	return A == B;
}

bool UKismetSystemLibrary::NotEqual_PrimaryAssetId(FPrimaryAssetId A, FPrimaryAssetId B)
{
	return A != B;
}

bool UKismetSystemLibrary::IsValidPrimaryAssetType(FPrimaryAssetType PrimaryAssetType)
{
	return PrimaryAssetType.IsValid();
}

FString UKismetSystemLibrary::Conv_PrimaryAssetTypeToString(FPrimaryAssetType PrimaryAssetType)
{
	return PrimaryAssetType.ToString();
}

bool UKismetSystemLibrary::EqualEqual_PrimaryAssetType(FPrimaryAssetType A, FPrimaryAssetType B)
{
	return A == B;
}

bool UKismetSystemLibrary::NotEqual_PrimaryAssetType(FPrimaryAssetType A, FPrimaryAssetType B)
{
	return A != B;
}

void UKismetSystemLibrary::UnloadPrimaryAsset(FPrimaryAssetId PrimaryAssetId)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		Manager->UnloadPrimaryAsset(PrimaryAssetId);
	}
}

void UKismetSystemLibrary::UnloadPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetIdList)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		Manager->UnloadPrimaryAssets(PrimaryAssetIdList);
	}
}

bool UKismetSystemLibrary::GetCurrentBundleState(FPrimaryAssetId PrimaryAssetId, bool bForceCurrentState, TArray<FName>& OutBundles)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		if (Manager->GetPrimaryAssetHandle(PrimaryAssetId, bForceCurrentState, &OutBundles).IsValid())
		{
			return true;
		}
	}
	return false;
}

void UKismetSystemLibrary::GetPrimaryAssetsWithBundleState(const TArray<FName>& RequiredBundles, const TArray<FName>& ExcludedBundles, const TArray<FPrimaryAssetType>& ValidTypes, bool bForceCurrentState, TArray<FPrimaryAssetId>& OutPrimaryAssetIdList)
{
	if (UAssetManager* Manager = UAssetManager::GetIfValid())
	{
		Manager->GetPrimaryAssetsWithBundleState(OutPrimaryAssetIdList, ValidTypes, RequiredBundles, ExcludedBundles, bForceCurrentState);
	}
}