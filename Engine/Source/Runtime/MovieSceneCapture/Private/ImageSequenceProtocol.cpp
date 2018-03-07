// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Protocols/ImageSequenceProtocol.h"

#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Templates/Casts.h"
#include "MovieSceneCaptureSettings.h"


void UBmpImageCaptureSettings::OnReleaseConfig(FMovieSceneCaptureSettings& InSettings)
{
	// Remove .{frame} if it exists. The "." before the {frame} is intentional because some media players denote frame numbers separated by "."
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT(".{frame}"), TEXT(""));

	Super::OnReleaseConfig(InSettings);
}

void UBmpImageCaptureSettings::OnLoadConfig(FMovieSceneCaptureSettings& InSettings)
{
	// Add .{frame} if it doesn't already exist
	FString OutputFormat = InSettings.OutputFormat;

	if (!OutputFormat.Contains(TEXT("{frame}")))
	{
		OutputFormat.Append(TEXT(".{frame}"));

		InSettings.OutputFormat = OutputFormat;
	}

	Super::OnLoadConfig(InSettings);
}

void UImageCaptureSettings::OnReleaseConfig(FMovieSceneCaptureSettings& InSettings)
{
	// Remove .{frame} if it exists. The "." before the {frame} is intentional because some media players denote frame numbers separated by "."
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT(".{frame}"), TEXT(""));

	Super::OnReleaseConfig(InSettings);
}

void UImageCaptureSettings::OnLoadConfig(FMovieSceneCaptureSettings& InSettings)
{
	// Add .{frame} if it doesn't already exist
	FString OutputFormat = InSettings.OutputFormat;

	if (!OutputFormat.Contains(TEXT("{frame}")))
	{
		OutputFormat.Append(TEXT(".{frame}"));

		InSettings.OutputFormat = OutputFormat;
	}

	Super::OnLoadConfig(InSettings);
}

#if WITH_EDITOR

#include "IImageWrapperModule.h"

namespace
{
	static const int32 MaxAsyncWrites = 6;
}

struct FImageFrameData : IFramePayload
{
	FString Filename;
};

FImageSequenceProtocol::FImageSequenceProtocol(EImageFormat InFormat)
{
	Format = InFormat;
	CompressionQuality = 100;
}

bool FImageSequenceProtocol::Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host)
{
	if (!FFrameGrabberProtocol::Initialize(InSettings, Host))
	{
		return false;
	}

	UImageCaptureSettings* CaptureSettings = Cast<UImageCaptureSettings>(InSettings.ProtocolSettings);
	if (CaptureSettings)
	{
		CompressionQuality = CaptureSettings->CompressionQuality;
		
		FParse::Value( FCommandLine::Get(), TEXT( "-MovieQuality=" ), CompressionQuality );

		CompressionQuality = FMath::Clamp<int32>(CompressionQuality, 1, 100);
	}

	CaptureThread.Reset(new FImageCaptureThread(Format, CompressionQuality));
	
	return true;
}

bool FImageSequenceProtocol::HasFinishedProcessing() const
{
	return FFrameGrabberProtocol::HasFinishedProcessing() && CaptureThread->GetNumOutstandingFrames() == 0;
}

void FImageSequenceProtocol::Finalize()
{
	CaptureThread->Close();
	CaptureThread.Reset();

	FFrameGrabberProtocol::Finalize();
}

FFramePayloadPtr FImageSequenceProtocol::GetFramePayload(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host)
{
	TSharedRef<FImageFrameData, ESPMode::ThreadSafe> FrameData = MakeShareable(new FImageFrameData);

	const TCHAR* Extension = TEXT("");
	switch(Format)
	{
	case EImageFormat::BMP:		Extension = TEXT(".bmp"); break;
	case EImageFormat::PNG:		Extension = TEXT(".png"); break;
	case EImageFormat::JPEG:	Extension = TEXT(".jpg"); break;
	}

	FrameData->Filename = Host.GenerateFilename(FrameMetrics, Extension);
	Host.EnsureFileWritable(FrameData->Filename);

	// Add our custom formatting rules as well
	// @todo: document these on the tooltip?
	FrameData->Filename = FString::Format(*FrameData->Filename, StringFormatMap);

	return FrameData;
}

void FImageSequenceProtocol::ProcessFrame(FCapturedFrameData Frame)
{
	CaptureThread->Add(MoveTemp(Frame));
}

void FImageSequenceProtocol::AddFormatMappings(TMap<FString, FStringFormatArg>& FormatMappings) const
{
	if (Format == EImageFormat::JPEG || Format == EImageFormat::PNG)
	{
		FormatMappings.Add(TEXT("quality"), CompressionQuality);
	}
	else
	{
		FormatMappings.Add(TEXT("quality"), TEXT(""));
	}
}


