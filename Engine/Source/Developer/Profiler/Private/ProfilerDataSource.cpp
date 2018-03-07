// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerDataSource.h"
#include "Containers/MapBuilder.h"
#include "ProfilerStream.h"
#include "ProfilerDataProvider.h"
#include "ProfilerSession.h"

#define LOCTEXT_NAMESPACE "ProfilerDataSource"


/*-----------------------------------------------------------------------------
	FGraphDataSource
-----------------------------------------------------------------------------*/

const int32 FTimeAccuracy::_FPS008 = 8;
const int32 FTimeAccuracy::_FPS015 = 15;
const int32 FTimeAccuracy::_FPS030 = 30;
const int32 FTimeAccuracy::_FPS060 = 60;
const int32 FTimeAccuracy::_FPS120 = 120;


/*-----------------------------------------------------------------------------
	FGraphDataSource
-----------------------------------------------------------------------------*/

FGraphDataSource::FGraphDataSource( const TSharedRef<FProfilerSession>& InProfilerSession, const uint32 InStatID ) 
	: FGraphDataSourceDescription( InStatID )
	, ThisCachedDataByIndex()
	, ThisCachedDataByTime( FTimeAccuracy::FPS060 )
	, ProfilerSession( InProfilerSession ) 
{
	const TSharedRef<FProfilerStatMetaData> MetaData = ProfilerSession->GetMetaData();
	const FProfilerStat& Stat = MetaData->GetStatByID(InStatID);
	const FProfilerGroup& Group = Stat.OwningGroup();

	Initialize( Stat.Name().GetPlainNameString(), Group.Name().GetPlainNameString(), Stat.Type(), ProfilerSession->GetCreationTime() );

	switch( GetSampleType() )
	{
	case EProfilerSampleTypes::Memory:
		{
			// By default we show memory data as KBs.
			Scale = 1.0f / 1024.0f;
			break;
		}

	default:
		{
			Scale = 1.0f;
		}
	}
}

const TGraphDataType FGraphDataSource::GetUncachedValueFromIndex( const uint32 FrameIndex ) const
{
	check( FrameIndex < ProfilerSession->GetDataProvider()->GetNumFrames() );
	double Result = 0.0;

	// Hierarchical samples are stored in different location.
	// We skip hierarchical samples to ignore misleading recursion which would be counted twice etc.
	if (GetSampleType() == EProfilerSampleTypes::HierarchicalTime)
	{
		const TMap<uint32, FInclusiveTime>& InclusiveAggregates = ProfilerSession->GetInclusiveAggregateStackStats( FrameIndex );
		const FInclusiveTime* InclusiveTime = InclusiveAggregates.Find( GetStatID() );

		if (InclusiveTime)
		{
			Result = ProfilerSession->GetMetaData()->ConvertCyclesToMS( InclusiveTime->DurationCycles ) * Scale;
		}
	}
	else
	{
		const TSharedRef<IDataProvider> DataProvider = ProfilerSession->GetDataProvider();

		const FIntPoint& IndicesForFrame = DataProvider->GetSamplesIndicesForFrame( FrameIndex );
		const uint32 SampleStartIndex = IndicesForFrame.X;
		const uint32 SampleEndIndex = IndicesForFrame.Y;

		const FProfilerSampleArray& Collection = DataProvider->GetCollection();

		for (uint32 SampleIndex = SampleStartIndex; SampleIndex < SampleEndIndex; SampleIndex++)
		{
			const FProfilerSample& ProfilerSample = Collection[SampleIndex];
			const bool bValidStat = ProfilerSample.StatID() == GetStatID();

			if (bValidStat)
			{
				Result += ProfilerSample.GetDoubleValue() * Scale;
			}
		}
	}

	return (TGraphDataType)Result;
}


const TGraphDataType FGraphDataSource::GetUncachedValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const
{
	const TSharedRef<IDataProvider> DataProvider = ProfilerSession->GetDataProvider();
	const FIntPoint IndicesForFrame = DataProvider->GetClosestSamplesIndicesForTime( StartTimeMS, EndTimeMS );

	const uint32 StartFrameIndex = IndicesForFrame.X;
	const uint32 EndFrameIndex = IndicesForFrame.Y;

	TGraphDataType ResultMax = (TGraphDataType)MIN_int32;

	// Iterate through all frames and calculate the maximum value.
	for( uint32 FrameIndex = StartFrameIndex; FrameIndex < EndFrameIndex; ++FrameIndex )
	{
		const TGraphDataType DataSourceValue = const_cast<FGraphDataSource*>(this)->GetValueFromIndex( FrameIndex );
		ResultMax = FMath::Max( ResultMax, DataSourceValue );
	}

	return (TGraphDataType)ResultMax;
}

