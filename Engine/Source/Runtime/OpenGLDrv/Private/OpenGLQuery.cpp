// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLQuery.cpp: OpenGL query RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

void FOpenGLDynamicRHI::RHIBeginOcclusionQueryBatch()
{
}

void FOpenGLDynamicRHI::RHIEndOcclusionQueryBatch()
{
}

FRenderQueryRHIRef FOpenGLDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	VERIFY_GL_SCOPE();

	check(QueryType == RQT_Occlusion || QueryType == RQT_AbsoluteTime);

	if(QueryType == RQT_AbsoluteTime && FOpenGL::SupportsTimestampQueries() == false)
	{
		return NULL;
	}

	return new FOpenGLRenderQuery(QueryType);
}

void FOpenGLDynamicRHI::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	VERIFY_GL_SCOPE();

	FOpenGLRenderQuery* Query = ResourceCast(QueryRHI);
	Query->bResultIsCached = false;
	if(Query->QueryType == RQT_Occlusion)
	{
		check(PendingState.RunningOcclusionQuery == 0);

		if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
		{
			PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
			Query->bInvalidResource = true;
		}

		if (Query->bInvalidResource)
		{
			PlatformGetNewRenderQuery(&Query->Resource, &Query->ResourceContext);
			Query->bInvalidResource = false;
		}

		GLenum QueryType = FOpenGL::SupportsExactOcclusionQueries() ? UGL_SAMPLES_PASSED : UGL_ANY_SAMPLES_PASSED;
		FOpenGL::BeginQuery(QueryType, Query->Resource);
		PendingState.RunningOcclusionQuery = Query->Resource;
	}
	else
	{
		// not supported/needed for RQT_AbsoluteTime
		check(0);
	}
}

void FOpenGLDynamicRHI::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	VERIFY_GL_SCOPE();

	FOpenGLRenderQuery* Query = ResourceCast(QueryRHI);

	if (Query)
	{
		if(Query->QueryType == RQT_Occlusion)
		{
			if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
			{
				PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
				Query->Resource = 0;
				Query->bInvalidResource = true;
			}

			if (!Query->bInvalidResource)
			{
				check(PendingState.RunningOcclusionQuery == Query->Resource);
				PendingState.RunningOcclusionQuery = 0;
				GLenum QueryType = FOpenGL::SupportsExactOcclusionQueries() ? UGL_SAMPLES_PASSED : UGL_ANY_SAMPLES_PASSED;
				FOpenGL::EndQuery(QueryType);
			}
		}
		else if(Query->QueryType == RQT_AbsoluteTime)
		{
			// query can be silently invalidated in GetRenderQueryResult
			if (Query->bInvalidResource)
			{
				PlatformGetNewRenderQuery(&Query->Resource, &Query->ResourceContext);
				Query->bInvalidResource = false;
			}

			FOpenGL::QueryTimestampCounter(Query->Resource);
			Query->bResultIsCached = false;
		}
	}
}

