// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HttpThread.h"
#include "IHttpThreadedRequest.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "HttpModule.h"
#include "Http.h"

// FHttpThread

FHttpThread::FHttpThread()
	:	Thread(nullptr)
{
	HttpThreadActiveFrameTimeInSeconds = FHttpModule::Get().GetHttpThreadActiveFrameTimeInSeconds();
	HttpThreadActiveMinimumSleepTimeInSeconds = FHttpModule::Get().GetHttpThreadActiveMinimumSleepTimeInSeconds();
	HttpThreadIdleFrameTimeInSeconds = FHttpModule::Get().GetHttpThreadIdleFrameTimeInSeconds();
	HttpThreadIdleMinimumSleepTimeInSeconds = FHttpModule::Get().GetHttpThreadIdleMinimumSleepTimeInSeconds();

	UE_LOG(LogHttp, Log, TEXT("HTTP thread active frame time %.1f ms. Minimum active sleep time is %.1f ms. HTTP thread idle frame time %.1f ms. Minimum idle sleep time is %.1f ms."), HttpThreadActiveFrameTimeInSeconds * 1000.0, HttpThreadActiveMinimumSleepTimeInSeconds * 1000.0, HttpThreadIdleFrameTimeInSeconds * 1000.0, HttpThreadIdleMinimumSleepTimeInSeconds * 1000.0);
}

FHttpThread::~FHttpThread()
{
	StopThread();
}

void FHttpThread::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("HttpManagerThread"), 128 * 1024, TPri_Normal);
}

void FHttpThread::StopThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
}

void FHttpThread::AddRequest(IHttpThreadedRequest* Request)
{
	FScopeLock ScopeLock(&RequestArraysLock);
	PendingThreadedRequests.Add(Request);
}

void FHttpThread::CancelRequest(IHttpThreadedRequest* Request)
{
	FScopeLock ScopeLock(&RequestArraysLock);
	CancelledThreadedRequests.Add(Request);
}

void FHttpThread::GetCompletedRequests(TArray<IHttpThreadedRequest*>& OutCompletedRequests)
{
	FScopeLock ScopeLock(&RequestArraysLock);
	OutCompletedRequests = CompletedThreadedRequests;
	CompletedThreadedRequests.Reset();
}

bool FHttpThread::Init()
{
	LastTime = FPlatformTime::Seconds();
	return true;
}

uint32 FHttpThread::Run()
{
	TArray<IHttpThreadedRequest*> RequestsToCancel;
	TArray<IHttpThreadedRequest*> RequestsToStart;
	TArray<IHttpThreadedRequest*> RequestsToComplete;
	while (!ExitRequest.GetValue())
	{
		double OuterLoopBegin = FPlatformTime::Seconds();
		double OuterLoopEnd = 0.0;
		bool bKeepProcessing = true;
		while (bKeepProcessing)
		{
			double InnerLoopBegin = FPlatformTime::Seconds();
			
			Process(RequestsToCancel, RequestsToStart, RequestsToComplete);
			
			if (RunningThreadedRequests.Num() == 0)
			{
				bKeepProcessing = false;
			}

			double InnerLoopEnd = FPlatformTime::Seconds();
			if (bKeepProcessing)
			{
				double InnerLoopTime = InnerLoopEnd - InnerLoopBegin;
				double InnerSleep = FMath::Max(HttpThreadActiveFrameTimeInSeconds - InnerLoopTime, HttpThreadActiveMinimumSleepTimeInSeconds);
				FPlatformProcess::SleepNoStats(InnerSleep);
			}
			else
			{
				OuterLoopEnd = InnerLoopEnd;
			}
		}
		double OuterLoopTime = OuterLoopEnd - OuterLoopBegin;
		double OuterSleep = FMath::Max(HttpThreadIdleFrameTimeInSeconds - OuterLoopTime, HttpThreadIdleMinimumSleepTimeInSeconds);
		FPlatformProcess::SleepNoStats(OuterSleep);
	}
	return 0;
}

void FHttpThread::Tick()
{
	TArray<IHttpThreadedRequest*> RequestsToCancel;
	TArray<IHttpThreadedRequest*> RequestsToStart;
	TArray<IHttpThreadedRequest*> RequestsToComplete;
	Process(RequestsToCancel, RequestsToStart, RequestsToComplete);
}

void FHttpThread::HttpThreadTick(float DeltaSeconds)
{
	// empty
}

bool FHttpThread::StartThreadedRequest(IHttpThreadedRequest* Request)
{
	return Request->StartThreadedRequest();
}

void FHttpThread::CompleteThreadedRequest(IHttpThreadedRequest* Request)
{
	// empty
}

void FHttpThread::Process(TArray<IHttpThreadedRequest*>& RequestsToCancel, TArray<IHttpThreadedRequest*>& RequestsToStart, TArray<IHttpThreadedRequest*>& RequestsToComplete)
{
	// cache all cancelled and pending requests
	{
		FScopeLock ScopeLock(&RequestArraysLock);

		RequestsToCancel = CancelledThreadedRequests;
		CancelledThreadedRequests.Reset();

		RequestsToStart = PendingThreadedRequests;
		PendingThreadedRequests.Reset();
	}

	// Cancel any pending cancel requests
	for (IHttpThreadedRequest* Request : RequestsToCancel)
	{
		if (RunningThreadedRequests.Remove(Request) > 0)
		{
			RequestsToComplete.Add(Request);
		}
	}

	// Start any pending requests
	for (IHttpThreadedRequest* Request : RequestsToStart)
	{
		if (StartThreadedRequest(Request))
		{
			RunningThreadedRequests.Add(Request);
		}
		else
		{
			RequestsToComplete.Add(Request);
		}
	}

	const double AppTime = FPlatformTime::Seconds();
	const double ElapsedTime = AppTime - LastTime;
	LastTime = AppTime;

	// Tick any running requests
	for (int32 Index = 0; Index < RunningThreadedRequests.Num(); ++Index)
	{
		IHttpThreadedRequest* Request = RunningThreadedRequests[Index];
		Request->TickThreadedRequest(ElapsedTime);
	}

	HttpThreadTick(ElapsedTime);

	// Move any completed requests
	for (int32 Index = 0; Index < RunningThreadedRequests.Num(); ++Index)
	{
		IHttpThreadedRequest* Request = RunningThreadedRequests[Index];
		if (Request->IsThreadedRequestComplete())
		{
			RequestsToComplete.Add(Request);
			RunningThreadedRequests.RemoveAtSwap(Index);
			--Index;
		}
	}

	if (RequestsToComplete.Num() > 0)
	{
		for (IHttpThreadedRequest* Request : RequestsToComplete)
		{
			CompleteThreadedRequest(Request);
		}

		{
			FScopeLock ScopeLock(&RequestArraysLock);
			CompletedThreadedRequests.Append(RequestsToComplete);
		}
		RequestsToComplete.Reset();
	}
}

void FHttpThread::Stop()
{
	ExitRequest.Set(true);
}
	
void FHttpThread::Exit()
{
	// empty
}
