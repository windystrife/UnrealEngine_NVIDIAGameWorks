// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IProfilerModule.h"
#include "Stats/StatsFile.h"
#include "ProfilerRawStatsForMemory.h"
#include "ISessionManager.h"
#include "ProfilerManager.h"
#include "Widgets/SWidget.h"
#include "Widgets/SProfilerWindow.h"
#include "Widgets/Docking/SDockTab.h"

/**
 * Implements the FProfilerModule module.
 */
class FProfilerModule
	: public IProfilerModule
{
public:

	virtual TSharedRef<SWidget> CreateProfilerWindow( const TSharedRef<ISessionManager>& InSessionManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab ) override;

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	void StatsMemoryDumpCommand( const TCHAR* Filename );

	FRawStatsMemoryProfiler* OpenRawStatsForMemoryProfiling( const TCHAR* Filename );

protected:

	/** Shutdowns the profiler manager. */
	void Shutdown( TSharedRef<SDockTab> TabBeingClosed );
};


IMPLEMENT_MODULE( FProfilerModule, Profiler );


/*-----------------------------------------------------------------------------
	FProfilerModule
-----------------------------------------------------------------------------*/

TSharedRef<SWidget> FProfilerModule::CreateProfilerWindow( const TSharedRef<ISessionManager>& InSessionManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab )
{
	FProfilerManager::Initialize( InSessionManager );
	TSharedRef<SProfilerWindow> ProfilerWindow = SNew( SProfilerWindow );
	FProfilerManager::Get()->AssignProfilerWindow( ProfilerWindow );
	// Register OnTabClosed to handle profiler manager shutdown.
	ConstructUnderMajorTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateRaw( this, &FProfilerModule::Shutdown ) );

	return ProfilerWindow;
}

void FProfilerModule::ShutdownModule()
{
	if (FProfilerManager::Get().IsValid())
	{
		FProfilerManager::Get()->Shutdown();
	}
}

void FProfilerModule::Shutdown( TSharedRef<SDockTab> TabBeingClosed )
{
	FProfilerManager::Get()->Shutdown();
	TabBeingClosed->SetOnTabClosed( SDockTab::FOnTabClosedCallback() );
}

/*-----------------------------------------------------------------------------
	StatsMemoryDumpCommand
-----------------------------------------------------------------------------*/

void FProfilerModule::StatsMemoryDumpCommand( const TCHAR* Filename )
{
	TUniquePtr<FRawStatsMemoryProfiler> Instance( FStatsReader<FRawStatsMemoryProfiler>::Create( Filename ) );
	if (Instance)
	{
		Instance->ReadAndProcessSynchronously();

		while (Instance->IsBusy())
		{
			FPlatformProcess::Sleep( 1.0f );

			UE_LOG( LogStats, Log, TEXT( "Async: Stage: %s / %3i%%" ), *Instance->GetProcessingStageAsString(), Instance->GetStageProgress() );
		}
		//Instance->RequestStop();

		if (Instance->HasValidData())
		{
			// Dump scoped allocations between the first and the last snapshot.
			const FName FirstSnapshotName = Instance->GetSnapshotNames()[0];
			const FName LastSnapshotName = Instance->GetSnapshotNames()[Instance->GetSnapshotNames().Num()-1];

			TMap<FString, FCombinedAllocationInfo> FrameBegin_End;
			Instance->CompareSnapshotsHumanReadable( FirstSnapshotName, LastSnapshotName, FrameBegin_End );
			Instance->DumpScopedAllocations( TEXT( "Begin_End" ), FrameBegin_End );

			Instance->ProcessAndDumpUObjectAllocations( TEXT( "Frame-240" ) );

#if	1/*UE_BUILD_DEBUG*/
			// Dump debug scoped allocation generated when debug.EnableLeakTest=1 
			TMap<FString, FCombinedAllocationInfo> Frame060_120;
			Instance->CompareSnapshotsHumanReadable( TEXT( "Frame-060" ), TEXT( "Frame-120" ), Frame060_120 );
			Instance->DumpScopedAllocations( TEXT( "Frame060_120" ), Frame060_120 );

			TMap<FString, FCombinedAllocationInfo> Frame060_240;
			Instance->CompareSnapshotsHumanReadable( TEXT( "Frame-060" ), TEXT( "Frame-240" ), Frame060_240 );
			Instance->DumpScopedAllocations( TEXT( "Frame060_240" ), Frame060_240 );

			// Generate scoped tree view.
			{
				// 1. Compare snapshots.
				TMap<FName, FCombinedAllocationInfo> FrameBegin_End_FName;
				Instance->CompareSnapshots( FirstSnapshotName, LastSnapshotName, FrameBegin_End_FName );

				// 2. Generate tree of allocations.
				FNodeAllocationInfo Root;
				Root.EncodedCallstack = TEXT( "ThreadRoot" );
				Root.HumanReadableCallstack = TEXT( "ThreadRoot" );
				Instance->GenerateScopedTreeAllocations( FrameBegin_End_FName, Root );

				// 3. Display.
			}


			{
				TMap<FName, FCombinedAllocationInfo> Frame060_240_FName;
				Instance->CompareSnapshots( TEXT( "Frame-060" ), TEXT( "Frame-240" ), Frame060_240_FName );

				FNodeAllocationInfo Root;
				Root.EncodedCallstack = TEXT( "ThreadRoot" );
				Root.HumanReadableCallstack = TEXT( "ThreadRoot" );
				Instance->GenerateScopedTreeAllocations( Frame060_240_FName, Root );
			}
#endif // UE_BUILD_DEBUG
		}
	}
}


/*-----------------------------------------------------------------------------
	FRawStatsMemoryProfiler
-----------------------------------------------------------------------------*/

FRawStatsMemoryProfiler* FProfilerModule::OpenRawStatsForMemoryProfiling( const TCHAR* Filename )
{
	FRawStatsMemoryProfiler* Instance = FStatsReader<FRawStatsMemoryProfiler>::Create( Filename );
	if (Instance)
	{
		Instance->ReadAndProcessAsynchronously();
	}
	return Instance;
}



