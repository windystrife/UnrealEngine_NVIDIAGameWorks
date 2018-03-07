// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OodleHandlerComponent.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

#include "OodleTrainerCommandlet.h"

#if !UE_BUILD_SHIPPING
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#endif


DEFINE_LOG_CATEGORY(OodleHandlerComponentLog);


// @todo #JohnB: You're not taking into account, the overhead of sending 'DecompressedLength', in the stats

// @todo #JohnB: The 'bCompressedPacket' bit goes at the very start of the packet.
//					It would be useful if you could reserve a bit at the start of the bit reader, to eliminate some memcpy's


#if HAS_OODLE_SDK
#define OODLE_INI_SECTION TEXT("OodleHandlerComponent")

// @todo #JohnB: Remove after Oodle update, and after checking with Luigi
#define OODLE_DICTIONARY_SLACK 65536 // Refers to bOutDataSlack in OodleArchives.cpp


#if STATS
DEFINE_STAT(STAT_PacketReservedOodle);

DEFINE_STAT(STAT_Oodle_OutRaw);
DEFINE_STAT(STAT_Oodle_OutCompressed);
DEFINE_STAT(STAT_Oodle_OutSavings);
DEFINE_STAT(STAT_Oodle_OutTotalSavings);
DEFINE_STAT(STAT_Oodle_InRaw);
DEFINE_STAT(STAT_Oodle_InCompressed);
DEFINE_STAT(STAT_Oodle_InSavings);
DEFINE_STAT(STAT_Oodle_InTotalSavings);
DEFINE_STAT(STAT_Oodle_CompressFailSavings);
DEFINE_STAT(STAT_Oodle_CompressFailSize);


#if !UE_BUILD_SHIPPING
DEFINE_STAT(STAT_Oodle_InDecompressTime);
DEFINE_STAT(STAT_Oodle_OutCompressTime);
#endif

DEFINE_STAT(STAT_Oodle_DictionaryCount);
DEFINE_STAT(STAT_Oodle_DictionaryBytes);
DEFINE_STAT(STAT_Oodle_SharedBytes);
DEFINE_STAT(STAT_Oodle_StateBytes);
#endif


FString GOodleSaveDir = TEXT("");
FString GOodleContentDir = TEXT("");


typedef TMap<FString, TSharedPtr<FOodleDictionary>> FDictionaryMap;

/** Persistent map of loaded dictionaries */
static FDictionaryMap DictionaryMap;


/** Whether or not Oodle is presently force-enabled */
static bool bOodleForceEnable = false;

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
/** Whether or not compression is presently force-disabled (does not affect decompression i.e. incoming packets, only outgoing) */
static bool bOodleCompressionDisabled = false;

/** Stores a runtime list of active OodleHandlerComponent's - for debugging/testing code */
static TArray<OodleHandlerComponent*> OodleComponentList;
#endif


// @todo #JohnB: Remove after Oodle update, and after checking with Luigi
static rrbool STDCALL UEOodleDisplayAssert(const char* File, const int Line, const char* Function, const char* Message)
{
	UE_LOG(OodleHandlerComponentLog, Log, TEXT("Oodle Assert: File: %s, Line: %i, Function: %s, Message: %s"), UTF8_TO_TCHAR(File),
			Line, UTF8_TO_TCHAR(Function), UTF8_TO_TCHAR(Message));

	// @todo #JohnB: Get a good repro for this, and test it

	return false;
}


/**
 * Serialization functions which allow PacketSize == MAX_OODLE_PACKET_SIZE, by assuming PacketSize is never 0
 */

FORCEINLINE void SerializeOodlePacketSize(FBitWriter& Writer, uint32 PacketSize)
{
	if (PacketSize > 0)
	{
		PacketSize--;

		// @todo #JohnB: Restore when serialize changes are stable
		//Writer.NetSerializeInt<MAX_OODLE_PACKET_BYTES>(PacketSize);

		// @todo #JohnB: Remove when restoring the above
		Writer.SerializeInt(PacketSize, MAX_OODLE_PACKET_BYTES);
	}
	else
	{
		Writer.SetError();

		UE_LOG(OodleHandlerComponentLog, Error, TEXT("Oodle attempted to process zero-size packet."));
	}
}

FORCEINLINE void SerializeOodlePacketSize(FBitReader& Reader, uint32& OutPacketSize)
{
	// @todo #JohnB: Restore when serialize changes are stable
	//Reader.NetSerializeInt<MAX_OODLE_PACKET_BYTES>(OutPacketSize);

	// @todo #JohnB: Remove when restoring the above
	Reader.SerializeInt(OutPacketSize, MAX_OODLE_PACKET_BYTES);

	if (!Reader.IsError())
	{
		OutPacketSize++;
	}
}


#if STATS
/** The global net stats tracker for Oodle */
static FOodleNetStats GOodleNetStats;

/**
 * FOodleNetStats
 */

void FOodleNetStats::UpdateStats(float DeltaTime)
{
	// Input
	const uint32 InRaw = FMath::TruncToInt(InDecompressedLength / DeltaTime);
	const uint32 InCompressed = FMath::TruncToInt(InCompressedLength / DeltaTime);

	SET_DWORD_STAT(STAT_Oodle_InRaw, InRaw);
	SET_DWORD_STAT(STAT_Oodle_InCompressed, InCompressed);

	double InSavings = InCompressedLength > 0 ? ((1.0 - ((double)InCompressedLength/(double)InDecompressedLength)) * 100.0) : 0.0;

	SET_FLOAT_STAT(STAT_Oodle_InSavings, InSavings);


	// Output
	uint32 OutRaw = FMath::TruncToInt(OutUncompressedLength / DeltaTime);
	uint32 OutCompressed = FMath::TruncToInt(OutCompressedLength / DeltaTime);

	SET_DWORD_STAT(STAT_Oodle_OutRaw, OutRaw);
	SET_DWORD_STAT(STAT_Oodle_OutCompressed, OutCompressed);

	double OutSavings = OutCompressedLength > 0 ? ((1.0 - ((double)OutCompressedLength/(double)OutUncompressedLength)) * 100.0) : 0.0;

	SET_FLOAT_STAT(STAT_Oodle_OutSavings, OutSavings);


	// Crude process-lifetime accumulation of all stat savings
	if (TotalInCompressedLength > 0)
	{
		SET_FLOAT_STAT(STAT_Oodle_InTotalSavings, (1.0 - ((double)TotalInCompressedLength/(double)TotalInDecompressedLength)) * 100.0);
	}

	if (TotalOutCompressedLength > 0)
	{
		SET_FLOAT_STAT(STAT_Oodle_OutTotalSavings,
						(1.0 - ((double)TotalOutCompressedLength/(double)TotalOutUncompressedLength)) * 100.0);
	}


	// Reset stats accumulated since last update
	InCompressedLength = 0;
	InDecompressedLength = 0;
	OutCompressedLength = 0;
	OutUncompressedLength = 0;
}

