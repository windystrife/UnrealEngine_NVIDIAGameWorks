// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Misc/Parse.h"
#include "Containers/StringConv.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Async/AsyncFileHandle.h"

class IAsyncReadFileHandle;
class FPlatformFileOpenLog;

#if !UE_BUILD_SHIPPING

class FLoggingAsyncReadFileHandle final : public IAsyncReadFileHandle
{
	FPlatformFileOpenLog* Owner;
	FString Filename;
	IAsyncReadFileHandle* ActualRequest;
public:
	FLoggingAsyncReadFileHandle(FPlatformFileOpenLog* InOwner, const TCHAR* InFilename, IAsyncReadFileHandle* InActualRequest)
		: Owner(InOwner)
		, Filename(InFilename)
		, ActualRequest(InActualRequest)
	{
	}
	~FLoggingAsyncReadFileHandle()
	{
		delete ActualRequest;
	}
	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override
	{
		return ActualRequest->SizeRequest(CompleteCallback);
	}
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr, uint8* UserSuppliedMemory = nullptr) override;
};


class CORE_API FPlatformFileOpenLog : public IPlatformFile
{
protected:

	IPlatformFile*			LowerLevel;
	FCriticalSection		CriticalSection;
	int64					OpenOrder;
	TMap<FString, int64>	FilenameAccessMap;
	TArray<IFileHandle*>	LogOutput;

public:

	FPlatformFileOpenLog()
		: LowerLevel(nullptr)
		, OpenOrder(0)
	{
	}

	virtual ~FPlatformFileOpenLog()
	{
	}

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override
	{
		bool bResult = FParse::Param(CmdLine, TEXT("FileOpenLog"));
		return bResult;
	}

	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) override
	{
		LowerLevel = Inner;
		FString LogFileDirectory;
		FString LogFilePath;
		FString PlatformStr;

		if (FParse::Value(CommandLineParam, TEXT("TARGETPLATFORM="), PlatformStr))
		{
			TArray<FString> PlatformNames;
			if (!(PlatformStr == TEXT("None") || PlatformStr == TEXT("All")))
			{
				PlatformStr.ParseIntoArray(PlatformNames, TEXT("+"), true);
			}

			for (int32 Platform = 0;Platform < PlatformNames.Num(); ++Platform)
			{
				LogFileDirectory = FPaths::Combine( FPlatformMisc::ProjectDir(), TEXT( "Build" ), *PlatformNames[Platform], TEXT("FileOpenOrder"));
#if WITH_EDITOR
				LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("EditorOpenOrder.log"));
#else 
				LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("GameOpenOrder.log"));
#endif
				Inner->CreateDirectoryTree(*LogFileDirectory);
				auto* FileHandle = Inner->OpenWrite(*LogFilePath, false, false);
				if (FileHandle) 
				{
					LogOutput.Add(FileHandle);
				}
			}
		}
		else
		{
			LogFileDirectory = FPaths::Combine( FPlatformMisc::ProjectDir(), TEXT( "Build" ), StringCast<TCHAR>(FPlatformProperties::PlatformName()).Get(), TEXT("FileOpenOrder"));
#if WITH_EDITOR
			LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("EditorOpenOrder.log"));
#else 
			LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("GameOpenOrder.log"));