const uint32 FGraphDataSource::GetNumFrames() const
{
	return ProfilerSession->GetDataProvider()->GetNumFrames();
}

const float FGraphDataSource::GetTotalTimeMS() const
{
	return ProfilerSession->GetDataProvider()->GetTotalTimeMS();
}

const TSharedRef<IDataProvider> FGraphDataSource::GetDataProvider() const
{
	return ProfilerSession->GetDataProvider();
}

const FProfilerAggregatedStat* FGraphDataSource::GetAggregatedStat() const
{
	return ProfilerSession->GetAggregatedStat( StatID );
}

const FGuid FGraphDataSource::GetSessionInstanceID() const
{
	return ProfilerSession->GetInstanceID();
}


/*-----------------------------------------------------------------------------
	FCombinedGraphDataSource
-----------------------------------------------------------------------------*/

FCombinedGraphDataSource::FCombinedGraphDataSource( const uint32 InStatID, const FTimeAccuracy::Type InTimeAccuracy ) 
	: FGraphDataSourceDescription( InStatID )
	, ThisCachedDataByTime( InTimeAccuracy )
{
}

const FVector FCombinedGraphDataSource::GetUncachedValueFromTimeRange( const float StartTimeMS, const float EndTimeMS ) const
{
	// X=Min, Y=Max, Z=Avg
	FVector AggregatedValue( (TGraphDataType)MAX_int32, (TGraphDataType)MIN_int32, 0.0f );

	const uint32 NumSources = GraphDataSources.Num();
	const float InvNumSources = 1.0f / (float)NumSources;

	for( auto It = GetSourcesIterator(); It; ++It )
	{
		const TSharedRef<const FGraphDataSource>& GraphDataSource = It.Value();
		const float DataSourceValue = GraphDataSource->GetValueFromTimeRange( StartTimeMS, EndTimeMS );

		AggregatedValue.X = FMath::Min( AggregatedValue.X, DataSourceValue );
		AggregatedValue.Y = FMath::Max( AggregatedValue.Y, DataSourceValue );
		AggregatedValue.Z += DataSourceValue;
	}
	AggregatedValue.Z *= InvNumSources;

	return AggregatedValue;
}

void FCombinedGraphDataSource::GetStartIndicesFromTimeRange( const float StartTimeMS, const float EndTimeMS, TMap<FGuid,uint32>& out_StartIndices ) const
{
	for( auto It = GetSourcesIterator(); It; ++It )
	{
		const TSharedRef<const FGraphDataSource>& GraphDataSource = It.Value();
		const FIntPoint IndicesForFrame = GraphDataSource->GetDataProvider()->GetClosestSamplesIndicesForTime( StartTimeMS, EndTimeMS );

		const uint32 StartFrameIndex = IndicesForFrame.X;
		const uint32 EndFrameIndex = IndicesForFrame.Y;

		uint32 FrameIndex = 0;
		float MaxFrameTime = 0.0f;

		// Iterate through all frames and find the highest frame time.
		for( uint32 InnerFrameIndex = StartFrameIndex; InnerFrameIndex < EndFrameIndex; ++InnerFrameIndex )
		{
			const float InnerFrameTime = GraphDataSource->GetDataProvider()->GetFrameTimeMS( InnerFrameIndex );
			if( InnerFrameTime > MaxFrameTime )
			{
				MaxFrameTime = InnerFrameTime;
				FrameIndex = InnerFrameIndex;
			}
		}

		if( MaxFrameTime > 0.0f )
		{
			out_StartIndices.Add( GraphDataSource->GetSessionInstanceID(), FrameIndex );
		}
	}
}


/*-----------------------------------------------------------------------------
	FEventGraphData
-----------------------------------------------------------------------------*/

//namespace NEventGraphSample {

