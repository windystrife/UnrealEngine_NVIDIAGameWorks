// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight single-threaded CSV profiler which can be used for profiling in Test/Shipping builds
*/

#include "CsvProfiler.h"
#include "CoreGlobals.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"
#include "CoreDelegates.h"
#include "RenderCore.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"

#if CSV_PROFILER

#define RECORD_TIMESTAMPS 1 

DEFINE_LOG_CATEGORY_STATIC(LogCsvProfiler, Log, All);

#define USE_THREAD_LOCAL 0

FCsvProfiler* FCsvProfiler::Instance = nullptr;

static void HandleCSVProfileCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return;
	}
	FString Param = Args[0];
	if (Param == TEXT("START"))
	{
		FCsvProfiler::Get()->BeginCapture();
	}
	else if (Param == TEXT("STOP"))
	{
		FCsvProfiler::Get()->EndCapture();
	}
	else
	{
		int32 CaptureFrames = 0;
		if (FParse::Value(*Param, TEXT("FRAMES="), CaptureFrames))
		{
			FCsvProfiler::Get()->BeginCapture(CaptureFrames);
		}
	}
}

static void CsvProfilerBeginFrame()
{
	FCsvProfiler::Get()->BeginFrame();
}

static void CsvProfilerEndFrame()
{
	FCsvProfiler::Get()->EndFrame();
}


static FAutoConsoleCommand HandleCSVProfileCmd(
	TEXT("CsvProfile"),
	TEXT("Starts or stops Csv Profiles"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleCSVProfileCommand)
);

 
//-----------------------------------------------------------------------------
//	TSingleProducerSingleConsumerList : fast lock-free single producer/single 
//  consumer list implementation. 
//  Uses a linked list of blocks for allocations. Note that one block will always 
//  leak, because removing the tail cannot be done without locking
//-----------------------------------------------------------------------------
template <class T, int BlockSize>
class TSingleProducerSingleConsumerList
{
	// A block of BlockSize entries
	struct FBlock
	{
		FBlock() : Next(nullptr)
		{
		}
		T Entries[BlockSize];
		FBlock* Next;
	};

	//MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) 
	struct FCacheLineAlignedCounter
	{
		#define NUM_PADDING_WORDS ( PLATFORM_CACHE_LINE_SIZE/4 - 2 )
		uint32 Pad[NUM_PADDING_WORDS];

		volatile uint64 Value;
	};
	//GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);

public:
	TSingleProducerSingleConsumerList()
	{
		HeadBlock = nullptr;
		TailBlock = nullptr;
#if DO_CHECK
		bElementReserved = false;
#endif
		Counter.Value = 0;
		ConsumerThreadLastReadIndex = 0;
	}

	// Reserve an element prior to writing it
	// Must be called from the Producer thread
	T* ReserveElement()
	{
#if DO_CHECK
		check(!bElementReserved);
		bElementReserved = true;
#endif
		uint32 TailBlockSize = Counter.Value % BlockSize;
		if (TailBlockSize == 0)
		{
			AddTailBlock();
		}
		return &TailBlock->Entries[TailBlockSize];
	}

	// Commit an element after writing it
	// Must be called from the Producer thread after a call to ReserveElement
	void CommitElement()
	{
#if DO_CHECK
		check(bElementReserved);
		bElementReserved = false;
#endif
		FPlatformMisc::MemoryBarrier();
		
		// Keep track of the count of all the elements we ever committed. This value is never reset, even on a PopAll
		Counter.Value++;
	}

