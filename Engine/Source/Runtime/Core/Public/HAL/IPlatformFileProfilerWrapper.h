// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Misc/Parse.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformTime.h"
#include "Templates/ScopedPointer.h"
#include "Misc/ScopeLock.h"
#include "UniquePtr.h"

class IAsyncReadFileHandle;

#if !UE_BUILD_SHIPPING

/**
 * Wrapper to log the low level file system
**/
DECLARE_LOG_CATEGORY_EXTERN(LogProfiledFile, Log, All);

extern bool bSuppressProfiledFileLog;

#define PROFILERFILE_LOG(CategoryName, Verbosity, Format, ...) \
	if (!bSuppressProfiledFileLog) \
	{ \
		bSuppressProfiledFileLog = true; \
		UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); \
		bSuppressProfiledFileLog = false; \
	}

struct FProfiledFileStatsBase
{
	/** Start time (ms) */
	double StartTime;
	/** Duration (ms) */
	double Duration;

	FProfiledFileStatsBase()
		: StartTime( 0.0 )
		, Duration( 0.0 )
	{
	}
};


struct FProfiledFileStatsOp : public FProfiledFileStatsBase
{
	enum class EOpType : uint8
	{
		Unknown = 0,
		Tell = 1,
		Seek,
		Read,
		Write,
		Size,
		OpenRead,
		OpenWrite,
		Exists,
		Delete,
		Move,
		IsReadOnly,
		SetReadOnly,
		GetTimeStamp,
		SetTimeStamp,
		GetFilenameOnDisk,
		Create,
		Copy,
		Iterate,
		IterateStat,
		GetStatData,

		Count
	};

	/** Operation type */
	FProfiledFileStatsOp::EOpType Type;

	/** Number of bytes processed */
	int64 Bytes;

	/** The last time this operation was executed */
	double LastOpTime;

	FProfiledFileStatsOp( FProfiledFileStatsOp::EOpType InType )
		: Type( InType )
		, Bytes( 0 )
		, LastOpTime( 0.0 )
	{}
};

struct FProfiledFileStatsFileBase : public FProfiledFileStatsBase
{
	/** File name */
	FString	Name;
	/** Child stats */
	TArray< TSharedPtr< FProfiledFileStatsOp > > Children;
	FCriticalSection SynchronizationObject;

	FProfiledFileStatsFileBase( const TCHAR* Filename )
	: Name( Filename )
	{}
};

struct FProfiledFileStatsFileDetailed : public FProfiledFileStatsFileBase
{
	FProfiledFileStatsFileDetailed( const TCHAR* Filename )
	: FProfiledFileStatsFileBase( Filename )
	{}

	FORCEINLINE FProfiledFileStatsOp* CreateOpStat( FProfiledFileStatsOp::EOpType Type )
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		TSharedPtr< FProfiledFileStatsOp > Stat( new FProfiledFileStatsOp( Type ) );
		Children.Add( Stat );
		Stat->StartTime = FPlatformTime::Seconds() * 1000.0;
		Stat->LastOpTime = Stat->StartTime;
		return Stat.Get();
	}
};

struct FProfiledFileStatsFileSimple : public FProfiledFileStatsFileBase
{
	FProfiledFileStatsFileSimple( const TCHAR* Filename )
	: FProfiledFileStatsFileBase( Filename )
	{
		for( uint8 TypeIndex = 0; TypeIndex < (uint8)FProfiledFileStatsOp::EOpType::Count; TypeIndex++ )
		{
			TSharedPtr< FProfiledFileStatsOp > Stat( new FProfiledFileStatsOp( FProfiledFileStatsOp::EOpType( TypeIndex ) ) );
			Children.Add( Stat );
		}
	}

	FORCEINLINE FProfiledFileStatsOp* CreateOpStat( FProfiledFileStatsOp::EOpType Type )
	{
		TSharedPtr< FProfiledFileStatsOp > Stat = Children[ (uint8)Type ];
		Stat->LastOpTime = FPlatformTime::Seconds() * 1000.0;
		if( Stat->StartTime == 0.0 )
		{
			Stat->StartTime = Stat->LastOpTime;
		}
		return Stat.Get();
	}
};

