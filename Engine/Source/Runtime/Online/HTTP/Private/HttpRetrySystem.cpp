// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HttpRetrySystem.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "HttpModule.h"
#include "Http.h"


FHttpRetrySystem::FRequest::FRequest(
	FManager& InManager,
	const TSharedRef<IHttpRequest>& HttpRequest, 
	const FHttpRetrySystem::FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FHttpRetrySystem::FRetryResponseCodes& InRetryResponseCodes
	)
    : FHttpRequestAdapterBase(HttpRequest)
    , Status(FHttpRetrySystem::FRequest::EStatus::NotStarted)
    , RetryLimitCountOverride(InRetryLimitCountOverride)
    , RetryTimeoutRelativeSecondsOverride(InRetryTimeoutRelativeSecondsOverride)
	, RetryResponseCodes(InRetryResponseCodes)
	, RetryManager(InManager)
{
    // if the InRetryTimeoutRelativeSecondsOverride override is being used the value cannot be negative
    check(!(InRetryTimeoutRelativeSecondsOverride.bUseValue) || (InRetryTimeoutRelativeSecondsOverride.Value >= 0.0));
}

bool FHttpRetrySystem::FRequest::ProcessRequest()
{ 
	TSharedRef<FRequest> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	HttpRequest->OnRequestProgress().BindSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnRequestProgress);

	return RetryManager.ProcessRequest(RetryRequest);
}

void FHttpRetrySystem::FRequest::CancelRequest() 
{ 
	TSharedRef<FRequest> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	RetryManager.CancelRequest(RetryRequest);
}

void FHttpRetrySystem::FRequest::HttpOnRequestProgress(FHttpRequestPtr InHttpRequest, int32 BytesSent, int32 BytesRcv)
{
	OnRequestProgress().ExecuteIfBound(AsShared(), BytesSent, BytesRcv);
}

FHttpRetrySystem::FManager::FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault)
    : RandomFailureRate(FRandomFailureRateSetting::Unused())
    , RetryLimitCountDefault(InRetryLimitCountDefault)
	, RetryTimeoutRelativeSecondsDefault(InRetryTimeoutRelativeSecondsDefault)
{}

TSharedRef<FHttpRetrySystem::FRequest> FHttpRetrySystem::FManager::CreateRequest(
	const FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FRetryResponseCodes& InRetryResponseCodes)
{
	return MakeShareable(new FRequest(
		*this,
		FHttpModule::Get().CreateRequest(),
		InRetryLimitCountOverride,
		InRetryTimeoutRelativeSecondsOverride,
		InRetryResponseCodes
		));
}

bool FHttpRetrySystem::FManager::ShouldRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

	FHttpResponsePtr Response = HttpRetryRequestEntry.Request->GetResponse();
	// invalid response means connection or network error but we need to know which one
	if (!Response.IsValid())
	{
		// ONLY retry bad responses if they are connection errors (NOT protocol errors or unknown) otherwise request may be sent (and processed!) twice
		EHttpRequestStatus::Type Status = HttpRetryRequestEntry.Request->GetStatus();
		if (Status == EHttpRequestStatus::Failed_ConnectionError)
		{
			bResult = true;
		}
		else if (Status == EHttpRequestStatus::Failed)
		{
			// we will also allow retry for GET and HEAD requests even if they may duplicate on the server
			FString Verb = HttpRetryRequestEntry.Request->GetVerb();
			if (Verb == TEXT("GET") || Verb == TEXT("HEAD"))
			{
				bResult = true;
			}
		}
	}
	else
	{
		// this may be a successful response with one of the explicitly listed response codes we want to retry on
		if (HttpRetryRequestEntry.Request->RetryResponseCodes.Contains(Response->GetResponseCode()))
		{
			bResult = true;
		}
	}

    return bResult;
}

bool FHttpRetrySystem::FManager::CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

    bool bShouldTestCurrentRetryCount = false;
    double RetryLimitCount = 0;
    if (HttpRetryRequestEntry.Request->RetryLimitCountOverride.bUseValue)
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = HttpRetryRequestEntry.Request->RetryLimitCountOverride.Value;
    }
    else if (RetryLimitCountDefault.bUseValue)
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = RetryLimitCountDefault.Value;
    }

    if (bShouldTestCurrentRetryCount)
    {
        if (HttpRetryRequestEntry.CurrentRetryCount < RetryLimitCount)
        {
            bResult = true;
        }
    }

    return bResult;
}