	// Called from the consumer thread
	void PopAll(TArray<T>& ElementsOut)
	{
		volatile uint64 CurrentCounterValue = Counter.Value;
		FPlatformMisc::MemoryBarrier();

		uint32 ElementCount = uint32( CurrentCounterValue - ConsumerThreadLastReadIndex );
		ConsumerThreadLastReadIndex = CurrentCounterValue;
		ElementsOut.AddUninitialized(ElementCount);

		uint32 IndexInBlock = 0;
		for (uint32 i = 0; i < ElementCount; i++)
		{
			// if this block is full and it's completed, retire it and move to the next block (update the head)
			if (i > 0 && ( i % BlockSize == 0) )
			{
				// Both threads are done with the head block now, so we can safely delete it 
				// Note that the Producer thread only reads/writes to the HeadBlock pointer on startup, so it's safe to update it at this point
				FBlock* PrevBlock = HeadBlock;
				HeadBlock = HeadBlock->Next;
				IndexInBlock = 0;
				delete PrevBlock;
			}
			check(HeadBlock != nullptr);
			ElementsOut[i] = HeadBlock->Entries[IndexInBlock++];
		}
	}
private:
	void AddTailBlock()
	{
		FBlock* NewTail = new FBlock;
		if (TailBlock == nullptr)
		{
			// This must only happen on startup, otherwise it's not thread-safe
			check(Counter.Value == 0);
			check(HeadBlock == nullptr);
			HeadBlock = NewTail;
		}
		else
		{
			TailBlock->Next = NewTail;
		}
		TailBlock = NewTail;
	}


	FBlock* HeadBlock;
	FBlock* TailBlock;

	FCacheLineAlignedCounter Counter;

	// Used from the consumer thread
	uint64 ConsumerThreadLastReadIndex;

#if DO_CHECK
	bool bElementReserved;
#endif

};

//-----------------------------------------------------------------------------
//	FCsvTimingMarker : records timestamps. Uses StatName pointer as a unique ID
//-----------------------------------------------------------------------------
namespace ECsvMarkerType
{
	enum Type 
	{
		TimestampStart,
		TimestampEnd,
		CustomStat
	};
};

struct FCsvTimingMarker
{
	const char*			 StatName;
	uint64				 Timestamp;
	float				 CustomValue;
	ECsvMarkerType::Type MarkerType;
};

//-----------------------------------------------------------------------------
//	FCsvProfilerThread class : records all timings for a particular thread
//-----------------------------------------------------------------------------
class FCsvProfilerThread
{
public:
	FCsvProfilerThread( uint32 InThreadId ) : ThreadId(InThreadId)
	{
		CurrentCaptureStartCycles = FPlatformTime::Cycles64();
	}

	void FlushResults(TArray<FCsvTimingMarker>& OutMarkers )
	{
		uint64 ValidTimestampStart = CurrentCaptureStartCycles;
		CurrentCaptureStartCycles = FPlatformTime::Cycles64();

		TimingMarkers.PopAll(OutMarkers);
	}

	void AddTimestamp(const char* StatName, const bool bBegin)
	{
		// TODO: fast pool allocator for timing markers
		FCsvTimingMarker* Marker = TimingMarkers.ReserveElement();

		Marker->MarkerType = bBegin ? ECsvMarkerType::TimestampStart : ECsvMarkerType::TimestampEnd;
		Marker->StatName = StatName;
		Marker->CustomValue = 0.0f;
		Marker->Timestamp = FPlatformTime::Cycles64();

		TimingMarkers.CommitElement();
	}

	void AddCustomStat(const char* StatName, const float Value )
	{
		FCsvTimingMarker* Marker = TimingMarkers.ReserveElement();

		Marker->MarkerType = ECsvMarkerType::CustomStat;
		Marker->StatName = StatName;
		Marker->CustomValue = Value;
		Marker->Timestamp = FPlatformTime::Cycles64();

		TimingMarkers.CommitElement();
	}

	uint32 ThreadId;
	uint64 CurrentCaptureStartCycles;
	TSingleProducerSingleConsumerList<FCsvTimingMarker, 128> TimingMarkers;
};


FCsvProfiler* FCsvProfiler::Get()
{
	if (Instance == nullptr)
	{
		Instance = new FCsvProfiler;
	}
	return Instance;
}

