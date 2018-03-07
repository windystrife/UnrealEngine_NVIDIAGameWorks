// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidFile.cpp: Android platform implementations of File functions
=============================================================================*/

#include "AndroidFile.h"
#include "Misc/App.h"
#include "Misc/Paths.h"

#include <dirent.h>
#include <jni.h>
#include <unistd.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/storage_manager.h>
#include "AndroidJava.h"
#include "Map.h"
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogAndroidFile, Log, All);

#define LOG_ANDROID_FILE 0

#define LOG_ANDROID_FILE_MANIFEST 0

// Support 64 bit file access.
#define UE_ANDROID_FILE_64 0
// #define UE_ANDROID_FILE_64 1

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime AndroidEpoch(1970, 1, 1);

namespace
{
	FFileStatData AndroidStatToUEFileData(struct stat& FileInfo)
	{
		const bool bIsDirectory = S_ISDIR(FileInfo.st_mode);

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			FileSize = FileInfo.st_size;
		}

		return FFileStatData(
			AndroidEpoch + FTimespan::FromSeconds(FileInfo.st_ctime), 
			AndroidEpoch + FTimespan::FromSeconds(FileInfo.st_atime), 
			AndroidEpoch + FTimespan::FromSeconds(FileInfo.st_mtime), 
			FileSize,
			bIsDirectory,
			!!(FileInfo.st_mode & S_IWUSR)
		);
	}
}

#define USE_UTIME 0


// AndroidProcess uses this for executable name
FString GAndroidProjectName;
FString GPackageName;
int32 GAndroidPackageVersion = 0;
int32 GAndroidPackagePatchVersion = 0;

// External File Path base - setup during load
FString GFilePathBase;
// Obb File Path base - setup during load
FString GOBBFilePathBase;
// External File Direcory Path (for application) - setup during load
FString GExternalFilePath;
// External font path base - setup during load
FString GFontPathBase;

// Is the OBB in an APK file or not
bool GOBBinAPK;
FString GAPKFilename;

#define FILEBASE_DIRECTORY "/UE4Game/"

extern jobject AndroidJNI_GetJavaAssetManager();
extern AAssetManager * AndroidThunkCpp_GetAssetManager();

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeSetObbInfo(String PackageName, int Version, int PatchVersion);"
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeSetObbInfo(JNIEnv* jenv, jobject thiz, jstring ProjectName, jstring PackageName, jint Version, jint PatchVersion)
{
	const char* JavaProjectChars = jenv->GetStringUTFChars(ProjectName, 0);
	GAndroidProjectName = UTF8_TO_TCHAR(JavaProjectChars);
	const char* JavaPackageChars = jenv->GetStringUTFChars(PackageName, 0);
	GPackageName = UTF8_TO_TCHAR(JavaPackageChars);
	GAndroidPackageVersion = Version;
	GAndroidPackagePatchVersion = PatchVersion;

	//Release the strings
	jenv->ReleaseStringUTFChars(ProjectName, JavaProjectChars);
	jenv->ReleaseStringUTFChars(PackageName, JavaPackageChars);
}

// Constructs the base path for any files which are not in OBB/pak data
const FString &GetFileBasePath()
{
	static FString BasePath = GFilePathBase + FString(FILEBASE_DIRECTORY) + FApp::GetProjectName() + FString("/");
	return BasePath;
}

/**
	Android file handle implementation for partial (i.e. parcels) files.
	This can handle all the variations of accessing data for assets and files.
**/
class CORE_API FFileHandleAndroid : public IFileHandle
{
	enum { READWRITE_SIZE = 1024 * 1024 };

	void LogInfo()
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("FFileHandleAndroid => Asset = %p, Handle = %d, Bounds = [%d,%d)"),
			File->Asset, File->Handle, int32(Start), int32(Start+Length));
#endif
	}

public:

	struct FileReference
	{
		FString Path;
		AAsset * Asset;
		int32 Handle;

		FileReference()
			: Asset(nullptr), Handle(-1)
		{
		}

		FileReference(const FString & path, AAsset * asset)
			: Path(path), Asset(asset), Handle(0)
		{
		}

		FileReference(const FString & path, int32 handle)
			: Path(path), Asset(nullptr), Handle(handle)
		{
		}

		~FileReference()
		{
			if (Handle != -1)
			{
				close(Handle);
			}
			if (nullptr != Asset)
			{
				AAsset_close(Asset);
			}
		}
	};

	TSharedPtr<FileReference> File;
	int64 Start;
	int64 Length;
	int64 CurrentOffset;

	FORCEINLINE void CheckValid()
	{
		check(File.IsValid() && File->Handle != -1);
	}

	// Invalid handle.
	FFileHandleAndroid()
		: File(MakeShareable(new FileReference()))
		, Start(0), Length(0), CurrentOffset(0)
	{
	}

	// Handle that covers a subsegment of another handle.
	FFileHandleAndroid(const FFileHandleAndroid & base,
		int64 start, int64 length)
		: File(base.File)
		, Start(base.Start + start), Length(length)
		, CurrentOffset(base.Start + start)
	{
		CheckValid();
		LogInfo();
	}

	// Handle that covers a subsegment of provided file.
	FFileHandleAndroid(const FString & path, int32 filehandle, int64 filestart, int64 filelength)
		: File(MakeShareable(new FileReference(path, filehandle)))
		, Start(filestart), Length(filelength), CurrentOffset(0)
	{
		CheckValid();
#if UE_ANDROID_FILE_64
		lseek64(File->Handle, filestart, SEEK_SET);
#else
		lseek(File->Handle, filestart, SEEK_SET);
#endif
		LogInfo();
	}

	// Handle that covers the entire file content.
	FFileHandleAndroid(const FString & path, int32 filehandle)
		: File(MakeShareable(new FileReference(path, filehandle)))
		, Start(0), Length(0), CurrentOffset(0)
	{
		CheckValid();
#if UE_ANDROID_FILE_64
		Length = lseek64(File->Handle, 0, SEEK_END);
		lseek64(File->Handle, 0, SEEK_SET);
#else
		Length = lseek(File->Handle, 0, SEEK_END);
		lseek(File->Handle, 0, SEEK_SET);
#endif
		LogInfo();
	}

	// Handle that covers the entire content of an asset.
	FFileHandleAndroid(const FString & path, AAsset * asset)
		: File(MakeShareable(new FileReference(path, asset)))
		, Start(0), Length(0), CurrentOffset(0)
	{
#if UE_ANDROID_FILE_64
		File->Handle = AAsset_openFileDescriptor64(File->Asset, &Start, &Length);
#else
		off_t OutStart = Start;
		off_t OutLength = Length;
		File->Handle = AAsset_openFileDescriptor(File->Asset, &OutStart, &OutLength);
		Start = OutStart;
		Length = OutLength;
#endif
		CheckValid();
		LogInfo();
	}

	virtual ~FFileHandleAndroid()
	{
	}

	virtual int64 Tell() override
	{
		CheckValid();
		int64 pos = CurrentOffset;
		check(pos != -1);
		return pos - Start; // We are treating 'tell' as a virtual location from file Start
	}

	virtual bool Seek(int64 NewPosition) override
	{
		CheckValid();
		// we need to offset all positions by the Start offset
		CurrentOffset = NewPosition += Start;
		check(NewPosition >= 0);
		
		return true;
	}

    virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		CheckValid();
		check(NewPositionRelativeToEnd <= 0);
		// We need to convert this to a virtual offset inside the file we are interested in
		CurrentOffset = Start + (Length - NewPositionRelativeToEnd);
		return true;
	}

	

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		CheckValid();
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("(%d/%d) FFileHandleAndroid:Read => Path = %s, BytesToRead = %d"),
			FAndroidTLS::GetCurrentThreadId(), File->Handle,
			*(File->Path), int32(BytesToRead));