void FOodleNetStats::ResetStats()
{
	InCompressedLength = 0;
	InDecompressedLength = 0;
	OutCompressedLength = 0;
	OutUncompressedLength = 0;
	TotalInCompressedLength = 0;
	TotalInDecompressedLength = 0;
	TotalOutCompressedLength = 0;
	TotalOutUncompressedLength = 0;

	SET_DWORD_STAT(STAT_Oodle_InRaw, 0);
	SET_DWORD_STAT(STAT_Oodle_InCompressed, 0);
	SET_FLOAT_STAT(STAT_Oodle_InSavings, 0.0);
	SET_DWORD_STAT(STAT_Oodle_OutRaw, 0);
	SET_DWORD_STAT(STAT_Oodle_OutCompressed, 0);
	SET_FLOAT_STAT(STAT_Oodle_OutSavings, 0.0);
	SET_FLOAT_STAT(STAT_Oodle_InTotalSavings, 0.0);
	SET_FLOAT_STAT(STAT_Oodle_OutTotalSavings, 0.0);
}
#endif


/**
 * FOodleDictionary
 */

FOodleDictionary::FOodleDictionary(uint32 InHashTableSize, uint8* InDictionaryData, uint32 InDictionarySize,
									OodleNetwork1_Shared* InSharedDictionary, uint32 InSharedDictionarySize,
									OodleNetwork1UDP_State* InInitialCompressorState, uint32 InCompressorStateSize)
	: HashTableSize(InHashTableSize)
	, DictionaryData(InDictionaryData)
	, DictionarySize(InDictionarySize)
	, SharedDictionary(InSharedDictionary)
	, SharedDictionarySize(InSharedDictionarySize)
	, CompressorState(InInitialCompressorState)
	, CompressorStateSize(InCompressorStateSize)
{
#if STATS
	INC_DWORD_STAT(STAT_Oodle_DictionaryCount);
	INC_MEMORY_STAT_BY(STAT_Oodle_DictionaryBytes, (DictionarySize + OODLE_DICTIONARY_SLACK));
	INC_MEMORY_STAT_BY(STAT_Oodle_SharedBytes, SharedDictionarySize);
	INC_MEMORY_STAT_BY(STAT_Oodle_StateBytes, CompressorStateSize);
#endif
}

FOodleDictionary::~FOodleDictionary()
{
#if STATS
	DEC_DWORD_STAT(STAT_Oodle_DictionaryCount);
	DEC_MEMORY_STAT_BY(STAT_Oodle_DictionaryBytes, (DictionarySize + OODLE_DICTIONARY_SLACK));
	DEC_MEMORY_STAT_BY(STAT_Oodle_SharedBytes, SharedDictionarySize);
	DEC_MEMORY_STAT_BY(STAT_Oodle_StateBytes, CompressorStateSize);
#endif

	if (DictionaryData != nullptr)
	{
		delete [] DictionaryData;
	}

	if (SharedDictionary != nullptr)
	{
		FMemory::Free(SharedDictionary);
	}

	if (CompressorState != nullptr)
	{
		FMemory::Free(CompressorState);
	}
}


/**
 * OodleHandlerComponent
 */

OodleHandlerComponent::OodleHandlerComponent()
	: bEnableOodle(false)
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	, InPacketLog(nullptr)
	, OutPacketLog(nullptr)
	, bUseDictionaryIfPresent(false)
	, bCaptureMode(false)
#endif
	, OodleReservedPacketBits(0)
	, ServerDictionary()
	, ClientDictionary()
{
	SetActive(true);

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	OodleComponentList.Add(this);
#endif
}

OodleHandlerComponent::~OodleHandlerComponent()
{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	OodleComponentList.Remove(this);

	FreePacketLogs();
#endif


	FreeDictionary(ServerDictionary);
	FreeDictionary(ClientDictionary);
}

void OodleHandlerComponent::InitFirstRunConfig()
{
	// Check that the OodleHandlerComponent section exists, and if not, init with defaults
	if (!GConfig->DoesSectionExist(OODLE_INI_SECTION, GEngineIni))
	{
		GConfig->SetBool(OODLE_INI_SECTION, TEXT("bEnableOodle"), true, GEngineIni);

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		GConfig->SetBool(OODLE_INI_SECTION, TEXT("bUseDictionaryIfPresent"), false, GEngineIni);
		GConfig->SetString(OODLE_INI_SECTION, TEXT("PacketLogFile"), TEXT("PacketDump"), GEngineIni);
#endif

		GConfig->SetString(OODLE_INI_SECTION, TEXT("ServerDictionary"), TEXT(""), GEngineIni);
		GConfig->SetString(OODLE_INI_SECTION, TEXT("ClientDictionary"), TEXT(""), GEngineIni);

		GConfig->Flush(false);
	}
}