FCsvProfiler::FCsvProfiler() 
	: NumFramesToCapture(-1)
	, CaptureFrameNumber(0)
	, bRequestStartCapture(false)
	, bRequestStopCapture(false)
	, bCapturing(false)
{
	check(IsInGameThread());
	GetProfilerThread();

	FCoreDelegates::OnBeginFrame.AddStatic(CsvProfilerBeginFrame);
	FCoreDelegates::OnEndFrame.AddStatic(CsvProfilerEndFrame);

}

/** Per-frame update */
void FCsvProfiler::BeginFrame()
{
	check(IsInGameThread());
	if (bRequestStartCapture)
	{
		bCapturing = true;
		bRequestStartCapture = false;

		UE_LOG(LogCsvProfiler, Display, TEXT("Capture Starting"));

		LastEndFrameTimestamp = FPlatformTime::Cycles64();
	}

	if (bCapturing)
	{
		FrameBeginTimestamps.Add(FPlatformTime::Cycles64());
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND(
		CSVBeginFrame,
		{
			FCsvProfiler::Get()->BeginFrameRT();
		});

}

void FCsvProfiler::EndFrame()
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND(
		CSVEndFrame,
		{
			FCsvProfiler::Get()->EndFrameRT();
		});

	if (!bCapturing)
	{
		return;
	}


	// CSV profiler 
	{
		CSV_CUSTOM_STAT(RenderThreadTime, FPlatformTime::ToMilliseconds(GRenderThreadTime));
		CSV_CUSTOM_STAT(GameThreadTime, FPlatformTime::ToMilliseconds(GGameThreadTime));
		CSV_CUSTOM_STAT(GPUTime, FPlatformTime::ToMilliseconds(GGPUFrameTime));
		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		float PhysicalMBFree = float(MemoryStats.AvailablePhysical / 1024) / 1024.0f;
		CSV_CUSTOM_STAT(MemoryFreeMB, PhysicalMBFree);
	}

	{
		// Record the frametime (measured since the last EndFrame)
		uint64 CurrentTimeStamp = FPlatformTime::Cycles64();
		uint64 ElapsedCycles = CurrentTimeStamp - LastEndFrameTimestamp;
		float ElapsedMs = FPlatformTime::ToMilliseconds64(ElapsedCycles);
		CSV_CUSTOM_STAT(FrameTime, ElapsedMs);
		LastEndFrameTimestamp = CurrentTimeStamp;
	}

	if (NumFramesToCapture >= 0)
	{
		NumFramesToCapture--;
		if (NumFramesToCapture == 0)
		{
			bRequestStopCapture = true;
		}
	}

	if (bRequestStopCapture)
	{
		FScopeLock Lock(&GetResultsLock);

		bCapturing = false;

		// We'd need to move this into engine to do this:
		FlushRenderingCommands();

		// Sleep for a bit to give the threads a chance to finish writing stats
		// Doesn't matter massively if they haven't, since we're using a lock-free queue
		//FPlatformProcess::Sleep(0.002f);

		WriteCaptureToFile();

		FrameBeginTimestamps.Empty();
		FrameBeginTimestampsRT.Empty();

		bRequestStopCapture = false;
	}

	CaptureFrameNumber++;
}


/** Per-frame update */
void FCsvProfiler::BeginFrameRT()
{
	check(IsInRenderingThread());
	if (bCapturing)
	{
		// Mark where the renderthread frames begin
		FScopeLock Lock(&GetResultsLock);
		FrameBeginTimestampsRT.Add(FPlatformTime::Cycles64());
	}
}

void FCsvProfiler::EndFrameRT()
{
	check(IsInRenderingThread());
}


/** Final cleanup */
void FCsvProfiler::Release()
{

}

void FCsvProfiler::BeginCapture(int InNumFramesToCapture)
{
	NumFramesToCapture = InNumFramesToCapture;
	bRequestStartCapture = true;
}