FName FEventGraphConsts::RootEvent = TEXT("RootEvent");
FName FEventGraphConsts::Self = TEXT("Self");
FName FEventGraphConsts::FakeRoot = TEXT("FakeRoot");


/*-----------------------------------------------------------------------------
	FEventGraphSample
-----------------------------------------------------------------------------*/

FEventProperty FEventGraphSample::Properties[ EEventPropertyIndex::InvalidOrMax ] =
{
	// Properties
	FEventProperty( EEventPropertyIndex::StatName, TEXT( "StatName" ), STRUCT_OFFSET( FEventGraphSample, _StatName ), EEventPropertyFormatters::Name ),
	FEventProperty( EEventPropertyIndex::InclusiveTimeMS, TEXT( "InclusiveTimeMS" ), STRUCT_OFFSET( FEventGraphSample, _InclusiveTimeMS ), EEventPropertyFormatters::TimeMS ),
	FEventProperty( EEventPropertyIndex::InclusiveTimePct, TEXT( "InclusiveTimePct" ), STRUCT_OFFSET( FEventGraphSample, _InclusiveTimePct ), EEventPropertyFormatters::TimePct ),
	FEventProperty( EEventPropertyIndex::ExclusiveTimeMS, TEXT( "ExclusiveTimeMS" ), STRUCT_OFFSET( FEventGraphSample, _ExclusiveTimeMS ), EEventPropertyFormatters::TimeMS ),
	FEventProperty( EEventPropertyIndex::ExclusiveTimePct, TEXT( "ExclusiveTimePct" ), STRUCT_OFFSET( FEventGraphSample, _ExclusiveTimePct ), EEventPropertyFormatters::TimePct ),
	FEventProperty( EEventPropertyIndex::NumCallsPerFrame, TEXT( "NumCallsPerFrame" ), STRUCT_OFFSET( FEventGraphSample, _NumCallsPerFrame ), EEventPropertyFormatters::Number ),
	// Special none property
	FEventProperty( EEventPropertyIndex::None, NAME_None, INDEX_NONE, EEventPropertyFormatters::Name ),

	FEventProperty( EEventPropertyIndex::MinInclusiveTimeMS, TEXT( "MinInclusiveTimeMS" ), STRUCT_OFFSET( FEventGraphSample, _MinInclusiveTimeMS ), EEventPropertyFormatters::TimeMS ),
	FEventProperty( EEventPropertyIndex::MaxInclusiveTimeMS, TEXT( "MaxInclusiveTimeMS" ), STRUCT_OFFSET( FEventGraphSample, _MaxInclusiveTimeMS ), EEventPropertyFormatters::TimeMS ),
	FEventProperty( EEventPropertyIndex::AvgInclusiveTimeMS, TEXT( "AvgInclusiveTimeMS" ), STRUCT_OFFSET( FEventGraphSample, _AvgInclusiveTimeMS ), EEventPropertyFormatters::TimeMS ),
	
	FEventProperty( EEventPropertyIndex::MinNumCallsPerFrame, TEXT( "MinNumCallsPerFrame" ), STRUCT_OFFSET( FEventGraphSample, _MinNumCallsPerFrame ), EEventPropertyFormatters::Number ),
	FEventProperty( EEventPropertyIndex::MaxNumCallsPerFrame, TEXT( "MaxNumCallsPerFrame" ), STRUCT_OFFSET( FEventGraphSample, _MaxNumCallsPerFrame ), EEventPropertyFormatters::Number ),
	FEventProperty( EEventPropertyIndex::AvgNumCallsPerFrame, TEXT( "AvgNumCallsPerFrame" ), STRUCT_OFFSET( FEventGraphSample, _AvgNumCallsPerFrame ), EEventPropertyFormatters::Number ),

	FEventProperty( EEventPropertyIndex::ThreadName, TEXT( "ThreadName" ), STRUCT_OFFSET( FEventGraphSample, _ThreadName ), EEventPropertyFormatters::Name ),
	FEventProperty( EEventPropertyIndex::ThreadDurationMS, TEXT( "ThreadDurationMS" ), STRUCT_OFFSET( FEventGraphSample, _ThreadDurationMS ), EEventPropertyFormatters::TimeMS ),
	FEventProperty( EEventPropertyIndex::FrameDurationMS, TEXT( "FrameDurationMS" ), STRUCT_OFFSET( FEventGraphSample, _FrameDurationMS ), EEventPropertyFormatters::TimeMS ),
	FEventProperty( EEventPropertyIndex::ThreadPct, TEXT( "ThreadPct" ), STRUCT_OFFSET( FEventGraphSample, _ThreadPct ), EEventPropertyFormatters::TimePct ),
	FEventProperty( EEventPropertyIndex::FramePct, TEXT( "FramePct" ), STRUCT_OFFSET( FEventGraphSample, _FramePct ), EEventPropertyFormatters::TimePct ),
	FEventProperty( EEventPropertyIndex::ThreadToFramePct, TEXT( "ThreadToFramePct" ), STRUCT_OFFSET( FEventGraphSample, _ThreadToFramePct ), EEventPropertyFormatters::TimePct ),
	FEventProperty( EEventPropertyIndex::GroupName, TEXT( "GroupName" ), STRUCT_OFFSET( FEventGraphSample, _GroupName ), EEventPropertyFormatters::Name ),

	// Booleans
	FEventProperty(EEventPropertyIndex::bIsHotPath,TEXT("bIsHotPath"),STRUCT_OFFSET(FEventGraphSample,_bIsHotPath),EEventPropertyFormatters::Bool),
	FEventProperty(EEventPropertyIndex::bIsFiltered,TEXT("bIsFiltered"),STRUCT_OFFSET(FEventGraphSample,_bIsFiltered),EEventPropertyFormatters::Bool),
	FEventProperty(EEventPropertyIndex::bIsCulled,TEXT("bIsCulled"),STRUCT_OFFSET(FEventGraphSample,_bIsCulled),EEventPropertyFormatters::Bool),

	// Booleans internal
	FEventProperty(EEventPropertyIndex::bNeedNotCulledChildrenUpdate,TEXT("bNeedNotCulledChildrenUpdate"),STRUCT_OFFSET(FEventGraphSample,bNeedNotCulledChildrenUpdate),EEventPropertyFormatters::Bool),
};