void OodleHandlerComponent::Initialize()
{
	// Reset stats
	SET_DWORD_STAT(STAT_Oodle_CompressFailSavings, 0);
	SET_DWORD_STAT(STAT_Oodle_CompressFailSize, 0);

	InitFirstRunConfig();

	// Class config variables
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bEnableOodle"), bEnableOodle, GEngineIni);

	if (!bEnableOodle && bOodleForceEnable)
	{
		UE_LOG(OodleHandlerComponentLog, Log, TEXT("Force-enabling Oodle from commandline."));
		bEnableOodle = true;
	}

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bUseDictionaryIfPresent"), bUseDictionaryIfPresent, GEngineIni);

	if (!bUseDictionaryIfPresent && bOodleForceEnable)
	{
		UE_LOG(OodleHandlerComponentLog, Log, TEXT("Force-enabling 'bUseDictionaryIfPresent', due to -Oodle on commandline."));
		bUseDictionaryIfPresent = true;
	}
#endif

	if (bEnableOodle)
	{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		bCaptureMode = FParse::Param(FCommandLine::Get(), TEXT("OodleCapturing"));

		if (bCaptureMode)
		{
			int32 CapturePercentage = 100;
			FParse::Value(FCommandLine::Get(), TEXT("CapturePercentage="), CapturePercentage);

			int32 RandNum = FMath::RandRange(0, 100);
			UE_LOG(OodleHandlerComponentLog, Log, TEXT("Enabling Oodle capture mode. Random number is: %d, Capture Percentage is: %d, random number must be less than capture percentage to capture."), RandNum, CapturePercentage);
			if (RandNum <= CapturePercentage)
			{
				InitializePacketLogs();
			}
		}
#endif

		InitializeDictionaries();
	}

	Initialized();
}

void OodleHandlerComponent::InitializeDictionaries()
{
	FString ServerDictionaryPath;
	FString ClientDictionaryPath;
	bool bGotDictionaryPath = false;

#if (!UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING) && !(PLATFORM_PS4 || PLATFORM_XBOXONE)
	if (bUseDictionaryIfPresent)
	{
		bGotDictionaryPath = FindFallbackDictionaries(ServerDictionaryPath, ClientDictionaryPath);
	}
#endif

	if (!bGotDictionaryPath)
	{
		bGotDictionaryPath = GetDictionaryPaths(ServerDictionaryPath, ClientDictionaryPath, false);
	}

	if (bGotDictionaryPath)
	{
		InitializeDictionary(ServerDictionaryPath, ServerDictionary);
		InitializeDictionary(ClientDictionaryPath, ClientDictionary);
	}
	else
	{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		if (bCaptureMode)
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Failed to load Oodle dictionaries. Continuing due to capture mode."));
		}
		else
#endif
		{
			LowLevelFatalError(TEXT("Failed to load Oodle dictionaries."));
		}
	}
}

void OodleHandlerComponent::InitializeDictionary(FString FilePath, TSharedPtr<FOodleDictionary>& OutDictionary)
{
	TSharedPtr<FOodleDictionary>* DictionaryRef = DictionaryMap.Find(FilePath);

	// Load the dictionary, if it's not yet loaded
	if (DictionaryRef == nullptr)
	{
		FArchive* ReadArc = IFileManager::Get().CreateFileReader(*FilePath);

		if (ReadArc != nullptr)
		{
			FOodleDictionaryArchive BoundArc(*ReadArc);

			uint8* DictionaryData = nullptr;
			uint32 DictionaryBytes = 0;
			uint8* CompactCompressorState = nullptr;
			uint32 CompactCompressorStateBytes = 0;

			BoundArc.SerializeHeader();
			BoundArc.SerializeDictionaryAndState(DictionaryData, DictionaryBytes, CompactCompressorState, CompactCompressorStateBytes);

			if (!BoundArc.IsError())
			{
				UE_LOG(OodleHandlerComponentLog, Log, TEXT("Loading dictionary file: %s"), *FilePath);

				// Uncompact the compressor state
				uint32 CompressorStateSize = OodleNetwork1UDP_State_Size();
				OodleNetwork1UDP_State* CompressorState = (OodleNetwork1UDP_State*)FMemory::Malloc(CompressorStateSize);

				OodleNetwork1UDP_State_Uncompact(CompressorState, (OodleNetwork1UDP_StateCompacted*)CompactCompressorState);


				// Create the shared dictionary state
				int32 HashTableSize = BoundArc.Header.HashTableSize.Get();
				uint32 SharedDictionarySize = OodleNetwork1_Shared_Size(HashTableSize);
				OodleNetwork1_Shared* SharedDictionary = (OodleNetwork1_Shared*)FMemory::Malloc(SharedDictionarySize);

				OodleNetwork1_Shared_SetWindow(SharedDictionary, HashTableSize, (void*)DictionaryData, DictionaryBytes);


				// Now add the dictionary data to the map
				FOodleDictionary* NewDictionary = new FOodleDictionary(HashTableSize, DictionaryData, DictionaryBytes, SharedDictionary,
																		SharedDictionarySize, CompressorState, CompressorStateSize);

				DictionaryRef = &DictionaryMap.Add(FilePath, MakeShareable(NewDictionary));


				if (CompactCompressorState != nullptr)
				{
					delete [] CompactCompressorState;
				}
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Error loading dictionary file: %s"), *FilePath);

				if (DictionaryData != nullptr)
				{
					delete [] DictionaryData;
				}

				if (CompactCompressorState != nullptr)
				{
					delete [] CompactCompressorState;
				}
			}


			ReadArc->Close();

			delete ReadArc;
			ReadArc = nullptr;
		}
		else
		{
			LowLevelFatalError(TEXT("Incorrect DictionaryFile Provided"));
		}
	}


	if (DictionaryRef != nullptr)
	{
		OutDictionary = *DictionaryRef;
	}
}

void OodleHandlerComponent::FreeDictionary(TSharedPtr<FOodleDictionary>& InDictionary)
{
	if (InDictionary.IsValid())
	{
		// The dictionary is always referenced within DictionaryMap, so 2 represents last ref within an OodleHandlerComponent
		bool bLastDictionaryRef = InDictionary.GetSharedReferenceCount() == 2;

		if (bLastDictionaryRef)
		{
			for (FDictionaryMap::TIterator It(DictionaryMap); It; ++It)
			{
				if (It.Value() == InDictionary)
				{
					It.RemoveCurrent();
					break;
				}
			}
		}

		InDictionary.Reset();
	}
}