bool FHttpRetrySystem::FManager::HasTimedOut(const FHttpRetryRequestEntry& HttpRetryRequestEntry, const double NowAbsoluteSeconds)
{
    bool bResult = false;

    bool bShouldTestRetryTimeout = false;
    double RetryTimeoutAbsoluteSeconds = HttpRetryRequestEntry.RequestStartTimeAbsoluteSeconds;
    if (HttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.bUseValue)
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += HttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.Value;
    }
    else if (RetryTimeoutRelativeSecondsDefault.bUseValue)
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += RetryTimeoutRelativeSecondsDefault.Value;
    }

    if (bShouldTestRetryTimeout)
    {
        if (NowAbsoluteSeconds >= RetryTimeoutAbsoluteSeconds)
        {
            bResult = true;
        }
    }

    return bResult;
}

float FHttpRetrySystem::FManager::GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    float lockoutTime = 0.0f;

    if(HttpRetryRequestEntry.CurrentRetryCount >= 1)
    {
        lockoutTime = 5.0f + 5.0f * ((HttpRetryRequestEntry.CurrentRetryCount - 1) >> 1);
        lockoutTime = lockoutTime > 30.0f ? 30.0f : lockoutTime;
    }

    return lockoutTime;
}

static FRandomStream temp(4435261);

bool FHttpRetrySystem::FManager::Update(uint32* FileCount, uint32* FailingCount, uint32* FailedCount, uint32* CompletedCount)
{
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update);
	bool bIsGreen = true;

	if (FileCount != nullptr)
	{
		*FileCount = RequestList.Num();
	}

	const double NowAbsoluteSeconds = FPlatformTime::Seconds();

	// Basic algorithm
	// for each managed item
	//    if the item hasn't timed out
	//       if the item's retry state is NotStarted
	//          if the item's request's state is not NotStarted
	//             move the item's retry state to Processing
	//          endif
	//       endif
	//       if the item's retry state is Processing
	//          if the item's request's state is Failed
	//             flag return code to false
	//             if the item can be retried
	//                increment FailingCount if applicable
	//                retry the item's request
	//                increment the item's retry count
	//             else
	//                increment FailedCount if applicable
	//                set the item's retry state to FailedRetry
	//             endif
	//          else if the item's request's state is Succeeded
	//          endif
	//       endif
	//    else
	//       flag return code to false
	//       set the item's retry state to FailedTimeout
	//       increment FailedCount if applicable
	//    endif
	//    if the item's retry state is FailedRetry
	//       do stuff
	//    endif
	//    if the item's retry state is FailedTimeout
	//       do stuff
	//    endif
	//    if the item's retry state is Succeeded
	//       do stuff
	//    endif
	// endfor

	int32 index = 0;
	while (index < RequestList.Num())
	{
		FHttpRetryRequestEntry& HttpRetryRequestEntry = RequestList[index];
		TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest = HttpRetryRequestEntry.Request;

		EHttpRequestStatus::Type RequestStatus = HttpRetryRequest->GetStatus();

		if (HttpRetryRequestEntry.bShouldCancel)
		{
			UE_LOG(LogHttp, Warning, TEXT("Request cancelled on %s"), *(HttpRetryRequest->GetURL()));
			HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Cancelled;
		}
		else
		{
			if (!HasTimedOut(HttpRetryRequestEntry, NowAbsoluteSeconds))
			{
				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::NotStarted)
				{
					if (RequestStatus != EHttpRequestStatus::NotStarted)
					{
						HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Processing;
					}
				}

				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Processing)
				{
					bool forceFail = false;

					// Code to simulate request failure
					if (RequestStatus == EHttpRequestStatus::Succeeded && RandomFailureRate.bUseValue)
					{
						float random = temp.GetFraction();
						if (random < RandomFailureRate.Value)
						{
							forceFail = true;
						}
					}

					// Save these for failure case retry checks if we hit a completion state
					bool bShouldRetry = false;
					bool bCanRetry = false;
					if (RequestStatus == EHttpRequestStatus::Failed || RequestStatus == EHttpRequestStatus::Failed_ConnectionError || RequestStatus == EHttpRequestStatus::Succeeded)
					{
						bShouldRetry = ShouldRetry(HttpRetryRequestEntry);
						bCanRetry = CanRetry(HttpRetryRequestEntry);
					}

					if (RequestStatus == EHttpRequestStatus::Failed || RequestStatus == EHttpRequestStatus::Failed_ConnectionError || forceFail || (bShouldRetry && bCanRetry))
					{
						bIsGreen = false;

						if (forceFail || (bShouldRetry && bCanRetry))
						{
							float lockoutPeriod = GetLockoutPeriodSeconds(HttpRetryRequestEntry);

							if (lockoutPeriod > 0.0f)
							{
								UE_LOG(LogHttp, Warning, TEXT("Lockout of %fs on %s"), lockoutPeriod, *(HttpRetryRequest->GetURL()));
							}

							HttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds = NowAbsoluteSeconds + lockoutPeriod;
							HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::ProcessingLockout;
						}
						else
						{
							UE_LOG(LogHttp, Warning, TEXT("Retry exhausted on %s"), *(HttpRetryRequest->GetURL()));
							if (FailedCount != nullptr)
							{
								++(*FailedCount);
							}
							HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedRetry;
						}
					}
					else if (RequestStatus == EHttpRequestStatus::Succeeded)
					{
						if (HttpRetryRequestEntry.CurrentRetryCount > 0)
						{
							UE_LOG(LogHttp, Warning, TEXT("Success on %s"), *(HttpRetryRequest->GetURL()));
						}

						if (CompletedCount != nullptr)
						{
							++(*CompletedCount);
						}

						HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Succeeded;
					}
				}

				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::ProcessingLockout)
				{
					if (NowAbsoluteSeconds >= HttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds)
					{
						// if this fails the HttpRequest's state will be failed which will cause the retry logic to kick(as expected)
						bool success = HttpRetryRequest->HttpRequest->ProcessRequest();
						if (success)
						{
							UE_LOG(LogHttp, Warning, TEXT("Retry %d on %s"), HttpRetryRequestEntry.CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));

							++HttpRetryRequestEntry.CurrentRetryCount;
							HttpRetryRequest->Status = FRequest::EStatus::Processing;
						}
					}

					if (FailingCount != nullptr)
					{
						++(*FailingCount);
					}
				}
			}
			else
			{
				UE_LOG(LogHttp, Warning, TEXT("Timeout on retry %d: %s"), HttpRetryRequestEntry.CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));
				bIsGreen = false;
				HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedTimeout;
				if (FailedCount != nullptr)
				{
					++(*FailedCount);
				}
			}
		}

		bool bWasCompleted = false;
		bool bWasSuccessful = false;

        if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Cancelled ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedRetry ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedTimeout ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded)
		{
			bWasCompleted = true;
            bWasSuccessful = HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded;
		}

		if (bWasCompleted)
		{
			HttpRetryRequest->OnProcessRequestComplete().ExecuteIfBound(HttpRetryRequest, HttpRetryRequest->GetResponse(), bWasSuccessful);
		}

        if(bWasSuccessful)
        {
            if(CompletedCount != nullptr)
            {
                ++(*CompletedCount);
            }
        }

		if (bWasCompleted)
		{
			RequestList.RemoveAtSwap(index);
		}
		else
		{
			++index;
		}
	}

	return bIsGreen;
}