template< typename StatType >
class TProfiledFileHandle : public IFileHandle
{
	TUniquePtr<IFileHandle> FileHandle;
	FString Filename;
	StatType* FileStats;

public:

	TProfiledFileHandle(IFileHandle* InFileHandle, const TCHAR* InFilename, StatType* InStats)
		: FileHandle(InFileHandle)
		, Filename(InFilename)
		, FileStats(InStats)
	{
	}

	virtual int64		Tell() override
	{
		FProfiledFileStatsOp* Stat( FileStats->CreateOpStat( FProfiledFileStatsOp::EOpType::Tell ) );
		int64 Result = FileHandle->Tell();
		Stat->Duration += FPlatformTime::Seconds() * 1000.0 - Stat->LastOpTime;
		return Result;
	}
	virtual bool		Seek(int64 NewPosition) override
	{
		FProfiledFileStatsOp* Stat( FileStats->CreateOpStat( FProfiledFileStatsOp::EOpType::Seek ) );
		bool Result = FileHandle->Seek(NewPosition);
		Stat->Duration += FPlatformTime::Seconds() * 1000.0 - Stat->LastOpTime;
		return Result;
	}
	virtual bool		SeekFromEnd(int64 NewPositionRelativeToEnd) override
	{
		FProfiledFileStatsOp* Stat( FileStats->CreateOpStat( FProfiledFileStatsOp::EOpType::Seek ) );
		bool Result = FileHandle->SeekFromEnd(NewPositionRelativeToEnd);
		Stat->Duration += FPlatformTime::Seconds() * 1000.0 - Stat->LastOpTime;
		return Result;
	}
	virtual bool		Read(uint8* Destination, int64 BytesToRead) override
	{
		FProfiledFileStatsOp* Stat( FileStats->CreateOpStat( FProfiledFileStatsOp::EOpType::Read ) );
		bool Result = FileHandle->Read(Destination, BytesToRead);
		Stat->Duration += FPlatformTime::Seconds() * 1000.0 - Stat->LastOpTime;
		Stat->Bytes += BytesToRead;
		return Result;
	}
	virtual bool		Write(const uint8* Source, int64 BytesToWrite) override
	{
		FProfiledFileStatsOp* Stat( FileStats->CreateOpStat( FProfiledFileStatsOp::EOpType::Write ) );
		bool Result = FileHandle->Write(Source, BytesToWrite);
		Stat->Duration += FPlatformTime::Seconds() * 1000.0 - Stat->LastOpTime;
		Stat->Bytes += BytesToWrite;
		return Result;
	}
	virtual int64		Size() override
	{
		FProfiledFileStatsOp* Stat( FileStats->CreateOpStat( FProfiledFileStatsOp::EOpType::Size ) );
		int64 Result = FileHandle->Size();
		Stat->Duration += FPlatformTime::Seconds() * 1000.0 - Stat->LastOpTime;
		return Result;
	}
};

class CORE_API FProfiledPlatformFile : public IPlatformFile
{
protected:

	IPlatformFile* LowerLevel;
	TMap< FString, TSharedPtr< FProfiledFileStatsFileBase > > Stats;
	double StartTime;
	FCriticalSection SynchronizationObject;

	FProfiledPlatformFile()
		: LowerLevel(nullptr)
		, StartTime(0.0)
	{
	}

public:

	virtual ~FProfiledPlatformFile()
	{
	}

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override
	{
		return FParse::Param( CmdLine, GetName() );
	}

	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) override
	{
		// Inner is required.
		check(Inner != nullptr);
		LowerLevel = Inner;
		StartTime = FPlatformTime::Seconds() * 1000.0;
		return !!LowerLevel;
	}

	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}

	double GetStartTime() const
	{
		return StartTime;
	}

	const TMap< FString, TSharedPtr< FProfiledFileStatsFileBase > >& GetStats() const
	{
		return Stats;
	}
};