bool OodleHandlerComponent::GetDictionaryPaths(FString& OutServerDictionary, FString& OutClientDictionary, bool bFailFatal/*=true*/)
{
	bool bSuccess = false;

	FString ServerDictionaryPath;
	FString ClientDictionaryPath;

	bSuccess = GConfig->GetString(OODLE_INI_SECTION, TEXT("ServerDictionary"), ServerDictionaryPath, GEngineIni);
	bSuccess = bSuccess && GConfig->GetString(OODLE_INI_SECTION, TEXT("ClientDictionary"), ClientDictionaryPath, GEngineIni);

	if (bSuccess && (ServerDictionaryPath.Len() <= 0 || ClientDictionaryPath.Len() <= 0))
	{
		const TCHAR* Msg = TEXT("Specify both Server/Client dictionaries for Oodle compressor in DefaultEngine.ini")
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
							TEXT(", or run Server and Client with -OodleCapturing and generate a dictionary.")
#endif
							;

		if (bFailFatal)
		{
			LowLevelFatalError(TEXT("%s"), Msg);
		}
		else
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("%s"), Msg);
		}

		bSuccess = false;
	}

	if (bSuccess)
	{
		// Path must be within game directory, e.g: "Content/Oodle/Output.udic" becomes "ShooterGam/Content/Oodle/Output.udic"
		ServerDictionaryPath = FPaths::Combine(*FPaths::ProjectDir(), *ServerDictionaryPath);
		ClientDictionaryPath = FPaths::Combine(*FPaths::ProjectDir(), *ClientDictionaryPath);

		FPaths::CollapseRelativeDirectories(ServerDictionaryPath);
		FPaths::CollapseRelativeDirectories(ClientDictionaryPath);

		FPaths::NormalizeDirectoryName(ServerDictionaryPath);
		FPaths::NormalizeDirectoryName(ClientDictionaryPath);

		// Don't allow directory traversal to escape the game directory
		if (!ServerDictionaryPath.StartsWith(FPaths::ProjectDir()) || !ClientDictionaryPath.StartsWith(FPaths::ProjectDir()))
		{
			const TCHAR* Msg = TEXT("DictionaryFile not allowed to use ../ paths to escape game directory.");

			if (bFailFatal)
			{
				LowLevelFatalError(TEXT("%s"), Msg);
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Warning, TEXT("%s"), Msg);
			}

			bSuccess = false;
		}
	}

	if (bSuccess)
	{
		OutServerDictionary = ServerDictionaryPath;
		OutClientDictionary = ClientDictionaryPath;
	}
	else
	{
		OutServerDictionary = TEXT("");
		OutClientDictionary = TEXT("");
	}


	return bSuccess;
}

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
bool OodleHandlerComponent::FindFallbackDictionaries(FString& OutServerDictionary, FString& OutClientDictionary,
														bool bTestOnly/*=false*/)
{
	bool bSuccess = false;

	OutServerDictionary = TEXT("");
	OutClientDictionary = TEXT("");

	// First test the normal dictionary config paths
	FString DefaultServerDicPath;
	FString DefaultClientDicPath;
	IFileManager& FileMan = IFileManager::Get();

	bSuccess = GetDictionaryPaths(DefaultServerDicPath, DefaultClientDicPath, false);

	bSuccess = bSuccess && FileMan.FileExists(*DefaultServerDicPath);
	bSuccess = bSuccess && FileMan.FileExists(*DefaultClientDicPath);


	if (bSuccess)
	{
		OutServerDictionary = DefaultServerDicPath;
		OutClientDictionary = DefaultClientDicPath;
	}
	// If either of the default dictionaries do not exist, do a more speculative search
	else
	{
		TArray<FString> DictionaryList;
		
		FileMan.FindFilesRecursive(DictionaryList, *FPaths::ProjectDir(), TEXT("*.udic"), true, false);

		if (DictionaryList.Num() > 0)
		{
			// Sort the list alphabetically (case-insensitive)
			DictionaryList.Sort();

			// Very simple matching - anything 'server/output' is a server dictionary, anything 'client/input' is a client dictionary
			int32 FoundServerIdx = DictionaryList.IndexOfByPredicate(
				[](const FString& CurEntry)
				{
					return CurEntry.Contains(TEXT("Server")) || CurEntry.Contains(TEXT("Output"));
				});

			int32 FoundClientIdx = DictionaryList.IndexOfByPredicate(
				[](const FString& CurEntry)
				{
					return CurEntry.Contains(TEXT("Client")) || CurEntry.Contains(TEXT("Input"));
				});

			bSuccess = FoundServerIdx != INDEX_NONE && FoundClientIdx != INDEX_NONE;

			if (!bTestOnly)
			{
				UE_LOG(OodleHandlerComponentLog, Log,
						TEXT("Searched for Oodle dictionary files, and selected the following non-default dictionaries:"));
			}

			if (bSuccess)
			{
				OutServerDictionary = DictionaryList[FoundServerIdx];
				OutClientDictionary = DictionaryList[FoundClientIdx];
			}
			else
			{
				// If all else fails, use any found dictionary, or just use the first listed dictionary, for both client/server
				int32 DicIdx = FMath::Max3(0, FoundServerIdx, FoundClientIdx);

				OutServerDictionary = DictionaryList[DicIdx];
				OutClientDictionary = DictionaryList[DicIdx];

				bSuccess = true;

				if (!bTestOnly)
				{
					UE_LOG(OodleHandlerComponentLog, Log, TEXT("WARNING: Using the same dictionary for both server/client!"));
				}
			}

			if (!bTestOnly)
			{
				UE_LOG(OodleHandlerComponentLog, Log, TEXT("   Server: %s"), *OutServerDictionary);
				UE_LOG(OodleHandlerComponentLog, Log, TEXT("   Client: %s"), *OutClientDictionary);
			}
		}
	}

	return bSuccess;
}

