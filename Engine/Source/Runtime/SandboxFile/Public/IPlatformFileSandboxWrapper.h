// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Templates/ScopedPointer.h"
#include "Misc/Paths.h"
#include "UniquePtr.h"

class IAsyncReadFileHandle;

/**
 * Wrapper to log the low level file system
**/
DECLARE_LOG_CATEGORY_EXTERN(SandboxFile, Log, All);


class SANDBOXFILE_API FSandboxFileHandle : public IFileHandle
{
	TUniquePtr<IFileHandle>	FileHandle;
	FString					Filename;

public:

	FSandboxFileHandle(IFileHandle* InFileHandle, const TCHAR* InFilename)
		: FileHandle(InFileHandle)
		, Filename(InFilename)
	{
	}

	virtual ~FSandboxFileHandle()
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
	virtual bool		Read(uint8* Destination, int64 BytesToRead) override
	{
		return FileHandle->Read(Destination, BytesToRead);
	}
	virtual bool		Write(const uint8* Source, int64 BytesToWrite) override
	{
		return FileHandle->Write(Source, BytesToWrite);
	}
	virtual int64		Size() override
	{
		return FileHandle->Size();
	}
};

class SANDBOXFILE_API FSandboxPlatformFile : public IPlatformFile
{
	/** Wrapped file */
	IPlatformFile*		LowerLevel;
	/** Absolute path to the sandbox directory */
	FString			SandboxDirectory;
	/** Name of the game's sandbox directory */
	FString			GameSandboxDirectoryName;
	/** Relative path to root directory. Cached for faster access */
	FString			RelativeRootDirectory;
	/** Absolute path to root directory. Cached for faster access */
	FString			AbsoluteRootDirectory;
	/** Absolute game directory. Cached for faster access */
	FString			AbsoluteGameDirectory;
	/** Absolute path to game directory. Cached for faster access */
	FString			AbsolutePathToGameDirectory;
	/** Access to any file (in unreal standard form) matching this is not allowed */
	TArray<FString>		FileExclusionWildcards;
	/** Access to any directory (in unreal standard form) matching this is not allowed */
	TArray<FString>		DirectoryExclusionWildcards;
	bool				bEntireEngineWillUseThisSandbox;

	/**
	 *	Whether the sandbox is enabled or not.
	 *	Defaults to true.
	 *	Set to false when operations require writing to the actual physical location given.
	 */
	bool				bSandboxEnabled;

	/**
	 * Clears the contents of the specified folder
	 *
	 * @param AbsolutePath Absolute path to the folder to wipe
	 * @return true if the folder's contents could be deleted, false otherwise
	 */
	bool WipeSandboxFolder( const TCHAR* AbsolutePath );	

	/**
	 * Finds all files or folders in the given directory.
	 * This is partially copied from file manager but since IPlatformFile is lower level
	 * it's better to have local version which doesn't use the wrapped IPlatformFile.
	 *
	 * @param Result List of found files or folders.
	 * @param InFilename Path to the folder to look in.
	 * @param Files true to include files in the Result
	 * @param Files true to include directories in the Result
	 */
	void FindFiles( TArray<FString>& Result, const TCHAR* InFilename, bool Files, bool Directories );
	
	/** Allow IPlatformFile::FindFiles */
	using IPlatformFile::FindFiles;

	/**
	 * Deletes a directory
	 * This is partially copied from file manager but since IPlatformFile is lower level
	 * it's better to have local version which doesn't use the wrapped IPlatformFile.
	 *
	 * @param Path Path to the directory to delete.
	 * @param Tree true to recursively delete the directory and its contents
	 * @return true if the operaton was successful.
	 */
	bool DeleteDirectory( const TCHAR* Path, bool Tree );

