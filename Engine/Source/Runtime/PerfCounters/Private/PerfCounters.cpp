// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerfCounters.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/RunnableThread.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonWriter.h"
#include "ZeroLoad.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"

#define JSON_ARRAY_NAME					TEXT("PerfCounters")
#define JSON_PERFCOUNTER_NAME			TEXT("Name")
#define JSON_PERFCOUNTER_SIZE_IN_BYTES	TEXT("SizeInBytes")

#define PERF_COUNTER_CONNECTION_TIMEOUT 5.0f

FPerfCounters::FPerfCounters(const FString& InUniqueInstanceId)
	: SocketSubsystem(nullptr)
	, UniqueInstanceId(InUniqueInstanceId)
	, InternalCountersUpdateInterval(60)
	, Socket(nullptr)
	, ZeroLoadThread(nullptr)
	, ZeroLoadRunnable(nullptr)
{
}

FPerfCounters::~FPerfCounters()
{
	if (Socket)
	{
		ISocketSubsystem* SocketSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSystem)
		{
			SocketSystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}
}

bool FPerfCounters::Initialize()
{
	float ConfigInternalCountersUpdateInterval = 60.0;
	if (GConfig->GetFloat(TEXT("PerfCounters"), TEXT("InternalCountersUpdateInterval"), ConfigInternalCountersUpdateInterval, GEngineIni))
	{
		InternalCountersUpdateInterval = ConfigInternalCountersUpdateInterval;
	}
	LastTimeInternalCountersUpdated = FPlatformTime::Seconds() - InternalCountersUpdateInterval * FMath::FRand();	// randomize between servers

	// get the requested port from the command line (if specified)
	int32 StatsPort = -1;
	FParse::Value(FCommandLine::Get(), TEXT("statsPort="), StatsPort);
	if (StatsPort < 0)
	{
		UE_LOG(LogPerfCounters, Log, TEXT("FPerfCounters JSON socket disabled."));
		return true;
	}

	// get the socket subsystem
	SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to get socket subsystem"));
		return false;
	}

	ScratchIPAddr = SocketSubsystem->CreateInternetAddr();

	// make our listen socket
	Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("FPerfCounters"));
	if (Socket == nullptr)
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to allocate stream socket"));
		return false;
	}

	// make us non blocking
	Socket->SetNonBlocking(true);

	// create a localhost binding for the requested port
	TSharedRef<FInternetAddr> LocalhostAddr = SocketSubsystem->CreateInternetAddr(0x7f000001 /* 127.0.0.1 */, StatsPort);
	if (!Socket->Bind(*LocalhostAddr))
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to bind to %s"), *LocalhostAddr->ToString(true));
		return false;
	}
	StatsPort = Socket->GetPortNo();

	// log the port
	UE_LOG(LogPerfCounters, Display, TEXT("FPerfCounters listening on port %d"), StatsPort);

	// for now, jack this up so we can send in one go
	int32 NewSize;
	Socket->SetSendBufferSize(512 * 1024, NewSize); // best effort 512k buffer to avoid not being able to send in one go

	// listen on the port
	if (!Socket->Listen(16))
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to listen on socket"));
		return false;
	}

	return true;
}

FString FPerfCounters::GetAllCountersAsJson()
{
	FString JsonStr;
	TSharedRef< TJsonWriter<> > Json = TJsonWriterFactory<>::Create(&JsonStr);
	Json->WriteObjectStart();
	for (const auto& It : PerfCounterMap)
	{
		const FJsonVariant& JsonValue = It.Value;
		switch (JsonValue.Format)
		{
		case FJsonVariant::String:
			Json->WriteValue(It.Key, JsonValue.StringValue);
			break;
		case FJsonVariant::Number:
			Json->WriteValue(It.Key, JsonValue.NumberValue);
			break;
		case FJsonVariant::Callback:
			if (JsonValue.CallbackValue.IsBound())
			{
				Json->WriteIdentifierPrefix(It.Key);
				JsonValue.CallbackValue.Execute(Json);
			}
			else
			{
				// write an explicit null since the callback is unbound and the implication is this would have been an object
				Json->WriteNull(It.Key);
			}
			break;
		case FJsonVariant::Null:
		default:
			// don't write anything since wash may expect a scalar
			break;
		}
	}
	Json->WriteObjectEnd();
	Json->Close();
	return JsonStr;
}