void OodleHandlerComponent::InitializePacketLogs()
{
	// @todo #JohnB: Convert this code so that just one capture file is used for all connections, per session
	//					(could set it up much like the dictionary sharing code)
	//					Downside, is potential for corruption. Lots of files is a bit unwieldy, yet very stable.
	if (bCaptureMode && Handler->Mode == Handler::Mode::Server && InPacketLog == nullptr && OutPacketLog == nullptr)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString ReadOutputLogDirectory = FPaths::Combine(*GOodleSaveDir, TEXT("Server"));
		FString BaseFilename;

		PlatformFile.CreateDirectoryTree(*ReadOutputLogDirectory);
		PlatformFile.CreateDirectoryTree(*FPaths::Combine(*ReadOutputLogDirectory, TEXT("Input")));
		PlatformFile.CreateDirectoryTree(*FPaths::Combine(*ReadOutputLogDirectory, TEXT("Output")));
		GConfig->GetString(OODLE_INI_SECTION, TEXT("PacketLogFile"), BaseFilename, GEngineIni);

		BaseFilename = FPaths::GetBaseFilename(BaseFilename);

		BaseFilename = BaseFilename + TEXT("_") + FApp::GetBranchName() + TEXT("_") +
						*FString::Printf(TEXT("%d"), FEngineVersion::Current().GetChangelist()) + TEXT("_") +
						*FString::Printf(TEXT("%d"), FPlatformProcess::GetCurrentProcessId()) + TEXT("_") +
						FDateTime::Now().ToString();

		FString PreExtInFilePath = FPaths::Combine(*ReadOutputLogDirectory, TEXT("Input"),
													*(BaseFilename + TEXT("_Input")));

		FString PreExtOutFilePath = FPaths::Combine(*ReadOutputLogDirectory, TEXT("Output"),
													*(BaseFilename + TEXT("_Output")));

		FString InPath = PreExtInFilePath + CAPTURE_EXT;
		FString OutPath = PreExtOutFilePath + CAPTURE_EXT;

		// Ensure the In/Out filenames are unique
		for (int32 i=1; PlatformFile.FileExists(*InPath) || PlatformFile.FileExists(*OutPath); i++)
		{
			InPath = PreExtInFilePath + FString::Printf(TEXT("_%i"), i) + CAPTURE_EXT;
			OutPath = PreExtOutFilePath + FString::Printf(TEXT("_%i"), i) + CAPTURE_EXT;
		}

		FArchive* InArc = IFileManager::Get().CreateFileWriter(*InPath);
		FArchive* OutArc = (InArc != nullptr ? IFileManager::Get().CreateFileWriter(*OutPath) : nullptr);

		InPacketLog = (InArc != nullptr ? new FPacketCaptureArchive(*InArc) : nullptr);
		OutPacketLog = (OutArc != nullptr ? new FPacketCaptureArchive(*OutArc) : nullptr);


		if (InPacketLog != nullptr && OutPacketLog != nullptr)
		{
			InPacketLog->SerializeCaptureHeader();
			OutPacketLog->SerializeCaptureHeader();
		}
		else
		{
			LowLevelFatalError(TEXT("Failed to create files '%s' and '%s'"), *InPath, *OutPath);
		}
	}
}

void OodleHandlerComponent::FreePacketLogs()
{
	if (OutPacketLog != nullptr)
	{
		OutPacketLog->Close();
		OutPacketLog->DeleteInnerArchive();

		delete OutPacketLog;

		OutPacketLog = nullptr;
	}

	if (InPacketLog != nullptr)
	{
		InPacketLog->Close();
		InPacketLog->DeleteInnerArchive();

		delete InPacketLog;

		InPacketLog = nullptr;
	}
}
#endif

bool OodleHandlerComponent::IsValid() const
{
	return true;
}

void OodleHandlerComponent::Incoming(FBitReader& Packet)
{
#if !UE_BUILD_SHIPPING
	// Oodle must be the first HandlerComponent to process incoming packets, so does not support bit-shifted reads
	check(Packet.GetPosBits() == 0);
#endif

	if (bEnableOodle)
	{
		uint8 bCompressedPacket = Packet.ReadBit();

		// If the packet is not compressed, no further processing is necessary
		if (bCompressedPacket)
		{
			FOodleDictionary* CurDict = ((Handler->Mode == Handler::Mode::Server) ? ClientDictionary.Get() : ServerDictionary.Get());

			if (CurDict != nullptr)
			{
				uint32 DecompressedLength;

				SerializeOodlePacketSize(Packet, DecompressedLength);

#if !UE_BUILD_SHIPPING
				// Never allow DecompressedLength values bigger than this, due to performance/security considerations
				check(MAX_OODLE_PACKET_BYTES <= 16384);
#endif

				if (DecompressedLength < MAX_OODLE_PACKET_BYTES)
				{
					static uint8 CompressedData[MAX_OODLE_BUFFER];
					static uint8 DecompressedData[MAX_OODLE_BUFFER];

					const int32 CompressedLength = Packet.GetBytesLeft();

					Packet.Serialize(CompressedData, CompressedLength);

					bool bSuccess = !Packet.IsError();

					if (bSuccess)
					{
						{
#if STATS && !UE_BUILD_SHIPPING
							SCOPE_CYCLE_COUNTER(STAT_Oodle_InDecompressTime);
#endif

							bSuccess = !!OodleNetwork1UDP_Decode(CurDict->CompressorState, CurDict->SharedDictionary, CompressedData,
															CompressedLength, DecompressedData, DecompressedLength);
						}


						if (!bSuccess)
						{
#if !UE_BUILD_SHIPPING
							UE_LOG(OodleHandlerComponentLog, Error, TEXT("Error decoding Oodle network data."));
#endif

							// Packets which fail to compress are detected before send, and bCompressedPacket is disabled;
							// failed Oodle decodes are not used to detect this anymore, so this now represents an error.
							Packet.SetError();
						}
					}
					else
					{
#if !UE_BUILD_SHIPPING
						UE_LOG(OodleHandlerComponentLog, Error, TEXT("Error serializing received packet data"));
#endif

						Packet.SetError();
					}

					if (bSuccess)
					{
						// @todo #JohnB: Decompress directly into FBitReader's buffer
						FBitReader UnCompressedPacket(DecompressedData, DecompressedLength * 8);
						Packet = UnCompressedPacket;

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
						if (bCaptureMode && Handler->Mode == Handler::Mode::Server && InPacketLog != nullptr)
						{
							InPacketLog->SerializePacket((void*)Packet.GetData(), DecompressedLength);
						}
#endif

#if STATS
						GOodleNetStats.IncomingStats(CompressedLength, DecompressedLength);
#endif
					}
					else
					{
						Packet.SetError();
					}
				}
				else
				{
#if !UE_BUILD_SHIPPING
					UE_LOG(OodleHandlerComponentLog, Error,
							TEXT("Received packet with DecompressedLength (%i) >= MAX_OODLE_PACKET_SIZE"), DecompressedLength);
#endif

					Packet.SetError();
				}
			}
			else
			{
				LowLevelFatalError(TEXT("Received compressed packet, but no dictionary is present for decompression."));

				Packet.SetError();
			}
		}
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		else if (bCaptureMode && Handler->Mode == Handler::Mode::Server && InPacketLog != nullptr)
		{
			uint32 SizeOfPacket = Packet.GetBytesLeft();

			if (SizeOfPacket > 0)
			{
				InPacketLog->SerializePacket((void*)Packet.GetData(), SizeOfPacket);
			}
		}
#endif
	}
}