	/**
	 * Check if a file or directory has been filtered, and hence is unavailable to the outside world
	 *
	 * @param FilenameOrDirectoryName 
	 * @param bIsDirectory if true, this is a directory
	 * @return true if it is ok to access the non-sandboxed files here
	 */
	bool OkForInnerAccess(const TCHAR* InFilenameOrDirectoryName, bool bIsDirectory = false) const
	{
		if (DirectoryExclusionWildcards.Num() || FileExclusionWildcards.Num())
		{
			FString FilenameOrDirectoryName(InFilenameOrDirectoryName);
			FPaths::MakeStandardFilename(FilenameOrDirectoryName);
			if (bIsDirectory)
			{
				for (int32 Index = 0; Index < DirectoryExclusionWildcards.Num(); Index++)
				{
					if (FilenameOrDirectoryName.MatchesWildcard(DirectoryExclusionWildcards[Index]))
					{
						return false;
	 				}
				}
			}
			else
			{
				for (int32 Index = 0; Index < FileExclusionWildcards.Num(); Index++)
				{
					if (FilenameOrDirectoryName.MatchesWildcard(FileExclusionWildcards[Index]))
					{
						return false;
					}
				}
			}
		}
		return true;
	}

	
public:

	static const TCHAR* GetTypeName()
	{
		return TEXT("SandboxFile");
	}

	/**
	 * Converts passed in filename to use a sandbox path.
	 * @param	InLowerLevel						File system to use for actual disk access
	 * @param	InSandboxDirectory					Directory to use for the sandbox, reads are tried here first and all deletes and writes go here
	 * @param	bEntireEngineWillUseThisSandbox		If true, the we set up the engine so that subprocesses also use this subdirectory
	 */
	FSandboxPlatformFile(bool bInEntireEngineWillUseThisSandbox = false);

	virtual ~FSandboxPlatformFile()
	{
	}

	/**
	 *	Set whether the sandbox is enabled or not
	 *
	 *	@param	bInEnabled		true to enable the sandbox, false to disable it
	 */
	virtual void SetSandboxEnabled(bool bInEnabled) override
	{
		bSandboxEnabled = bInEnabled;
	}