TMap<FName,const FEventProperty*> FEventGraphSample::NamedProperties;

void FEventGraphSample::InitializePropertyManagement()
{
	static bool bInitialized = false;
	if( !bInitialized )
	{
		NamedProperties = TMapBuilder<FName,const FEventProperty*>()
			// Properties
			.Add( TEXT( "StatName" ), &Properties[EEventPropertyIndex::StatName] )
			.Add( TEXT( "InclusiveTimeMS" ), &Properties[EEventPropertyIndex::InclusiveTimeMS] )
			.Add( TEXT( "InclusiveTimePct" ), &Properties[EEventPropertyIndex::InclusiveTimePct] )
			.Add( TEXT( "ExclusiveTimeMS" ), &Properties[EEventPropertyIndex::ExclusiveTimeMS] )
			.Add( TEXT( "ExclusiveTimePct" ), &Properties[EEventPropertyIndex::ExclusiveTimePct] )
			.Add( TEXT( "NumCallsPerFrame" ), &Properties[EEventPropertyIndex::NumCallsPerFrame] )

			// Special none property
			.Add( NAME_None, &Properties[EEventPropertyIndex::None] )

			.Add( TEXT( "MinInclusiveTimeMS" ), &Properties[EEventPropertyIndex::MinInclusiveTimeMS] )
			.Add( TEXT( "MaxInclusiveTimeMS" ), &Properties[EEventPropertyIndex::MaxInclusiveTimeMS] )
			.Add( TEXT( "AvgInclusiveTimeMS" ), &Properties[EEventPropertyIndex::AvgInclusiveTimeMS] )	

			.Add( TEXT( "MinNumCallsPerFrame" ), &Properties[EEventPropertyIndex::MinNumCallsPerFrame] )
			.Add( TEXT( "MaxNumCallsPerFrame" ), &Properties[EEventPropertyIndex::MaxNumCallsPerFrame] )
			.Add( TEXT( "AvgNumCallsPerFrame" ), &Properties[EEventPropertyIndex::AvgNumCallsPerFrame] )

			.Add( TEXT( "ThreadName" ), &Properties[EEventPropertyIndex::ThreadName] )
			.Add( TEXT( "ThreadDurationMS" ), &Properties[EEventPropertyIndex::ThreadDurationMS] )
			.Add( TEXT( "FrameDurationMS" ), &Properties[EEventPropertyIndex::FrameDurationMS] )
			.Add( TEXT( "ThreadPct" ), &Properties[EEventPropertyIndex::ThreadPct] )
			.Add( TEXT( "FramePct" ), &Properties[EEventPropertyIndex::FramePct] )
			.Add( TEXT( "ThreadToFramePct" ), &Properties[EEventPropertyIndex::ThreadToFramePct] )
			.Add( TEXT( "GroupName" ), &Properties[EEventPropertyIndex::GroupName] )

			// Booleans
			.Add( TEXT( "bIsHotPath" ), &Properties[EEventPropertyIndex::bIsHotPath] )
			.Add( TEXT( "bIsFiltered" ), &Properties[EEventPropertyIndex::bIsFiltered] )
			.Add( TEXT( "bIsCulled" ), &Properties[EEventPropertyIndex::bIsCulled] )

			// Booleans internal
			.Add( TEXT( "bNeedNotCulledChildrenUpdate" ), &Properties[EEventPropertyIndex::bNeedNotCulledChildrenUpdate] )
			;
	
		// Make sure that the minimal property manager has been initialized.
		bInitialized = true;

		// Sanity checks.
		check( NamedProperties.FindChecked( NAME_None )->Name == NAME_None );
		check( NamedProperties.FindChecked( NAME_None )->Offset == INDEX_NONE );

		check( FEventGraphSample::Properties[ EEventPropertyIndex::None ].Name == NAME_None );
		check( FEventGraphSample::Properties[ EEventPropertyIndex::None ].Offset == INDEX_NONE );
	}
}