void OodleHandlerComponent::Outgoing(FBitWriter& Packet)
{
	if (bEnableOodle)
	{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		if (bCaptureMode && Handler->Mode == Handler::Mode::Server && OutPacketLog != nullptr)
		{
			uint32 SizeOfPacket = Packet.GetNumBytes();

			if (SizeOfPacket > 0)
			{
				OutPacketLog->SerializePacket((void*)Packet.GetData(), SizeOfPacket);
			}
		}
#endif

		// @todo #JohnB: You should be able to share the same two buffers between Incoming and Outgoing, as an optimization
		static uint8 UncompressedData[MAX_OODLE_BUFFER];
		static uint8 CompressedData[MAX_OODLE_BUFFER];

		FOodleDictionary* CurDict = ((Handler->Mode == Handler::Mode::Server) ? ServerDictionary.Get() : ClientDictionary.Get());

#if UE_BUILD_SHIPPING
		if (CurDict != nullptr)
#else
		if (CurDict != nullptr && !bOodleCompressionDisabled)
#endif
		{
#if !UE_BUILD_SHIPPING
			check(MaxOutgoingBits <= (MAX_OODLE_PACKET_BYTES * 8));
#endif
			uint32 MaxAdjustedLengthBits = MaxOutgoingBits - OodleReservedPacketBits;
			uint32 UncompressedBits = Packet.GetNumBits();
			uint32 UncompressedBytes = Packet.GetNumBytes();

			bool bWithinBitBounds = UncompressedBits > 0 && ensure(UncompressedBits <= MaxAdjustedLengthBits) &&
									ensure(OodleLZ_GetCompressedBufferSizeNeeded(UncompressedBytes) <= MAX_OODLE_BUFFER);

			if (bWithinBitBounds)
			{
				SINTa CompressedLengthSINT = 0;

				// @todo #JohnB: Redundant memcpy?
				FMemory::Memcpy(UncompressedData, Packet.GetData(), UncompressedBytes);
					
				{
#if STATS && !UE_BUILD_SHIPPING
					SCOPE_CYCLE_COUNTER(STAT_Oodle_OutCompressTime);
#endif

					CompressedLengthSINT = OodleNetwork1UDP_Encode(CurDict->CompressorState, CurDict->SharedDictionary,
																		UncompressedData, UncompressedBytes, CompressedData);
				}

				uint32 CompressedBytes = static_cast<uint32>(CompressedLengthSINT);

				if (CompressedBytes <= UncompressedBytes)
				{
					// It's possible for the packet to be within bit bounds, but to overstep bounds when rounded-up to nearest byte,
					// after processing by Oodle.
					// If this happens, the packet will fail to fit if Oodle failed to compress enough - so will be sent uncompressed.
					bWithinBitBounds = (CompressedBytes * 8) <= MaxAdjustedLengthBits;


					// Don't write the compressed data, if it's not within bit bounds, or compression failed to provide savings
					uint8 bCompressedPacket = bWithinBitBounds && (CompressedBytes < UncompressedBytes);

					Packet.Reset();
					Packet.WriteBit(bCompressedPacket);

					if (bCompressedPacket)
					{
						// @todo #JohnB: Compress directly into a (deliberately oversized) FBitWriter buffer, which you can shrink after
						//					(need to be careful of bit order)

						SerializeOodlePacketSize(Packet, UncompressedBytes);

						Packet.Serialize(CompressedData, CompressedBytes);

#if STATS
						GOodleNetStats.OutgoingStats(CompressedBytes, UncompressedBytes);
#endif
					}
					else
					{
						// @todo #JohnB: Try to eliminate this appBitscpy, by reserving the bCompressedPacket bit and other data,
						//					at the start of the bit writer
						Packet.SerializeBits(UncompressedData, UncompressedBits);

#if STATS
						if (!bWithinBitBounds)
						{
							INC_DWORD_STAT(STAT_Oodle_CompressFailSize);
						}
						else if (CompressedBytes >= UncompressedBytes)
						{
							GOodleNetStats.OutgoingStats(UncompressedBytes, UncompressedBytes);

							INC_DWORD_STAT(STAT_Oodle_CompressFailSavings);
						}
						else
						{
							// @todo #JohnB
						}
#endif
					}
				}
				else
				{
					UE_LOG(OodleHandlerComponentLog, Error, TEXT("Compressed packet larger than uncompressed packet! (%u vs %u)"),
							CompressedBytes, UncompressedBytes);

					Packet.Reset();
					Packet.SetError();
				}
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Error, TEXT("Failed to compress packet of size: %u bytes (%u bits)"),
						UncompressedBytes, UncompressedBits);

				Packet.Reset();
				Packet.SetError();
			}
		}
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		// Allow a lack of dictionary in capture mode, or when compression is disabled
		else if ((CurDict == nullptr && bCaptureMode) || bOodleCompressionDisabled)
		{
			uint8 bCompresedPacket = false;
			uint32 UncompressedBits = Packet.GetNumBits();

			FMemory::Memcpy(UncompressedData, Packet.GetData(), Packet.GetNumBytes());

			Packet.Reset();
			Packet.WriteBit(bCompresedPacket);
			Packet.SerializeBits(UncompressedData, UncompressedBits);
		}
#endif
		else
		{
			LowLevelFatalError(TEXT("Tried to compress a packet, but no dictionary is present for compression."));

			Packet.Reset();
			Packet.SetError();
		}
	}
}