#endif
		check(BytesToRead >= 0);
		check(Destination);
		
		while (BytesToRead > 0)
		{

			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToRead);
			
			ThisSize = pread(File->Handle, Destination, ThisSize, CurrentOffset);
#if LOG_ANDROID_FILE
			FPlatformMisc::LowLevelOutputDebugStringf(
				TEXT("(%d/%d) FFileHandleAndroid:Read => Path = %s, ThisSize = %d, destination = %X"),
				FAndroidTLS::GetCurrentThreadId(), File->Handle,
				*(File->Path), int32(ThisSize), Destination);
#endif
			if (ThisSize < 0)
			{
				return false;
			}
			else if (ThisSize == 0)
			{
				break;
			}
			CurrentOffset += ThisSize;
			Destination += ThisSize;
			BytesToRead -= ThisSize;
		}

		return BytesToRead == 0;
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		CheckValid();
		if (nullptr != File->Asset)
		{
			// Can't write to assets.
			return false;
		}

		bool bSuccess = true;
		while (BytesToWrite)
		{
			check(BytesToWrite >= 0);
			int64 ThisSize = FMath::Min<int64>(READWRITE_SIZE, BytesToWrite);
			check(Source);
			if (pwrite(File->Handle, Source, ThisSize, CurrentOffset) != ThisSize)
			{
				bSuccess = false;
				break;
			}
			CurrentOffset += ThisSize;
			Source += ThisSize;
			BytesToWrite -= ThisSize;
		}
		
		// Update the cached file length
		Length = FMath::Max(Length, CurrentOffset);

		return bSuccess;
	}

	virtual void Flush() override
	{
		CheckValid();
		if (nullptr != File->Asset)
		{
			// Can't write to assets.
			return;
		}

		fsync(File->Handle);
	}

	virtual int64 Size() override
	{
		return Length;
	}
};


class FAndroidFileManifestReader
{
private:
	bool bInitialized;
	FString ManifestFileName;
	TMap<FString, FDateTime> ManifestEntries;
public:

	FAndroidFileManifestReader( const FString& InManifestFileName ) : ManifestFileName(InManifestFileName), bInitialized(false)
	{
	}

	bool GetFileTimeStamp( const FString& FileName, FDateTime& DateTime ) 
	{
		if ( bInitialized == false )
		{
			Read();
			bInitialized = true;
		}

		const FDateTime* Result = ManifestEntries.Find( FileName );
		if ( Result )
		{
			DateTime = *Result;
#if LOG_ANDROID_FILE_MANIFEST
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Found time stamp for '%s', %s"), *FileName, *DateTime.ToString());
#endif
			return true;
		}
#if LOG_ANDROID_FILE_MANIFEST
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Didn't find time stamp for '%s'"), *FileName);
#endif
		return false;
	}


	bool SetFileTimeStamp( const FString& FileName, const FDateTime& DateTime )
	{
		if (bInitialized == false)
		{
			Read();
			bInitialized = true;
		}

		FDateTime* Result = ManifestEntries.Find( FileName );
		if ( Result == NULL )
		{
			ManifestEntries.Add(FileName, DateTime );
		}
		else
		{
			*Result = DateTime;
		}
#if LOG_ANDROID_FILE_MANIFEST
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SetFileTimeStamp '%s', %s"), *FileName, *DateTime.ToString());
#endif
		return true;
	}

	// read manifest from disk
	void Read()
	{
		// Local filepaths are directly in the deployment directory.
		static const FString &BasePath = GetFileBasePath();
		const FString ManifestPath = BasePath + ManifestFileName;

		ManifestEntries.Empty();

		// int Handle = open( TCHAR_TO_UTF8(ManifestFileName), O_RDWR );
		int Handle = open( TCHAR_TO_UTF8(*ManifestPath), O_RDONLY );

		if ( Handle == -1 )
		{
#if LOG_ANDROID_FILE_MANIFEST
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to open file for read'%s'"), *ManifestFileName);
#endif
			return;
		}

		FString EntireFile;
		char Buffer[1024];
		Buffer[1023] = '\0';
		int BytesRead = 1023;
		while ( BytesRead == 1023 )
		{
			BytesRead = read(Handle, Buffer, 1023);
			check( Buffer[1023] == '\0');
			EntireFile.Append(FString(UTF8_TO_TCHAR(Buffer)));
		}

		close( Handle );

		TArray<FString> Lines;
		EntireFile.ParseIntoArrayLines(Lines);

#if LOG_ANDROID_FILE_MANIFEST
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Loaded manifest file %s"), *ManifestFileName);
		for( const auto& Line : Lines )
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Manifest Line %s"), *Line );
		}
#endif
		for ( const auto& Line : Lines) 
		{
#if LOG_ANDROID_FILE_MANIFEST
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Processing line '%s' "), *Line);
#endif
			FString Filename;
			FString DateTimeString;
			if ( Line.Split(TEXT("\t"), &Filename, &DateTimeString) )
			{
				FDateTime ModifiedDate;
				if ( FDateTime::ParseIso8601(*DateTimeString, ModifiedDate) )
				{
#if LOG_ANDROID_FILE_MANIFEST
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Read time stamp '%s' %s"), *Filename, *ModifiedDate.ToString());
#endif
					Filename.ReplaceInline( TEXT("\\"), TEXT("/") );
					ManifestEntries.Emplace( MoveTemp(Filename), ModifiedDate );
				}
				else
				{
#if LOG_ANDROID_FILE_MANIFEST
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to parse date for file '%s' %s"), *Filename, *DateTimeString);
#endif					
				}
			}
#if LOG_ANDROID_FILE_MANIFEST
			else
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Unable to split line '%s'"), *Line);
			}
#endif
		}
	}

	void Write()
	{
		
		// Local filepaths are directly in the deployment directory.
		static const FString &BasePath = GetFileBasePath();
		const FString ManifestPath = BasePath + ManifestFileName;


		int Handle = open(TCHAR_TO_UTF8(*ManifestPath), O_WRONLY | O_CREAT | O_TRUNC);

		if ( Handle == -1 )
		{
#if LOG_ANDROID_FILE_MANIFEST
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to open file for write '%s'"), *ManifestFileName);
#endif
			return;
		}


		for ( const auto& Line : ManifestEntries )
		{
			const int BufferSize = 4096;
			char Buffer[BufferSize] = {"\0"}; 
			const FString RawDateTimeString = Line.Value.ToIso8601();
			const FString DateTimeString = FString::Printf(TEXT("%s\r\n"), *RawDateTimeString);
			strncpy(Buffer, TCHAR_TO_UTF8(*Line.Key), BufferSize-1);
			strncat(Buffer, "\t", BufferSize-1 );
			strncat(Buffer, TCHAR_TO_UTF8(*DateTimeString), BufferSize-1 );
			write( Handle, Buffer, strlen( Buffer ) );
		}

		close( Handle );
	}
};