void FPerfCounters::ResetStatsForNextPeriod()
{
	UE_LOG(LogPerfCounters, Verbose, TEXT("Clearing perf counters."));
	for (TMap<FString, FJsonVariant>::TIterator It(PerfCounterMap); It; ++It)
	{
		if (It.Value().Flags & IPerfCounters::Flags::Transient)
		{
			UE_LOG(LogPerfCounters, Verbose, TEXT("  Removed '%s'"), *It.Key());
			It.RemoveCurrent();
		}
	}
};

static bool SendAsUtf8(FSocket* Conn, const FString& Message)
{
	FTCHARToUTF8 ConvertToUtf8(*Message);
	int32 BytesSent = 0;
	const bool bSendSuccess = Conn->Send(reinterpret_cast<const uint8*>(ConvertToUtf8.Get()), ConvertToUtf8.Length(), BytesSent);
	const bool bSendSizeSuccess = (BytesSent == ConvertToUtf8.Length());

	if (!bSendSuccess)
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Failed to send buffer size: %d"), ConvertToUtf8.Length());
	}
	else if (!bSendSizeSuccess)
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Failed to send entire buffer size: %d sent: %d"), ConvertToUtf8.Length(), BytesSent);
	}

	return bSendSuccess && bSendSizeSuccess;
}

bool FPerfCounters::Tick(float DeltaTime)
{
	if (LIKELY(Socket != nullptr))
	{
		TickSocket(DeltaTime);
	}

	if (LIKELY(ZeroLoadThread != nullptr))
	{
		TickZeroLoad(DeltaTime);
	}

	TickSystemCounters(DeltaTime);

	// keep ticking
	return true;
}

void FPerfCounters::TickZeroLoad(float DeltaTime)
{
	checkf(ZeroLoadThread != nullptr, TEXT("FPerfCounters::TickZeroThread() called without a valid socket!"));

	TArray<FString> LogMessages;
	if (LIKELY(ZeroLoadThread->GetHitchMessages(LogMessages)))
	{
		for (const FString& LogMessage : LogMessages)
		{
			UE_LOG(LogPerfCounters, Warning, TEXT("%s"), *LogMessage);
		}
	}
}