void FEventGraphSample::SetMaximumTimesForAllChildren()
{
	struct FCopyMaximum
	{
		void operator()( FEventGraphSample* EventPtr, FEventGraphSample* RootEvent, FEventGraphSample* ThreadEvent )
		{
			EventPtr->CopyMaximum( RootEvent, ThreadEvent );
		}
	};

	FEventGraphSamplePtr RootEvent = AsShared();
	const int32 NumChildren = _ChildrenPtr.Num();
	for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const FEventGraphSamplePtr& ThreadEvent = _ChildrenPtr[ChildIndex];
		ThreadEvent->ExecuteOperationForAllChildren( FCopyMaximum(), (FEventGraphSample*)RootEvent.Get(), (FEventGraphSample*)ThreadEvent.Get() );
	}
}

void FEventGraphSample::SetRootAndThreadForAllChildren()
{
	struct FSetRootAndThread
	{
		void operator()( FEventGraphSample* EventPtr, FEventGraphSample* RootEvent, FEventGraphSample* ThreadEvent )
		{
			EventPtr->_RootPtr = RootEvent;
			EventPtr->_ThreadPtr = ThreadEvent;
		}
	};

	FEventGraphSamplePtr RootEvent = AsShared();
	const int32 NumChildren = _ChildrenPtr.Num();
	for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const FEventGraphSamplePtr& ThreadEvent = _ChildrenPtr[ChildIndex];
		ThreadEvent->ExecuteOperationForAllChildren( FSetRootAndThread(), (FEventGraphSample*)RootEvent.Get(), (FEventGraphSample*)ThreadEvent.Get() );
	}
}

void FEventGraphSample::FixChildrenTimesAndCalcAveragesForAllChildren( const double InNumFrames )
{
	struct FFixChildrenTimesAndCalcAverages
	{
		void operator()( FEventGraphSample* EventPtr, const double NumFrames )
		{
			EventPtr->FixChildrenTimesAndCalcAverages( NumFrames );
		}
	};

	ExecuteOperationForAllChildren( FFixChildrenTimesAndCalcAverages(), InNumFrames );
}

FEventGraphSamplePtr FEventGraphSample::FindChildPtr( const FEventGraphSamplePtr& OtherChild )
{
	FEventGraphSamplePtr FoundChildPtr;
	for( int32 ChildIndex = 0; ChildIndex < _ChildrenPtr.Num(); ++ChildIndex )
	{
		const FEventGraphSamplePtr& ThisChild = _ChildrenPtr[ChildIndex];

		const bool bTheSame = OtherChild->AreTheSamePtr( ThisChild );
		if( bTheSame )
		{
			FoundChildPtr = ThisChild;
			break;
		}
	}
	return FoundChildPtr;
}