bool FOpenGLDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutResult,bool bWait)
{
	check(IsInRenderingThread());
	VERIFY_GL_SCOPE();

	FOpenGLRenderQuery* Query = ResourceCast(QueryRHI);

	if (!Query)
	{
		// If timer queries are unsupported, just make sure that OutResult does not contain any random values.
		OutResult = 0;
		return false;
	}

	bool bSuccess = true;

	if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
	{
		PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
		Query->Resource = 0;
		Query->bInvalidResource = true;
	}

	if(!Query->bResultIsCached)
	{
		// Check if the query is valid first
		if (Query->bInvalidResource)
		{
			bSuccess = false;
		}
		else
		{
			// Check if the query is finished
			GLuint Result = 0;
			{
				VERIFY_GL_SCOPE();
				FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_ResultAvailable, &Result);
			}

			// Isn't the query finished yet, and can we wait for it?
			if (Result == GL_FALSE && bWait)
			{
				SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
				uint32 IdleStart = FPlatformTime::Cycles();
				double StartTime = FPlatformTime::Seconds();
				do
				{
					VERIFY_GL_SCOPE();
					FPlatformProcess::Sleep(0);	// yield to other threads - some of them may be OpenGL driver's and we'd be starving them

					if (Query->bInvalidResource)
					{
						// Query got invalidated while we were sleeping.
						// Bail out, no sense to wait and generate OpenGL errors,
						// we're in a new OpenGL context that knows nothing about us.
						Query->bResultIsCached = true;
						Query->Result = 1000;	// safe value
						Result = GL_FALSE;
						bWait = false;
						bSuccess = true;
						break;
					}

					FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_ResultAvailable, &Result);

					// timer queries are used for Benchmarks which can stall a bit more
					double TimeoutValue = (Query->QueryType == RQT_AbsoluteTime) ? 2.0 : 0.5;

					if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (%.1f s)"), TimeoutValue);
						break;
					}
				} while (Result == GL_FALSE);
				GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
				GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
			}

			if (Result == GL_TRUE)
			{
				VERIFY_GL_SCOPE();
				FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_Result, &Query->Result);
			}
			else if (Result == GL_FALSE && bWait)
			{
				bSuccess = false;
			}
		}
	}

	if(Query->QueryType == RQT_AbsoluteTime)
	{
		// GetTimingFrequency is the number of ticks per second
		uint64 Div = FMath::Max(1llu, FOpenGLBufferedGPUTiming::GetTimingFrequency() / (1000 * 1000));

		// convert from GPU specific timestamp to micro sec (1 / 1 000 000 s) which seems a reasonable resolution
		OutResult = Query->Result / Div;
	}
	else
	{
		OutResult = Query->Result;
	}


	Query->bResultIsCached = bSuccess;

	return bSuccess;
}



extern void OnQueryCreation( FOpenGLRenderQuery* Query );
extern void OnQueryDeletion( FOpenGLRenderQuery* Query );

FOpenGLRenderQuery::FOpenGLRenderQuery(ERenderQueryType InQueryType)
	: bResultIsCached(false)
	, bInvalidResource(false)
	, QueryType(InQueryType)
{
	PlatformGetNewRenderQuery(&Resource, &ResourceContext);
	OnQueryCreation( this );
}

FOpenGLRenderQuery::FOpenGLRenderQuery(FOpenGLRenderQuery const& OtherQuery)
{
	operator=(OtherQuery);
	OnQueryCreation( this );
}

FOpenGLRenderQuery::~FOpenGLRenderQuery()
{
	OnQueryDeletion( this );
	if (Resource && !bInvalidResource)
	{
		PlatformReleaseRenderQuery(Resource, ResourceContext);
	}
}

FOpenGLRenderQuery& FOpenGLRenderQuery::operator=(FOpenGLRenderQuery const& OtherQuery)
{
	if(this != &OtherQuery)
	{
		Resource = OtherQuery.Resource;
		ResourceContext = OtherQuery.ResourceContext;
		Result = OtherQuery.Result;
		bResultIsCached = OtherQuery.bResultIsCached;
		bInvalidResource = OtherQuery.bInvalidResource;
		QueryType = OtherQuery.QueryType;
		const_cast<FOpenGLRenderQuery*>(&OtherQuery)->bInvalidResource = true;
	}
	return *this;
}


void FOpenGLEventQuery::IssueEvent()
{
	VERIFY_GL_SCOPE();
	if(Sync)
	{
		FOpenGL::DeleteSync(Sync);
		Sync = UGLsync();
	}
	Sync = FOpenGL::FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#ifndef __EMSCRIPTEN__ // https://answers.unrealengine.com/questions/409649/html5-opengl-backend-doesnt-need-to-flush-gl-comma.html
	FOpenGL::Flush();
#endif

	checkSlow(FOpenGL::IsSync(Sync));

}