template <class StatsType>
class TProfiledPlatformFile : public FProfiledPlatformFile
{
	FORCEINLINE StatsType* CreateStat( const TCHAR* Filename )
	{
		FString Path( Filename );
		FScopeLock ScopeLock(&SynchronizationObject);

		TSharedPtr< FProfiledFileStatsFileBase >* ExistingStat = Stats.Find( Path );
		if( ExistingStat != nullptr )
		{
			return (StatsType*)(ExistingStat->Get());
		}
		else
		{
			TSharedPtr< StatsType > Stat( new StatsType( *Path ) );
			Stats.Add( Path, Stat );
			Stat->StartTime = FPlatformTime::Seconds() * 1000.0;
			
			return Stat.Get();
		}		
	}

public:

	TProfiledPlatformFile()
	{
	}
	static const TCHAR* GetTypeName()
	{
		return nullptr;
	}
	virtual const TCHAR* GetName() const override
	{
		return TProfiledPlatformFile::GetTypeName();
	}
	virtual bool		FileExists(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Exists );
		bool Result = LowerLevel->FileExists(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual int64		FileSize(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Size );
		int64 Result = LowerLevel->FileSize(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		DeleteFile(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Delete );
		bool Result = LowerLevel->DeleteFile(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		IsReadOnly(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::IsReadOnly );
		bool Result = LowerLevel->IsReadOnly(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		StatsType* FileStat = CreateStat( From );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Move );
		bool Result = LowerLevel->MoveFile(To, From);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::SetReadOnly );
		bool Result = LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual FDateTime	GetTimeStamp(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::GetTimeStamp );
		double OpStartTime = FPlatformTime::Seconds();
		FDateTime Result = LowerLevel->GetTimeStamp(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000 - OpStat->LastOpTime;
		return Result;
	}
	virtual void		SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::SetTimeStamp );
		double OpStartTime = FPlatformTime::Seconds();
		LowerLevel->SetTimeStamp(Filename, DateTime);
		OpStat->Duration += FPlatformTime::Seconds() * 1000 - OpStat->LastOpTime;
	}
	virtual FDateTime	GetAccessTimeStamp(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::GetTimeStamp );
		double OpStartTime = FPlatformTime::Seconds();
		FDateTime Result = LowerLevel->GetAccessTimeStamp(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000 - OpStat->LastOpTime;
		return Result;
	}
	virtual FString	GetFilenameOnDisk(const TCHAR* Filename) override
	{
		StatsType* FileStat = CreateStat(Filename);
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::GetFilenameOnDisk );
		double OpStartTime = FPlatformTime::Seconds();
		FString Result = LowerLevel->GetFilenameOnDisk(Filename);
		OpStat->Duration += FPlatformTime::Seconds() * 1000 - OpStat->LastOpTime;
		return Result;
	}
	virtual IFileHandle*	OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::OpenRead );
		IFileHandle* Result = LowerLevel->OpenRead(Filename, bAllowWrite);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result ? (new TProfiledFileHandle< StatsType >( Result, Filename, FileStat )) : Result;
	}
	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		StatsType* FileStat = CreateStat( Filename );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::OpenWrite );
		IFileHandle* Result = LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result ? (new TProfiledFileHandle< StatsType >( Result, Filename, FileStat )) : Result;
	}

	virtual bool		DirectoryExists(const TCHAR* Directory) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Exists );
		bool Result = LowerLevel->DirectoryExists(Directory);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		CreateDirectory(const TCHAR* Directory) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Create );
		bool Result = LowerLevel->CreateDirectory(Directory);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		DeleteDirectory(const TCHAR* Directory) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Delete );
		bool Result = LowerLevel->DeleteDirectory(Directory);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		StatsType* FileStat = CreateStat( FilenameOrDirectory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::GetStatData );
		FFileStatData Result = LowerLevel->GetStatData(FilenameOrDirectory);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}

	virtual bool		IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Iterate );
		bool Result = LowerLevel->IterateDirectory( Directory, Visitor );
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Iterate );
		bool Result = LowerLevel->IterateDirectoryRecursively( Directory, Visitor );
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}

	virtual bool		IterateDirectoryStat(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::IterateStat );
		bool Result = LowerLevel->IterateDirectoryStat( Directory, Visitor );
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		IterateDirectoryStatRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::IterateStat );
		bool Result = LowerLevel->IterateDirectoryStatRecursively( Directory, Visitor );
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}

	virtual bool		DeleteDirectoryRecursively(const TCHAR* Directory) override
	{
		StatsType* FileStat = CreateStat( Directory );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Delete );
		bool Result = LowerLevel->DeleteDirectoryRecursively( Directory );
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual bool		CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None) override
	{
		StatsType* FileStat = CreateStat( From );
		FProfiledFileStatsOp* OpStat = FileStat->CreateOpStat( FProfiledFileStatsOp::EOpType::Copy );
		bool Result = LowerLevel->CopyFile( To, From, ReadFlags, WriteFlags);
		OpStat->Duration += FPlatformTime::Seconds() * 1000.0 - OpStat->LastOpTime;
		return Result;
	}
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override
	{
		return LowerLevel->OpenAsyncRead(Filename);
	}

	//static void CreateProfileVisualizer
};