#endif
			Inner->CreateDirectoryTree(*LogFileDirectory);
			auto* FileHandle = Inner->OpenWrite(*LogFilePath, false, false);
			if (FileHandle)
			{
				LogOutput.Add(FileHandle);
			}
		}
		return true;
	}
	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}
	static const TCHAR* GetTypeName()
	{
		return TEXT("FileOpenLog");
	}
	virtual const TCHAR* GetName() const override
	{
		return GetTypeName();
	}
	virtual bool		FileExists(const TCHAR* Filename) override
	{
		return LowerLevel->FileExists(Filename);
	}
	virtual int64		FileSize(const TCHAR* Filename) override
	{
		return LowerLevel->FileSize(Filename);
	}
	virtual bool		DeleteFile(const TCHAR* Filename) override
	{
		return LowerLevel->DeleteFile(Filename);
	}
	virtual bool		IsReadOnly(const TCHAR* Filename) override
	{
		return LowerLevel->IsReadOnly(Filename);
	}
	virtual bool		MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return LowerLevel->MoveFile(To, From);
	}
	virtual bool		SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		return LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
	}
	virtual FDateTime	GetTimeStamp(const TCHAR* Filename) override
	{
		return LowerLevel->GetTimeStamp(Filename);
	}
	virtual void		SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		LowerLevel->SetTimeStamp(Filename, DateTime);
	}
	virtual FDateTime	GetAccessTimeStamp(const TCHAR* Filename) override
	{
		return LowerLevel->GetAccessTimeStamp(Filename);
	}
	virtual FString	GetFilenameOnDisk(const TCHAR* Filename) override
	{
		return LowerLevel->GetFilenameOnDisk(Filename);
	}
	virtual IFileHandle*	OpenRead(const TCHAR* Filename, bool bAllowWrite) override
	{
		IFileHandle* Result = LowerLevel->OpenRead(Filename, bAllowWrite);
		if (Result)
		{
			AddToOpenLog(Filename);
		}
		return Result;
	}
	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
	}
	virtual bool		DirectoryExists(const TCHAR* Directory) override
	{
		return LowerLevel->DirectoryExists(Directory);
	}
	virtual bool		CreateDirectory(const TCHAR* Directory) override
	{
		return LowerLevel->CreateDirectory(Directory);
	}
	virtual bool		DeleteDirectory(const TCHAR* Directory) override
	{
		return LowerLevel->DeleteDirectory(Directory);
	}
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		return LowerLevel->GetStatData(FilenameOrDirectory);
	}
	virtual bool		IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override
	{
		return LowerLevel->IterateDirectory( Directory, Visitor );
	}
	virtual bool		IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override
	{
		return LowerLevel->IterateDirectoryRecursively( Directory, Visitor );
	}
	virtual bool		IterateDirectoryStat(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor) override
	{
		return LowerLevel->IterateDirectoryStat( Directory, Visitor );
	}
	virtual bool		IterateDirectoryStatRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor) override
	{
		return LowerLevel->IterateDirectoryStatRecursively( Directory, Visitor );
	}
	virtual bool		DeleteDirectoryRecursively(const TCHAR* Directory) override
	{
		return LowerLevel->DeleteDirectoryRecursively( Directory );
	}
	virtual bool		CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None) override
	{
		return LowerLevel->CopyFile( To, From, ReadFlags, WriteFlags);
	}
	virtual bool		CreateDirectoryTree(const TCHAR* Directory) override
	{
		return LowerLevel->CreateDirectoryTree(Directory);
	}
	virtual bool		CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting) override
	{
		return LowerLevel->CopyDirectoryTree(DestinationDirectory, Source, bOverwriteAllExisting);
	}
	virtual FString		ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename ) override
	{
		return LowerLevel->ConvertToAbsolutePathForExternalAppForRead(Filename);
	}
	virtual FString		ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename ) override
	{
		return LowerLevel->ConvertToAbsolutePathForExternalAppForWrite(Filename);
	}
	virtual bool		SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler) override
	{
		return LowerLevel->SendMessageToServer(Message, Handler);
	}
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override
	{
		// we must not record the "open" here...what matters is when we start reading the file!
		return new FLoggingAsyncReadFileHandle(this, Filename, LowerLevel->OpenAsyncRead(Filename));
	}

	void AddToOpenLog(const TCHAR* Filename)
	{
		CriticalSection.Lock();
		if (FilenameAccessMap.Find(Filename) == nullptr)
		{
			FilenameAccessMap.Emplace(Filename, ++OpenOrder);
			FString Text = FString::Printf(TEXT("\"%s\" %llu\n"), Filename, OpenOrder);
			for (auto File = LogOutput.CreateIterator(); File; ++File)
			{
				(*File)->Write((uint8*)StringCast<ANSICHAR>(*Text).Get(), Text.Len());
			}
		}
		CriticalSection.Unlock();
	}
};

IAsyncReadRequest* FLoggingAsyncReadFileHandle::ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority, FAsyncFileCallBack* CompleteCallback, uint8* UserSuppliedMemory)
{
	if (Priority != AIOP_Precache)
	{
		Owner->AddToOpenLog(*Filename);
	}
	return ActualRequest->ReadRequest(Offset, BytesToRead, Priority, CompleteCallback, UserSuppliedMemory);
}

#endif // !UE_BUILD_SHIPPING