void FOpenGLEventQuery::WaitForCompletion()
{
	VERIFY_GL_SCOPE();


	checkSlow(FOpenGL::IsSync(Sync));


	// Wait up to 1/2 second for sync execution
	FOpenGL::EFenceResult Status = FOpenGL::ClientWaitSync( Sync, 0, 500*1000*1000);

	if ( Status != FOpenGL::FR_AlreadySignaled && Status != FOpenGL::FR_ConditionSatisfied )
	{
		//failure of some type, determine type and send diagnostic message
		if ( Status == FOpenGL::FR_TimeoutExpired )
		{
			UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
		}
		else if ( Status == FOpenGL::FR_WaitFailed )
		{
			UE_LOG(LogRHI, Log, TEXT("Wait on GPU failed in driver"));
		}
		else
		{
			UE_LOG(LogRHI, Log, TEXT("Unknown error while waiting on GPU"));
			check(0);
		}
	}

}

void FOpenGLEventQuery::InitDynamicRHI()
{
	VERIFY_GL_SCOPE();
	// Initialize the query by issuing an initial event.
	IssueEvent();

	checkSlow(FOpenGL::IsSync(Sync));
}

void FOpenGLEventQuery::ReleaseDynamicRHI()
{
	FOpenGL::DeleteSync(Sync);
}

/*=============================================================================
 * class FOpenGLBufferedGPUTiming
 *=============================================================================*/

/**
 * Constructor.
 *
 * @param InOpenGLRHI			RHI interface
 * @param InBufferSize		Number of buffered measurements
 */
FOpenGLBufferedGPUTiming::FOpenGLBufferedGPUTiming( FOpenGLDynamicRHI* InOpenGLRHI, int32 InBufferSize )
:	OpenGLRHI( InOpenGLRHI )
,	BufferSize( InBufferSize )
,	CurrentTimestamp( -1 )
,	NumIssuedTimestamps( 0 )
,	bIsTiming( false )
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FOpenGLBufferedGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	if ( !GAreGlobalsInitialized )
	{
		GIsSupported = FOpenGL::SupportsTimestampQueries();
		GTimingFrequency = 1000 * 1000 * 1000;
		GAreGlobalsInitialized = true;
	}
}

/**
 * Initializes all OpenGL resources and if necessary, the static variables.
 */
void FOpenGLBufferedGPUTiming::InitDynamicRHI()
{
	VERIFY_GL_SCOPE();

	StaticInitialize(OpenGLRHI, PlatformStaticInitialize);

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = false;
	GIsSupported = FOpenGL::SupportsTimestampQueries();

	if ( GIsSupported )
	{
		StartTimestamps.Reserve(BufferSize);
		EndTimestamps.Reserve(BufferSize);

		for(int32 BufferIndex = 0; BufferIndex < BufferSize; ++BufferIndex)
		{
			StartTimestamps.Add(new FOpenGLRenderQuery(RQT_AbsoluteTime));
			EndTimestamps.Add(new FOpenGLRenderQuery(RQT_AbsoluteTime));
		}
	}
}

/**
 * Releases all OpenGL resources.
 */
void FOpenGLBufferedGPUTiming::ReleaseDynamicRHI()
{
	VERIFY_GL_SCOPE();

	for(FOpenGLRenderQuery* Query : StartTimestamps)
	{
		delete Query;
	}

	for(FOpenGLRenderQuery* Query : EndTimestamps)
	{
		delete Query;
	}

	StartTimestamps.Reset();
	EndTimestamps.Reset();

}

/**
 * Start a GPU timing measurement.
 */
