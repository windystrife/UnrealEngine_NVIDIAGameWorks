// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "InAppPurchaseQueryCallbackProxy.h"
#include "Async/TaskGraphInterfaces.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSubsystem.h"
#include "Engine/World.h"

//////////////////////////////////////////////////////////////////////////
// UInAppPurchaseQueryCallbackProxy

UInAppPurchaseQueryCallbackProxy::UInAppPurchaseQueryCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInAppPurchaseQueryCallbackProxy::TriggerQuery(APlayerController* PlayerController, const TArray<FString>& ProductIdentifiers)
{
	bFailedToEvenSubmit = true;

	WorldPtr = (PlayerController != nullptr) ? PlayerController->GetWorld() : nullptr;
	if (APlayerState* PlayerState = (PlayerController != NULL) ? PlayerController->PlayerState : nullptr)
	{
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::IsLoaded() ? IOnlineSubsystem::Get() : nullptr)
		{
			IOnlineStorePtr StoreInterface = OnlineSub->GetStoreInterface();
			if (StoreInterface.IsValid())
			{
				bFailedToEvenSubmit = false;

				// Register the completion callback
				InAppPurchaseReadCompleteDelegate       = FOnQueryForAvailablePurchasesCompleteDelegate::CreateUObject(this, &UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead);
				InAppPurchaseReadCompleteDelegateHandle = StoreInterface->AddOnQueryForAvailablePurchasesCompleteDelegate_Handle(InAppPurchaseReadCompleteDelegate);


				ReadObject = MakeShareable(new FOnlineProductInformationRead());
				FOnlineProductInformationReadRef ReadObjectRef = ReadObject.ToSharedRef();
				StoreInterface->QueryForAvailablePurchases(ProductIdentifiers, ReadObjectRef);
			}
			else
			{
				FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseQueryCallbackProxy::TriggerQuery - In App Purchases are not supported by Online Subsystem"), ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseQueryCallbackProxy::TriggerQuery - Invalid or uninitialized OnlineSubsystem"), ELogVerbosity::Warning);
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseQueryCallbackProxy::TriggerQuery - Invalid player state"), ELogVerbosity::Warning);
	}

	if (bFailedToEvenSubmit && (PlayerController != NULL))
	{
		OnInAppPurchaseRead(false);
	}
}

void UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead(bool bWasSuccessful)
{
	RemoveDelegate();

	if (bWasSuccessful && ReadObject.IsValid())
	{
		SavedProductInformation = ReadObject->ProvidedProductInformation;
		bSavedWasSuccessful = true;
	}
	else
	{
		bSavedWasSuccessful = false;
	}

	if (UWorld* World = WorldPtr.Get())
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DelayInAppPurchaseRead"), STAT_FSimpleDelegateGraphTask_DelayInAppPurchaseRead, STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){

				OnInAppPurchaseRead_Delayed();

			}),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DelayInAppPurchaseRead), 
			nullptr, 
			ENamedThreads::GameThread
		);
	}

	ReadObject = NULL;
}

void UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead_Delayed()
{
	if (bSavedWasSuccessful)
	{
		OnSuccess.Broadcast(SavedProductInformation);
	}
	else
	{
		OnFailure.Broadcast(SavedProductInformation);
	}
}

void UInAppPurchaseQueryCallbackProxy::RemoveDelegate()
{
	if (!bFailedToEvenSubmit)
	{
		if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::IsLoaded() ? IOnlineSubsystem::Get() : nullptr)
		{
			IOnlineStorePtr InAppPurchases = OnlineSub->GetStoreInterface();
			if (InAppPurchases.IsValid())
			{
				InAppPurchases->ClearOnQueryForAvailablePurchasesCompleteDelegate_Handle(InAppPurchaseReadCompleteDelegateHandle);
			}
		}
	}
}

void UInAppPurchaseQueryCallbackProxy::BeginDestroy()
{
	ReadObject = NULL;
	RemoveDelegate();

	Super::BeginDestroy();
}

UInAppPurchaseQueryCallbackProxy* UInAppPurchaseQueryCallbackProxy::CreateProxyObjectForInAppPurchaseQuery(class APlayerController* PlayerController, const TArray<FString>& ProductIdentifiers)
{
	UInAppPurchaseQueryCallbackProxy* Proxy = NewObject<UInAppPurchaseQueryCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->TriggerQuery(PlayerController, ProductIdentifiers);
	return Proxy;
}
