// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"

#if ENABLE_VISUAL_LOG

#define VISLOG_FILENAME_EXT TEXT("bvlog")

class FVisualLoggerBinaryFileDevice : public FVisualLogDevice
{
public:
	static FVisualLoggerBinaryFileDevice& Get()
	{
		static FVisualLoggerBinaryFileDevice GDevice;
		return GDevice;
	}

	FVisualLoggerBinaryFileDevice();
	virtual void Cleanup(bool bReleaseMemory = false) override;
	virtual void StartRecordingToFile(float TImeStamp) override;
	virtual void StopRecordingToFile(float TImeStamp) override;
	virtual void DiscardRecordingToFile() override;
	virtual void SetFileName(const FString& InFileName) override;
	virtual void Serialize(const class UObject* LogOwner, FName OwnerName, FName OwnerClassName, const FVisualLogEntry& LogEntry) override;
	virtual void GetRecordedLogs(TArray<FVisualLogEntryItem>& RecordedLogs) const override { RecordedLogs = FrameCache; }
	virtual bool HasFlags(int32 InFlags) const override { return !!(InFlags & (EVisualLoggerDeviceFlags::CanSaveToFile | EVisualLoggerDeviceFlags::StoreLogsLocally)); }

protected:
	int32 bUseCompression : 1;
	float FrameCacheLenght;
	float StartRecordingTime;
	float LastLogTimeStamp;
	FArchive* FileArchive;
	FString TempFileName;
	FString FileName;
	TArray<FVisualLogEntryItem> FrameCache;
};
#endif