FAndroidFileManifestReader NonUFSManifest(TEXT("Manifest_NonUFSFiles_Android.txt"));
FAndroidFileManifestReader UFSManifest(TEXT("Manifest_UFSFiles_Android.txt"));

/*
	Access to files in multiple ZIP archives.
*/
class FZipUnionFile
{
public:
	struct Entry
	{
		TSharedPtr<FFileHandleAndroid> File;
		FString FileName;
		int32 ModTime;

		Entry(TSharedPtr<FFileHandleAndroid> file, const FString & filename, int32 modtime = 0)
			: File(file), FileName(filename), ModTime(modtime)
		{
		}
	};

	typedef TMap<FString, TSharedPtr<Entry>> FEntryMap;

	struct Directory
	{
		FEntryMap::TIterator Current;
		FString Path;

		Directory(FEntryMap & entries, const FString & dirpath)
			: Current(entries.CreateIterator()), Path(dirpath)
		{
			if (!Path.IsEmpty())
			{
				Path /= "";
			}
			// This would be much easier, and efficient, if TMap
			// supported getting iterators to found entries in
			// the map. Instead we need to iterate the entire
			// map to find the initial directory entry.
			while (Current && Current.Key() != Path)
			{
				++Current;
			}
		}

		bool Next()
		{
			for (++Current; Current; ++Current)
			{
				if (Current.Key().StartsWith(Path))
				{
					int32 i = -1;
					Current.Key().FindLastChar('/', i);
					if (i == Path.Len() - 1)
					{
						break;
					}
				}
			}
			return !!Current;
		}
	};

	FZipUnionFile()
	{
	}

	~FZipUnionFile()
	{
	}

	void AddPatchFile(TSharedPtr<FFileHandleAndroid> file)
	{
		int64 FileLength = file->Size();

		// Is it big enough to be a ZIP?
		check( FileLength >= kEOCDLen );

		int64 ReadAmount = kMaxEOCDSearch;
		if (ReadAmount > FileLength)
		{
			ReadAmount = FileLength;
		}

		// Check magic signature for ZIP.
		file->Seek(0);
		uint32 Header;
		verify( file->Read((uint8*)&Header, sizeof(Header)) );
		check( Header != kEOCDSignature );
		check( Header == kLFHSignature );

		/*
		* Perform the traditional EOCD snipe hunt. We're searching for the End
		* of Central Directory magic number, which appears at the start of the
		* EOCD block. It's followed by 18 bytes of EOCD stuff and up to 64KB of
		* archive comment. We need to read the last part of the file into a
		* buffer, dig through it to find the magic number, parse some values
		* out, and use those to determine the extent of the CD. We start by
		* pulling in the last part of the file.
		*/
		int64 SearchStart = FileLength - ReadAmount;
		ByteBuffer Buffer(ReadAmount);
		verify( file->Seek(SearchStart) );
		verify( file->Read(Buffer.Data, Buffer.Size) );

		/*
		* Scan backward for the EOCD magic. In an archive without a trailing
		* comment, we'll find it on the first try. (We may want to consider
		* doing an initial minimal read; if we don't find it, retry with a
		* second read as above.)
		*/
		int64 EOCDIndex = Buffer.Size - kEOCDLen;
		for (; EOCDIndex >= 0; --EOCDIndex)
		{
			if (Buffer.GetValue<uint32>(EOCDIndex) == kEOCDSignature)
			{
				break;
			}
		}
		check( EOCDIndex >= 0 );

		/*
		* Grab the CD offset and size, and the number of entries in the
		* archive.
		*/
		uint16 NumEntries = (Buffer.GetValue<uint16>(EOCDIndex + kEOCDNumEntries));
		int64 DirSize = (Buffer.GetValue<uint32>(EOCDIndex + kEOCDSize));
		int64 DirOffset = (Buffer.GetValue<uint32>(EOCDIndex + kEOCDFileOffset));
		check( DirOffset + DirSize <= FileLength );
		check( NumEntries > 0 );

		/*
		* Walk through the central directory, adding entries to the hash table.
		*/
		FFileHandleAndroid DirectoryMap(*file, DirOffset, DirSize);
		int64 Offset = 0;
		for (uint16 EntryIndex = 0; EntryIndex < NumEntries; ++EntryIndex)
		{
			uint32 Signature;
			verify( DirectoryMap.Seek(Offset) );
			verify( DirectoryMap.Read((uint8*)&Signature, sizeof(Signature)) );

			// NumEntries may be 65535 so also stop if signature invalid.
			if (Signature != kCDESignature)
			{
				// Hit the end of the central directory, stop.
				break;
			}

			// Entry information. Note, we try and read in incremental
			// order to avoid missing read-aheads.

			uint16 Method;
			verify( DirectoryMap.Seek(Offset + kCDEMethod) );
			verify( DirectoryMap.Read((uint8*)&Method, sizeof(Method)) );

			int32 WhenModified;
			verify( DirectoryMap.Seek(Offset + kCDEModWhen) );
			verify( DirectoryMap.Read((uint8*)&WhenModified, sizeof(WhenModified)) );

			uint32 UncompressedLength;
			verify( DirectoryMap.Seek(Offset + kCDEUncompLen) );
			verify( DirectoryMap.Read((uint8*)&UncompressedLength, sizeof(UncompressedLength)) );

			uint16 FileNameLen;
			verify( DirectoryMap.Seek(Offset + kCDENameLen) );
			verify( DirectoryMap.Read((uint8*)&FileNameLen, sizeof(FileNameLen)) );

			uint16 ExtraLen;
			verify( DirectoryMap.Seek(Offset + kCDEExtraLen) );
			verify( DirectoryMap.Read((uint8*)&ExtraLen, sizeof(ExtraLen)) );

			uint16 CommentLen;
			verify( DirectoryMap.Seek(Offset + kCDECommentLen) );
			verify( DirectoryMap.Read((uint8*)&CommentLen, sizeof(CommentLen)) );

			// We only add uncompressed entries as we don't support decompression.
			if (Method == kCompressStored)
			{
				uint32 LocalOffset;
				verify( DirectoryMap.Seek(Offset + kCDELocalOffset) );
				verify( DirectoryMap.Read((uint8*)&LocalOffset, sizeof(LocalOffset)) );

				ByteBuffer FileNameBuffer(FileNameLen+1);
				verify( DirectoryMap.Seek(Offset + kCDELen) );
				verify( DirectoryMap.Read(FileNameBuffer.Data, FileNameBuffer.Size) );
				FileNameBuffer.Data[FileNameBuffer.Size-1] = 0;
				FString FileName(UTF8_TO_TCHAR(FileNameBuffer.Data));

				verify( file->Seek(LocalOffset) );

				uint32 LocalSignature;
				verify( file->Read((uint8*)&LocalSignature, sizeof(LocalSignature)) );

				uint16 LocalFileNameLen;
				verify( file->Seek(LocalOffset + kLFHNameLen) );
				verify( file->Read((uint8*)&LocalFileNameLen, sizeof(LocalFileNameLen)) );

				uint16 LocalExtraLen;
				verify( file->Seek(LocalOffset + kLFHExtraLen) );
				verify( file->Read((uint8*)&LocalExtraLen, sizeof(LocalExtraLen)) );

				int64 EntryOffset = LocalOffset + kLFHLen + LocalFileNameLen + LocalExtraLen;

				// Add the entry.
#if LOG_ANDROID_FILE
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FUnionZipFile::AddPatchFile.. FILE: '%s'"), *FileName);
#endif
				Entries.Add(
					FileName,
					MakeShareable(new Entry(
						MakeShareable(new FFileHandleAndroid(
							*file, EntryOffset, UncompressedLength)),
						FileName,
						WhenModified)));

				// Add extra directory entries to contain the file
				// that we can use to later discover directory contents.
				FileName = FPaths::GetPath(FileName);
				while (!FileName.IsEmpty())
				{
					FString DirName = FileName + "/";
					if (!Entries.Contains(DirName))
					{
#if LOG_ANDROID_FILE
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FUnionZipFile::AddPatchFile.. DIR: '%s'"), *DirName);
#endif
						Entries.Add(
							DirName,
							MakeShareable(new Entry(
								nullptr, DirName, 0))
							);
					}
					FileName = FPaths::GetPath(FileName);
				}
			}

			// Skip to next entry.
			Offset += kCDELen + FileNameLen + ExtraLen + CommentLen;
		}

		// Keep the entries sorted so that we can do iteration to discover
		// directory contents, and other stuff.
		Entries.KeySort(TLess<FString>());
	}