void FOpenGLBufferedGPUTiming::StartTiming()
{
	VERIFY_GL_SCOPE();
	// Issue a timestamp query for the 'start' time.
	if ( GIsSupported && !bIsTiming )
	{
		int32 NewTimestampIndex = (CurrentTimestamp + 1) % BufferSize;
		FOpenGLRenderQuery* TimerQuery = StartTimestamps[NewTimestampIndex];
		{
			if (!TimerQuery->bInvalidResource && !PlatformContextIsCurrent(TimerQuery->ResourceContext))
			{
				PlatformReleaseRenderQuery(TimerQuery->Resource, TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = true;
			}

			if (TimerQuery->bInvalidResource)
			{
				PlatformGetNewRenderQuery(&TimerQuery->Resource, &TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = false;
			}
		}

		FOpenGL::QueryTimestampCounter(StartTimestamps[NewTimestampIndex]->Resource);
		CurrentTimestamp = NewTimestampIndex;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FOpenGLBufferedGPUTiming::EndTiming()
{
	VERIFY_GL_SCOPE();
	// Issue a timestamp query for the 'end' time.
	if ( GIsSupported && bIsTiming )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );

		FOpenGLRenderQuery* TimerQuery = EndTimestamps[CurrentTimestamp];
		{
			if (!TimerQuery->bInvalidResource && !PlatformContextIsCurrent(TimerQuery->ResourceContext))
			{
				PlatformReleaseRenderQuery(TimerQuery->Resource, TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = true;
			}

			if (TimerQuery->bInvalidResource && PlatformOpenGLContextValid())
			{
				PlatformGetNewRenderQuery(&TimerQuery->Resource, &TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = false;
			}
		}

		FOpenGL::QueryTimestampCounter(EndTimestamps[CurrentTimestamp]->Resource);
		NumIssuedTimestamps = FMath::Min<int32>(NumIssuedTimestamps + 1, BufferSize);
		bIsTiming = false;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FOpenGLBufferedGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{

	VERIFY_GL_SCOPE();

	if ( GIsSupported )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		GLuint64 StartTime, EndTime;

		int32 TimestampIndex = CurrentTimestamp;

		{
			FOpenGLRenderQuery* EndStamp = EndTimestamps[TimestampIndex];
			if (!EndStamp->bInvalidResource && !PlatformContextIsCurrent(EndStamp->ResourceContext))
			{
				PlatformReleaseRenderQuery(EndStamp->Resource, EndStamp->ResourceContext);
				EndStamp->bInvalidResource = true;
			}

			FOpenGLRenderQuery* StartStamp = StartTimestamps[TimestampIndex];
			if (!StartStamp->bInvalidResource && !PlatformContextIsCurrent(StartStamp->ResourceContext))
			{
				PlatformReleaseRenderQuery(StartStamp->Resource, StartStamp->ResourceContext);
				StartStamp->bInvalidResource = true;

			}

			if(StartStamp->bInvalidResource || EndStamp->bInvalidResource)
			{
				UE_LOG(LogRHI, Log, TEXT("timing invalid, since the stamp queries have invalid resources"));
				return 0.0f;
			}
		}

		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for ( int32 IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex )
			{
				GLuint EndAvailable = GL_FALSE;
				FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &EndAvailable);

				if ( EndAvailable == GL_TRUE )
				{
					GLuint StartAvailable = GL_FALSE;
					FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &StartAvailable);

					if(StartAvailable == GL_TRUE)
					{
						FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &EndTime);
						FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &StartTime);
						if (EndTime > StartTime)
						{
							return EndTime - StartTime;
						}
					}
				}

				TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
			}
		}

		if ( NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock )
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind
			const bool bBlocking = ( NumIssuedTimestamps == BufferSize ) || bGetCurrentResultsAndBlock;

			uint32 IdleStart = FPlatformTime::Cycles();
			double StartTimeoutTime = FPlatformTime::Seconds();

			GLuint EndAvailable = GL_FALSE;

			SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
			// If we are blocking, retry until the GPU processes the time stamp command
			do
			{
				FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &EndAvailable);

				if ((FPlatformTime::Seconds() - StartTimeoutTime) > 0.5)
				{
					UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms) EndTimeStamp"));
					return 0;
				}
			} while ( EndAvailable == GL_FALSE && bBlocking );

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			if ( EndAvailable == GL_TRUE )
			{
				IdleStart = FPlatformTime::Cycles();
				StartTimeoutTime = FPlatformTime::Seconds();

				GLuint StartAvailable = GL_FALSE;

				do
				{
					FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &StartAvailable);

					if ((FPlatformTime::Seconds() - StartTimeoutTime) > 0.5)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms) StartTimeStamp"));
						return 0;
					}
				} while ( StartAvailable == GL_FALSE && bBlocking );

				GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;

				if(StartAvailable == GL_TRUE)
				{
					FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &EndTime);
					FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &StartTime);
					if (EndTime > StartTime)
					{
						return EndTime - StartTime;
					}
				}
			}
		}
	}
	return 0;
}