FHttpRetrySystem::FManager::FHttpRetryRequestEntry::FHttpRetryRequestEntry(TSharedRef<FHttpRetrySystem::FRequest>& InRequest)
    : bShouldCancel(false)
    , CurrentRetryCount(0)
	, RequestStartTimeAbsoluteSeconds(FPlatformTime::Seconds())
	, Request(InRequest)
{}

bool FHttpRetrySystem::FManager::ProcessRequest(TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest)
{
	bool bResult = HttpRetryRequest->HttpRequest->ProcessRequest();

	if (bResult)
	{
		RequestList.Add(FHttpRetryRequestEntry(HttpRetryRequest));
	}

	return bResult;
}

void FHttpRetrySystem::FManager::CancelRequest(TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest)
{
	// Find the existing request entry if is was previously processed.
	bool bFound = false;
	for (int32 i = 0; i < RequestList.Num(); ++i)
	{
		FHttpRetryRequestEntry& EntryRef = RequestList[i];

		if (EntryRef.Request == HttpRetryRequest)
		{
			EntryRef.bShouldCancel = true;
			bFound = true;
		}
	}
	// If we did not find the entry, likely auth failed for the request, in which case ProcessRequest does not get called.
	// Adding it to the list and flagging as cancel will process it on next tick.
	if (!bFound)
	{
		FHttpRetryRequestEntry RetryRequestEntry(HttpRetryRequest);
		RetryRequestEntry.bShouldCancel = true;
		RequestList.Add(RetryRequestEntry);
	}
	HttpRetryRequest->HttpRequest->CancelRequest();
}