int32 OodleHandlerComponent::GetReservedPacketBits()
{
	int32 ReturnVal = 0;

	if (bEnableOodle)
	{
		if (OodleReservedPacketBits == 0)
		{
			// Add a bit for bCompressedPacket
			OodleReservedPacketBits += 1;

			// Oodle writes the decompressed packet size, as its addition to the protocol - it writes using SerializeInt however,
			// so determine the worst case number of packed bits that will be written, based on the MAX_OODLE_PACKET_SIZE packet limit.
			FBitWriter MeasureAr(0, true);
			uint32 MaxOodlePacket = MAX_OODLE_PACKET_BYTES;

			SerializeOodlePacketSize(MeasureAr, MaxOodlePacket);

			if (!MeasureAr.IsError())
			{
				OodleReservedPacketBits += MeasureAr.GetNumBits();

#if !UE_BUILD_SHIPPING
				SET_DWORD_STAT(STAT_PacketReservedOodle, OodleReservedPacketBits);
#endif
			}
			else
			{
				LowLevelFatalError(TEXT("Failed to determine OodleHandlerComponent reserved packet bits."));
			}
		}

		ReturnVal += OodleReservedPacketBits;
	}

	return ReturnVal;
}


#if !UE_BUILD_SHIPPING
/**
 * Exec Interface
 */
static bool OodleExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bReturnVal = false;

	if (FParse::Command(&Cmd, TEXT("Oodle")))
	{
		// Used by unit testing code, to enable/disable Oodle during a unit test
		// NOTE: Do not use while a NetConnection is using Oodle, as this will cause it to break. Debugging/Testing only.
		if (FParse::Command(&Cmd, TEXT("ForceEnable")))
		{
			bool bTurnOn = false;

			if (FParse::Command(&Cmd, TEXT("On")))
			{
				bTurnOn = true;
			}
			else if (FParse::Command(&Cmd, TEXT("Off")))
			{
				bTurnOn = false;
			}
			else if (FParse::Command(&Cmd, TEXT("Default")))
			{
				bTurnOn = FParse::Param(FCommandLine::Get(), TEXT("Oodle"));
			}
			else
			{
				bTurnOn = !bOodleForceEnable;
			}

			if (bTurnOn != bOodleForceEnable)
			{
				bOodleForceEnable = bTurnOn;

				if (bOodleForceEnable)
				{
					UOodleTrainerCommandlet::HandleEnable();
				}
			}
		}
		// Used for enabling/disabling compression of outgoing packets (does not affect decompression of incoming packets)
		else if (FParse::Command(&Cmd, TEXT("Compression")))
		{
			bool bTurnOff = false;

			if (FParse::Command(&Cmd, TEXT("On")))
			{
				bOodleCompressionDisabled = false;
			}
			else if (FParse::Command(&Cmd, TEXT("Off")))
			{
				bOodleCompressionDisabled = true;
			}
			else
			{
				bOodleCompressionDisabled = !bOodleCompressionDisabled;
			}

			if (bOodleCompressionDisabled)
			{
				Ar.Logf(TEXT("Oodle compression disabled (packets will still be decompressed, just not compressed on send)."));
			}
			else
			{
				Ar.Logf(TEXT("Oodle compression re-enabled."));
			}


			// Automatically execute the same command on the server, if the 'admin' command is likely present
			bool bSendServerCommand = FModuleManager::Get().IsModuleLoaded(TEXT("NetcodeUnitTest"));

			bSendServerCommand = bSendServerCommand && OodleComponentList.Num() > 0 &&
									OodleComponentList[0]->Handler->Mode == Handler::Mode::Client;

			if (bSendServerCommand)
			{
				FString ServerCmd = TEXT("Admin Oodle Compression ");

				ServerCmd += (bOodleCompressionDisabled ? TEXT("Off") : TEXT("On"));

				Ar.Logf(TEXT("Sending command '%s' to server."), *ServerCmd);

				GEngine->Exec(nullptr, *ServerCmd, Ar);
			}
		}
		/**
		 * Used to unload/load dictionaries at runtime - particularly, for generating and loading new dictionaries at runtime.
		 *
		 *
		 * The proper sequence of commands/events for generating/loading a dictionary at runtime (NOTE: Enable NetcodeUnitTest plugin):
		 *	*Start and connect client/server with -OodleCapturing*
		 *	"Oodle Compression Off"
		 *	"Oodle Dictionary Unload"
		 *
		 *	*Run the trainer commandlet to generate a new dictionary*
		 *
		 *	"Oodle Dictionary Load"
		 *	"Oodle Compression On"
		 *
		 * The wrong sequence of commands above, will result in an assert/crash.
		 * Do not execute the commands simultaneously, or there will be an assert/crash (a few milliseconds delay is needed).
		 */
		else if (FParse::Command(&Cmd, TEXT("Dictionary")))
		{
			bool bLoadDic = false;
			bool bValidCmd = true;

			if (FParse::Command(&Cmd, TEXT("Load")))
			{
				bLoadDic = true;
			}
			else if (FParse::Command(&Cmd, TEXT("Unload")))
			{
				bLoadDic = false;

				if (!bOodleCompressionDisabled)
				{
					Ar.Logf(TEXT("Can't unload dictionaries unless compression is disabled. Use 'Oodle Compression Off'"));

					bValidCmd = false;
				}
			}


			if (bValidCmd)
			{
				if (bLoadDic)
				{
					// Reset the stats before loading
					GEngine->Exec(nullptr, TEXT("Oodle ResetStats"), Ar);

					Ar.Logf(TEXT("Loading Oodle dictionaries (has no effect, if they have not been unloaded prior to this)."));
				}
				else
				{
					Ar.Logf(TEXT("Unloading Oodle dictionaries."));
				}


				for (OodleHandlerComponent* CurComp : OodleComponentList)
				{
					if (CurComp != nullptr)
					{
						if (bLoadDic)
						{
							if (!CurComp->ServerDictionary.IsValid() && !CurComp->ClientDictionary.IsValid())
							{
								CurComp->InitializeDictionaries();
							}
							else
							{
								Ar.Logf(TEXT("An OodleHandlerComponent already had loaded dictionaries."));
							}
						}
						else
						{
							if (CurComp->ServerDictionary.IsValid())
							{
								CurComp->FreeDictionary(CurComp->ServerDictionary);
							}

							if (CurComp->ClientDictionary.IsValid())
							{
								CurComp->FreeDictionary(CurComp->ClientDictionary);
							}
						}
					}
				}


				// Automatically execute the same command on the server, if the 'admin' command is likely present
				bool bSendServerCommand = FModuleManager::Get().IsModuleLoaded(TEXT("NetcodeUnitTest"));

				bSendServerCommand = bSendServerCommand && OodleComponentList.Num() > 0 &&
										OodleComponentList[0]->Handler->Mode == Handler::Mode::Client;

				if (bSendServerCommand)
				{
					FString ServerCmd = TEXT("Admin Oodle Dictionary ");

					ServerCmd += (bLoadDic ? TEXT("Load") : TEXT("Unload"));

					Ar.Logf(TEXT("Sending command '%s' to server."), *ServerCmd);

					GEngine->Exec(nullptr, *ServerCmd, Ar);


					// Also automatically disable packet capturing, to free the capture files for dictionary generation
					Ar.Logf(TEXT("Disabling packet capturing serverside (to allow dictionary generation)."));

					GEngine->Exec(nullptr, TEXT("Admin Oodle Capture Off"), Ar);
				}
			}
		}
#if STATS
		// Resets most Oodle stats, relevant to evaluating dictionary performance
		else if (FParse::Command(&Cmd, TEXT("ResetStats")))
		{
			Ar.Logf(TEXT("Resetting Oodle stats."));

			GOodleNetStats.ResetStats();

			SET_DWORD_STAT(STAT_Oodle_CompressFailSavings, 0);
			SET_DWORD_STAT(STAT_Oodle_CompressFailSize, 0);
		}
#endif
		else if (FParse::Command(&Cmd, TEXT("Capture")))
		{
			bool bDoCapture = false;

			if (FParse::Command(&Cmd, TEXT("On")))
			{
				bDoCapture = true;
			}
			else if (FParse::Command(&Cmd, TEXT("Off")))
			{
				bDoCapture = false;
			}


			if (bDoCapture)
			{
				Ar.Logf(TEXT("Enabling Oodle capturing."));
			}
			else
			{
				Ar.Logf(TEXT("Disabling Oodle capturing"));
			}


			for (OodleHandlerComponent* CurComp : OodleComponentList)
			{
				if (CurComp != nullptr)
				{
					if (bDoCapture)
					{
						CurComp->InitializePacketLogs();
					}
					else
					{
						CurComp->FreePacketLogs();
					}
				}
			}
		}
		else
		{
			Ar.Logf(TEXT("Unknown Oodle command 'Oodle %s'"), Cmd);
		}

		bReturnVal = true;
	}

	return bReturnVal;
}