void FCsvProfiler::EndCapture()
{
	bRequestStopCapture = true;
}

class FCsvColumn
{
public:
	FCsvColumn(const FString& InName) : Name(InName) {}
	
	float GetValue(uint32 Row)
	{
		if (Row < (uint32)Values.Num())
		{
			return Values[Row];
		}
		return 0.0f;
	}

	void SetValue(uint32 Row, float Value)
	{
		// Grow the column if needed (pad with zeroes)
		if (Row >= (uint32)Values.Num())
		{
			Values.Reserve((int32)Row + 1);
			int32 GrowBy = (int32)Row + 1 - Values.Num();
			for (int i = 0; i < GrowBy; i++)
			{
				Values.Add(0.0f);
			}
		}
		Values[Row] = Value;
	}

	FString Name;
	TArray<float> Values;
};

class FCSVTable
{
public:
	FCSVTable() : NumRows(0)
	{
	}

	~FCSVTable()
	{
		for (int i = 0; i < Columns.Num(); i++)
		{
			delete Columns[i];
		}
	}

	uint32 AddColumn(const FString& ColumnName)
	{
		Columns.Add(new FCsvColumn(ColumnName));
		return (uint32)Columns.Num() - 1;
	}

	void SetValue(uint32 Row, uint32 Column, float Value)
	{
		if (Column < (uint32)Columns.Num())
		{
			Columns[Column]->SetValue(Row, Value);
		}

		if (Row >= NumRows)
		{
			NumRows = Row + 1;
		}
		
	}

	void AccumulateValue(uint32 Row, uint32 Column, float Value)
	{
		if (Column < (uint32)Columns.Num())
		{
			Columns[Column]->SetValue(Row, Columns[Column]->GetValue(Row) + Value);
		}
		if (Row >= NumRows)
		{
			NumRows = Row + 1;
		}
	}

	void WriteToFile(const FString& FileName)
	{
		char Comma = ',';
		char NewLine = '\n';
		FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*FileName);
		for (int c = 0; c< Columns.Num(); c++)
		{
			FCsvColumn& Column = *Columns[c];
			
			auto AnsiStr = StringCast<ANSICHAR>(*Column.Name);
			OutputFile->Serialize( (void*)AnsiStr.Get(), AnsiStr.Length() );
			if (c < Columns.Num() - 1)
			{
				OutputFile->Serialize( (void*)&Comma, sizeof(ANSICHAR));
			}
		}
		OutputFile->Serialize((void*)&NewLine, sizeof(ANSICHAR));

		// Write the data
		char StringBuffer[256];
		for (int r = 0; r < (int)NumRows; r++)
		{
			for (int c = 0; c < Columns.Num(); c++)
			{
				FCsvColumn& Column = *Columns[c];
				float Value = Column.GetValue(r);
				FCStringAnsi::Snprintf(StringBuffer, 256, "%.4f", Value);
				OutputFile->Serialize((void*)StringBuffer, sizeof(ANSICHAR)*FCStringAnsi::Strlen(StringBuffer));

				if (c < Columns.Num() - 1)
				{
					OutputFile->Serialize((void*)&Comma, sizeof(ANSICHAR));
				}
			}
			OutputFile->Serialize((void*)&NewLine, sizeof(ANSICHAR));
		}

		OutputFile->Close();
	}
private:
	TArray<FCsvColumn*> Columns;
	uint32 NumRows;
};

