// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Protocols/VideoCaptureProtocol.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Templates/Casts.h"

bool FVideoCaptureProtocol::Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host)
{
	InitSettings = InSettings;
	if (!FFrameGrabberProtocol::Initialize(InSettings, Host))
	{
		return false;
	}

	ConditionallyCreateWriter(Host);

	return AVIWriters.Num() && AVIWriters.Last()->IsCapturing();
}

void FVideoCaptureProtocol::ConditionallyCreateWriter(const ICaptureProtocolHost& Host)
{
#if PLATFORM_MAC
	static const TCHAR* Extension = TEXT(".mov");
#elif PLATFORM_LINUX
	static const TCHAR* Extension = TEXT(".unsupp");
	UE_LOG(LogInit, Warning, TEXT("Writing movies is not currently supported on Linux"));
	return;
#else
	static const TCHAR* Extension = TEXT(".avi");
#endif

	FString VideoFilename = Host.GenerateFilename(FFrameMetrics(), Extension);

	if (AVIWriters.Num() && VideoFilename == AVIWriters.Last()->Options.OutputFilename)
	{
		return;
	}

	Host.EnsureFileWritable(VideoFilename);

	UVideoCaptureSettings* CaptureSettings = CastChecked<UVideoCaptureSettings>(InitSettings->ProtocolSettings);

	FAVIWriterOptions Options;
	Options.OutputFilename = MoveTemp(VideoFilename);
	Options.CaptureFPS = Host.GetCaptureFrequency();
	Options.CodecName = CaptureSettings->VideoCodec;
	Options.bSynchronizeFrames = Host.GetCaptureStrategy().ShouldSynchronizeFrames();
	Options.Width = InitSettings->DesiredSize.X;
	Options.Height = InitSettings->DesiredSize.Y;

	if (CaptureSettings->bUseCompression)
	{
		Options.CompressionQuality = CaptureSettings->CompressionQuality / 100.f;
		
		float QualityOverride = 100.f;
		if (FParse::Value( FCommandLine::Get(), TEXT( "-MovieQuality=" ), QualityOverride ))
		{
			Options.CompressionQuality = FMath::Clamp(QualityOverride, 1.f, 100.f) / 100.f;
		}

		Options.CompressionQuality = FMath::Clamp<float>(Options.CompressionQuality.GetValue(), 0.f, 1.f);
	}

	AVIWriters.Emplace(FAVIWriter::CreateInstance(Options));
	AVIWriters.Last()->Initialize();
}

struct FVideoFrameData : IFramePayload
{
	FFrameMetrics Metrics;
	int32 WriterIndex;
};

FFramePayloadPtr FVideoCaptureProtocol::GetFramePayload(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host)
{
	ConditionallyCreateWriter(Host);

	TSharedRef<FVideoFrameData, ESPMode::ThreadSafe> FrameData = MakeShareable(new FVideoFrameData);
	FrameData->Metrics = FrameMetrics;
	FrameData->WriterIndex = AVIWriters.Num() - 1;
	return FrameData;
}

void FVideoCaptureProtocol::ProcessFrame(FCapturedFrameData Frame)
{
	FVideoFrameData* Payload = Frame.GetPayload<FVideoFrameData>();

	const int32 WriterIndex = Payload->WriterIndex;

	if (WriterIndex >= 0)
	{
		AVIWriters[WriterIndex]->DropFrames(Payload->Metrics.NumDroppedFrames);
		AVIWriters[WriterIndex]->Update(Payload->Metrics.TotalElapsedTime, MoveTemp(Frame.ColorBuffer));
	
		// Finalize previous writers if necessary
		for (int32 Index = 0; Index < Payload->WriterIndex; ++Index)
		{
			TUniquePtr<FAVIWriter>& Writer = AVIWriters[Index];
			if (Writer->IsCapturing())
			{
				Writer->Finalize();
			}
		}
	}
}

void FVideoCaptureProtocol::Finalize()
{
	for (TUniquePtr<FAVIWriter>& Writer : AVIWriters)
	{
		if (Writer->IsCapturing())
		{
			Writer->Finalize();
		}
	}
	
	AVIWriters.Empty();

	FFrameGrabberProtocol::Finalize();
}

bool FVideoCaptureProtocol::CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting) const
{
	// When recording video, if the filename changes (ie due to the shot changing), we create new AVI writers.
	// If we're not overwriting existing filenames we need to check if we're already recording a video of that name,
	// before we can deem whether we can write to a new file (we can always write to a filename we're already writing to)
	if (!bOverwriteExisting)
	{
		for (const TUniquePtr<FAVIWriter>& Writer : AVIWriters)
		{
			if (Writer->Options.OutputFilename == InFilename)
			{
				return true;
			}
		}

		return IFileManager::Get().FileSize(InFilename) == -1;
	}

	return true;
}