FImageCaptureThread::FImageCaptureThread(EImageFormat InFormat, int32 InCompressionQuality)
{
	Format = InFormat;
	CompressionQuality = InCompressionQuality;

	WorkToDoEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadEmptyEvent = FPlatformProcess::GetSynchEventFromPool();

	CapturedFrames.Reserve(MaxAsyncWrites);

	if (Format == EImageFormat::PNG || Format == EImageFormat::JPEG)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );

		for (int32 Index = 0; Index < MaxAsyncWrites; ++Index)
		{
			ImageWrappers.Add(ImageWrapperModule.CreateImageWrapper(Format));
		}
	}

	static int32 Index = 0;
	Thread = FRunnableThread::Create(this, *FString::Printf(TEXT("ImageCaptureThread_%d"), ++Index));
}

FImageCaptureThread::~FImageCaptureThread()
{
	FPlatformProcess::ReturnSynchEventToPool(WorkToDoEvent);
	FPlatformProcess::ReturnSynchEventToPool(ThreadEmptyEvent);
}

void FImageCaptureThread::Add(FCapturedFrameData Frame)
{
	bool bThreadIsChoked = false;
	{
		FScopeLock Lock(&CommandMutex);
		CapturedFrames.Add(MoveTemp(Frame));

		bThreadIsChoked = CapturedFrames.Num() > MaxAsyncWrites;
	}

	WorkToDoEvent->Trigger();

	if (bThreadIsChoked)
	{
		ThreadEmptyEvent->Wait(~0);
	}
}

uint32 FImageCaptureThread::GetNumOutstandingFrames() const
{
	FScopeLock Lock(&CommandMutex);
	return CapturedFrames.Num();
}

void FImageCaptureThread::Close()
{
	Thread->Kill(true);
}

void FImageCaptureThread::Stop()
{
	bRunning = false;
	WorkToDoEvent->Trigger();
}

uint32 FImageCaptureThread::Run()
{
	bRunning = true;

	TArray<TFuture<void>> FrameFutures;
	TArray<FCapturedFrameData> ProcessingFrames;

	ProcessingFrames.Reserve(MaxAsyncWrites);
	FrameFutures.Reserve(MaxAsyncWrites);

	for(;;)
	{
		WorkToDoEvent->Wait(~0);
		if (!bRunning)
		{
			ThreadEmptyEvent->Trigger();
			return 0;
		}

		// process all outstanding frames, MaxAsyncWrites at a time
		for (;;)
		{
			{
				FScopeLock Lock(&CommandMutex);
				for (int32 Index = 0; Index < MaxAsyncWrites && Index < CapturedFrames.Num(); ++Index)
				{
					ProcessingFrames.Add(MoveTemp(CapturedFrames[Index]));
				}

				if (ProcessingFrames.Num())
				{
					CapturedFrames.RemoveAt(0, ProcessingFrames.Num(), false);
				}
				else
				{
					break;
				}
			}

			for (int32 FrameIndex = 0; FrameIndex < ProcessingFrames.Num(); ++FrameIndex)
			{
				FrameFutures.Add(Async<void>(EAsyncExecution::TaskGraph, [&, FrameIndex] {
					TSharedPtr<IImageWrapper> ImageWrapper = ImageWrappers.Num() ? ImageWrappers[FrameIndex] : nullptr;
					WriteFrameToDisk(ProcessingFrames[FrameIndex], ImageWrapper);
				}));
			}

			for (TFuture<void>& Future : FrameFutures)
			{
				Future.Wait();
			}

			ProcessingFrames.Reset();
			FrameFutures.Reset();
		}

		ThreadEmptyEvent->Trigger();
	}

	return 0;
}

void FImageCaptureThread::WriteFrameToDisk(FCapturedFrameData& Frame, TSharedPtr<IImageWrapper> ImageWrapper) const
{
	const float Width = Frame.BufferSize.X;
	const float Height = Frame.BufferSize.Y;

	FImageFrameData* Payload = Frame.GetPayload<FImageFrameData>();
	switch (Format)
	{
	case EImageFormat::BMP:
		FFileHelper::CreateBitmap(*Payload->Filename, Width, Height, Frame.ColorBuffer.GetData());
		break;
	case EImageFormat::PNG:
		for (FColor& Color : Frame.ColorBuffer)
		{
			Color.A = 255;
		}

		if (ImageWrapper->SetRaw(Frame.ColorBuffer.GetData(), Frame.ColorBuffer.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(CompressionQuality), *Payload->Filename);
		}
		break;

	case EImageFormat::JPEG:
		if (ImageWrapper->SetRaw(Frame.ColorBuffer.GetData(), Frame.ColorBuffer.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(CompressionQuality), *Payload->Filename);
		}
		break;
	}
}

#endif