	bool HasEntry(const FString & Path)
	{
		return Entries.Contains(Path);
	}

	Entry & GetEntry(const FString & Path)
	{
		return *Entries[Path];
	}

	int64 GetEntryLength(const FString & Path)
	{
		return Entries[Path]->File->Size();
	}

	int64 GetEntryModTime(const FString & Path)
	{
		return Entries[Path]->ModTime;
	}

	Directory OpenDirectory(const FString & Path)
	{
		return Directory(Entries, Path);
	}

	AAsset * GetEntryAsset(const FString & Path)
	{
		return Entries[Path]->File->File->Asset;
	}

	FString GetEntryRootPath(const FString & Path)
	{
		return Entries[Path]->File->File->Path;
	}

private:
	FEntryMap Entries;

	// Zip file constants.

	const uint32 kEOCDSignature = 0x06054b50;
	const uint32 kEOCDLen = 22;
	const uint32 kEOCDNumEntries = 8; // offset to #of entries in file
	const uint32 kEOCDSize = 12; // size of the central directory
	const uint32 kEOCDFileOffset = 16; // offset to central directory

	const uint32 kMaxCommentLen = 65535; // longest possible in ushort
	const uint32 kMaxEOCDSearch = (kMaxCommentLen + kEOCDLen);

	const uint32 kLFHSignature = 0x04034b50;
	const uint32 kLFHLen = 30; // excluding variable-len fields
	const uint32 kLFHNameLen = 26; // offset to filename length
	const uint32 kLFHExtraLen = 28; // offset to extra length

	const uint32 kCDESignature = 0x02014b50;
	const uint32 kCDELen = 46; // excluding variable-len fields
	const uint32 kCDEMethod = 10; // offset to compression method
	const uint32 kCDEModWhen = 12; // offset to modification timestamp
	const uint32 kCDECRC = 16; // offset to entry CRC
	const uint32 kCDECompLen = 20; // offset to compressed length
	const uint32 kCDEUncompLen = 24; // offset to uncompressed length
	const uint32 kCDENameLen = 28; // offset to filename length
	const uint32 kCDEExtraLen = 30; // offset to extra length
	const uint32 kCDECommentLen = 32; // offset to comment length
	const uint32 kCDELocalOffset = 42; // offset to local hdr

	const uint32 kCompressStored = 0; // no compression
	const uint32 kCompressDeflated = 8; // standard deflate

	struct ByteBuffer
	{
		uint8* Data;
		int64 Size;

		ByteBuffer(int64 size)
			: Data(new uint8[size]), Size(size)
		{
		}

		~ByteBuffer()
		{
			delete[] Data;
		}

		template <typename T>
		T GetValue(int64 offset)
		{
			return *reinterpret_cast<T*>(this->Data + offset);
		}
	};
};

// NOTE: Files are stored either loosely in the deployment directory
// or packed in an OBB archive. We don't know which one unless we try
// and get the files. We always first check if the files are local,
// i.e. loosely stored in deployment dir, if they aren't we assume
// they are archived (and can use the asset system to get them).

/**
	Implementation for Android file I/O. These handles access to these
	kinds of files:

	1. Plain-old-files in the file system (i.e. sdcard).
	2. Resources packed in OBBs (aka ZIPs) placed in download locations.
	3. Resources packed in OBBs embedded in the APK.
	4. Direct assets packaged in the APK.

	The base filenames are checked in the above order to allow for
	overriding content from the most "frozen" to the most "fluid"
	state. Hence creating a virtual single union file-system.
**/
class CORE_API FAndroidPlatformFile : public IAndroidPlatformFile
{
public:
	// Singleton implementation.
	static FAndroidPlatformFile & GetPlatformPhysical()
	{
		static FAndroidPlatformFile AndroidPlatformSingleton;
		return AndroidPlatformSingleton;
	}

	FAndroidPlatformFile()
		: AssetMgr(nullptr)
	{
		AssetMgr = AndroidThunkCpp_GetAssetManager();
	}