void FPerfCounters::TickSocket(float DeltaTime)
{
	checkf(Socket != nullptr, TEXT("FPerfCounters::TickSocket() called without a valid socket!"));

	// accept any connections
	static const FString PerfCounterRequest = TEXT("FPerfCounters Request");
	FSocket* IncomingConnection = Socket->Accept(PerfCounterRequest);
	if (IncomingConnection)
	{
		const bool bLogConnections = true;
		if (bLogConnections)
		{
			IncomingConnection->GetPeerAddress(*ScratchIPAddr);
			UE_LOG(LogPerfCounters, Log, TEXT("New connection from %s"), *ScratchIPAddr->ToString(true));
		}

		// make sure this is non-blocking
		IncomingConnection->SetNonBlocking(true);

		new (Connections) FPerfConnection(IncomingConnection);
	}
	else
	{
		ESocketErrors ErrorCode = SocketSubsystem->GetLastErrorCode();
		if (ErrorCode != SE_EWOULDBLOCK &&
			ErrorCode != SE_NO_ERROR)
		{
			const TCHAR* ErrorStr = SocketSubsystem->GetSocketError();
			UE_LOG(LogPerfCounters, Warning, TEXT("Error accepting connection [%d] %s"), (int32)ErrorCode, ErrorStr);
		}
	}

	TArray<FPerfConnection> ConnectionsToClose;
	for (FPerfConnection& Connection : Connections)
	{
		FSocket* ExistingSocket = Connection.Connection;
		if (ExistingSocket && ExistingSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::Zero()))
		{
			// read any data that's ready
			// NOTE: this is not a full HTTP implementation, just enough to be usable by curl
			uint8 Buffer[2 * 1024] = { 0 };
			int32 DataLen = 0;
			if (ExistingSocket->Recv(Buffer, sizeof(Buffer) - 1, DataLen, ESocketReceiveFlags::None))
			{
				double StartTime = FPlatformTime::Seconds();

				FResponse Response;
				if (ProcessRequest(Buffer, DataLen, Response))
				{
					if (!EHttpResponseCodes::IsOk(Response.Code))
					{
						UE_LOG(LogPerfCounters, Warning, TEXT("Sending error response: [%d] %s"), Response.Code, *Response.Body);
					}

					if (SendAsUtf8(ExistingSocket, Response.Header))
					{
						if (!SendAsUtf8(ExistingSocket, Response.Body))
						{
							UE_LOG(LogPerfCounters, Warning, TEXT("Unable to send full HTTP response body size: %d"), Response.Body.Len());
						}
					}
					else
					{
						UE_LOG(LogPerfCounters, Warning, TEXT("Unable to send HTTP response header: %s"), *Response.Header);
					}
				}
				else
				{
					UE_LOG(LogPerfCounters, Warning, TEXT("Failed to process request"));
				}

				double EndTime = FPlatformTime::Seconds();
				ExistingSocket->GetPeerAddress(*ScratchIPAddr);
				UE_LOG(LogPerfCounters, Log, TEXT("Request for %s processed in %0.2f s"), *ScratchIPAddr->ToString(true), EndTime - StartTime);
			}
			else
			{
				UE_LOG(LogPerfCounters, Warning, TEXT("Unable to immediately receive request header"));
			}

			ConnectionsToClose.Add(Connection);
		}
		else if (Connection.ElapsedTime > PERF_COUNTER_CONNECTION_TIMEOUT)
		{
			UE_LOG(LogPerfCounters, Warning, TEXT("Closing connection due to timeout %d"), Connection.ElapsedTime);
			ConnectionsToClose.Add(Connection);
		}

		Connection.ElapsedTime += DeltaTime;
	}

	for (FPerfConnection& Connection : ConnectionsToClose)
	{
		Connections.RemoveSingleSwap(Connection);

		FSocket* ClosingSocket = Connection.Connection;
		if (ClosingSocket)
		{
			const bool bLogConnectionClosure = true;
			if (bLogConnectionClosure)
			{
				ClosingSocket->GetPeerAddress(*ScratchIPAddr);
				UE_LOG(LogPerfCounters, Log, TEXT("Closed connection to %s."), *ScratchIPAddr->ToString(true));
			}

			// close the socket (whether we processed or not)
			ClosingSocket->Close();
			SocketSubsystem->DestroySocket(ClosingSocket);
		}
	}
}

void FPerfCounters::TickSystemCounters(float DeltaTime)
{
	// set some internal perf stats ([RCL] FIXME 2015-12-08: move to a better place)
	float CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastTimeInternalCountersUpdated > InternalCountersUpdateInterval)
	{
		// get CPU stats first
		FCPUTime CPUStats = FPlatformTime::GetCPUTime();
		Set(TEXT("ProcessCPUUsageRelativeToCore"), CPUStats.CPUTimePctRelative);

		// memory
		FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		Set(TEXT("AvailablePhysicalMemoryMB"), static_cast<uint64>(Stats.AvailablePhysical / (1024 * 1024)));
		Set(TEXT("AvailableVirtualMemoryMB"), static_cast<uint64>(Stats.AvailableVirtual / (1024 * 1024)));
		Set(TEXT("ProcessPhysicalMemoryMB"), static_cast<uint64>(Stats.UsedPhysical/ (1024 * 1024)));
		Set(TEXT("ProcessVirtualMemoryMB"), static_cast<uint64>(Stats.UsedVirtual / (1024 * 1024)));

		// disk space
		const FString LogFilename = FPlatformOutputDevices::GetAbsoluteLogFilename();
		uint64 TotalBytesOnLogDrive = 0, FreeBytesOnLogDrive = 0;
		if (FPlatformMisc::GetDiskTotalAndFreeSpace(LogFilename, TotalBytesOnLogDrive, FreeBytesOnLogDrive))
		{
			Set(TEXT("FreeSpaceOnLogFileDiskInMB"), static_cast<uint64>(FreeBytesOnLogDrive / (1024 * 1024)));
		}

		LastTimeInternalCountersUpdated = CurrentTime;
	}
}