void FEventGraphSample::Combine_Recurrent( const FEventGraphSamplePtr& Other )
{
	Combine( Other );

	// Check other children.
	for( int32 ChildIndex = 0; ChildIndex < Other->_ChildrenPtr.Num(); ++ChildIndex )
	{
		const FEventGraphSamplePtr& OtherChild = Other->_ChildrenPtr[ChildIndex];
		FEventGraphSamplePtr ThisChild = FindChildPtr( OtherChild );

		if( ThisChild.IsValid() )
		{
			ThisChild->Combine_Recurrent( OtherChild );
		}
		else
		{
			AddChildAndSetParentPtr( OtherChild->DuplicateWithHierarchyPtr() );
		}
	}
}

/*-----------------------------------------------------------------------------
	FEventGraphData
-----------------------------------------------------------------------------*/

FEventGraphData::FEventGraphData() 
	: RootEvent( FEventGraphSample::CreateNamedEvent( FEventGraphConsts::RootEvent ) )
	, FrameStartIndex( 0 )
	, FrameEndIndex( 0 )
{}

FEventGraphData::FEventGraphData( const FProfilerSession * const InProfilerSession, const uint32 InFrameIndex )
	: FrameStartIndex( InFrameIndex )
	, FrameEndIndex( InFrameIndex+1 )
{
	static FTotalTimeAndCount Current(0.0f, 0);
	PROFILER_SCOPE_LOG_TIME( TEXT( "FEventGraphData::FEventGraphData" ), &Current );

	Description = FString::Printf( TEXT("%s: %i"), *InProfilerSession->GetShortName(), InFrameIndex );

	// @TODO: Duplicate is not needed, remove it later.
	const TSharedRef<IDataProvider>& SessionDataProvider = InProfilerSession->GetDataProvider(); 
	const TSharedRef<IDataProvider> DataProvider = SessionDataProvider->Duplicate<FArrayDataProvider>( FrameStartIndex );

	const double FrameDurationMS = DataProvider->GetFrameTimeMS( 0 ); 
	const FProfilerSample& RootProfilerSample = DataProvider->GetCollection()[0];

	RootEvent = FEventGraphSample::CreateNamedEvent( FEventGraphConsts::RootEvent );

	PopulateHierarchy_Recurrent( InProfilerSession, RootEvent, RootProfilerSample, DataProvider );

	// Root sample contains FrameDurationMS
	const TSharedRef<FProfilerStatMetaData>& MetaData = InProfilerSession->GetMetaData();
	RootEvent->_InclusiveTimeMS = MetaData->ConvertCyclesToMS( RootProfilerSample.GetDurationCycles() );
	RootEvent->_MaxInclusiveTimeMS = RootEvent->_MinInclusiveTimeMS = RootEvent->_AvgInclusiveTimeMS = RootEvent->_InclusiveTimeMS;
	RootEvent->_InclusiveTimePct = 100.0f;

	RootEvent->_MinNumCallsPerFrame = RootEvent->_MaxNumCallsPerFrame = RootEvent->_AvgNumCallsPerFrame = RootEvent->_NumCallsPerFrame;

	// Set root and thread event.
	RootEvent->SetRootAndThreadForAllChildren();
	// Fix all children. 
	const double MyNumFrames = 1.0;
	RootEvent->FixChildrenTimesAndCalcAveragesForAllChildren( MyNumFrames );
}