	// On initialization we search for OBBs that we need to
	// open to find resources.
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::Initialize(..)"));
#endif
		if (!IPhysicalPlatformFile::Initialize(Inner, CmdLine))
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::Initialize failed"));
			return false;
		}
		if (GOBBinAPK)
		{
			// Open the APK as a ZIP
			FZipUnionFile APKZip;
			int32 Handle = open(TCHAR_TO_UTF8(*GAPKFilename), O_RDONLY);
			if (Handle == -1)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::Initialize unable to open APK: %s"), *GAPKFilename);
				return false;
			}
			FFileHandleAndroid* APKFile = new FFileHandleAndroid(GAPKFilename, Handle);
			APKZip.AddPatchFile(MakeShareable(APKFile));

			// Now open the OBB in the APK and mount it
			if (APKZip.HasEntry("assets/main.obb.png"))
			{
				auto OBBEntry = APKZip.GetEntry("assets/main.obb.png");
				FFileHandleAndroid* OBBFile = static_cast<FFileHandleAndroid*>(new FFileHandleAndroid(*OBBEntry.File, 0, OBBEntry.File->Size()));
				check(nullptr != OBBFile);
				ZipResource.AddPatchFile(MakeShareable(OBBFile));
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mounted OBB in APK: %s"), *GAPKFilename);
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OBB not found in APK: %s"), *GAPKFilename);
				return false;
			}
		}
		else
		{
			// For external OBBs we mount the specific OBB files,
			// main and patch, only. As required by Android specs.
			// See <http://developer.android.com/google/play/expansion-files.html>
			FString OBBDir1 = GOBBFilePathBase + FString(TEXT("/Android/obb/") + GPackageName);
			FString OBBDir2 = GOBBFilePathBase + FString(TEXT("/obb/") + GPackageName);
			FString MainOBBName = FString::Printf(TEXT("main.%d.%s.obb"), GAndroidPackageVersion, *GPackageName);
			FString PatchOBBName = FString::Printf(TEXT("patch.%d.%s.obb"), GAndroidPackageVersion, *GPackageName);
			if (FileExists(*(OBBDir1 / MainOBBName), true))
			{
				MountOBB(*(OBBDir1 / MainOBBName));
			}
			else if (FileExists(*(OBBDir2 / MainOBBName), true))
			{
				MountOBB(*(OBBDir2 / MainOBBName));
			}
			if (FileExists(*(OBBDir1 / PatchOBBName), true))
			{
				MountOBB(*(OBBDir1 / PatchOBBName));
			}
			else if (FileExists(*(OBBDir2 / PatchOBBName), true))
			{
				MountOBB(*(OBBDir2 / PatchOBBName));
			}
		}

		// make sure the base path directory exists (UE4Game and UE4Game/ProjectName)
		FString FileBaseDir = GFilePathBase + FString(FILEBASE_DIRECTORY);
		mkdir(TCHAR_TO_UTF8(*FileBaseDir), 0766);
		mkdir(TCHAR_TO_UTF8(*(FileBaseDir + GAndroidProjectName)), 0766);

		return true;
	}

	virtual bool FileExists(const TCHAR* Filename) override
	{
		return FileExists(Filename, false);
	}

	bool FileExists(const TCHAR* Filename, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::FileExists('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		bool result = false;
		struct stat FileInfo;
		if (!LocalPath.IsEmpty() && (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == 0))
		{
			// For local files we need to check if it's a plain
			// file, as opposed to directories.
			result = S_ISREG(FileInfo.st_mode);
		}
		else
		{
			// For everything else we only check existence.
			result = IsResource(AssetPath) || IsAsset(AssetPath);
		}
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::FileExists('%s') => %s\nResolved as %s"), Filename, result ? TEXT("TRUE") : TEXT("FALSE"), *LocalPath);
#endif
		return result;
	}

	virtual int64 FileSize(const TCHAR* Filename) override
	{
		return FileSize(Filename, false);
	}

	int64 FileSize(const TCHAR* Filename, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::FileSize('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		struct stat FileInfo;
		FileInfo.st_size = -1;
		if (!LocalPath.IsEmpty() && (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == 0))
		{
			// make sure to return -1 for directories
			if (S_ISDIR(FileInfo.st_mode))
			{
				FileInfo.st_size = -1;
			}
			return FileInfo.st_size;
		}
		else if (IsResource(AssetPath))
		{
			FileInfo.st_size = ZipResource.GetEntryLength(AssetPath);
		}
		else
		{
			AAsset * file = AAssetManager_open(AssetMgr, TCHAR_TO_UTF8(*AssetPath), AASSET_MODE_UNKNOWN);
			if (nullptr != file)
			{
				FileInfo.st_size = AAsset_getLength(file);
				AAsset_close(file);
			}
		}
		return FileInfo.st_size;
	}

	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		return DeleteFile(Filename, false);
	}

	bool DeleteFile(const TCHAR* Filename, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DeleteFile('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		// Only delete if we have a local file.
		if (IsLocal(LocalPath))
		{
			return unlink(TCHAR_TO_UTF8(*LocalPath)) == 0;
		}
		return false;
	}

	// NOTE: Returns false if the file is not found.
	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		return IsReadOnly(Filename, false);
	}

	bool IsReadOnly(const TCHAR* Filename, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::IsReadOnly('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		if (IsLocal(LocalPath))
		{
			if (access(TCHAR_TO_UTF8(*LocalPath), W_OK) == -1)
			{
				return errno == EACCES;
			}
		}
		else
		{
			// Anything other than local files are from read-only sources.
			return IsResource(AssetPath) || IsAsset(AssetPath);
		}
		return false;
	}

	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		return MoveFile(To, From, false);
	}

	bool MoveFile(const TCHAR* To, const TCHAR* From, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::MoveFile('%s', '%s')"), To, From);
#endif
		// Can only move local files.
		FString ToLocalPath;
		FString ToAssetPath;
		PathToAndroidPaths(ToLocalPath, ToAssetPath, To, AllowLocal);
		FString FromLocalPath;
		FString FromAssetPath;
		PathToAndroidPaths(FromLocalPath, FromAssetPath, From, AllowLocal);

		if (IsLocal(FromLocalPath))
		{
			return rename(TCHAR_TO_UTF8(*FromLocalPath), TCHAR_TO_UTF8(*ToLocalPath)) != -1;
		}
		return false;
	}

	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		return SetReadOnly(Filename, bNewReadOnlyValue, false);
	}

	bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::SetReadOnly('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		if (IsLocal(LocalPath))
		{
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) != -1)
			{
				if (bNewReadOnlyValue)
				{
					FileInfo.st_mode &= ~S_IWUSR;
				}
				else
				{
					FileInfo.st_mode |= S_IWUSR;
				}
				return chmod(TCHAR_TO_UTF8(*LocalPath), FileInfo.st_mode) == 0;
			}
		}
		return false;
	}

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		return GetTimeStamp(Filename, false);
	}

	FDateTime GetTimeStamp(const TCHAR* Filename, bool AllowLocal)
	{
#if LOG_ANDROID_FILE_MANIFEST
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::GetTimeStamp('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		if (IsLocal(LocalPath))
		{

#if USE_UTIME
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == -1)
			{
				return FDateTime::MinValue();
			}
			// convert _stat time to FDateTime
			FTimespan TimeSinceEpoch(0, 0, FileInfo.st_mtime);
			return AndroidEpoch + TimeSinceEpoch;
#else
			FDateTime Result;
			if ( NonUFSManifest.GetFileTimeStamp(AssetPath,Result) )
			{
				return Result;
			}

			if ( UFSManifest.GetFileTimeStamp( AssetPath, Result ) )
			{
				return Result;
			}

#if LOG_ANDROID_FILE_MANIFEST
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to find time stamp in NonUFSManifest for file '%s'"), Filename);
#endif

			// pak file outside of obb may not be in manifest so check if it exists
			if (AssetPath.EndsWith(".pak"))
			{
				// return local file access timestamp (if exists)
				return GetAccessTimeStamp(Filename, true);
			}

			return FDateTime::MinValue();

#endif
		}
		else if (IsResource(AssetPath))
		{
			FTimespan TimeSinceEpoch(0, 0, ZipResource.GetEntryModTime(AssetPath));
			return AndroidEpoch + TimeSinceEpoch;
		}
		else
		{
			// No TimeStamp for assets, so just return a default timespan for now.
			return FDateTime::MinValue();
		}
	}

	virtual void SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime) override
	{
		return SetTimeStamp(Filename, DateTime, false);
	}
	
	void SetTimeStamp(const TCHAR* Filename, const FDateTime DateTime, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::SetTimeStamp('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		// Can only set time stamp on local files
		if (IsLocal(LocalPath))
		{
#if USE_UTIME
			// get file times
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == -1)
			{
				return;
			}
			// change the modification time only
			struct utimbuf Times;
			Times.actime = FileInfo.st_atime;
			Times.modtime = (DateTime - AndroidEpoch).GetTotalSeconds();
			utime(TCHAR_TO_UTF8(*LocalPath), &Times);
#else
			// do something better as utime isn't supported on android very well...
			FDateTime TempDateTime;
			if ( NonUFSManifest.GetFileTimeStamp( AssetPath, TempDateTime ) )
			{
				NonUFSManifest.SetFileTimeStamp( AssetPath, DateTime );
				NonUFSManifest.Write();
			}
			else
			{
				UFSManifest.SetFileTimeStamp( AssetPath, DateTime );
				UFSManifest.Write();
			}
#endif
		}
	}

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override
	{
		return GetAccessTimeStamp(Filename, false);
	}

	FDateTime GetAccessTimeStamp(const TCHAR* Filename, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::GetAccessTimeStamp('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		if (IsLocal(LocalPath))
		{
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) == -1)
			{
				return FDateTime::MinValue();
			}
			// convert _stat time to FDateTime
			FTimespan TimeSinceEpoch(0, 0, FileInfo.st_atime);
			return AndroidEpoch + TimeSinceEpoch;
		}
		else
		{
			// No TimeStamp for resources nor assets, so just return a default timespan for now.
			return FDateTime::MinValue();
		}
	}

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
	{
		return GetStatData(FilenameOrDirectory, false);
	}

	FFileStatData GetStatData(const TCHAR* FilenameOrDirectory, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::GetStatData('%s')"), FilenameOrDirectory);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, FilenameOrDirectory, AllowLocal);

		if (IsLocal(LocalPath))
		{
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) != -1)
			{
				return AndroidStatToUEFileData(FileInfo);
			}
		}
		else if (IsResource(AssetPath))
		{
			return FFileStatData(
				FDateTime::MinValue(),					// CreationTime
				FDateTime::MinValue(),					// AccessTime
				FDateTime::MinValue(),					// ModificationTime
				ZipResource.GetEntryLength(AssetPath),	// FileSize
				false,									// bIsDirectory
				true									// bIsReadOnly
				);
		}
		else
		{
			AAsset * file = AAssetManager_open(AssetMgr, TCHAR_TO_UTF8(*AssetPath), AASSET_MODE_UNKNOWN);
			if (nullptr != file)
			{
				bool isDirectory = false;
				AAssetDir * subdir = AAssetManager_openDir(AssetMgr, TCHAR_TO_UTF8(*AssetPath));
				if (nullptr != subdir)
				{
					isDirectory = true;
					AAssetDir_close(subdir);
				}

				int64 FileSize = -1;
				if (!isDirectory)
				{
					FileSize = AAsset_getLength(file);
				}

				FFileStatData StatData(
					FDateTime::MinValue(),				// CreationTime
					FDateTime::MinValue(),				// AccessTime
					FDateTime::MinValue(),				// ModificationTime
					FileSize,							// FileSize
					isDirectory,						// bIsDirectory
					true								// bIsReadOnly
					);

				AAsset_close(file);

				return StatData;
			}
		}
		
		return FFileStatData();
	}

	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override
	{
		return Filename;
	}

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
	{
		return OpenRead(Filename, false, bAllowWrite);
	}

	IFileHandle* OpenRead(const TCHAR* Filename, bool AllowLocal, bool bAllowWrite)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::OpenRead('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		if (IsLocal(LocalPath))
		{
			int32 Handle = open(TCHAR_TO_UTF8(*LocalPath), O_RDONLY);
			if (Handle != -1)
			{
				return new FFileHandleAndroid(LocalPath, Handle);
			}
		}
		else if (IsResource(AssetPath))
		{
			return new FFileHandleAndroid(
				*ZipResource.GetEntry(AssetPath).File,
				0, ZipResource.GetEntry(AssetPath).File->Size());
		}
		else
		{
			AAsset * asset = AAssetManager_open(AssetMgr, TCHAR_TO_UTF8(*AssetPath), AASSET_MODE_RANDOM);
			if (nullptr != asset)
			{
				return new FFileHandleAndroid(AssetPath, asset);
			}
		}
		return nullptr;
	}

	// Regardless of the file being local, asset, or resource, we
	// assert that opening a file for write will open a local file.
	// The intent is to allow creating fresh files that override
	// packaged content.
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead) override
	{
		return OpenWrite(Filename, bAppend, bAllowRead, false);
	}

	IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::OpenWrite('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, AllowLocal);

		int Flags = O_CREAT;
		if (!bAppend)
		{
			Flags |= O_TRUNC;
		}
		if (bAllowRead)
		{
			Flags |= O_RDWR;
		}
		else
		{
			Flags |= O_WRONLY;
		}

		int32 Handle = open(TCHAR_TO_UTF8(*LocalPath), Flags, S_IRUSR | S_IWUSR);
		if (Handle != -1)
		{
			FFileHandleAndroid* FileHandleAndroid = new FFileHandleAndroid(LocalPath, Handle);
			if (bAppend)
			{
				FileHandleAndroid->SeekFromEnd(0);
			}
			return FileHandleAndroid;
		}
		return nullptr;
	}

	virtual bool DirectoryExists(const TCHAR* Directory) override
	{
		return DirectoryExists(Directory, false, false);
	}
	
	bool DirectoryExists(const TCHAR* Directory, bool AllowLocal, bool AllowAsset)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DirectoryExists('%s')"), Directory);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Directory, AllowLocal);

		bool Found = false;
		if (IsLocal(LocalPath))
		{
#if LOG_ANDROID_FILE
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DirectoryExists('%s') => Check IsLocal: '%s'"), Directory, *(LocalPath + "/"));
#endif
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*LocalPath), &FileInfo) != -1)
			{
				Found = S_ISDIR(FileInfo.st_mode);
			}
		}
		else if (IsResource(AssetPath + "/"))
		{
			Found = true;
#if LOG_ANDROID_FILE
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DirectoryExists('%s') => Found as resource: '%s'"), Directory, *(AssetPath + "/"));
#endif
		}
		else if (AllowAsset)
		{
			// This can be very slow on some devices so only do it if we enabled
			AAssetDir * dir = AAssetManager_openDir(AssetMgr, TCHAR_TO_UTF8(*AssetPath));
			Found = AAssetDir_getNextFileName(dir) != nullptr;
			AAssetDir_close(dir);
#if LOG_ANDROID_FILE
			if (Found)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DirectoryExists('%s') => Found as asset: '%s'"), Directory, *(AssetPath));
			}