bool FPerfCounters::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// ignore everything that doesn't start with PerfCounters
	if (!FParse::Command(&Cmd, TEXT("perfcounters")))
	{
		return false;
	}

	if (FParse::Command(&Cmd, TEXT("clear")))
	{
		ResetStatsForNextPeriod();
		return true;
	}

	return false;
}

bool FPerfCounters::ProcessRequest(uint8* Buffer, int32 BufferLen, FResponse& Response)
{
	bool bSuccess = false;

	// scan the buffer for a line
	FUTF8ToTCHAR WideBuffer(reinterpret_cast<const ANSICHAR*>(Buffer));
	const TCHAR* BufferEnd = FCString::Strstr(WideBuffer.Get(), TEXT("\r\n"));
	if (BufferEnd != nullptr)
	{
		// crack into pieces
		FString MainLine(BufferEnd - WideBuffer.Get(), WideBuffer.Get());
		TArray<FString> Tokens;
		MainLine.ParseIntoArrayWS(Tokens);
		if (Tokens.Num() >= 2)
		{
			FString ContentType(TEXT("application/json"));
			Response.Code = EHttpResponseCodes::Ok;

			// handle the request
			if (Tokens[0] != TEXT("GET"))
			{
				Response.Body = FString::Printf(TEXT("{ \"error\": \"Method %s not allowed\" }"), *Tokens[0]);
				Response.Code = EHttpResponseCodes::BadMethod;
			}
			else if (Tokens[1].StartsWith(TEXT("/stats")))
			{
				Response.Body = GetAllCountersAsJson();
			}
			else if (Tokens[1].StartsWith(TEXT("/exec?c=")))
			{
				FString ExecCmd = Tokens[1].Mid(8);
				FString ExecCmdDecoded = FPlatformHttp::UrlDecode(ExecCmd);

				FStringOutputDevice StringOutDevice;
				StringOutDevice.SetAutoEmitLineTerminator(true);

				bool bResult = false;
				if (ExecCmdCallback.IsBound())
				{
					bResult = ExecCmdCallback.Execute(ExecCmdDecoded, StringOutDevice);
					Response.Body = StringOutDevice;
					ContentType = TEXT("text/text");
				}
				else
				{
					Response.Body = FString::Printf(TEXT("{ \"error\": \"exec handler not found\" }"));
				}

				Response.Code = bResult ? EHttpResponseCodes::Ok : EHttpResponseCodes::NotFound;
			}
			else
			{
				Response.Body = FString::Printf(TEXT("{ \"error\": \"%s not found\" }"), *Tokens[1]);
				Response.Code = EHttpResponseCodes::NotFound;
			}

			// send the response headers
			Response.Header = FString::Printf(TEXT("HTTP/1.0 %d\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n"), Response.Code, Response.Body.Len(), *ContentType);
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogPerfCounters, Warning, TEXT("Unable to parse HTTP request header: %s"), *MainLine);
		}
	}
	else
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("ProcessRequest: request incomplete"));
	}

	return bSuccess;
}