void FEventGraphData::PopulateHierarchy_Recurrent
( 
	const FProfilerSession * const ProfilerSession,
	const FEventGraphSamplePtr ParentEvent, 
	const FProfilerSample& ParentSample, 
	const TSharedRef<IDataProvider> DataProvider
)
{
	const TSharedRef<FProfilerStatMetaData>& MetaData = ProfilerSession->GetMetaData();

	for( int32 ChildIndex = 0; ChildIndex < ParentSample.ChildrenIndices().Num(); ChildIndex++ )
	{
		const FProfilerSample& ChildSample = DataProvider->GetCollection()[ ParentSample.ChildrenIndices()[ChildIndex] ];

		const FProfilerStat& ProfilerThread = MetaData->GetStatByID( ChildSample.ThreadID() );
		const FName& ThreadName = ProfilerThread.Name();

		const FProfilerStat& ProfilerStat = MetaData->GetStatByID( ChildSample.StatID() );
		const FName& StatName = ProfilerStat.Name();
		const FName& GroupName = ProfilerStat.OwningGroup().Name();

		FEventGraphSample* ChildEvent = new FEventGraphSample
		(
			ThreadName,	GroupName, ChildSample.StatID(), StatName, 
			MetaData->ConvertCyclesToMS( ChildSample.GetDurationCycles() ), (double)ChildSample.GetCallCount(),
			ParentEvent
		);

		FEventGraphSamplePtr ChildEventPtr = MakeShareable( ChildEvent );
		ParentEvent->AddChildPtr( ChildEventPtr );

		PopulateHierarchy_Recurrent( ProfilerSession, ChildEventPtr, ChildSample, DataProvider );	
	}
}

FEventGraphData::FEventGraphData( const FEventGraphData& Source ) 
	: FrameStartIndex( Source.FrameStartIndex )
	, FrameEndIndex( Source.FrameEndIndex )
{
	RootEvent = Source.GetRoot()->DuplicateWithHierarchyPtr();
	RootEvent->SetRootAndThreadForAllChildren();
}

FEventGraphDataRef FEventGraphData::DuplicateAsRef()
{
	FEventGraphDataRef EventGraphRef = MakeShareable( new FEventGraphData(*this) );
	return EventGraphRef;
}

void FEventGraphData::Combine( const FEventGraphData& Other )
{
	RootEvent->Combine_Recurrent( Other.GetRoot() );
	Description = FString::Printf( TEXT("Combine: %i"), GetNumFrames() );
}

void FEventGraphData::SetAsAverage()
{
	struct FCopyAverage
	{
		void operator()( FEventGraphSample* EventPtr, const double NumFrames )
		{
			EventPtr->CopyAverage( NumFrames );
		}
	};

	const double NumFrames = (double)GetNumFrames();
	RootEvent->ExecuteOperationForAllChildren( FCopyAverage(), NumFrames );
	Description = FString::Printf( TEXT("Average: %i"), GetNumFrames() );
}

void FEventGraphData::SetAsMaximim()
{
	RootEvent->SetMaximumTimesForAllChildren();
	Description = FString::Printf( TEXT( "Maximum: %i" ), GetNumFrames() );
}

void FEventGraphData::Finalize( const uint32 InFrameStartIndex, const uint32 InFrameEndIndex )
{
	FrameStartIndex = InFrameStartIndex;
	FrameEndIndex = InFrameEndIndex;
	const double NumFrames = (double)GetNumFrames();

	// Set root and thread event.
	RootEvent->SetRootAndThreadForAllChildren();
	// Fix all children. 
	RootEvent->FixChildrenTimesAndCalcAveragesForAllChildren( NumFrames );
}

//}//namespace NEventGraphSample

FString EEventGraphTypes::ToName( const EEventGraphTypes::Type EventGraphType )
{
	switch( EventGraphType )
	{
		case Average: return LOCTEXT("EventGraphType_Name_Average", "Average").ToString();
		case Maximum: return LOCTEXT("EventGraphType_Name_Maximum", "Maximum").ToString();
		case OneFrame: return LOCTEXT("EventGraphType_Name_OneFrame", "OneFrame").ToString();
		case Total: return LOCTEXT( "EventGraphType_Name_Total", "Total" ).ToString();

		default: return LOCTEXT("InvalidOrMax", "InvalidOrMax").ToString();
	}
}

FString EEventGraphTypes::ToDescription( const EEventGraphTypes::Type EventGraphType )
{
	switch( EventGraphType )
	{
		case Average: return LOCTEXT("EventGraphType_Desc_Average", "Per-frame average event graph").ToString();
		case Maximum: return LOCTEXT("EventGraphType_Desc_Maximum", "Highest \"per-frame\" event graph").ToString();
		case OneFrame: return LOCTEXT("EventGraphType_Desc_OneFrame", "Event graph for one frame").ToString();
		case Total: return LOCTEXT( "EventGraphType_Desc_Total", "Event graph for selected frames" ).ToString();

		default: return LOCTEXT("InvalidOrMax", "InvalidOrMax").ToString();
	}
}

#undef LOCTEXT_NAMESPACE