#endif
		}
#if LOG_ANDROID_FILE
		if (Found)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DirectoryExists('%s') => FOUND"), Directory);
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DirectoryExists('%s') => NOT"), Directory);
		}
#endif
		return Found;
	}

	// We assert that created dirs are in the local file-system.
	virtual bool CreateDirectory(const TCHAR* Directory) override
	{
		return CreateDirectory(Directory, false);
	}

	bool CreateDirectory(const TCHAR* Directory, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::CreateDirectory('%s')"), Directory);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Directory, AllowLocal);

		return mkdir(TCHAR_TO_UTF8(*LocalPath), 0766) || (errno == EEXIST);
	}

	// We assert that modifying dirs are in the local file-system.
	virtual bool DeleteDirectory(const TCHAR* Directory) override
	{
		return DeleteDirectory(Directory, false);
	}

	bool DeleteDirectory(const TCHAR* Directory, bool AllowLocal)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::DeleteDirectory('%s')"), Directory);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Directory, AllowLocal);

		return rmdir(TCHAR_TO_UTF8(*LocalPath));
	}

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
	{
		return IterateDirectory(Directory, Visitor, false, false);
	}

	bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor, bool AllowLocal, bool AllowAsset)
	{
		const FString DirectoryStr = Directory;

		auto InternalVisitor = [&](const FString& InLocalPath, struct dirent* InEntry) -> bool
		{
			const FString DirPath = DirectoryStr / UTF8_TO_TCHAR(InEntry->d_name);
			return Visitor.Visit(*DirPath, InEntry->d_type == DT_DIR);
		};
		
		auto InternalResourceVisitor = [&](const FString& InResourceName) -> bool
		{
			return Visitor.Visit(*InResourceName, false);
		};
		
		auto InternalAssetVisitor = [&](const char* InAssetPath) -> bool
		{
			bool isDirectory = false;
			AAssetDir* subdir = AAssetManager_openDir(AssetMgr, InAssetPath);
			if (nullptr != subdir)
			{
				isDirectory = true;
				AAssetDir_close(subdir);
			}

			return Visitor.Visit(UTF8_TO_TCHAR(InAssetPath), isDirectory);
		};

		return IterateDirectoryCommon(Directory, InternalVisitor, InternalResourceVisitor, InternalAssetVisitor, AllowLocal, AllowAsset);
	}

	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override
	{
		return IterateDirectoryStat(Directory, Visitor, false, false);
	}

	bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor, bool AllowLocal, bool AllowAsset)
	{
		const FString DirectoryStr = Directory;

		auto InternalVisitor = [&](const FString& InLocalPath, struct dirent* InEntry) -> bool
		{
			const FString DirPath = DirectoryStr / UTF8_TO_TCHAR(InEntry->d_name);

			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*(InLocalPath / UTF8_TO_TCHAR(InEntry->d_name))), &FileInfo) != -1)
			{
				return Visitor.Visit(*DirPath, AndroidStatToUEFileData(FileInfo));
			}

			return true;
		};
		
		auto InternalResourceVisitor = [&](const FString& InResourceName) -> bool
		{
			return Visitor.Visit(
				*InResourceName, 
				FFileStatData(
					FDateTime::MinValue(),						// CreationTime
					FDateTime::MinValue(),						// AccessTime
					FDateTime::MinValue(),						// ModificationTime
					ZipResource.GetEntryLength(InResourceName),	// FileSize
					false,										// bIsDirectory
					true										// bIsReadOnly
					)
				);
		};
		
		auto InternalAssetVisitor = [&](const char* InAssetPath) -> bool
		{
			bool isDirectory = false;
			AAssetDir* subdir = AAssetManager_openDir(AssetMgr, InAssetPath);
			if (nullptr != subdir)
			{
				isDirectory = true;
				AAssetDir_close(subdir);
			}

			int64 FileSize = -1;
			if (!isDirectory)
			{
				AAsset* file = AAssetManager_open(AssetMgr, InAssetPath, AASSET_MODE_UNKNOWN);
				FileSize = AAsset_getLength(file);
				AAssetDir_close(subdir);
			}

			return Visitor.Visit(
				UTF8_TO_TCHAR(InAssetPath), 
				FFileStatData(
					FDateTime::MinValue(),	// CreationTime
					FDateTime::MinValue(),	// AccessTime
					FDateTime::MinValue(),	// ModificationTime
					FileSize,				// FileSize
					isDirectory,			// bIsDirectory
					true					// bIsReadOnly
					)
				);
		};

		return IterateDirectoryCommon(Directory, InternalVisitor, InternalResourceVisitor, InternalAssetVisitor, AllowLocal, AllowAsset);
	}

	bool IterateDirectoryCommon(const TCHAR* Directory, const TFunctionRef<bool(const FString&, struct dirent*)>& Visitor, const TFunctionRef<bool(const FString&)>& ResourceVisitor, const TFunctionRef<bool(const char*)>& AssetVisitor, bool AllowLocal, bool AllowAsset)
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::IterateDirectory('%s')"), Directory);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Directory, AllowLocal);

		if (IsLocal(LocalPath))
		{
			DIR* Handle = opendir(TCHAR_TO_UTF8(*LocalPath));
			if (Handle)
			{
				bool Result = true;
				struct dirent *Entry;
				while ((Entry = readdir(Handle)) != nullptr && Result == true)
				{
					if (FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT(".")) && FCString::Strcmp(UTF8_TO_TCHAR(Entry->d_name), TEXT("..")))
					{
#if LOG_ANDROID_FILE
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::IterateDirectory('%s').. LOCAL Visit: '%s'"), Directory, *(FString(Directory) / UTF8_TO_TCHAR(Entry->d_name)));
#endif
						Result = Visitor(LocalPath, Entry);
					}
				}
				closedir(Handle);
				return true;
			}
		}
		else if (IsResource(AssetPath))
		{
			FZipUnionFile::Directory ResourceDir = ZipResource.OpenDirectory(AssetPath);
			bool Result = true;
			while (Result && ResourceDir.Next())
			{
#if LOG_ANDROID_FILE
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::IterateDirectory('%s').. RESOURCE Visit: '%s'"), Directory, *ResourceDir.Current.Key());
#endif
				Result = ResourceVisitor(ResourceDir.Current.Key());
			}
		}
		else if (IsResource(AssetPath + "/"))
		{
			FZipUnionFile::Directory ResourceDir = ZipResource.OpenDirectory(AssetPath + "/");
			bool Result = true;
			while (Result && ResourceDir.Next())
			{
#if LOG_ANDROID_FILE
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::IterateDirectory('%s').. RESOURCE/ Visit: '%s'"), Directory, *ResourceDir.Current.Key());
#endif
				Result = ResourceVisitor(ResourceDir.Current.Key());
			}
		}
		else if (AllowAsset)
		{
			AAssetDir * dir = AAssetManager_openDir(AssetMgr, TCHAR_TO_UTF8(*AssetPath));
			if (nullptr != dir)
			{
				bool Result = true;
				const char * fileName = nullptr;
				while ((fileName = AAssetDir_getNextFileName(dir)) != nullptr && Result == true)
				{
#if LOG_ANDROID_FILE
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::IterateDirectory('%s').. ASSET Visit: '%s'"), Directory, UTF8_TO_TCHAR(fileName));
#endif
					Result = AssetVisitor(fileName);
				}
				AAssetDir_close(dir);
				return true;
			}
		}
		return false;
	}

	virtual jobject GetAssetManager() override
	{
		return AndroidJNI_GetJavaAssetManager();
	}

	virtual bool IsAsset(const TCHAR* Filename) override
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::FileIsAsset('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, true);

		if (IsLocal(LocalPath))
		{
			return false;
		}
		else if (IsResource(AssetPath))
		{
			return ZipResource.GetEntryAsset(AssetPath) != nullptr;
		}
		else if (IsAsset(AssetPath))
		{
			return true;
		}
		return false;
	}

	virtual int64 FileStartOffset(const TCHAR* Filename) override
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::FileStartOffset('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, true);

		if (IsLocal(LocalPath))
		{
			return 0;
		}
		else if (IsResource(AssetPath))
		{
			return ZipResource.GetEntry(AssetPath).File->Start;
		}
		else if (IsAsset(AssetPath))
		{
			AAsset * file = AAssetManager_open(AssetMgr, TCHAR_TO_UTF8(*AssetPath), AASSET_MODE_UNKNOWN);
			if (nullptr != file)
			{
				off_t start = -1;
				off_t length = -1;
				int handle = AAsset_openFileDescriptor(file, &start, &length);
				if (handle != -1)
				{
					close(handle);
				}
				AAsset_close(file);
				return start;
			}
		}
		return -1;
	}

	virtual FString FileRootPath(const TCHAR* Filename) override
	{
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::FileRootPath('%s')"), Filename);
#endif
		FString LocalPath;
		FString AssetPath;
		PathToAndroidPaths(LocalPath, AssetPath, Filename, true);

		if (IsLocal(LocalPath))
		{
			return LocalPath;
		}
		else if (IsResource(AssetPath))
		{
			return ZipResource.GetEntryRootPath(AssetPath);
		}
		else if (IsAsset(AssetPath))
		{
			return AssetPath;
		}
		return "";
	}