template<>
inline const TCHAR* TProfiledPlatformFile<FProfiledFileStatsFileDetailed>::GetTypeName()
{
	return TEXT("ProfileFile");
}

template<>
inline const TCHAR* TProfiledPlatformFile<FProfiledFileStatsFileSimple>::GetTypeName()
{
	return TEXT("SimpleProfileFile");
}

class FPlatformFileReadStatsHandle : public IFileHandle
{
	TUniquePtr<IFileHandle> FileHandle;
	FString Filename;
	volatile int32* BytesPerSecCounter;
	volatile int32* BytesReadCounter;
	volatile int32* ReadsCounter;

public:

	FPlatformFileReadStatsHandle(IFileHandle* InFileHandle, const TCHAR* InFilename, volatile int32* InBytesPerSec, volatile int32* InByteRead, volatile int32* InReads)
		: FileHandle(InFileHandle)
		, Filename(InFilename)
		, BytesPerSecCounter(InBytesPerSec)
		, BytesReadCounter(InByteRead)
		, ReadsCounter(InReads)
	{
	}

	virtual int64		Tell() override
	{
		return FileHandle->Tell();
	}
	virtual bool		Seek(int64 NewPosition) override
	{
		return FileHandle->Seek(NewPosition);
	}
	virtual bool		SeekFromEnd(int64 NewPositionRelativeToEnd) override
	{
		return FileHandle->SeekFromEnd(NewPositionRelativeToEnd);
	}
	virtual bool		Read(uint8* Destination, int64 BytesToRead) override;
	virtual bool		Write(const uint8* Source, int64 BytesToWrite) override
	{
		return FileHandle->Write(Source, BytesToWrite);
	}
	virtual int64		Size() override
	{
		return FileHandle->Size();
	}
};

class CORE_API FPlatformFileReadStats : public IPlatformFile
{
protected:

	IPlatformFile*	LowerLevel;
	double			LifetimeReadSpeed;	// Total maintained over lifetime of runtime, in KB per sec
	double			LifetimeReadSize;	// Total maintained over lifetime of runtime, in bytes
	int64			LifetimeReadCalls;	// Total maintained over lifetime of runtime
	double			Timer;
	volatile int32	BytePerSecThisTick;
	volatile int32	BytesReadThisTick;
	volatile int32	ReadsThisTick;

public:

	FPlatformFileReadStats()
		: LowerLevel(nullptr)
		, Timer(0.f)
	{
	}

	virtual ~FPlatformFileReadStats()
	{
	}

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override
	{
#if STATS
		bool bResult = FParse::Param(CmdLine, TEXT("FileReadStats"));
		return bResult;
#else
		return false;
#endif
	}

	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) override;
	using IPlatformFile::Tick;
	bool Tick(float Delta);
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
		return TEXT("FileReadStats");
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
		return Result ? (new FPlatformFileReadStatsHandle(Result, Filename, &BytePerSecThisTick, &BytesReadThisTick, &ReadsThisTick)) : Result;
	}
	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		IFileHandle* Result = LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
		return Result ? (new FPlatformFileReadStatsHandle(Result, Filename, &BytePerSecThisTick, &BytesReadThisTick, &ReadsThisTick)) : Result;
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
		return LowerLevel->CopyFile( To, From, ReadFlags, WriteFlags );
	}
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override
	{
		//@todo no wrapped async handles (yet)
		return LowerLevel->OpenAsyncRead(Filename);
	}
};


#endif // !UE_BUILD_SHIPPING