void FCsvProfiler::WriteCaptureToFile()
{

	// Write the results out to a file
	FString Filename = FString::Printf(TEXT("Profile(%s)"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FString CSVRootPath = FPaths::ProfilingDir() + TEXT("CSV/");
	FString OutputFilename = CSVRootPath + Filename + TEXT(".csv");
	FCSVTable Csv;

	for (int t = 0; t < ProfilerThreads.Num(); t++)
	{
		FCsvProfilerThread* CurrentThread = ProfilerThreads[t];

		// Get the thread name
		FString ThreadName = FThreadManager::Get().GetThreadName(CurrentThread->ThreadId);
		if (CurrentThread->ThreadId == GGameThreadId)
		{
			ThreadName = TEXT("GameThread");
		}
		else if (CurrentThread->ThreadId == GRenderThreadId )
		{
			ThreadName = TEXT("RenderThread");
		}

		// Read the results
		TArray<FCsvTimingMarker> ThreadMarkers;
		CurrentThread->FlushResults(ThreadMarkers);

		TMap<FString, uint32> StatNameToColumnIndex;
		TArray<uint32> MarkerFrameIndices;
		TArray<uint32> MarkerColumnIndices;

		FString Slash(TEXT("/"));

		MarkerFrameIndices.Reserve(ThreadMarkers.Num());
		MarkerColumnIndices.Reserve(ThreadMarkers.Num());

		bool bUseRenderthreadTimeline = CurrentThread->ThreadId == GRenderThreadId;
		const TArray<uint64>& FrameBeginTimestampsForThread = bUseRenderthreadTimeline ? FrameBeginTimestampsRT : FrameBeginTimestamps;

		// Create a column in the CSV for each unique stat name
		//static bool bCreateEndToEndMarkers = false;
		uint32 CurrentFrameIndex = 0;
		for (int i = 0; i < ThreadMarkers.Num(); i++)
		{
			FCsvTimingMarker& Marker = ThreadMarkers[i];
			FString StatName = (TCHAR*)StringCast<TCHAR>((Marker.StatName)).Get();
			uint32 ColumnIndex = 0;
			uint32* IndexPtr = StatNameToColumnIndex.Find(StatName);

			if (IndexPtr)
			{
				ColumnIndex = *IndexPtr;
			}
			else
			{
				FString ColumnName = (Marker.MarkerType == ECsvMarkerType::CustomStat) ? StatName : ThreadName + Slash + StatName;
				ColumnIndex = Csv.AddColumn(ColumnName);
				StatNameToColumnIndex.Add(StatName, ColumnIndex);
#if 0
				if (bCreateEndToEndMarkers && Marker.MarkerType != ECsvMarkerType::CustomStat)
				{
					// Add another column for the end to end markers
					FString ColumnName2 = ThreadName + Slash + TEXT("EndToEnd_") + StatName;
					Csv.AddColumn(ColumnName2);
				}
#endif
			}
			MarkerColumnIndices.Add(ColumnIndex);

			while (CurrentFrameIndex + 1 < (uint32)FrameBeginTimestampsForThread.Num() && Marker.Timestamp > FrameBeginTimestampsForThread[CurrentFrameIndex + 1])
			{
				CurrentFrameIndex++;
			}
			MarkerFrameIndices.Add(CurrentFrameIndex);
		}

		// Process the markers
		TArray<FCsvTimingMarker*> MarkerStack;
		for (int i = 0; i < ThreadMarkers.Num(); i++)
		{
			FCsvTimingMarker& Marker = ThreadMarkers[i];
			uint32 ColumnIndex = MarkerColumnIndices[i];
			uint32 FrameIndex = MarkerFrameIndices[i];

			switch (Marker.MarkerType)
			{
				case ECsvMarkerType::TimestampStart:
				{
					MarkerStack.Push(&Marker);
				}
				break;
				case ECsvMarkerType::TimestampEnd:
				{
					// Markers might not match up if they were truncated mid-frame, so we need to be robust to that
					if (MarkerStack.Num() > 0)
					{
						FCsvTimingMarker* StartMarker = MarkerStack.Pop();
						if (Marker.Timestamp > StartMarker->Timestamp)
						{
							uint64 ElapsedCycles = Marker.Timestamp - StartMarker->Timestamp;
							float ElapsedMs = FPlatformTime::ToMilliseconds64(ElapsedCycles);

							// Add the elapsed time to the table entry for this frame/stat 
							Csv.AccumulateValue(FrameIndex, ColumnIndex, ElapsedMs);
						}
					}
				}
				break;
				case ECsvMarkerType::CustomStat:
				{
					// Add the elapsed time to the table entry for this frame/stat 
					Csv.AccumulateValue(FrameIndex, ColumnIndex, Marker.CustomValue);
				}
				break;
			}

#if 0
			if (bCreateEndToEndMarkers)
			{
				// Array sized by num CSV Columns
				// Track frame of each
				// Process the markers
				TArray<FCsvTimingMarker*> MarkerStack;
				for (int i = 0; i < ThreadMarkers.Num(); i++)
				{
					FCsvTimingMarker& Marker = ThreadMarkers[i];
					uint32 ColumnIndex = MarkerColumnIndices[i]+1;
					uint32 FrameIndex = MarkerFrameIndices[i];

					switch (Marker.MarkerType)
					{
						case ECsvMarkerType::TimestampStart:
						{
							MarkerStack.Push(&Marker);
						}
						break;
						case ECsvMarkerType::TimestampEnd:
						{
							// Markers might not match up if they were truncated mid-frame, so we need to be robust to that
							if (MarkerStack.Num() > 0)
							{
								FCsvTimingMarker* StartMarker = MarkerStack.Pop();
								if (Marker.Timestamp > StartMarker->Timestamp)
								{
									uint64 ElapsedCycles = Marker.Timestamp - StartMarker->Timestamp;
									float ElapsedMs = FPlatformTime::ToMilliseconds64(ElapsedCycles);

									// Add the elapsed time to the table entry for this frame/stat 
									Csv.AccumulateValue(FrameIndex, ColumnIndex, ElapsedMs);
								}
							}
						}
						break;
					}
				}
#endif

		}


	} // Thread loop

	UE_LOG(LogCsvProfiler, Display, TEXT("Capture Ended. Writing CSV to file : %s"), *OutputFilename);

	Csv.WriteToFile(OutputFilename);
}


// Create the TLS profiler thread lazily
FCsvProfilerThread* FCsvProfiler::GetProfilerThread()
{
#if USE_THREAD_LOCAL == 1
	static thread_local FCsvProfilerThread* ProfilerThread = nullptr;
#else 
	static uint32 TlsSlot = FPlatformTLS::AllocTlsSlot();
	FCsvProfilerThread* ProfilerThread = (FCsvProfilerThread*)FPlatformTLS::GetTlsValue(TlsSlot);
#endif
	if (!ProfilerThread)
	{
		ProfilerThread = new FCsvProfilerThread(FPlatformTLS::GetCurrentThreadId());
#if USE_THREAD_LOCAL == 0
		FPlatformTLS::SetTlsValue(TlsSlot, ProfilerThread);
#endif
		{
			FScopeLock Lock(&GetResultsLock);
			ProfilerThreads.Add(ProfilerThread);
		}
	}
	return ProfilerThread;
}

/** Push/pop events */
void FCsvProfiler::BeginStat(const char * StatName)
{
#if RECORD_TIMESTAMPS
	if (!bCapturing)
	{
		return;
	}
	GetProfilerThread()->AddTimestamp(StatName, true);
#endif
}

void FCsvProfiler::EndStat(const char * StatName)
{
#if RECORD_TIMESTAMPS
	if (!bCapturing)
	{
		return;
	}
	GetProfilerThread()->AddTimestamp(StatName, false);
#endif
}

void FCsvProfiler::RecordCustomStat(const char * StatName, float Value)
{
	if (!bCapturing)
	{
		return;
	}
	GetProfilerThread()->AddCustomStat(StatName, Value);
}


void FCsvProfiler::Init()
{
	int32 NumCsvFrames = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("csvCaptureFrames="), NumCsvFrames))
	{
		check(IsInGameThread());
		BeginCapture(NumCsvFrames);
		BeginFrame();
	}
}


#endif // CSV_PROFILER