private:
	FString NormalizePath(const TCHAR* Path)
	{
		FString Result(Path);
		Result.ReplaceInline(TEXT("\\"), TEXT("/"));
		// This replacement addresses a "bug" where some callers
		// pass in paths that are badly composed with multiple
		// subdir separators.
		Result.ReplaceInline(TEXT("//"), TEXT("/"));
		if (!Result.IsEmpty() && Result[Result.Len() - 1] == TEXT('/'))
		{
			Result.LeftChop(1);
		}
		// Remove redundant current-dir references.
		Result.ReplaceInline(TEXT("/./"), TEXT("/"));
		return Result;
	}

	void PathToAndroidPaths(FString & LocalPath, FString & AssetPath, const TCHAR* Path, bool AllowLocal)
	{
		LocalPath.Empty();
		AssetPath.Empty();

		FString AndroidPath = NormalizePath(Path);
#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::PathToAndroidPaths('%s') => AndroidPath = '%s'"), Path, *AndroidPath);
#endif
		if (!AndroidPath.IsEmpty())
		{
			if ((AllowLocal && AndroidPath.StartsWith(TEXT("/"))) ||
				AndroidPath.StartsWith(GFontPathBase) ||
				AndroidPath.StartsWith(TEXT("/system/etc/")) ||
				AndroidPath.StartsWith(GExternalFilePath.Left(AndroidPath.Len())))
			{
				// Absolute paths are only local.
				LocalPath = AndroidPath;
				AssetPath = AndroidPath;
			}
			else
			{
				while (AndroidPath.StartsWith(TEXT("../")))
				{
					AndroidPath = AndroidPath.RightChop(3);
				}
				AndroidPath.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
				if (AndroidPath.Equals(TEXT("..")))
				{
					AndroidPath = TEXT("");
				}

				// Local filepaths are directly in the deployment directory.
				static FString BasePath = GetFileBasePath();
				LocalPath = BasePath + AndroidPath;

				// Asset paths are relative to the base directory.
				AssetPath = AndroidPath;
			}
		}

#if LOG_ANDROID_FILE
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::PathToAndroidPaths('%s') => LocalPath = '%s'"), Path, *LocalPath);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidPlatformFile::PathToAndroidPaths('%s') => AssetPath = '%s'"), Path, *AssetPath);
#endif
	}

	// There is a limited set of paths we allow local file access to.
	// We filter out non-permitted local paths here unless explicitly
	// allowed. The current two cases are direct font access and with
	// the AllowLocal flag (as used to mount local OBBs). Eventually
	// we may need to also allow access to downloads somehow!
	bool IsLocal(const FString & LocalPath)
	{
		return !LocalPath.IsEmpty() && access(TCHAR_TO_UTF8(*LocalPath), F_OK) == 0;
	}

	bool IsAsset(const FString & AssetPath)
	{
		AAsset * file = AAssetManager_open(AssetMgr, TCHAR_TO_UTF8(*AssetPath), AASSET_MODE_UNKNOWN);
		if (nullptr != file)
		{
			AAsset_close(file);
			return true;
		}
		return false;
	}

	bool IsResource(const FString & ResourcePath)
	{
		return ZipResource.HasEntry(ResourcePath);
	}

	class FMountOBBVisitor : public IPlatformFile::FDirectoryVisitor
	{
		FAndroidPlatformFile & AndroidPlatformFile;

	public:
		FMountOBBVisitor(FAndroidPlatformFile & APF)
			: AndroidPlatformFile(APF)
		{
		}

		virtual ~FMountOBBVisitor()
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (FString(FilenameOrDirectory).EndsWith(TEXT(".obb")) ||
				FString(FilenameOrDirectory).EndsWith(TEXT(".obb.png")))
			{
				// It's and OBB (actually a ZIP) so we fake mount it.
				AndroidPlatformFile.MountOBB(FilenameOrDirectory);
			}
			return true;
		}
	};

	void MountOBB(const TCHAR* Filename)
	{
		FFileHandleAndroid* File
			= static_cast<FFileHandleAndroid*>(OpenRead(Filename, true, false));
		check(nullptr != File);
		ZipResource.AddPatchFile(MakeShareable(File));
		FPlatformMisc::LowLevelOutputDebugStringf(
			TEXT("Mounted OBB '%s'"), Filename);
	}

	AAssetManager* AssetMgr;
	FZipUnionFile ZipResource;
};

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	return FAndroidPlatformFile::GetPlatformPhysical();
}

IAndroidPlatformFile & IAndroidPlatformFile::GetPlatformPhysical()
{
	return FAndroidPlatformFile::GetPlatformPhysical();
}