FOpenGLDisjointTimeStampQuery::FOpenGLDisjointTimeStampQuery(class FOpenGLDynamicRHI* InOpenGLRHI)
:	bIsResultValid(false)
,	DisjointQuery(0)
,	Context(0)
,	OpenGLRHI(InOpenGLRHI)
{
}

void FOpenGLDisjointTimeStampQuery::StartTracking()
{
	VERIFY_GL_SCOPE();
	if (IsSupported())
	{

		if (!PlatformContextIsCurrent(Context))
		{
			PlatformReleaseRenderQuery(DisjointQuery, Context);
			PlatformGetNewRenderQuery(&DisjointQuery, &Context);
		}
		// Dummy query to reset the driver's internal disjoint status
		FOpenGL::TimerQueryDisjoint();
		FOpenGL::BeginQuery(UGL_TIME_ELAPSED, DisjointQuery);
	}
}

void FOpenGLDisjointTimeStampQuery::EndTracking()
{
	VERIFY_GL_SCOPE();

	if(IsSupported())
	{
		FOpenGL::EndQuery( UGL_TIME_ELAPSED );

		// Check if the GPU changed clock frequency since the last time GL_GPU_DISJOINT_EXT was checked.
		// If so, any timer query will be undefined.
		bIsResultValid = !FOpenGL::TimerQueryDisjoint();
	}

}

bool FOpenGLDisjointTimeStampQuery::IsResultValid()
{
	checkSlow(IsSupported());
	return bIsResultValid;
}

bool FOpenGLDisjointTimeStampQuery::GetResult( uint64* OutResult/*=NULL*/ )
{
	VERIFY_GL_SCOPE();

	if (IsSupported())
	{
		GLuint Result = 0;
		FOpenGL::GetQueryObject(DisjointQuery, FOpenGL::QM_ResultAvailable, &Result);
		const double StartTime = FPlatformTime::Seconds();

		while (Result == GL_FALSE && (FPlatformTime::Seconds() - StartTime) < 0.5)
		{
			FPlatformProcess::Sleep(0.005f);
			FOpenGL::GetQueryObject(DisjointQuery, FOpenGL::QM_ResultAvailable, &Result);
		}

		// Presently just discarding the result, because timing is handled by timestamps inside
		if (Result != GL_FALSE)
		{
			GLuint64 ElapsedTime = 0;
			FOpenGL::GetQueryObject(DisjointQuery, FOpenGL::QM_Result, &ElapsedTime);
			if (OutResult)
			{
				*OutResult = ElapsedTime;
			}
		}
		bIsResultValid = Result != GL_FALSE;
	}
	return bIsResultValid;
}

void FOpenGLDisjointTimeStampQuery::InitDynamicRHI()
{
	VERIFY_GL_SCOPE();
	if ( IsSupported() )
	{
		PlatformGetNewRenderQuery(&DisjointQuery, &Context);
	}
}

void FOpenGLDisjointTimeStampQuery::ReleaseDynamicRHI()
{
	VERIFY_GL_SCOPE();
	if ( IsSupported() )
	{
		PlatformReleaseRenderQuery(DisjointQuery, Context);
	}
}