	/**
	 *	Returns whether the sandbox is enabled or not
	 *
	 *	@return	bool			true if enabled, false if not
	 */
	virtual bool IsSandboxEnabled() const override
	{
		return bSandboxEnabled;
	}

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override;
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;

	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}

	virtual const TCHAR* GetName() const override
	{
		return FSandboxPlatformFile::GetTypeName();
	}

	/**
	 * Converts passed in filename to use a sandbox path.
	 *
	 * @param	Filename	filename (under game directory) to convert to use a sandbox path. Can be relative or absolute.
	 * 
	 * @return	filename using sandbox path
	 */
	FString ConvertToSandboxPath( const TCHAR* Filename ) const;

	FString ConvertFromSandboxPath(const TCHAR* Filename) const;


	/** Returns sandbox directory */
	const FString& GetSandboxDirectory() const
	{
		return SandboxDirectory;
	}

	/** Returns the name of the sandbox directory for the game's content */
	const FString& GetGameSandboxDirectoryName();

	/** Returns absolute root directory */
	const FString& GetAbsoluteRootDirectory() const
	{
		return AbsoluteRootDirectory;
	}

	/** Returns absolute game directory */
	const FString& GetAbsoluteGameDirectory();

	/** Returns absolute path to game directory (without the game directory itself) */
	const FString& GetAbsolutePathToGameDirectory();

	/** 
	 * Add exclusion. These files and / or directories pretend not to exist so that they cannot be accessed at all (except in the sandbox) 
	 * @param Wildcard FString::MatchesWildcard-type wild card to test for exclusion
	 * @param bIsDirectory if true, this is a directory
	 * @Caution, these have a performance cost
	*/
	void AddExclusion(const TCHAR* Wildcard, bool bIsDirectory = false)
	{
		if (bIsDirectory)
		{
			DirectoryExclusionWildcards.AddUnique(FString(Wildcard));
		}
		else
		{
			FileExclusionWildcards.AddUnique(FString(Wildcard));
		}
	}

	// IPlatformFile Interface

	virtual bool		FileExists(const TCHAR* Filename) override
	{
		// First look for the file in the user dir.
		bool Result = LowerLevel->FileExists( *ConvertToSandboxPath( Filename ) );
		if( Result == false && OkForInnerAccess(Filename))
		{
			Result = LowerLevel->FileExists( Filename );
		}
		return Result;
	}

	virtual int64		FileSize(const TCHAR* Filename) override
	{
		// First look for the file in the user dir.
		int64 Result = LowerLevel->FileSize( *ConvertToSandboxPath( Filename ) );
		if( Result < 0  && OkForInnerAccess(Filename))
		{
			Result = LowerLevel->FileSize( Filename );
		}
		return Result;
	}

	virtual bool		DeleteFile(const TCHAR* Filename) override
	{
		// Delete only sandbox files. If the sendbox version doesn't exists
		// assume the delete was successful because we only care if the sandbox version is gone.
		bool Result = true;
		FString SandboxFilename( *ConvertToSandboxPath( Filename ) );
		if( LowerLevel->FileExists( *SandboxFilename ) )
		{
			Result = LowerLevel->DeleteFile( *ConvertToSandboxPath( Filename ) );
		}
		return Result;
	}

	virtual bool		IsReadOnly(const TCHAR* Filename) override
	{
		// If the file exists in the sandbox folder and is read-only return true
		// Otherwise it can always be 'overwritten' in the sandbox
		bool Result = false;
		FString SandboxFilename( *ConvertToSandboxPath( Filename ) );
		if(  LowerLevel->FileExists( *SandboxFilename ) )
		{
			// If the file exists in sandbox dir check its read-only flag
			Result = LowerLevel->IsReadOnly( *SandboxFilename );
		}
		//else
		//{
		//	// Fall back to normal directory
		//	Result = LowerLevel->IsReadOnly( Filename );
		//}
		return Result;
	}

	virtual bool		MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		// Only files within the sandbox dir can be moved
		bool Result = false;
		FString SandboxFilename( *ConvertToSandboxPath( From ) );
		if( LowerLevel->FileExists( *SandboxFilename ) )
		{
			Result = LowerLevel->MoveFile( *ConvertToSandboxPath( To ), *SandboxFilename );
		}
		return Result;
	}

	virtual bool		SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		bool Result = false;
		FString UserFilename( *ConvertToSandboxPath( Filename ) );
		if( LowerLevel->FileExists( *UserFilename ) )
		{
			Result = LowerLevel->SetReadOnly( *UserFilename, bNewReadOnlyValue );
		}
		return Result;
	}

	virtual FDateTime		GetTimeStamp(const TCHAR* Filename) override
	{
		FDateTime Result = FDateTime::MinValue();
		FString UserFilename( *ConvertToSandboxPath( Filename ) );
		if( LowerLevel->FileExists( *UserFilename ) )
		{
			Result = LowerLevel->GetTimeStamp( *UserFilename );
		}
		else if (OkForInnerAccess(Filename))
		{
			Result = LowerLevel->GetTimeStamp( Filename );
		}
		return Result;
	}

	virtual void 			SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		FString UserFilename( *ConvertToSandboxPath( Filename ) );
		if( LowerLevel->FileExists( *UserFilename ) )
		{
			LowerLevel->SetTimeStamp( *UserFilename, DateTime );
		}
		else if (OkForInnerAccess(Filename))
		{
			LowerLevel->SetTimeStamp( Filename, DateTime );
		}
	}

	virtual FDateTime		GetAccessTimeStamp(const TCHAR* Filename) override
	{
		FDateTime Result = FDateTime::MinValue();
		FString UserFilename( *ConvertToSandboxPath( Filename ) );
		if( LowerLevel->FileExists( *UserFilename ) )
		{
			Result = LowerLevel->GetAccessTimeStamp( *UserFilename );
		}
		else if (OkForInnerAccess(Filename))
		{
			Result = LowerLevel->GetAccessTimeStamp( Filename );
		}
		return Result;
	}

	virtual FString	GetFilenameOnDisk(const TCHAR* Filename) override
	{
		FString Result;
		FString UserFilename(*ConvertToSandboxPath(Filename));
		if (LowerLevel->FileExists(*UserFilename))
		{
			Result = LowerLevel->GetFilenameOnDisk(*UserFilename);
		}
		else if (OkForInnerAccess(Filename))
		{
			Result = LowerLevel->GetFilenameOnDisk(Filename);
		}
		return Result;
	}

	virtual IFileHandle*	OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		IFileHandle* Result = LowerLevel->OpenRead( *ConvertToSandboxPath(Filename), bAllowWrite );
		if( !Result  && OkForInnerAccess(Filename) )
		{
			Result = LowerLevel->OpenRead( Filename );
		}
		return Result;
	}

	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
	{
		// Only files from the sandbox directory can be opened for wiriting
		return LowerLevel->OpenWrite( *ConvertToSandboxPath( Filename ), bAppend, bAllowRead );
	}

	virtual bool		DirectoryExists(const TCHAR* Directory) override
	{
		bool Result = LowerLevel->DirectoryExists( *ConvertToSandboxPath( Directory ) );
		if( Result == false && OkForInnerAccess(Directory, true) )
		{
			Result = LowerLevel->DirectoryExists( Directory ); 
		}
		return Result;
	}

	virtual bool		CreateDirectory(const TCHAR* Directory) override
	{
		// Directories can be created only under the sandbox path
		return LowerLevel->CreateDirectory( *ConvertToSandboxPath( Directory ) );
	}

	virtual bool		DeleteDirectory(const TCHAR* Directory) override
	{
		// Directories can be deleted only under the sandbox path
		return LowerLevel->DeleteDirectory( *ConvertToSandboxPath( Directory ) );
	}

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		FFileStatData Result = LowerLevel->GetStatData( *ConvertToSandboxPath( FilenameOrDirectory ) );
		if (!Result.bIsValid && (OkForInnerAccess(FilenameOrDirectory, false) && OkForInnerAccess(FilenameOrDirectory, true)))
		{
			Result = LowerLevel->GetStatData( FilenameOrDirectory );
		}
		return Result;
	}

	class FSandboxVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FDirectoryVisitor&	Visitor;
		FSandboxPlatformFile& SandboxFile;
		TSet<FString> VisitedSandboxFiles;

		FSandboxVisitor( FDirectoryVisitor& InVisitor, FSandboxPlatformFile& InSandboxFile )
			: Visitor( InVisitor )
			, SandboxFile( InSandboxFile )
		{
		}
		virtual bool Visit( const TCHAR* FilenameOrDirectory, bool bIsDirectory ) override
		{
			bool CanVisit = true;
			FString LocalFilename( FilenameOrDirectory );
			
			if( FCString::Strnicmp( *LocalFilename, *SandboxFile.GetSandboxDirectory(), SandboxFile.GetSandboxDirectory().Len() ) == 0 )
			{
				// FilenameOrDirectory is already pointing to the sandbox directory so add it to the list of sanbox files.
				// The filename is always stored with the abslute sandbox path.
				VisitedSandboxFiles.Add( *LocalFilename );
				// Now convert the sandbox path back to engine path because the sandbox folder should not be exposed
				// to the engine and remain transparent.
				LocalFilename = LocalFilename.Mid(SandboxFile.GetSandboxDirectory().Len());
				if (LocalFilename.StartsWith(TEXT("Engine/")) || (FCString::Stricmp( *LocalFilename, TEXT("Engine") ) == 0))
				{
					LocalFilename = SandboxFile.GetAbsoluteRootDirectory() / LocalFilename;
				}
				else
				{
					LocalFilename = LocalFilename.Mid(SandboxFile.GetGameSandboxDirectoryName().Len());
					LocalFilename = SandboxFile.GetAbsoluteGameDirectory() / LocalFilename;
				}
			}
			else
			{
				// Favourize Sandbox files over normal path files.
				CanVisit = !VisitedSandboxFiles.Contains( SandboxFile.ConvertToSandboxPath(*LocalFilename ) )
					&& SandboxFile.OkForInnerAccess(*LocalFilename, bIsDirectory);
			}
			if( CanVisit )
			{
				bool Result = Visitor.Visit( *LocalFilename, bIsDirectory );
				return Result;
			}
			else
			{
				// Continue iterating.
				return true;
			}
		}
	};

	virtual bool		IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override
	{
		FSandboxVisitor SandboxVisitor( Visitor, *this );
		bool Result = false;
		LowerLevel->IterateDirectory( *ConvertToSandboxPath( Directory ), SandboxVisitor );
		Result = LowerLevel->IterateDirectory( Directory, SandboxVisitor );
		return Result;
	}

	virtual bool		IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override
	{
		FSandboxVisitor SandboxVisitor( Visitor, *this );
		bool Result = false;
		LowerLevel->IterateDirectoryRecursively( *ConvertToSandboxPath( Directory ), SandboxVisitor );
		Result = LowerLevel->IterateDirectoryRecursively( Directory, SandboxVisitor );
		return Result;
	}

	class FSandboxStatVisitor : public IPlatformFile::FDirectoryStatVisitor
	{
	public:
		FDirectoryStatVisitor&	Visitor;
		FSandboxPlatformFile& SandboxFile;
		TSet<FString> VisitedSandboxFiles;

		FSandboxStatVisitor( FDirectoryStatVisitor& InVisitor, FSandboxPlatformFile& InSandboxFile )
			: Visitor( InVisitor )
			, SandboxFile( InSandboxFile )
		{
		}
		virtual bool Visit( const TCHAR* FilenameOrDirectory, const FFileStatData& StatData ) override
		{
			bool CanVisit = true;
			FString LocalFilename( FilenameOrDirectory );
			
			if( FCString::Strnicmp( *LocalFilename, *SandboxFile.GetSandboxDirectory(), SandboxFile.GetSandboxDirectory().Len() ) == 0 )
			{
				// FilenameOrDirectory is already pointing to the sandbox directory so add it to the list of sanbox files.
				// The filename is always stored with the abslute sandbox path.
				VisitedSandboxFiles.Add( *LocalFilename );
				// Now convert the sandbox path back to engine path because the sandbox folder should not be exposed
				// to the engine and remain transparent.
				LocalFilename = LocalFilename.Mid(SandboxFile.GetSandboxDirectory().Len());
				if (LocalFilename.StartsWith(TEXT("Engine/")))
				{
					LocalFilename = SandboxFile.GetAbsoluteRootDirectory() / LocalFilename;
				}
				else
				{
					LocalFilename = SandboxFile.GetAbsolutePathToGameDirectory() / LocalFilename;
				}
			}
			else
			{
				// Favourize Sandbox files over normal path files.
				CanVisit = !VisitedSandboxFiles.Contains( SandboxFile.ConvertToSandboxPath(*LocalFilename ) )
					&& SandboxFile.OkForInnerAccess(*LocalFilename, StatData.bIsDirectory);
			}
			if( CanVisit )
			{
				bool Result = Visitor.Visit( *LocalFilename, StatData );
				return Result;
			}
			else
			{
				// Continue iterating.
				return true;
			}
		}
	};

	virtual bool		IterateDirectoryStat(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor) override
	{
		FSandboxStatVisitor SandboxVisitor( Visitor, *this );
		bool Result = false;
		LowerLevel->IterateDirectoryStat( *ConvertToSandboxPath( Directory ), SandboxVisitor );
		Result = LowerLevel->IterateDirectoryStat( Directory, SandboxVisitor );
		return Result;
	}

	virtual bool		IterateDirectoryStatRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor) override
	{
		FSandboxStatVisitor SandboxVisitor( Visitor, *this );
		bool Result = false;
		LowerLevel->IterateDirectoryStatRecursively( *ConvertToSandboxPath( Directory ), SandboxVisitor );
		Result = LowerLevel->IterateDirectoryStatRecursively( Directory, SandboxVisitor );
		return Result;
	}

	virtual bool		DeleteDirectoryRecursively(const TCHAR* Directory) override
	{
		// Directories can be deleted only under the sandbox path
		return LowerLevel->DeleteDirectoryRecursively( *ConvertToSandboxPath( Directory ) );
	}

	virtual bool CreateDirectoryTree(const TCHAR* Directory) override
	{
		// Directories can only be created only under the sandbox path
		return LowerLevel->CreateDirectoryTree( *ConvertToSandboxPath( Directory ) );
	}

	virtual bool		CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None) override
	{
		// Files can be copied only to the sandbox directory
		bool Result = false;
		if( LowerLevel->FileExists( *ConvertToSandboxPath( From ) ) )
		{
			Result = LowerLevel->CopyFile( *ConvertToSandboxPath( To ), *ConvertToSandboxPath( From ), ReadFlags, WriteFlags);
		}
		else
		{
			Result = LowerLevel->CopyFile( *ConvertToSandboxPath( To ), From, ReadFlags, WriteFlags);
		}
		return Result;
	}

	virtual FString ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename ) override;
	virtual FString ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename ) override;

	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override
	{
		FString UserFilename(*ConvertToSandboxPath(Filename));
		if (!OkForInnerAccess(Filename) || LowerLevel->FileExists(*UserFilename))
		{
			return LowerLevel->OpenAsyncRead(*UserFilename);
		}
		return LowerLevel->OpenAsyncRead(Filename);
	}

};