double FPerfCounters::GetNumber(const FString& Name, double DefaultValue)
{
	FJsonVariant * JsonValue = PerfCounterMap.Find(Name);
	if (JsonValue == nullptr)
	{
		return DefaultValue;
	}

	if (JsonValue->Format != FJsonVariant::Number)
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Attempting to get PerfCounter '%s' as number, but it is not (Json format=%d). Default value %f will be returned"), 
			*Name, static_cast<int32>(JsonValue->Format), DefaultValue);

		return DefaultValue;
	}

	return JsonValue->NumberValue;
}

void FPerfCounters::SetNumber(const FString& Name, double Value, uint32 Flags)
{
	FJsonVariant& JsonValue = PerfCounterMap.FindOrAdd(Name);
	JsonValue.Format = FJsonVariant::Number;
	JsonValue.Flags = Flags;
	JsonValue.NumberValue = Value;
}

void FPerfCounters::SetString(const FString& Name, const FString& Value, uint32 Flags)
{
	FJsonVariant& JsonValue = PerfCounterMap.FindOrAdd(Name);
	JsonValue.Format = FJsonVariant::String;
	JsonValue.Flags = Flags;
	JsonValue.StringValue = Value;
}

void FPerfCounters::SetJson(const FString& Name, const FProduceJsonCounterValue& InCallback, uint32 Flags)
{
	FJsonVariant& JsonValue = PerfCounterMap.FindOrAdd(Name);
	JsonValue.Format = FJsonVariant::Callback;
	JsonValue.Flags = Flags;
	JsonValue.CallbackValue = InCallback;
}

bool FPerfCounters::StartMachineLoadTracking()
{
	if (UNLIKELY(ZeroLoadRunnable != nullptr || ZeroLoadThread != nullptr))
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Machine load tracking has already been started."));
		return false;
	}

	ZeroLoadThread = new FZeroLoad(30.0);
	ZeroLoadRunnable = FRunnableThread::Create(ZeroLoadThread, TEXT("ZeroLoadThread"), 0, TPri_Normal);

	if (UNLIKELY(ZeroLoadRunnable == nullptr))
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Failed to create zero load thread."));

		delete ZeroLoadThread;
		ZeroLoadThread = nullptr;

		return false;
	}

	return true;
}

bool FPerfCounters::StopMachineLoadTracking()
{
	if (UNLIKELY(ZeroLoadRunnable == nullptr || ZeroLoadThread == nullptr))
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Machine load tracking has already been stopped."));
		return false;
	}

	// this will first call Stop()
	if (!ZeroLoadRunnable->Kill(true))
	{
		UE_LOG(LogPerfCounters, Warning, TEXT("Could not kill zero-load thread, crash imminent."));
	}

	// set the its histogram as one of counters
	FHistogram ZeroLoadFrameTimes;
	if (ZeroLoadThread->GetFrameTimeHistogram(ZeroLoadFrameTimes))
	{
		PerformanceHistograms().Add(IPerfCounters::Histograms::ZeroLoadFrameTime, ZeroLoadFrameTimes);
	}

	delete ZeroLoadRunnable;
	ZeroLoadRunnable = nullptr;

	delete ZeroLoadThread;
	ZeroLoadThread = nullptr;

	return true;
}

bool FPerfCounters::ReportUnplayableCondition(const FString& ConditionDescription)
{
	FString UnplayableConditionFile(FPaths::Combine(*FPaths::ProjectSavedDir(), *FString::Printf(TEXT("UnplayableConditionForPid_%d.txt"), FPlatformProcess::GetCurrentProcessId())));

	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*UnplayableConditionFile);
	if (UNLIKELY(ReportFile == nullptr))
	{
		return false;
	}

	// include description for debugging
	FTCHARToUTF8 Converter(*FString::Printf(TEXT("Unplayable condition encountered: %s\n"), *ConditionDescription));
	ReportFile->Serialize(reinterpret_cast<void *>(const_cast<char *>(Converter.Get())), Converter.Length());

	ReportFile->Close();
	delete ReportFile;

	return true;
}