FStaticSelfRegisteringExec OodleExecRegistration(&OodleExec);
#endif


/**
 * Module Interface
 */

TSharedPtr<HandlerComponent> FOodleComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new OodleHandlerComponent);
}

void FOodleComponentModuleInterface::StartupModule()
{
	// If Oodle is force-enabled on the commandline, execute the commandlet-enable command, which also adds to the PacketHandler list
	bOodleForceEnable = FParse::Param(FCommandLine::Get(), TEXT("Oodle"));

	if (bOodleForceEnable)
	{
		UOodleTrainerCommandlet::HandleEnable();
	}


	// Use an absolute path for this, as we want all relative paths, to be relative to this folder
	GOodleSaveDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("Oodle")));
	GOodleContentDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::ProjectContentDir(), TEXT("Oodle")));

#if PLATFORM_WINDOWS
	{
		// Load the Oodle library (NOTE: Path and fallback path mirrored in Oodle.Build.cs)
		FString OodleBinaryPath = FPaths::ProjectDir() / TEXT( "Binaries/ThirdParty/Oodle/" );
		FString OodleBinaryFile = TEXT( "oo2core_1" );

	#if PLATFORM_64BITS
		OodleBinaryPath += TEXT("Win64/");
		OodleBinaryFile += TEXT("_win64.dll");
	#else
		OodleBinaryPath += TEXT("Win32/");
		OodleBinaryFile += TEXT("_win32.dll");
	#endif

		FPlatformProcess::PushDllDirectory(*OodleBinaryPath);

		OodleDllHandle = FPlatformProcess::GetDllHandle(*(OodleBinaryPath + OodleBinaryFile));

		FPlatformProcess::PopDllDirectory(*OodleBinaryPath);

		if (OodleDllHandle == nullptr)
		{
			UE_LOG(OodleHandlerComponentLog, Fatal, TEXT("Could not find Oodle .dll's in path: %s" ),
					*(OodleBinaryPath + OodleBinaryFile));
		}
	}
#endif


	// @todo #JohnB: Remove after Oodle update, and after checking with Luigi
	OodlePlugins_SetAssertion(&UEOodleDisplayAssert);
}

void FOodleComponentModuleInterface::ShutdownModule()
{
	// @todo #JohnB: Remove after Oodle update, and after checking with Luigi
	OodlePlugins_SetAssertion(nullptr);

	if (OodleDllHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(OodleDllHandle);
		OodleDllHandle = nullptr;
	}
}
#else
TSharedPtr<HandlerComponent> FOodleComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	UE_LOG(OodleHandlerComponentLog, Error, TEXT("Can't create OodleHandlerComponent instance - HAS_OODLE_SDK is false."));

	return TSharedPtr<HandlerComponent>(nullptr);
}

void FOodleComponentModuleInterface::StartupModule()
{
}

void FOodleComponentModuleInterface::ShutdownModule()
{
}
#endif

IMPLEMENT_MODULE( FOodleComponentModuleInterface, OodleHandlerComponent );
