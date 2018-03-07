// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "RequiredProgramMainCPPInclude.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "BigInt.h"
#include "SignedArchiveWriter.h"
#include "KeyGenerator.h"
#include "AES.h"
#include "UniquePtr.h"
#include "Serialization/BufferWriter.h"
#include "AssetRegistryModule.h"
#include "DiagnosticTable.h"
#include "JsonSerializer.h"

IMPLEMENT_APPLICATION(UnrealPak, "UnrealPak");

struct FPakCommandLineParameters
{
	FPakCommandLineParameters()
		: CompressionBlockSize(64*1024)
		, CompressionBitWindow(DEFAULT_ZLIB_BIT_WINDOW)
		, FileSystemBlockSize(0)
		, PatchFilePadAlign(0)
		, GeneratePatch(false)
		, EncryptIndex(false)
	{}

	int32  CompressionBlockSize;
	int32  CompressionBitWindow;
	int64  FileSystemBlockSize;
	int64  PatchFilePadAlign;
	bool   GeneratePatch;
	FString SourcePatchPakFilename;
	FString SourcePatchDiffDirectory;
	bool EncryptIndex;
};

struct FPakEntryPair
{
	FString Filename;
	FPakEntry Info;
};

struct FPakInputPair
{
	FString Source;
	FString Dest;
	uint64 SuggestedOrder; 
	bool bNeedsCompression;
	bool bNeedEncryption;

	FPakInputPair()
		: SuggestedOrder(MAX_uint64)
		, bNeedsCompression(false)
		, bNeedEncryption(false)
	{}

	FPakInputPair(const FString& InSource, const FString& InDest)
		: Source(InSource)
		, Dest(InDest)
		, bNeedsCompression(false)
		, bNeedEncryption(false)
	{}

	FORCEINLINE bool operator==(const FPakInputPair& Other) const
	{
		return Source == Other.Source;
	}
};

struct FPakEntryOrder
{
	FPakEntryOrder() : Order(MAX_uint64) {}
	FString Filename;
	uint64  Order;
};

struct FCompressedFileBuffer
{
	FCompressedFileBuffer()
		: OriginalSize(0)
		,TotalCompressedSize(0)
		,FileCompressionBlockSize(0)
		,CompressedBufferSize(0)
	{

	}

	void Reinitialize(FArchive* File,ECompressionFlags CompressionMethod,int64 CompressionBlockSize)
	{
		OriginalSize = File->TotalSize();
		TotalCompressedSize = 0;
		FileCompressionBlockSize = 0;
		FileCompressionMethod = CompressionMethod;
		CompressedBlocks.Reset();
		CompressedBlocks.AddUninitialized((OriginalSize+CompressionBlockSize-1)/CompressionBlockSize);
	}

	void EnsureBufferSpace(int64 RequiredSpace)
	{
		if(RequiredSpace > CompressedBufferSize)
		{
			uint8* NewPtr = (uint8*)FMemory::Malloc(RequiredSpace);
			FMemory::Memcpy(NewPtr,CompressedBuffer.Get(),CompressedBufferSize);
			CompressedBuffer.Reset(NewPtr);
			CompressedBufferSize = RequiredSpace;
		}
	}

	bool CompressFileToWorkingBuffer(const FPakInputPair& InFile,uint8*& InOutPersistentBuffer,int64& InOutBufferSize,ECompressionFlags CompressionMethod,const int32 CompressionBlockSize,const int32 CompressionBitWindow);

	int64						 OriginalSize;
	int64						 TotalCompressedSize;
	int32						 FileCompressionBlockSize;
	ECompressionFlags			 FileCompressionMethod;
	TArray<FPakCompressedBlock>  CompressedBlocks;
	int64						 CompressedBufferSize;
	TUniquePtr<uint8>		     CompressedBuffer;
};

FKeyPair GPakSigningKeys;
FString GAESKey;
ANSICHAR* GAESKeyANSI;

bool PakSigningKeysAreValid()
{
	return !GPakSigningKeys.PrivateKey.Exponent.IsZero()
		&& !GPakSigningKeys.PrivateKey.Modulus.IsZero()
		&& !GPakSigningKeys.PublicKey.Exponent.IsZero()
		&& !GPakSigningKeys.PublicKey.Modulus.IsZero();
}

bool AESKeyIsValid()
{
	return GAESKey.Len() > 0;
}

void PrepareAESKeyANSI()
{
	GAESKeyANSI = nullptr;
	int64 KeyLen = GAESKey.Len();
	if (KeyLen > 0)
	{
		GAESKeyANSI = new ANSICHAR[KeyLen + 1];
		FCStringAnsi::Strcpy(GAESKeyANSI, KeyLen, TCHAR_TO_ANSI(*GAESKey));
		GAESKeyANSI[KeyLen] = '\0';
	}
}

FString GetLongestPath(TArray<FPakInputPair>& FilesToAdd)
{
	FString LongestPath;
	int32 MaxNumDirectories = 0;

	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num(); FileIndex++)
	{
		FString& Filename = FilesToAdd[FileIndex].Dest;
		int32 NumDirectories = 0;
		for (int32 Index = 0; Index < Filename.Len(); Index++)
		{
			if (Filename[Index] == '/')
			{
				NumDirectories++;
			}
		}
		if (NumDirectories > MaxNumDirectories)
		{
			LongestPath = Filename;
			MaxNumDirectories = NumDirectories;
		}
	}
	return FPaths::GetPath(LongestPath) + TEXT("/");
}

FString GetCommonRootPath(TArray<FPakInputPair>& FilesToAdd)
{
	FString Root = GetLongestPath(FilesToAdd);
	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num() && Root.Len(); FileIndex++)
	{
		FString Filename(FilesToAdd[FileIndex].Dest);
		FString Path = FPaths::GetPath(Filename) + TEXT("/");
		int32 CommonSeparatorIndex = -1;
		int32 SeparatorIndex = Path.Find(TEXT("/"), ESearchCase::CaseSensitive);
		while (SeparatorIndex >= 0)
		{
			if (FCString::Strnicmp(*Root, *Path, SeparatorIndex + 1) != 0)
			{
				break;
			}
			CommonSeparatorIndex = SeparatorIndex;
			if (CommonSeparatorIndex + 1 < Path.Len())
			{
				SeparatorIndex = Path.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CommonSeparatorIndex + 1);
			}
			else
			{
				break;
			}
		}
		if ((CommonSeparatorIndex + 1) < Root.Len())
		{
			Root = Root.Mid(0, CommonSeparatorIndex + 1);
		}
	}
	return Root;
}

bool FCompressedFileBuffer::CompressFileToWorkingBuffer(const FPakInputPair& InFile, uint8*& InOutPersistentBuffer, int64& InOutBufferSize, ECompressionFlags CompressionMethod, const int32 CompressionBlockSize, const int32 CompressionBitWindow)
{
	TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileReader(*InFile.Source));
	if(!FileHandle)
	{
		TotalCompressedSize = 0;
		return false;
	}

	Reinitialize(FileHandle.Get(), CompressionMethod, CompressionBlockSize);
	const int64 FileSize = OriginalSize;
	const int64 PaddedEncryptedFileSize = Align(FileSize,FAES::AESBlockSize);
	if(InOutBufferSize < PaddedEncryptedFileSize)
	{
		InOutPersistentBuffer = (uint8*)FMemory::Realloc(InOutPersistentBuffer,PaddedEncryptedFileSize);
		InOutBufferSize = FileSize;
	}

	// Load to buffer
	FileHandle->Serialize(InOutPersistentBuffer,FileSize);

	// Build buffers for working
	int64 UncompressedSize = FileSize;
	int32 CompressionBufferSize = Align(FCompression::CompressMemoryBound(CompressionMethod,CompressionBlockSize,CompressionBitWindow),FAES::AESBlockSize);
	EnsureBufferSpace(Align(FCompression::CompressMemoryBound(CompressionMethod,FileSize,CompressionBitWindow),FAES::AESBlockSize));


	TotalCompressedSize = 0;
	int64 UncompressedBytes = 0;
	int32 CurrentBlock = 0;
	while(UncompressedSize)
	{
		int32 BlockSize = (int32)FMath::Min<int64>(UncompressedSize,CompressionBlockSize);
		int32 MaxCompressedBlockSize = FCompression::CompressMemoryBound(CompressionMethod, BlockSize, CompressionBitWindow);
		int32 CompressedBlockSize = FMath::Max<int32>(CompressionBufferSize, MaxCompressedBlockSize);
		FileCompressionBlockSize = FMath::Max<uint32>(BlockSize, FileCompressionBlockSize);
		EnsureBufferSpace(Align(TotalCompressedSize+CompressedBlockSize,FAES::AESBlockSize));
		if(!FCompression::CompressMemory(CompressionMethod,CompressedBuffer.Get()+TotalCompressedSize,CompressedBlockSize,InOutPersistentBuffer+UncompressedBytes,BlockSize,CompressionBitWindow))
		{
			return false;
		}
		UncompressedSize -= BlockSize;
		UncompressedBytes += BlockSize;

		CompressedBlocks[CurrentBlock].CompressedStart = TotalCompressedSize;
		CompressedBlocks[CurrentBlock].CompressedEnd = TotalCompressedSize+CompressedBlockSize;
		++CurrentBlock;

		TotalCompressedSize += CompressedBlockSize;

		if(InFile.bNeedEncryption)
		{
			int32 EncryptionBlockPadding = Align(TotalCompressedSize,FAES::AESBlockSize);
			for(int64 FillIndex=TotalCompressedSize; FillIndex < EncryptionBlockPadding; ++FillIndex)
			{
				// Fill the trailing buffer with bytes from file. Note that this is now from a fixed location
				// rather than a random one so that we produce deterministic results
				CompressedBuffer.Get()[FillIndex] = CompressedBuffer.Get()[FillIndex % TotalCompressedSize];
			}
			TotalCompressedSize += EncryptionBlockPadding - TotalCompressedSize;
		}
	}

	return true;
}

bool PrepareCopyFileToPak(const FString& InMountPoint, const FPakInputPair& InFile, uint8*& InOutPersistentBuffer, int64& InOutBufferSize, FPakEntryPair& OutNewEntry, uint8*& OutDataToWrite, int64& OutSizeToWrite, ANSICHAR* InEncryptionKey)
{	
	TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileReader(*InFile.Source));
	bool bFileExists = FileHandle.IsValid();
	if (bFileExists)
	{
		const int64 FileSize = FileHandle->TotalSize();
		const int64 PaddedEncryptedFileSize = Align(FileSize, FAES::AESBlockSize); 
		OutNewEntry.Filename = InFile.Dest.Mid(InMountPoint.Len());
		OutNewEntry.Info.Offset = 0; // Don't serialize offsets here.
		OutNewEntry.Info.Size = FileSize;
		OutNewEntry.Info.UncompressedSize = FileSize;
		OutNewEntry.Info.CompressionMethod = COMPRESS_None;
		OutNewEntry.Info.bEncrypted = InFile.bNeedEncryption && InEncryptionKey != nullptr;

		if (InOutBufferSize < PaddedEncryptedFileSize)
		{
			InOutPersistentBuffer = (uint8*)FMemory::Realloc(InOutPersistentBuffer, PaddedEncryptedFileSize);
			InOutBufferSize = FileSize;
		}

		// Load to buffer
		FileHandle->Serialize(InOutPersistentBuffer, FileSize);

		{
			OutSizeToWrite = FileSize;
			if (InFile.bNeedEncryption && InEncryptionKey)
			{
				for(int64 FillIndex=FileSize; FillIndex < PaddedEncryptedFileSize && InFile.bNeedEncryption; ++FillIndex)
				{
					// Fill the trailing buffer with random bytes from file
					InOutPersistentBuffer[FillIndex] = InOutPersistentBuffer[rand()%FileSize];
				}

				//Encrypt the buffer before writing it to disk
				FAES::EncryptData(InOutPersistentBuffer, PaddedEncryptedFileSize, InEncryptionKey);
				// Update the size to be written
				OutSizeToWrite = PaddedEncryptedFileSize;
				OutNewEntry.Info.bEncrypted = true;
			}

			// Calculate the buffer hash value
			FSHA1::HashBuffer(InOutPersistentBuffer,FileSize,OutNewEntry.Info.Hash);			
			OutDataToWrite = InOutPersistentBuffer;
		}
	}
	return bFileExists;
}

void FinalizeCopyCompressedFileToPak(FArchive& InPak, const FCompressedFileBuffer& CompressedFile, FPakEntryPair& OutNewEntry)
{
	check(CompressedFile.TotalCompressedSize != 0);

	check(OutNewEntry.Info.CompressionBlocks.Num() == CompressedFile.CompressedBlocks.Num());
	check(OutNewEntry.Info.CompressionMethod == CompressedFile.FileCompressionMethod);

	int64 TellPos = InPak.Tell() + OutNewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
	const TArray<FPakCompressedBlock>& Blocks = CompressedFile.CompressedBlocks;
	for (int32 BlockIndex = 0, BlockCount = Blocks.Num(); BlockIndex < BlockCount; ++BlockIndex)
	{
		OutNewEntry.Info.CompressionBlocks[BlockIndex].CompressedStart = Blocks[BlockIndex].CompressedStart + TellPos;
		OutNewEntry.Info.CompressionBlocks[BlockIndex].CompressedEnd = Blocks[BlockIndex].CompressedEnd + TellPos;
	}
}

bool PrepareCopyCompressedFileToPak(const FString& InMountPoint, const FPakInputPair& InFile, const FCompressedFileBuffer& CompressedFile, FPakEntryPair& OutNewEntry, uint8*& OutDataToWrite, int64& OutSizeToWrite, ANSICHAR* InEncryptionKey)
{
	if (CompressedFile.TotalCompressedSize == 0)
	{
		return false;
	}

	OutNewEntry.Info.CompressionMethod = CompressedFile.FileCompressionMethod;
	OutNewEntry.Info.CompressionBlocks.AddZeroed(CompressedFile.CompressedBlocks.Num());

	if (InFile.bNeedEncryption && InEncryptionKey)
	{
		FAES::EncryptData(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize, InEncryptionKey);
	}

	//Hash the final buffer thats written
	FSHA1 Hash;
	Hash.Update(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);
	Hash.Final();

	// Update file size & Hash
	OutNewEntry.Info.CompressionBlockSize = CompressedFile.FileCompressionBlockSize;
	OutNewEntry.Info.UncompressedSize = CompressedFile.OriginalSize;
	OutNewEntry.Info.Size = CompressedFile.TotalCompressedSize;
	Hash.GetHash(OutNewEntry.Info.Hash);

	//	Write the header, then the data
	OutNewEntry.Filename = InFile.Dest.Mid(InMountPoint.Len());
	OutNewEntry.Info.Offset = 0; // Don't serialize offsets here.
	OutNewEntry.Info.bEncrypted = InFile.bNeedEncryption && InEncryptionKey != nullptr;
	OutSizeToWrite = CompressedFile.TotalCompressedSize;
	OutDataToWrite = CompressedFile.CompressedBuffer.Get();
	//OutNewEntry.Info.Serialize(InPak,FPakInfo::PakFile_Version_Latest);	
	//InPak.Serialize(CompressedFile.CompressedBuffer.Get(), CompressedFile.TotalCompressedSize);

	return true;
}

void ProcessOrderFile(int32 ArgC, TCHAR* ArgV[], TMap<FString, uint64>& OrderMap)
{
	// List of all items to add to pak file
	FString ResponseFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("-order="), ResponseFile))
	{
		FString Text;
		UE_LOG(LogPakFile, Display, TEXT("Loading pak order file %s..."), *ResponseFile);
		if (FFileHelper::LoadFileToString(Text, *ResponseFile))
		{
			// Read all lines
			TArray<FString> Lines;
			Text.ParseIntoArray(Lines, TEXT("\n"), true);
			for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); EntryIndex++)
			{
				Lines[EntryIndex].ReplaceInline(TEXT("\r"), TEXT(""));
				Lines[EntryIndex].ReplaceInline(TEXT("\n"), TEXT(""));
				int32 OpenOrderNumber = EntryIndex;
				if (Lines[EntryIndex].FindLastChar('"', OpenOrderNumber))
				{
					FString ReadNum = Lines[EntryIndex].RightChop(OpenOrderNumber+1);
					Lines[EntryIndex] = Lines[EntryIndex].Left(OpenOrderNumber+1);
					ReadNum.TrimStartInline();
					if (ReadNum.IsNumeric())
					{
						OpenOrderNumber = FCString::Atoi(*ReadNum);
					}
				}
				Lines[EntryIndex] = Lines[EntryIndex].TrimQuotes();
				FString Path=FString::Printf(TEXT("%s"), *Lines[EntryIndex]);
				FPaths::NormalizeFilename(Path);
				Path = Path.ToLower();
#if 0
				if (Path.EndsWith("uexp"))
				{
					OpenOrderNumber += (1 << 29);
				}
				if (Path.EndsWith("ubulk"))
				{
					OpenOrderNumber += (1 << 30);
				}
#endif
				OrderMap.Add(Path, OpenOrderNumber);
			}
			UE_LOG(LogPakFile, Display, TEXT("Finished loading pak order file %s."), *ResponseFile);
		}
		else 
		{
			UE_LOG(LogPakFile, Display, TEXT("Unable to load pak order file %s."), *ResponseFile);
		}
	}
}

static void CommandLineParseHelper(const TCHAR* InCmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while(FParse::Token(InCmdLine,NextToken,false))
	{
		if((**NextToken == TCHAR('-')))
		{
			new(Switches)FString(NextToken.Mid(1));
		}
		else
		{
			new(Tokens)FString(NextToken);
		}
	}
}

void ProcessCommandLine(int32 ArgC, TCHAR* ArgV[], TArray<FPakInputPair>& Entries, FPakCommandLineParameters& CmdLineParameters)
{
	// List of all items to add to pak file
	FString ResponseFile;
	FString ClusterSizeString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-blocksize="), ClusterSizeString) && 
		FParse::Value(FCommandLine::Get(), TEXT("-blocksize="), CmdLineParameters.FileSystemBlockSize))
	{
		if (ClusterSizeString.EndsWith(TEXT("MB")))
		{
			CmdLineParameters.FileSystemBlockSize *= 1024*1024;
		}
		else if (ClusterSizeString.EndsWith(TEXT("KB")))
		{
			CmdLineParameters.FileSystemBlockSize *= 1024;
		}
	}
	else
	{
		CmdLineParameters.FileSystemBlockSize = 0;
	}

	if (!FParse::Value(FCommandLine::Get(), TEXT("-bitwindow="), CmdLineParameters.CompressionBitWindow))
	{
		CmdLineParameters.CompressionBitWindow = DEFAULT_ZLIB_BIT_WINDOW;
	}

	if (!FParse::Value(FCommandLine::Get(), TEXT("-patchpaddingalign="), CmdLineParameters.PatchFilePadAlign))
	{
		CmdLineParameters.PatchFilePadAlign = 0;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("encryptindex")))
	{
		CmdLineParameters.EncryptIndex = true;
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("-create="), ResponseFile))
	{
		TArray<FString> Lines;

		CmdLineParameters.GeneratePatch = FParse::Value(FCommandLine::Get(), TEXT("-generatepatch="), CmdLineParameters.SourcePatchPakFilename);

		bool bCompress = FParse::Param(FCommandLine::Get(), TEXT("compress"));
		bool bEncrypt = FParse::Param(FCommandLine::Get(), TEXT("encrypt"));

		bool bParseLines = true;
		if (IFileManager::Get().DirectoryExists(*ResponseFile))
		{
			IFileManager::Get().FindFilesRecursive(Lines, *ResponseFile, TEXT("*"), true, false);
			bParseLines = false;
		}
		else
		{
			FString Text;
			UE_LOG(LogPakFile, Display, TEXT("Loading response file %s"), *ResponseFile);
			if (FFileHelper::LoadFileToString(Text, *ResponseFile))
			{
				// Remove all carriage return characters.
				Text.ReplaceInline(TEXT("\r"), TEXT(""));
				// Read all lines
				Text.ParseIntoArray(Lines, TEXT("\n"), true);
			}
			else
			{
				UE_LOG(LogPakFile, Error, TEXT("Failed to load %s"), *ResponseFile);
			}
		}

		for (int32 EntryIndex = 0; EntryIndex < Lines.Num(); EntryIndex++)
		{
			TArray<FString> SourceAndDest;
			TArray<FString> Switches;
			if (bParseLines)
			{
				Lines[EntryIndex].TrimStartInline();
				CommandLineParseHelper(*Lines[EntryIndex], SourceAndDest, Switches);
			}
			else
			{
				SourceAndDest.Add(Lines[EntryIndex]);
			}
			if( SourceAndDest.Num() == 0)
			{
				continue;
			}
			FPakInputPair Input;

			Input.Source = SourceAndDest[0];
			FPaths::NormalizeFilename(Input.Source);
			if (SourceAndDest.Num() > 1)
			{
				Input.Dest = FPaths::GetPath(SourceAndDest[1]);
			}
			else
			{
				Input.Dest = FPaths::GetPath(Input.Source);
			}
			FPaths::NormalizeFilename(Input.Dest);
			FPakFile::MakeDirectoryFromPath(Input.Dest);

			//check for compression switches
			for (int32 Index = 0; Index < Switches.Num(); ++Index)
			{
				if (Switches[Index] == TEXT("compress"))
				{
					Input.bNeedsCompression = true;
				}
				if (Switches[Index] == TEXT("encrypt"))
				{
					Input.bNeedEncryption = true;
				}
			}
			Input.bNeedsCompression |= bCompress;
			Input.bNeedEncryption |= bEncrypt;

			UE_LOG(LogPakFile, Log, TEXT("Added file Source: %s Dest: %s"), *Input.Source, *Input.Dest);
			Entries.Add(Input);
		}			
	}
	else
	{
		// Override destination path.
		FString MountPoint;
		FParse::Value(FCommandLine::Get(), TEXT("-dest="), MountPoint);
		FPaths::NormalizeFilename(MountPoint);
		FPakFile::MakeDirectoryFromPath(MountPoint);

		// Parse comand line params. The first param after the program name is the created pak name
		for (int32 Index = 2; Index < ArgC; Index++)
		{
			// Skip switches and add everything else to the Entries array
			TCHAR* Param = ArgV[Index];
			if (Param[0] != '-')
			{
				FPakInputPair Input;
				Input.Source = Param;
				FPaths::NormalizeFilename(Input.Source);
				if (MountPoint.Len() > 0)
				{
					FString SourceDirectory( FPaths::GetPath(Input.Source) );
					FPakFile::MakeDirectoryFromPath(SourceDirectory);
					Input.Dest = Input.Source.Replace(*SourceDirectory, *MountPoint, ESearchCase::IgnoreCase);
				}
				else
				{
					Input.Dest = FPaths::GetPath(Input.Source);
					FPakFile::MakeDirectoryFromPath(Input.Dest);
				}
				FPaths::NormalizeFilename(Input.Dest);
				Entries.Add(Input);
			}
		}
	}
	UE_LOG(LogPakFile, Display, TEXT("Added %d entries to add to pak file."), Entries.Num());
}

void CollectFilesToAdd(TArray<FPakInputPair>& OutFilesToAdd, const TArray<FPakInputPair>& InEntries, const TMap<FString, uint64>& OrderMap)
{
	UE_LOG(LogPakFile, Display, TEXT("Collecting files to add to pak file..."));
	const double StartTime = FPlatformTime::Seconds();

	// Start collecting files
	TSet<FString> AddedFiles;	
	for (int32 Index = 0; Index < InEntries.Num(); Index++)
	{
		const FPakInputPair& Input = InEntries[Index];
		const FString& Source = Input.Source;
		bool bCompression = Input.bNeedsCompression;
		bool bEncryption = Input.bNeedEncryption;


		FString Filename = FPaths::GetCleanFilename(Source);
		FString Directory = FPaths::GetPath(Source);
		FPaths::MakeStandardFilename(Directory);
		FPakFile::MakeDirectoryFromPath(Directory);

		if (Filename.IsEmpty())
		{
			Filename = TEXT("*.*");
		}
		if ( Filename.Contains(TEXT("*")) )
		{
			// Add multiple files
			TArray<FString> FoundFiles;
			IFileManager::Get().FindFilesRecursive(FoundFiles, *Directory, *Filename, true, false);

			for (int32 FileIndex = 0; FileIndex < FoundFiles.Num(); FileIndex++)
			{
				FPakInputPair FileInput;
				FileInput.Source = FoundFiles[FileIndex];
				FPaths::MakeStandardFilename(FileInput.Source);
				FileInput.Dest = FileInput.Source.Replace(*Directory, *Input.Dest, ESearchCase::IgnoreCase);
				const uint64* FoundOrder = OrderMap.Find(FileInput.Dest.ToLower());
				if(FoundOrder)
				{
					FileInput.SuggestedOrder = *FoundOrder;
				}
				else
				{
					// we will put all unordered files at 1 << 28 so that they are before any uexp or ubulk files we assign orders to here
					FileInput.SuggestedOrder = (1 << 28);
					// if this is a cook order or an old order it will not have uexp files in it, so we put those in the same relative order after all of the normal files, but before any ubulk files
					if (FileInput.Dest.EndsWith(TEXT("uexp")) || FileInput.Dest.EndsWith(TEXT("ubulk")))
					{
						FoundOrder = OrderMap.Find(FPaths::GetBaseFilename(FileInput.Dest.ToLower(), false) + TEXT(".uasset"));
						if (!FoundOrder)
						{
							FoundOrder = OrderMap.Find(FPaths::GetBaseFilename(FileInput.Dest.ToLower(), false) + TEXT(".umap"));
						}
						if (FileInput.Dest.EndsWith(TEXT("uexp")))
						{
							FileInput.SuggestedOrder = (FoundOrder ? *FoundOrder : 0) + (1 << 29);
						}
						else
						{
							FileInput.SuggestedOrder = (FoundOrder ? *FoundOrder : 0) + (1 << 30);
						}
					}
				}
				FileInput.bNeedsCompression = bCompression;
				FileInput.bNeedEncryption = bEncryption;
				if (!AddedFiles.Contains(FileInput.Source))
				{
					OutFilesToAdd.Add(FileInput);
					AddedFiles.Add(FileInput.Source);
				}
				else
				{
					int32 FoundIndex;
					OutFilesToAdd.Find(FileInput,FoundIndex);
					OutFilesToAdd[FoundIndex].bNeedEncryption |= bEncryption;
					OutFilesToAdd[FoundIndex].bNeedsCompression |= bCompression;
					OutFilesToAdd[FoundIndex].SuggestedOrder = FMath::Min<uint64>(OutFilesToAdd[FoundIndex].SuggestedOrder, FileInput.SuggestedOrder);
				}
			}
		}
		else
		{
			// Add single file
			FPakInputPair FileInput;
			FileInput.Source = Input.Source;
			FPaths::MakeStandardFilename(FileInput.Source);
			FileInput.Dest = FileInput.Source.Replace(*Directory, *Input.Dest, ESearchCase::IgnoreCase);
			const uint64* FoundOrder = OrderMap.Find(FileInput.Dest.ToLower());
			if (FoundOrder)
			{
				FileInput.SuggestedOrder = *FoundOrder;
			}
			FileInput.bNeedEncryption = bEncryption;
			FileInput.bNeedsCompression = bCompression;

			if (AddedFiles.Contains(FileInput.Source))
			{
				int32 FoundIndex;
				OutFilesToAdd.Find(FileInput, FoundIndex);
				OutFilesToAdd[FoundIndex].bNeedEncryption |= bEncryption;
				OutFilesToAdd[FoundIndex].bNeedsCompression |= bCompression;
				OutFilesToAdd[FoundIndex].SuggestedOrder = FMath::Min<uint64>(OutFilesToAdd[FoundIndex].SuggestedOrder, FileInput.SuggestedOrder);
			}
			else
			{
				OutFilesToAdd.Add(FileInput);
				AddedFiles.Add(FileInput.Source);
			}
		}
	}

	// Sort by suggested order then alphabetically
	struct FInputPairSort
	{
		FORCEINLINE bool operator()(const FPakInputPair& A, const FPakInputPair& B) const
		{
			return A.SuggestedOrder == B.SuggestedOrder ? A.Dest < B.Dest : A.SuggestedOrder < B.SuggestedOrder;
		}
	};
	OutFilesToAdd.Sort(FInputPairSort());
	UE_LOG(LogPakFile, Display, TEXT("Collected %d files in %.2lfs."), OutFilesToAdd.Num(), FPlatformTime::Seconds() - StartTime);
}

bool BufferedCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, void* Buffer, int64 BufferSize)
{	
	// Align down
	BufferSize = BufferSize & ~(FAES::AESBlockSize-1);
	int64 RemainingSizeToCopy = Entry.Size;
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		// If file is encrypted so we need to account for padding
		int64 SizeToRead = Entry.bEncrypted ? Align(SizeToCopy,FAES::AESBlockSize) : SizeToCopy;

		Source.Serialize(Buffer,SizeToRead);
		if (Entry.bEncrypted)
		{
			FAES::DecryptData((uint8*)Buffer,SizeToRead, GAESKeyANSI);
		}
		Dest.Serialize(Buffer, SizeToCopy);
		RemainingSizeToCopy -= SizeToRead;
	}
	return true;
}

bool UncompressCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, uint8*& PersistentBuffer, int64& BufferSize)
{
	if (Entry.UncompressedSize == 0)
	{
		return false;
	}

	int64 WorkingSize = Entry.CompressionBlockSize;
	int32 MaxCompressionBlockSize = FCompression::CompressMemoryBound((ECompressionFlags)Entry.CompressionMethod, WorkingSize);
	WorkingSize += MaxCompressionBlockSize;
	if (BufferSize < WorkingSize)
	{
		PersistentBuffer = (uint8*)FMemory::Realloc(PersistentBuffer, WorkingSize);
		BufferSize = WorkingSize;
	}

	uint8* UncompressedBuffer = PersistentBuffer+MaxCompressionBlockSize;

	for (uint32 BlockIndex=0, BlockIndexNum=Entry.CompressionBlocks.Num(); BlockIndex < BlockIndexNum; ++BlockIndex)
	{
		uint32 CompressedBlockSize = Entry.CompressionBlocks[BlockIndex].CompressedEnd - Entry.CompressionBlocks[BlockIndex].CompressedStart;
		uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(Entry.UncompressedSize - Entry.CompressionBlockSize*BlockIndex, Entry.CompressionBlockSize);
		Source.Seek(Entry.CompressionBlocks[BlockIndex].CompressedStart);
		uint32 SizeToRead = Entry.bEncrypted ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
		Source.Serialize(PersistentBuffer, SizeToRead);

		if (Entry.bEncrypted)
		{
			FAES::DecryptData(PersistentBuffer, SizeToRead, GAESKeyANSI);
		}

		if(!FCompression::UncompressMemory((ECompressionFlags)Entry.CompressionMethod,UncompressedBuffer,UncompressedBlockSize,PersistentBuffer,CompressedBlockSize))
		{
			return false;
		}
		Dest.Serialize(UncompressedBuffer,UncompressedBlockSize);
	}

	return true;
}

void PrepareEncryptionAndSigningKeys()
{
	bool bSigningEnabled = false;

	GPakSigningKeys.PrivateKey.Exponent.Zero();
	GPakSigningKeys.PrivateKey.Modulus.Zero();
	GPakSigningKeys.PublicKey.Exponent.Zero();
	GPakSigningKeys.PublicKey.Modulus.Zero();
	GAESKey = TEXT("");
	GAESKeyANSI = nullptr;

	if (FParse::Param(FCommandLine::Get(), TEXT("encryptionini")))
	{
		FString ProjectDir, EngineDir, Platform;

		if (FParse::Value(FCommandLine::Get(), TEXT("projectdir="), ProjectDir, false)
			&& FParse::Value(FCommandLine::Get(), TEXT("enginedir="), EngineDir, false)
			&& FParse::Value(FCommandLine::Get(), TEXT("platform="), Platform, false))
		{
			static const TCHAR* SectionName = TEXT("Core.Encryption");

			FConfigFile ConfigFile;
			FConfigCacheIni::LoadExternalIniFile(ConfigFile, TEXT("Encryption"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
			bool bSignPak = false;
			bool bEncryptPak = false;

			ConfigFile.GetBool(SectionName, TEXT("SignPak"), bSignPak);
			ConfigFile.GetBool(SectionName, TEXT("EncryptPak"), bEncryptPak);

			if (bSignPak)
			{
				FString RSAPublicExp, RSAPrivateExp, RSAModulus;
				ConfigFile.GetString(SectionName, TEXT("rsa.publicexp"), RSAPublicExp);
				ConfigFile.GetString(SectionName, TEXT("rsa.privateexp"), RSAPrivateExp);
				ConfigFile.GetString(SectionName, TEXT("rsa.modulus"), RSAModulus);

				GPakSigningKeys.PrivateKey.Exponent.Parse(RSAPrivateExp);
				GPakSigningKeys.PrivateKey.Modulus.Parse(RSAModulus);
				GPakSigningKeys.PublicKey.Exponent.Parse(RSAPublicExp);
				GPakSigningKeys.PublicKey.Modulus = GPakSigningKeys.PrivateKey.Modulus;

				bSigningEnabled = true;

				UE_LOG(LogPakFile, Display, TEXT("Parsed signature keys from config files."));
			}

			if (bEncryptPak)
			{
				ConfigFile.GetString(SectionName, TEXT("aes.key"), GAESKey);

				if (GAESKey.Len() > 0)
				{
					UE_LOG(LogPakFile, Display, TEXT("Parsed AES encryption key from config files."));
				}
			}
		}
	}
	else
	{
		FParse::Value(FCommandLine::Get(), TEXT("aes="), GAESKey, false);

		if (GAESKey.Len() > 0)
		{
			UE_LOG(LogPakFile, Display, TEXT("Parsed AES encryption key from command line."));
		}

		FString KeyFilename;
		if (FParse::Value(FCommandLine::Get(), TEXT("sign="), KeyFilename, false))
		{
			if (KeyFilename.StartsWith(TEXT("0x")))
			{
				TArray<FString> KeyValueText;
				int32 NumParts = KeyFilename.ParseIntoArray(KeyValueText, TEXT("+"), true);
				if (NumParts == 3)
				{
					GPakSigningKeys.PrivateKey.Exponent.Parse(KeyValueText[0]);
					GPakSigningKeys.PrivateKey.Modulus.Parse(KeyValueText[1]);
					GPakSigningKeys.PublicKey.Exponent.Parse(KeyValueText[2]);
					GPakSigningKeys.PublicKey.Modulus = GPakSigningKeys.PrivateKey.Modulus;

					bSigningEnabled = true;

					UE_LOG(LogPakFile, Display, TEXT("Parsed signature keys from command line."));
				}
				else
				{
					UE_LOG(LogPakFile, Error, TEXT("Expected 3, got %d, when parsing %s"), KeyValueText.Num(), *KeyFilename);
					GPakSigningKeys.PrivateKey.Exponent.Zero();
				}
			}
			else if (!ReadKeysFromFile(*KeyFilename, GPakSigningKeys))
			{
				UE_LOG(LogPakFile, Error, TEXT("Unable to load signature keys %s."), *KeyFilename);
			}
			else
			{
				bSigningEnabled = true;
			}
		}
	}

	if (GAESKey.Len() > 0 && GAESKey.Len() < 32)
	{
		UE_LOG(LogPakFile, Fatal, TEXT("AES encryption key parsed from command line must be at least 32 characters long"));
	}

	PrepareAESKeyANSI();

	if (bSigningEnabled)
	{
		if (PakSigningKeysAreValid())
		{
			if (!TestKeys(GPakSigningKeys))
			{
				GPakSigningKeys.PrivateKey.Exponent.Zero();
			}
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Supplied pak signing keys were not valid"));
		}
	}

	if (AESKeyIsValid())
	{
		FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda([]() { return GAESKeyANSI; });
	}
}

/**
 * Creates a pak file writer. This can be a signed writer if the encryption keys are specified in the command line
 */
FArchive* CreatePakWriter(const TCHAR* Filename)
{
	FArchive* Writer = IFileManager::Get().CreateFileWriter(Filename);
	FString KeyFilename;
	bool bSigningEnabled = false;
	
	if (Writer)
	{
		if (PakSigningKeysAreValid())
		{
			UE_LOG(LogPakFile, Display, TEXT("Creating signed pak %s."), Filename);
			Writer = new FSignedArchiveWriter(*Writer, Filename, GPakSigningKeys.PublicKey, GPakSigningKeys.PrivateKey);
		}
	}

	return Writer;
}

bool CreatePakFile(const TCHAR* Filename, TArray<FPakInputPair>& FilesToAdd, const FPakCommandLineParameters& CmdLineParameters)
{	
	const double StartTime = FPlatformTime::Seconds();

	// Create Pak
	TUniquePtr<FArchive> PakFileHandle(CreatePakWriter(Filename));
	if (!PakFileHandle)
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to create pak file \"%s\"."), Filename);
		return false;
	}

	FPakInfo Info;
	Info.bEncryptedIndex = (AESKeyIsValid() && CmdLineParameters.EncryptIndex);

	TArray<FPakEntryPair> Index;
	FString MountPoint = GetCommonRootPath(FilesToAdd);
	uint8* ReadBuffer = NULL;
	int64 BufferSize = 0;
	ECompressionFlags CompressionMethod = COMPRESS_None;
	FCompressedFileBuffer CompressedFileBuffer;

	uint8* PaddingBuffer = nullptr;
	int64 PaddingBufferSize = 0;
	if (CmdLineParameters.PatchFilePadAlign > 0)
	{
		PaddingBufferSize = CmdLineParameters.PatchFilePadAlign;
		PaddingBuffer = (uint8*)FMemory::Malloc(PaddingBufferSize);
		FMemory::Memset(PaddingBuffer, 0, PaddingBufferSize);
	}

	// Some platforms provide patch download size reduction by diffing the patch files.  However, they often operate on specific block
	// sizes when dealing with new data within the file.  Pad files out to the given alignment to work with these systems more nicely.
	// We also want to combine smaller files into the same padding size block so we don't waste as much space. i.e. grouping 64 1k files together
	// rather than padding each out to 64k.
	const uint32 RequiredPatchPadding = CmdLineParameters.PatchFilePadAlign;

	uint64 ContiguousTotalSizeSmallerThanBlockSize = 0;
	uint64 ContiguousFilesSmallerThanBlockSize = 0;

	uint64 TotalUncompressedSize = 0;
	uint64 TotalCompressedSize = 0;

	uint64 TotalRequestedEncryptedFiles = 0;
	uint64 TotalEncryptedFiles = 0;

	for (int32 FileIndex = 0; FileIndex < FilesToAdd.Num(); FileIndex++)
	{
		bool bIsUAssetUExpPairUAsset = false;
		bool bIsUAssetUExpPairUExp = false;

		if (FileIndex)
		{
			if (FPaths::GetBaseFilename(FilesToAdd[FileIndex - 1].Dest, false) == FPaths::GetBaseFilename(FilesToAdd[FileIndex].Dest, false) &&
				FPaths::GetExtension(FilesToAdd[FileIndex - 1].Dest, true) == TEXT(".uasset") && 
				FPaths::GetExtension(FilesToAdd[FileIndex].Dest, true) == TEXT(".uexp")
				)
			{
				bIsUAssetUExpPairUExp = true;
			}
		}
		if (!bIsUAssetUExpPairUExp && FileIndex + 1 < FilesToAdd.Num())
		{
			if (FPaths::GetBaseFilename(FilesToAdd[FileIndex].Dest, false) == FPaths::GetBaseFilename(FilesToAdd[FileIndex + 1].Dest, false) &&
				FPaths::GetExtension(FilesToAdd[FileIndex].Dest, true) == TEXT(".uasset") &&
				FPaths::GetExtension(FilesToAdd[FileIndex + 1].Dest, true) == TEXT(".uexp")
				)
			{
				bIsUAssetUExpPairUAsset = true;
			}
		}

		//  Remember the offset but don't serialize it with the entry header.
		int64 NewEntryOffset = PakFileHandle->Tell();
		FPakEntryPair NewEntry;

		//check if this file requested to be compression
		int64 OriginalFileSize = IFileManager::Get().FileSize(*FilesToAdd[FileIndex].Source);
		int64 RealFileSize = OriginalFileSize + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
		CompressionMethod = (FilesToAdd[FileIndex].bNeedsCompression && OriginalFileSize > 0) ? COMPRESS_Default : COMPRESS_None;

		if (CompressionMethod != COMPRESS_None)
		{
			if (CompressedFileBuffer.CompressFileToWorkingBuffer(FilesToAdd[FileIndex], ReadBuffer, BufferSize, CompressionMethod, CmdLineParameters.CompressionBlockSize, CmdLineParameters.CompressionBitWindow))
			{
				// Check the compression ratio, if it's too low just store uncompressed. Also take into account read size
				// if we still save 64KB it's probably worthwhile compressing, as that saves a file read operation in the runtime.
				// TODO: drive this threashold from the command line
				float PercentLess = ((float)CompressedFileBuffer.TotalCompressedSize / (OriginalFileSize / 100.f));
				if (PercentLess > 90.f && (OriginalFileSize - CompressedFileBuffer.TotalCompressedSize) < 65536)
				{
					CompressionMethod = COMPRESS_None;
				}
				else
				{
					NewEntry.Info.CompressionMethod = CompressionMethod;
					NewEntry.Info.CompressionBlocks.AddUninitialized(CompressedFileBuffer.CompressedBlocks.Num());
					RealFileSize = CompressedFileBuffer.TotalCompressedSize + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
					NewEntry.Info.CompressionBlocks.Reset();
				}
			}
			else
			{
				// Compression failed. Include file uncompressed and warn the user.
				UE_LOG(LogPakFile, Warning, TEXT("File \"%s\" failed compression. File will be saved uncompressed."), *FilesToAdd[FileIndex].Source);
				CompressionMethod = COMPRESS_None;
			}
		}

		// Account for file system block size, which is a boundary we want to avoid crossing.
		if (!bIsUAssetUExpPairUExp && // don't split uexp / uasset pairs
			CmdLineParameters.FileSystemBlockSize > 0 && OriginalFileSize != INDEX_NONE && RealFileSize <= CmdLineParameters.FileSystemBlockSize)
		{
			if ((NewEntryOffset / CmdLineParameters.FileSystemBlockSize) != ((NewEntryOffset + RealFileSize) / CmdLineParameters.FileSystemBlockSize))
			{
				//File crosses a block boundary, so align it to the beginning of the next boundary
				int64 OldOffset = NewEntryOffset;
				NewEntryOffset = AlignArbitrary(NewEntryOffset, CmdLineParameters.FileSystemBlockSize);
				int64 PaddingRequired = NewEntryOffset - OldOffset;

				if (PaddingRequired > 0)
				{
					// If we don't already have a padding buffer, create one
					if (PaddingBuffer == nullptr)
					{
						PaddingBufferSize = 64 * 1024;
						PaddingBuffer = (uint8*)FMemory::Malloc(PaddingBufferSize);
						FMemory::Memset(PaddingBuffer, 0, PaddingBufferSize);
					}

					UE_LOG(LogPakFile, Verbose, TEXT("%14llu - %14llu : %14llu padding."), PakFileHandle->Tell(), PakFileHandle->Tell() + PaddingRequired, PaddingRequired);
					while (PaddingRequired > 0)
					{
						int64 AmountToWrite = FMath::Min(PaddingRequired, PaddingBufferSize);
						PakFileHandle->Serialize(PaddingBuffer, AmountToWrite);
						PaddingRequired -= AmountToWrite;
					}

					check(PakFileHandle->Tell() == NewEntryOffset);
				}
			}
		}

		bool bCopiedToPak;
		int64 SizeToWrite = 0;
		uint8* DataToWrite = nullptr;
		if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
		{
			bCopiedToPak = PrepareCopyCompressedFileToPak(MountPoint, FilesToAdd[FileIndex], CompressedFileBuffer, NewEntry, DataToWrite, SizeToWrite, GAESKeyANSI);
			DataToWrite = CompressedFileBuffer.CompressedBuffer.Get();
		}
		else
		{
			bCopiedToPak = PrepareCopyFileToPak(MountPoint, FilesToAdd[FileIndex], ReadBuffer, BufferSize, NewEntry, DataToWrite, SizeToWrite, GAESKeyANSI);
			DataToWrite = ReadBuffer;
		}		

		int64 TotalSizeToWrite = SizeToWrite + NewEntry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
		if (bCopiedToPak)
		{
			
			if (RequiredPatchPadding > 0)
			{
				//if the next file is going to cross a patch-block boundary then pad out the current set of files with 0's
				//and align the next file up.
				bool bCrossesBoundary = AlignArbitrary(NewEntryOffset, RequiredPatchPadding) != AlignArbitrary(NewEntryOffset + TotalSizeToWrite - 1, RequiredPatchPadding);
				bool bPatchPadded = false;
				if (!bIsUAssetUExpPairUExp) // never patch-pad the uexp of a uasset/uexp pair
				{
					bool bPairProbablyCrossesBoundary = false; // we don't consider compression because we have not compressed the uexp yet.
					if (bIsUAssetUExpPairUAsset)
					{
						int64 UExpFileSize = IFileManager::Get().FileSize(*FilesToAdd[FileIndex + 1].Source) / 2; // assume 50% compression
						bPairProbablyCrossesBoundary = AlignArbitrary(NewEntryOffset, RequiredPatchPadding) != AlignArbitrary(NewEntryOffset + TotalSizeToWrite + UExpFileSize - 1, RequiredPatchPadding);
					}
					if (TotalSizeToWrite >= RequiredPatchPadding || // if it exactly the padding size and by luck does not cross a boundary, we still consider it "over" because it can't be packed with anything else
						bCrossesBoundary || bPairProbablyCrossesBoundary)
					{
						NewEntryOffset = AlignArbitrary(NewEntryOffset, RequiredPatchPadding);
						int64 CurrentLoc = PakFileHandle->Tell();
						int64 PaddingSize = NewEntryOffset - CurrentLoc;
						check(PaddingSize >= 0);
						if (PaddingSize)
						{
							UE_LOG(LogPakFile, Verbose, TEXT("%14llu - %14llu : %14llu patch padding."), PakFileHandle->Tell(), PakFileHandle->Tell() + PaddingSize, PaddingSize);
							check(PaddingSize <= PaddingBufferSize);

							//have to pad manually with 0's.  File locations skipped by Seek and never written are uninitialized which would defeat the whole purpose
							//of padding for certain platforms patch diffing systems.
							PakFileHandle->Serialize(PaddingBuffer, PaddingSize);
						}
						check(PakFileHandle->Tell() == NewEntryOffset);
						bPatchPadded = true;
					}
				}

				//if the current file is bigger than a patch block then we will always have to pad out the previous files.
				//if there were a large set of contiguous small files behind us then this will be the natural stopping point for a possible pathalogical patching case where growth in the small files causes a cascade 
				//to dirty up all the blocks prior to this one.  If this could happen let's warn about it.
				if (bPatchPadded ||
					FileIndex + 1 == FilesToAdd.Num()) // also check the last file, this won't work perfectly if we don't end up adding the last file for some reason
				{
					const uint64 ContiguousGroupedFilePatchWarningThreshhold = 50 * 1024 * 1024;
					if (ContiguousTotalSizeSmallerThanBlockSize > ContiguousGroupedFilePatchWarningThreshhold)
					{
						UE_LOG(LogPakFile, Display, TEXT("%i small files (%i) totaling %llu contiguous bytes found before first 'large' file.  Changes to any of these files could cause the whole group to be 'dirty' in a per-file binary diff based patching system."), ContiguousFilesSmallerThanBlockSize, RequiredPatchPadding, ContiguousTotalSizeSmallerThanBlockSize);
					}
					ContiguousTotalSizeSmallerThanBlockSize = 0;
					ContiguousFilesSmallerThanBlockSize = 0;
				}
				else
				{
					ContiguousTotalSizeSmallerThanBlockSize += TotalSizeToWrite;
					ContiguousFilesSmallerThanBlockSize++;				
				}
			}
			if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
			{
				FinalizeCopyCompressedFileToPak(*PakFileHandle, CompressedFileBuffer, NewEntry);
			}

			// Write to file
			int64 Offset = PakFileHandle->Tell();
			NewEntry.Info.Serialize(*PakFileHandle, FPakInfo::PakFile_Version_Latest);
			PakFileHandle->Serialize(DataToWrite, SizeToWrite);	
			int64 EndOffset = PakFileHandle->Tell();

			UE_LOG(LogPakFile, Verbose, TEXT("%14llu - %14llu : %14llu header+file %s."), Offset, EndOffset, EndOffset - Offset, *NewEntry.Filename);

			// Update offset now and store it in the index (and only in index)
			NewEntry.Info.Offset = NewEntryOffset;
			Index.Add(NewEntry);
			const TCHAR* EncryptedString = TEXT("");

			if (FilesToAdd[FileIndex].bNeedEncryption)
			{
				TotalRequestedEncryptedFiles++;

				if (GAESKeyANSI != nullptr)
				{
					TotalEncryptedFiles++;
					EncryptedString = TEXT("encrypted ");
				}
			}

			if (FilesToAdd[FileIndex].bNeedsCompression && CompressionMethod != COMPRESS_None)
			{
				TotalCompressedSize += NewEntry.Info.Size;
				TotalUncompressedSize += NewEntry.Info.UncompressedSize;
				float PercentLess = ((float)NewEntry.Info.Size / (NewEntry.Info.UncompressedSize / 100.f));
				if (FilesToAdd[FileIndex].SuggestedOrder < MAX_uint64)
				{
					UE_LOG(LogPakFile, Log, TEXT("Added compressed %sfile \"%s\", %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes (order %llu)."), EncryptedString, *NewEntry.Filename, PercentLess, NewEntry.Info.Size, NewEntry.Info.UncompressedSize, FilesToAdd[FileIndex].SuggestedOrder);
				}
				else
				{
					UE_LOG(LogPakFile, Log, TEXT("Added compressed %sfile \"%s\", %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes (no order given)."), EncryptedString, *NewEntry.Filename, PercentLess, NewEntry.Info.Size, NewEntry.Info.UncompressedSize);
				}
			}
			else
			{
				if (FilesToAdd[FileIndex].SuggestedOrder < MAX_uint64)
				{
					UE_LOG(LogPakFile, Log, TEXT("Added %sfile \"%s\", %lld bytes (order %llu)."), EncryptedString, *NewEntry.Filename, NewEntry.Info.Size, FilesToAdd[FileIndex].SuggestedOrder);
				}
				else
				{
					UE_LOG(LogPakFile, Log, TEXT("Added %sfile \"%s\", %lld bytes (no order given)."), EncryptedString, *NewEntry.Filename, NewEntry.Info.Size);
				}
			}
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Missing file \"%s\" will not be added to PAK file."), *FilesToAdd[FileIndex].Source);
		}
	}

	FMemory::Free(PaddingBuffer);
	FMemory::Free(ReadBuffer);
	ReadBuffer = NULL;

	// Remember IndexOffset
	Info.IndexOffset = PakFileHandle->Tell();

	// Serialize Pak Index at the end of Pak File
	TArray<uint8> IndexData;
	FMemoryWriter IndexWriter(IndexData);
	IndexWriter.SetByteSwapping(PakFileHandle->ForceByteSwapping());
	int32 NumEntries = Index.Num();
	IndexWriter << MountPoint;
	IndexWriter << NumEntries;
	for (int32 EntryIndex = 0; EntryIndex < Index.Num(); EntryIndex++)
	{
		FPakEntryPair& Entry = Index[EntryIndex];
		IndexWriter << Entry.Filename;
		Entry.Info.Serialize(IndexWriter, Info.Version);

		if (RequiredPatchPadding > 0)
		{
			int64 EntrySize = Entry.Info.GetSerializedSize(FPakInfo::PakFile_Version_Latest);
			int64 TotalSizeToWrite = Entry.Info.Size + EntrySize;
			if (TotalSizeToWrite >= RequiredPatchPadding)
			{
				int64 RealStart = Entry.Info.Offset;
				if ((RealStart % RequiredPatchPadding) != 0 && 
					!Entry.Filename.EndsWith(TEXT("uexp"))) // these are export sections of larger files and may be packed with uasset/umap and so we don't need a warning here
				{
					UE_LOG(LogPakFile, Warning, TEXT("File at offset %lld of size %lld not aligned to patch size %i"), RealStart, Entry.Info.Size, RequiredPatchPadding);
				}
			}
		}
	}

	if (Info.bEncryptedIndex)
	{
		int32 OriginalSize = IndexData.Num();
		int32 AlignedSize = Align(OriginalSize, FAES::AESBlockSize);

		for (int32 PaddingIndex = IndexData.Num(); PaddingIndex < AlignedSize; ++PaddingIndex)
		{
			uint8 Byte = IndexData[PaddingIndex % OriginalSize];
			IndexData.Add(Byte);
		}
	}

	FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), Info.IndexHash);

	if (Info.bEncryptedIndex)
	{
		UE_LOG(LogPakFile, Display, TEXT("Encrypting index..."));
		FAES::EncryptData(IndexData.GetData(), IndexData.Num(), GAESKeyANSI);
	}

	PakFileHandle->Serialize(IndexData.GetData(), IndexData.Num());

	Info.IndexSize = IndexData.Num();

	// Save trailer (offset, size, hash value)
	Info.Serialize(*PakFileHandle);

	UE_LOG(LogPakFile, Display, TEXT("Added %d files, %lld bytes total, time %.2lfs."), Index.Num(), PakFileHandle->TotalSize(), FPlatformTime::Seconds() - StartTime);
	if (TotalUncompressedSize)
	{
		float PercentLess = ((float)TotalCompressedSize / (TotalUncompressedSize / 100.f));
		UE_LOG(LogPakFile, Display, TEXT("Compression summary: %.2f%% of original size. Compressed Size %lld bytes, Original Size %lld bytes. "), PercentLess, TotalCompressedSize, TotalUncompressedSize);
	}

	if (TotalEncryptedFiles)
	{
		UE_LOG(LogPakFile, Display, TEXT("Encryption summary: %d files were encrypted. "), TotalEncryptedFiles);
	}

	if (TotalEncryptedFiles < TotalRequestedEncryptedFiles)
	{
		UE_LOG(LogPakFile, Display, TEXT("%d files requested encryption, but no AES key was supplied! Encryption was skipped for these files"), TotalRequestedEncryptedFiles);
	}

	PakFileHandle->Close();
	PakFileHandle.Reset();

	return true;
}

bool TestPakFile(const TCHAR* Filename)
{	
	FPakFile PakFile(Filename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	if (PakFile.IsValid())
	{
		return PakFile.Check();
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), Filename);
		return false;
	}
}

bool ListFilesInPak(const TCHAR * InPakFilename, int64 SizeFilter = 0)
{
	FPakFile PakFile(InPakFilename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	int32 FileCount = 0;
	int64 FileSize = 0;
	int64 FilteredSize = 0;

	if (PakFile.IsValid())
	{
		UE_LOG(LogPakFile, Display, TEXT("Mount point %s"), *PakFile.GetMountPoint());

		TArray<FPakFile::FFileIterator> Records;

		for (FPakFile::FFileIterator It(PakFile); It; ++It)
		{
			Records.Add(It);
		}

		struct FOffsetSort
		{
			FORCEINLINE bool operator()(const FPakFile::FFileIterator& A, const FPakFile::FFileIterator& B) const
			{
				return A.Info().Offset < B.Info().Offset;
			}
		};

		Records.Sort(FOffsetSort());

		for (auto It : Records)
		{
			const FPakEntry& Entry = It.Info();
			if (Entry.Size >= SizeFilter)
			{
				UE_LOG(LogPakFile, Display, TEXT("\"%s\" offset: %lld, size: %d bytes, sha1: %s."), *It.Filename(), Entry.Offset, Entry.Size, *BytesToHex(Entry.Hash, sizeof(Entry.Hash)));
				FilteredSize += Entry.Size;
			}
			FileSize += Entry.Size;
			FileCount++;
		}
		UE_LOG(LogPakFile, Display, TEXT("%d files (%lld bytes), (%lld filtered bytes)."), FileCount, FileSize, FilteredSize);

		return true;
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), InPakFilename);
		return false;
	}
}

struct FFileInfo
{
	uint64 FileSize;
	uint64 PatchIndex;
	uint8 Hash[16];
};

bool ExtractFilesFromPak(const TCHAR* InPakFilename, TMap<FString, FFileInfo>& InFileHashes, const TCHAR* InDestPath, bool bUseMountPoint = false)
{
	// Gather all patch versions of the requested pak file and run through each separately
	TArray<FString> PakFileList;
	FString PakFileDirectory = FPaths::GetPath(InPakFilename);
	// If file doesn't exist try using it as a search string, it may contain wild cards
	if (IFileManager::Get().FileExists(InPakFilename))
	{
		PakFileList.Add(*FPaths::GetCleanFilename(InPakFilename));
	}
	else
	{
		IFileManager::Get().FindFiles(PakFileList, *PakFileDirectory, *FPaths::GetCleanFilename(InPakFilename));
	}

	for (int32 PakFileIndex = 0; PakFileIndex < PakFileList.Num(); PakFileIndex++)
	{
		FString PakFilename = PakFileDirectory + "\\" + PakFileList[PakFileIndex];
		// Gather the pack file index from the filename. The base pak file holds index -1;
		int32 PakPriority = -1;
		if (PakFilename.EndsWith("_P.pak"))
		{
			FString PakIndexFromFilename = PakFilename.LeftChop(6);
			int32 PakIndexStart = INDEX_NONE;
			PakIndexFromFilename.FindLastChar('_', PakIndexStart);
			if (PakIndexStart != INDEX_NONE)
			{
				PakIndexFromFilename = PakIndexFromFilename.RightChop(PakIndexStart + 1);
				if (PakIndexFromFilename.IsNumeric())
				{
					PakPriority = FCString::Atoi(*PakIndexFromFilename);
				}
			}
		}

		FPakFile PakFile(*PakFilename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
		if (PakFile.IsValid())
		{
			FString DestPath(InDestPath);
			FArchive& PakReader = *PakFile.GetSharedReader(NULL);
			const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
			void* Buffer = FMemory::Malloc(BufferSize);
			int64 CompressionBufferSize = 0;
			uint8* PersistantCompressionBuffer = NULL;
			int32 ErrorCount = 0;
			int32 FileCount = 0;

			FString PakMountPoint = bUseMountPoint ? PakFile.GetMountPoint().Replace(TEXT("../../../"), TEXT("")) : TEXT("");

			for (FPakFile::FFileIterator It(PakFile); It; ++It, ++FileCount)
			{
				// Extract only the most recent version of a file when present in multiple paks
				FFileInfo* HashFileInfo = InFileHashes.Find(It.Filename());
				if (HashFileInfo == nullptr || HashFileInfo->PatchIndex == PakPriority)
				{
					const FPakEntry& Entry = It.Info();
					PakReader.Seek(Entry.Offset);
					uint32 SerializedCrcTest = 0;
					FPakEntry EntryInfo;
					EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);
					if (EntryInfo == Entry)
					{
						FString DestFilename(DestPath / PakMountPoint / It.Filename());

						TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));
						if (FileHandle)
						{
							if (Entry.CompressionMethod == COMPRESS_None)
							{
								BufferedCopyFile(*FileHandle, PakReader, Entry, Buffer, BufferSize);
							}
							else
							{
								UncompressCopyFile(*FileHandle, PakReader, Entry, PersistantCompressionBuffer, CompressionBufferSize);
							}
							UE_LOG(LogPakFile, Display, TEXT("Extracted \"%s\" to \"%s\"."), *It.Filename(), *DestFilename);
						}
						else
						{
							UE_LOG(LogPakFile, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
							ErrorCount++;
						}
					}
					else
					{
						UE_LOG(LogPakFile, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
						ErrorCount++;
					}
				}
			}
			FMemory::Free(Buffer);
			FMemory::Free(PersistantCompressionBuffer);

			UE_LOG(LogPakFile, Log, TEXT("Finished extracting %d files (including %d errors)."), FileCount, ErrorCount);
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), *PakFilename);
			return false;
		}
	}

	return true;
}

void CreateDiffRelativePathMap(TArray<FString>& FileNames, const FString& RootPath, TMap<FName, FString>& OutMap)
{
	for (int32 i = 0; i < FileNames.Num(); ++i)
	{
		const FString& FullPath = FileNames[i];
		FString RelativePath = FullPath.Mid(RootPath.Len());
		OutMap.Add(FName(*RelativePath), FullPath);
	}
}

bool DiffFilesInPaks(const FString InPakFilename1, const FString InPakFilename2)
{
	int32 NumUniquePAK1 = 0;
	int32 NumUniquePAK2 = 0;
	int32 NumDifferentContents = 0;
	int32 NumEqualContents = 0;

	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
	UE_LOG(LogPakFile, Log, TEXT("FileEventType, FileName, Size1, Size2"));

	// Allow the suppression of unique file logging for one or both files
	const bool bLogUniques = !FParse::Param(FCommandLine::Get(), TEXT("nouniques"));
	const bool bLogUniques1 = bLogUniques && !FParse::Param(FCommandLine::Get(), TEXT("nouniquesfile1"));
	const bool bLogUniques2 = bLogUniques && !FParse::Param(FCommandLine::Get(), TEXT("nouniquesfile2"));

	FPakFile PakFile1(*InPakFilename1, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	FPakFile PakFile2(*InPakFilename2, FParse::Param(FCommandLine::Get(), TEXT("signed")));
	if (PakFile1.IsValid() && PakFile2.IsValid())
	{		
		FArchive& PakReader1 = *PakFile1.GetSharedReader(NULL);
		FArchive& PakReader2 = *PakFile2.GetSharedReader(NULL);

		const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
		void* Buffer = FMemory::Malloc(BufferSize);
		int64 CompressionBufferSize = 0;
		uint8* PersistantCompressionBuffer = NULL;
		int32 ErrorCount = 0;
		int32 FileCount = 0;		
		
		//loop over pak1 entries.  compare against entry in pak2.
		for (FPakFile::FFileIterator It(PakFile1); It; ++It, ++FileCount)
		{
			const FString& PAK1FileName = It.Filename();

			//double check entry info and move pakreader into place
			const FPakEntry& Entry1 = It.Info();
			PakReader1.Seek(Entry1.Offset);

			FPakEntry EntryInfo1;
			EntryInfo1.Serialize(PakReader1, PakFile1.GetInfo().Version);

			if (EntryInfo1 != Entry1)
			{
				UE_LOG(LogPakFile, Log, TEXT("PakEntry1Invalid, %s, 0, 0"), *PAK1FileName);
				continue;
			}
			
			//see if entry exists in other pak							
			const FPakEntry* Entry2 = PakFile2.Find(PakFile1.GetMountPoint() / PAK1FileName);
			if (Entry2 == nullptr)
			{
				++NumUniquePAK1;
				if (bLogUniques1)
				{
					UE_LOG(LogPakFile, Log, TEXT("UniqueToFirstPak, %s, %i, 0"), *PAK1FileName, EntryInfo1.UncompressedSize);
				}
				continue;
			}

			//double check entry info and move pakreader into place
			PakReader2.Seek(Entry2->Offset);
			FPakEntry EntryInfo2;
			EntryInfo2.Serialize(PakReader2, PakFile2.GetInfo().Version);
			if (EntryInfo2 != *Entry2)
			{
				UE_LOG(LogPakFile, Log, TEXT("PakEntry2Invalid, %s, 0, 0"), *PAK1FileName);
				continue;;
			}

			//check sizes first as quick compare.
			if (EntryInfo1.UncompressedSize != EntryInfo2.UncompressedSize)
			{
				UE_LOG(LogPakFile, Log, TEXT("FilesizeDifferent, %s, %i, %i"), *PAK1FileName, EntryInfo1.UncompressedSize, EntryInfo2.UncompressedSize);
				continue;
			}
			
			//serialize and memcompare the two entries
			{
				void* PAKDATA1 = FMemory::Malloc(EntryInfo1.UncompressedSize);
				void* PAKDATA2 = FMemory::Malloc(EntryInfo2.UncompressedSize);
				FBufferWriter PAKWriter1(PAKDATA1, EntryInfo1.UncompressedSize, false);
				FBufferWriter PAKWriter2(PAKDATA2, EntryInfo2.UncompressedSize, false);

				if (EntryInfo1.CompressionMethod == COMPRESS_None)
				{
					BufferedCopyFile(PAKWriter1, PakReader1, Entry1, Buffer, BufferSize);
				}
				else
				{
					UncompressCopyFile(PAKWriter1, PakReader1, Entry1, PersistantCompressionBuffer, CompressionBufferSize);
				}

				if (EntryInfo2.CompressionMethod == COMPRESS_None)
				{
					BufferedCopyFile(PAKWriter2, PakReader2, *Entry2, Buffer, BufferSize);
				}
				else
				{
					UncompressCopyFile(PAKWriter2, PakReader2, *Entry2, PersistantCompressionBuffer, CompressionBufferSize);
				}

				if (FMemory::Memcmp(PAKDATA1, PAKDATA2, EntryInfo1.UncompressedSize) != 0)
				{
					++NumDifferentContents;
					UE_LOG(LogPakFile, Log, TEXT("ContentsDifferent, %s, %i, %i"), *PAK1FileName, EntryInfo1.UncompressedSize, EntryInfo2.UncompressedSize);
				}
				else
				{
					++NumEqualContents;
				}
				FMemory::Free(PAKDATA1);
				FMemory::Free(PAKDATA2);
			}			
		}
		
		//check for files unique to the second pak.
		for (FPakFile::FFileIterator It(PakFile2); It; ++It, ++FileCount)
		{
			const FPakEntry& Entry2 = It.Info();
			PakReader2.Seek(Entry2.Offset);

			FPakEntry EntryInfo2;
			EntryInfo2.Serialize(PakReader2, PakFile2.GetInfo().Version);

			if (EntryInfo2 == Entry2)
			{
				const FString& PAK2FileName = It.Filename();
				const FPakEntry* Entry1 = PakFile1.Find(PakFile2.GetMountPoint() / PAK2FileName);
				if (Entry1 == nullptr)
				{
					++NumUniquePAK2;
					if (bLogUniques2)
					{
						UE_LOG(LogPakFile, Log, TEXT("UniqueToSecondPak, %s, 0, %i"), *PAK2FileName, Entry2.UncompressedSize);
					}
					continue;
				}
			}
		}

		FMemory::Free(Buffer);
		Buffer = nullptr;
	}

	UE_LOG(LogPakFile, Log, TEXT("Comparison complete"));
	UE_LOG(LogPakFile, Log, TEXT("Unique to first pak: %i, Unique to second pak: %i, Num Different: %i, NumEqual: %i"), NumUniquePAK1, NumUniquePAK2, NumDifferentContents, NumEqualContents);	
	return true;
}

void GenerateHashForFile(uint8* ByteBuffer, uint64 TotalSize, FFileInfo& FileHash)
{
	FMD5 FileHasher;
	FileHasher.Update(ByteBuffer, TotalSize);
	FileHasher.Final(FileHash.Hash);
	FileHash.FileSize = TotalSize;
}

bool GenerateHashForFile( FString Filename, FFileInfo& FileHash)
{
	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);

	if ( File == NULL )
		return false;

	uint64 TotalSize = File->TotalSize();

	uint8* ByteBuffer = new uint8[TotalSize];

	File->Serialize(ByteBuffer, TotalSize);

	delete File;
	File = NULL;

	GenerateHashForFile(ByteBuffer, TotalSize, FileHash);
	
	delete[] ByteBuffer;
	return true;
}

bool GenerateHashesFromPak(const TCHAR* InPakFilename, const TCHAR* InDestPakFilename, TMap<FString, FFileInfo>& FileHashes, bool bUseMountPoint = false)
{
	if (!IFileManager::Get().FileExists(InPakFilename))
	{
		return false;
	}

	// Gather all patch pak files and run through them one at a time
	TArray<FString> PakFileList;
	FString PakFileDirectory = FPaths::GetPath(InPakFilename);
	IFileManager::Get().FindFiles(PakFileList, *PakFileDirectory, *FPaths::GetCleanFilename(InPakFilename));
	for (int32 PakFileIndex = 0; PakFileIndex < PakFileList.Num(); PakFileIndex++)
	{
		FString PakFilename = PakFileDirectory + "\\" + PakFileList[PakFileIndex];
		// Skip the destination pak file so we can regenerate an existing patch level
		if (PakFilename.Equals(InDestPakFilename))
		{
			continue;
		}
		// Parse the pak file index, the base pak file is index -1
		int32 PakPriority = -1;
		if (PakFilename.EndsWith("_P.pak"))
		{
			FString PakIndexFromFilename = PakFilename.LeftChop(6);
			int32 PakIndexStart = INDEX_NONE;
			PakIndexFromFilename.FindLastChar('_', PakIndexStart);
			if (PakIndexStart != INDEX_NONE)
			{
				PakIndexFromFilename = PakIndexFromFilename.RightChop(PakIndexStart + 1);
				if (PakIndexFromFilename.IsNumeric())
				{
					PakPriority = FCString::Atoi(*PakIndexFromFilename);
				}
			}
		}

		FPakFile PakFile(*PakFilename, FParse::Param(FCommandLine::Get(), TEXT("signed")));
		if (PakFile.IsValid())
		{
			FArchive& PakReader = *PakFile.GetSharedReader(NULL);
			const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
			void* Buffer = FMemory::Malloc(BufferSize);
			int64 CompressionBufferSize = 0;
			uint8* PersistantCompressionBuffer = NULL;
			int32 ErrorCount = 0;
			int32 FileCount = 0;

			FString PakMountPoint = bUseMountPoint ? PakFile.GetMountPoint().Replace(TEXT("../../../"), TEXT("")) : TEXT("");

			for (FPakFile::FFileIterator It(PakFile); It; ++It, ++FileCount)
			{
				const FPakEntry& Entry = It.Info();
				PakReader.Seek(Entry.Offset);
				uint32 SerializedCrcTest = 0;
				FPakEntry EntryInfo;
				EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);
				if (EntryInfo == Entry)
				{
					// TAutoPtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));
					TArray<uint8> Bytes;
					FMemoryWriter MemoryFile(Bytes);
					FArchive* FileHandle = &MemoryFile;
					// if (FileHandle.IsValid())
					{
						if (Entry.CompressionMethod == COMPRESS_None)
						{
							BufferedCopyFile(*FileHandle, PakReader, Entry, Buffer, BufferSize);
						}
						else
						{
							UncompressCopyFile(*FileHandle, PakReader, Entry, PersistantCompressionBuffer, CompressionBufferSize);
						}

						FString FullFilename = PakMountPoint;
						if (!FullFilename.IsEmpty() && !FullFilename.EndsWith("/"))
						{
							FullFilename += "/";
						}
						FullFilename += It.Filename();
						UE_LOG(LogPakFile, Display, TEXT("Generated hash for \"%s\""), *FullFilename);
						FFileInfo FileHash;
						GenerateHashForFile(Bytes.GetData(), Bytes.Num(), FileHash);
						FileHash.PatchIndex = PakPriority;

						// Keep only the hash of the most recent version of a file (across multiple pak patch files)
						if (!FileHashes.Contains(FullFilename))
						{
							FileHashes.Add(FullFilename, FileHash);
						}
						else if (FileHashes[FullFilename].PatchIndex < FileHash.PatchIndex)
						{
							FileHashes[FullFilename] = FileHash;
						}
					}
					/*else
					{
					UE_LOG(LogPakFile, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
					ErrorCount++;
					}*/

				}
				else
				{
					UE_LOG(LogPakFile, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
					ErrorCount++;
				}
			}
			FMemory::Free(Buffer);
			FMemory::Free(PersistantCompressionBuffer);

			UE_LOG(LogPakFile, Log, TEXT("Finished extracting %d files (including %d errors)."), FileCount, ErrorCount);
		}
		else
		{
			UE_LOG(LogPakFile, Error, TEXT("Unable to open pak file \"%s\"."), *PakFilename);
			return false;
		}
	}

	return true;
}

bool FileIsIdentical(FString SourceFile, FString DestFilename, const FFileInfo* Hash)
{
	int64 SourceTotalSize = Hash ? Hash->FileSize : IFileManager::Get().FileSize(*SourceFile);
	int64 DestTotalSize = IFileManager::Get().FileSize(*DestFilename);

	if (SourceTotalSize != DestTotalSize)
	{
		// file size doesn't match 
		UE_LOG(LogPakFile, Display, TEXT("Source file size for %s %d bytes doesn't match %s %d bytes, did find %d"), *SourceFile, SourceTotalSize, *DestFilename, DestTotalSize, Hash ? 1 : 0);
		return false;
	}

	FFileInfo SourceFileHash;
	if (!Hash)
	{
		if (GenerateHashForFile(SourceFile, SourceFileHash) == false)
		{
			// file size doesn't match 
			UE_LOG(LogPakFile, Display, TEXT("Source file size %s doesn't exist will be included in build"), *SourceFile);
			return false;;
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Generated hash for file %s but it should have been in the FileHashes array"), *SourceFile);
		}
	}
	else
	{
		SourceFileHash = *Hash;
	}

	FFileInfo DestFileHash;
	if (GenerateHashForFile(DestFilename, DestFileHash) == false)
	{
		// destination file was removed don't really care about it
		UE_LOG(LogPakFile, Display, TEXT("File was removed from destination cooked content %s not included in patch"), *DestFilename);
		return false;
	}

	int32 Diff = FMemory::Memcmp(&SourceFileHash.Hash, &DestFileHash.Hash, sizeof(DestFileHash.Hash));
	if (Diff != 0)
	{
		UE_LOG(LogPakFile, Display, TEXT("Source file hash for %s doesn't match dest file hash %s and will be included in patch"), *SourceFile, *DestFilename);
		return false;
	}

	return true;
}

void RemoveIdenticalFiles( TArray<FPakInputPair>& FilesToPak, const FString& SourceDirectory, const TMap<FString, FFileInfo>& FileHashes )
{
	FString HashFilename = SourceDirectory / TEXT("Hashes.txt");

	if (IFileManager::Get().FileExists(*HashFilename) )
	{
		FString EntireFile;
		FFileHelper::LoadFileToString(EntireFile, *HashFilename);
	}

	TArray<FString> FilesToRemove;

	for ( int I = FilesToPak.Num()-1; I >= 0; --I )
	{
		const auto& NewFile = FilesToPak[I]; 

		FString SourceFileNoMountPoint =  NewFile.Dest.Replace(TEXT("../../../"), TEXT(""));
		FString SourceFilename = SourceDirectory / NewFile.Dest.Replace(TEXT("../../../"), TEXT(""));
		
		const FFileInfo* FoundFileHash = FileHashes.Find(SourceFileNoMountPoint);
		if (!FoundFileHash)
		{
			FoundFileHash = FileHashes.Find(NewFile.Dest);
		}
		
 		if ( !FoundFileHash )
 		{
 			UE_LOG(LogPakFile, Display, TEXT("Didn't find hash for %s No mount %s"), *SourceFilename, *SourceFileNoMountPoint);
 		}
 
		// uexp files are always handled with their corresponding uasset file
		if (!FPaths::GetExtension(SourceFilename).Equals("uexp", ESearchCase::IgnoreCase))
		{
			FString DestFilename = NewFile.Source;
			if (FileIsIdentical(SourceFilename, DestFilename, FoundFileHash))
			{
				// Check for uexp files only for uasset files
				if (FPaths::GetExtension(SourceFilename).Equals("uasset", ESearchCase::IgnoreCase))
				{
					FString UexpSourceFilename = FPaths::ChangeExtension(SourceFilename, "uexp");
					FString UexpSourceFileNoMountPoint = FPaths::ChangeExtension(SourceFileNoMountPoint, "uexp");

					const FFileInfo* UexpFoundFileHash = FileHashes.Find(UexpSourceFileNoMountPoint);
					if (!UexpFoundFileHash)
					{
						UexpFoundFileHash = FileHashes.Find(FPaths::ChangeExtension(NewFile.Dest, "uexp"));
					}

					if (!UexpFoundFileHash)
					{
						UE_LOG(LogPakFile, Display, TEXT("Didn't find hash for %s No mount %s"), *UexpSourceFilename, *UexpSourceFileNoMountPoint);
					}

					if (UexpFoundFileHash || IFileManager::Get().FileExists(*UexpSourceFilename))
					{

						FString UexpDestFilename = FPaths::ChangeExtension(NewFile.Source, "uexp");
						if (!FileIsIdentical(UexpSourceFilename, UexpDestFilename, UexpFoundFileHash))
						{
							UE_LOG(LogPakFile, Display, TEXT("%s not identical for %s. Including both files in patch."), *UexpSourceFilename, *SourceFilename);
							continue;
						}
						// Add this file to the list to be removed from FilesToPak after we finish processing (since this file was found at random within 
						// the list we cannot remove it or we'll mess up our containing for loop)
						FilesToRemove.Add(UexpDestFilename);
					}
				}

				UE_LOG(LogPakFile, Display, TEXT("Source file %s matches dest file %s and will not be included in patch"), *SourceFilename, *DestFilename);
				// remove from the files to pak list
				FilesToPak.RemoveAt(I);
			}
		}
	}

	// Clean up uexp files that were marked for removal, assume files may only be listed one in FilesToPak
	for (int FileIndexToRemove = 0; FileIndexToRemove < FilesToRemove.Num(); FileIndexToRemove++)
	{
		const FPakInputPair FileSourceToRemove(FilesToRemove[FileIndexToRemove], "");
		FilesToPak.RemoveSingle(FileSourceToRemove);
	}
}

FString GetPakPath(const TCHAR* SpecifiedPath, bool bIsForCreation)
{
	FString PakFilename(SpecifiedPath);
	FPaths::MakeStandardFilename(PakFilename);
	
	// if we are trying to open (not create) it, but BaseDir relative doesn't exist, look in LaunchDir
	if (!bIsForCreation && !FPaths::FileExists(PakFilename))
	{
		PakFilename = FPaths::LaunchDir() + SpecifiedPath;

		if (!FPaths::FileExists(PakFilename))
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Existing pak file %s could not be found (checked against binary and launch directories)"), SpecifiedPath);
			return TEXT("");
		}
	}
	
	return PakFilename;
}

struct FPackage
{
public:
	TSet<FPackage*> DirectlyReferencing;
	TSet<FPackage*> DirectlyReferencedBy;
	TSet<FPackage*> AllReferences;

	FName Name;
	int64 InclusiveSize;
	int64 ExclusiveSize;
	int Id;

private:
	bool bUpdateHelper;
	static TMap<FName,FPackage*> NameToPackageMap;

public:
	FPackage(const FName& InName,int InId)
		: Name(InName)
		, InclusiveSize(0)
		, ExclusiveSize(0)
		, Id(InId)
		, bUpdateHelper(false)
	{}

	static FPackage* FindOrCreatePackage( FName PackageName )
	{
		static int Id = 1;
		FPackage* Package = NameToPackageMap.FindRef(PackageName);
		if(!Package)
		{
			Package = new FPackage(PackageName,Id++);
			NameToPackageMap.Add(PackageName,Package);
		}
		return Package;
	}

	void ResetUpdateHelper()
	{
		bUpdateHelper = false;
	}

	void RecurseUpdateReferences()
	{
		if( !bUpdateHelper )
		{
			bUpdateHelper = true;
			for( auto& DirectReference : DirectlyReferencing )
			{
				AllReferences.Add(DirectReference);
				DirectReference->RecurseUpdateReferences();
				AllReferences.Append(DirectReference->AllReferences);
			}
		}
	}

	void UpdateInclusiveSize()
	{
		InclusiveSize = ExclusiveSize;
		for(auto& Reference : AllReferences)
		{
			InclusiveSize += Reference->ExclusiveSize;
		}
	}

	static void GetAllPackages( TArray<FPackage*>& OutPackages )
	{
		OutPackages.Reset(NameToPackageMap.Num());
		for( const auto& Entry : NameToPackageMap )
		{
			OutPackages.Add(Entry.Value);
		}
	}

	TArray< TSharedPtr<FJsonValue> > ToJsonHelper( const TSet<FPackage*>& Packages )
	{
		TArray< TSharedPtr<FJsonValue> > JsonPackageNames;
		for( const auto Package : Packages )
		{
			JsonPackageNames.Add(MakeShareable(new FJsonValueString(Package->Name.ToString())));
		}
		return JsonPackageNames;
	}

	TSharedPtr<FJsonObject> ToJsonObject()
	{
		TSharedPtr<FJsonObject> JsonPackageObject = MakeShareable(new FJsonObject);
		
		JsonPackageObject->SetStringField(TEXT("Name"),*Name.ToString());
		JsonPackageObject->SetNumberField(TEXT("InclusiveSize"),InclusiveSize);
		JsonPackageObject->SetNumberField(TEXT("ExclusiveSize"),ExclusiveSize);

		JsonPackageObject->SetArrayField(TEXT("DirectlyReferencing"),ToJsonHelper(DirectlyReferencing));
		JsonPackageObject->SetArrayField(TEXT("DirectlyReferencedBy"),ToJsonHelper(DirectlyReferencedBy));
		JsonPackageObject->SetArrayField(TEXT("AllReferences"),ToJsonHelper(AllReferences));

		return JsonPackageObject;
	}
};
TMap<FName,FPackage*> FPackage::NameToPackageMap;

bool ExportDependencies(const TCHAR * PakFilename, const TCHAR* GameName, const TCHAR* GameFolderName, const TCHAR* OutputFilenameBase )
{
	// Example command line used for this tool
	// C:\Development\BB\WEX\Saved\StagedBuilds\WindowsNoEditor\WorldExplorers\Content\Paks\WorldExplorers-WindowsNoEditor.pak WorldExplorers WEX -exportdependencies=c:\dvtemp\output -debug -NoAssetRegistryCache -ForceDependsGathering
	
	FPakFile PakFile(PakFilename,FParse::Param(FCommandLine::Get(),TEXT("signed")));

	if(PakFile.IsValid())
	{
		// Get size information from PAK file.
		{
			TArray<FPakFile::FFileIterator> Records;
			FString PakGameContentFolder = FString(GameName) + TEXT("/Content");
			for(FPakFile::FFileIterator It(PakFile); It; ++It)
			{
				FString PackageName;
				It.Filename().Split(TEXT("."),&PackageName,NULL);
				int64 Size = It.Info().Size;			

				if( PackageName.StartsWith(TEXT("Engine/Content")) )
				{
					PackageName = PackageName.Replace(TEXT("Engine/Content"),TEXT("/Engine"));
				}
				else if( PackageName.StartsWith(*PakGameContentFolder))
				{
					PackageName = PackageName.Replace(*PakGameContentFolder,TEXT("/Game"));
				}

				FPackage* Package = FPackage::FindOrCreatePackage(FName(*PackageName));
				Package->ExclusiveSize += Size;
			}
		}

		TMap<FName,FName> PackageToClassMap;

		// Combine with dependency information from asset registry.
		{
			FString ProjectDir = FString(TEXT("../../../")) + GameFolderName + TEXT("/");
			FPlatformMisc::SetOverrideProjectDir(*ProjectDir);
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		
			AssetRegistryModule.Get().SearchAllAssets(true);
			TArray<FAssetData> AssetData;		
			AssetRegistryModule.Get().GetAllAssets(AssetData,true);	
	
			TSet<FName> PackageNames;
			for( int i=0; i<AssetData.Num(); i++ )
			{
				PackageNames.Add(AssetData[i].PackageName);
				PackageToClassMap.Add(AssetData[i].PackageName,AssetData[i].AssetClass);
			}

			for( const auto& PackageName : PackageNames )
			{
				TArray<FName> DependencyArray;
				AssetRegistryModule.Get().GetDependencies(PackageName,DependencyArray);

				FPackage* Package = FPackage::FindOrCreatePackage(PackageName);
				for( const auto& DependencyName : DependencyArray )
				{
					// exclude '/Script/' as it clutters up things significantly.
					if( !DependencyName.ToString().StartsWith(TEXT("/Script/")) )
					{
						FPackage* Dependency = FPackage::FindOrCreatePackage(DependencyName);
						Package->DirectlyReferencing.Add(Dependency);
						Dependency->DirectlyReferencedBy.Add(Package);
					}
				}
			}

			// 2 passes are required to deal with cycles.
			for(const auto& PackageName : PackageNames)
			{
				FPackage* Package = FPackage::FindOrCreatePackage(PackageName);
				Package->RecurseUpdateReferences();
			}
			for(const auto& PackageName : PackageNames)
			{
				FPackage* Package = FPackage::FindOrCreatePackage(PackageName);
				Package->ResetUpdateHelper();
			}
			for(const auto& PackageName : PackageNames)
			{
				FPackage* Package = FPackage::FindOrCreatePackage(PackageName);
				Package->RecurseUpdateReferences();
			}
		}

		// Update inclusive size, asset class, and export to CSV, JSON, and GDF
		{
			TSharedPtr<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
			TArray< TSharedPtr<FJsonValue> > JsonPackages;

			TArray<FPackage*> AllPackages;
			FPackage::GetAllPackages(AllPackages);

			for(auto Package : AllPackages)
			{
				Package->UpdateInclusiveSize();
				JsonPackages.Add( MakeShareable(new FJsonValueObject(Package->ToJsonObject())) );
			}
			JsonRootObject->SetArrayField(TEXT("Packages"),JsonPackages);

			FString JsonOutputString;
			TSharedRef<TJsonWriter<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonOutputString);
			FJsonSerializer::Serialize(JsonRootObject.ToSharedRef(),JsonWriter);

			FArchive* JsonFileWriter = IFileManager::Get().CreateFileWriter(*(FString(OutputFilenameBase)+TEXT(".json")));
			if(JsonFileWriter)
			{
				JsonFileWriter->Logf(TEXT("%s"),*JsonOutputString);
				JsonFileWriter->Close();
				delete JsonFileWriter;
			}

			FArchive* CSVFileWriter = IFileManager::Get().CreateFileWriter(*(FString(OutputFilenameBase)+TEXT(".csv")));
			if(CSVFileWriter)
			{
				CSVFileWriter->Logf(TEXT("class,name,inclusive,exclusive"));
				for(auto Package : AllPackages)
				{
					FName ClassName = PackageToClassMap.FindRef(Package->Name);
					CSVFileWriter->Logf(TEXT("%s,%s,%i,%i"),*ClassName.ToString(),*Package->Name.ToString(),Package->InclusiveSize,Package->ExclusiveSize);
				}
				CSVFileWriter->Close();
				delete CSVFileWriter;
				CSVFileWriter = NULL;
			}

			FArchive* GDFFileWriter = IFileManager::Get().CreateFileWriter(*(FString(OutputFilenameBase)+TEXT(".gdf")));
			if(GDFFileWriter)
			{
				GDFFileWriter->Logf(TEXT("nodedef> name VARCHAR,label VARCHAR,inclusive DOUBLE,exclusive DOUBLE"));
				GDFFileWriter->Logf(TEXT("0,root,0,0"));
				for(auto Package : AllPackages)
				{
					GDFFileWriter->Logf(TEXT("%i,%s,%i,%i"),Package->Id,*Package->Name.ToString(),Package->InclusiveSize,Package->ExclusiveSize);
				}
				GDFFileWriter->Logf(TEXT("edgedef> node1 VARCHAR,node2 VARCHAR"));
				// fake root to ensure spanning tree
				for(auto Package : AllPackages)
				{
					GDFFileWriter->Logf(TEXT("0,%i"),Package->Id);
				}
				for(auto Package : AllPackages)
				{
					for( auto ReferencedPackage : Package->DirectlyReferencing )
					{
						GDFFileWriter->Logf(TEXT("%i,%i"),Package->Id,ReferencedPackage->Id);
					}
				}
				GDFFileWriter->Close();
				delete GDFFileWriter;
				GDFFileWriter = NULL;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}


/**
 * Application entry point
 * Params:
 *   -Test test if the pak file is healthy
 *   -Extract extracts pak file contents (followed by a path, i.e.: -extract D:\ExtractedPak)
 *   -Create=filename response file to create a pak file with
 *   -Sign=filename use the key pair in filename to sign a pak file, or: -sign=key_hex_values_separated_with_+, i.e: -sign=0x123456789abcdef+0x1234567+0x12345abc
 *    where the first number is the private key exponend, the second one is modulus and the third one is the public key exponent.
 *   -Signed use with -extract and -test to let the code know this is a signed pak
 *   -GenerateKeys=filename generates encryption key pair for signing a pak file
 *   -P=prime will use a predefined prime number for generating encryption key file
 *   -Q=prime same as above, P != Q, GCD(P, Q) = 1 (which is always true if they're both prime)
 *   -GeneratePrimeTable=filename generates a prime table for faster prime number generation (.inl file)
 *   -TableMax=number maximum prime number in the generated table (default is 10000)
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// start up the main loop
	GEngineLoop.PreInit(ArgC, ArgV);

	if (ArgC < 2)
	{
		UE_LOG(LogPakFile, Error, TEXT("No pak file name specified. Usage:"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Test"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -List"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> <GameUProjectName> <GameFolderName> -ExportDependencies=<OutputFileBase> -NoAssetRegistryCache -ForceDependsGathering"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Extract <ExtractDir>"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Create=<ResponseFile> [Options]"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename> -Dest=<MountPoint>"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak GenerateKeys=<KeyFilename>"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak GeneratePrimeTable=<KeyFilename> [-TableMax=<N>]"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak <PakFilename1> <PakFilename2> -diff"));
		UE_LOG(LogPakFile, Error, TEXT("  UnrealPak -TestEncryption"));
		UE_LOG(LogPakFile, Error, TEXT("  Options:"));
		UE_LOG(LogPakFile, Error, TEXT("    -blocksize=<BlockSize>"));
		UE_LOG(LogPakFile, Error, TEXT("    -bitwindow=<BitWindow>"));
		UE_LOG(LogPakFile, Error, TEXT("    -compress"));
		UE_LOG(LogPakFile, Error, TEXT("    -encrypt"));
		UE_LOG(LogPakFile, Error, TEXT("    -order=<OrderingFile>"));
		UE_LOG(LogPakFile, Error, TEXT("    -diff (requires 2 filenames first)"));
		UE_LOG(LogPakFile, Error, TEXT("    -enginedir (specify engine dir for when using ini encryption configs)"));
		UE_LOG(LogPakFile, Error, TEXT("    -projectdir (specify project dir for when using ini encryption configs)"));
		UE_LOG(LogPakFile, Error, TEXT("    -encryptionini (specify ini base name to gather encryption settings from)"));
		UE_LOG(LogPakFile, Error, TEXT("    -encryptindex (encrypt the pak file index, making it unusable in unrealpak without supplying the key)"));
		return 1;
	}

	double StartTime = FPlatformTime::Seconds();
	PrepareEncryptionAndSigningKeys();

	FPakCommandLineParameters CmdLineParameters;
	int32 Result = 0;
	FString KeyFilename;
	if (FParse::Value(FCommandLine::Get(), TEXT("GenerateKeys="), KeyFilename, false))
	{
		Result = GenerateKeys(*KeyFilename) ? 0 : 1;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("GeneratePrimeTable="), KeyFilename, false))
	{
		int64 MaxPrimeValue = 10000;
		FParse::Value(FCommandLine::Get(), TEXT("TableMax="), MaxPrimeValue);
		GeneratePrimeNumberTable(MaxPrimeValue, *KeyFilename);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("TestEncryption")))
	{
		void TestEncryption();
		TestEncryption();
	}
	else 
	{
		FString ExportDependencyFilename;
		if (FParse::Param(FCommandLine::Get(), TEXT("Test")))
		{
			FString PakFilename = GetPakPath(ArgV[1], false);
			Result = TestPakFile(*PakFilename) ? 0 : 1;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("List")))
		{
			int64 SizeFilter = 0;
			FParse::Value(FCommandLine::Get(), TEXT("SizeFilter="), SizeFilter);

			FString PakFilename = GetPakPath(ArgV[1], false);
			Result = ListFilesInPak(*PakFilename, SizeFilter) ? 0 : 1;
		}
		else if(FParse::Value(FCommandLine::Get(),TEXT("ExportDependencies="),ExportDependencyFilename,false))
		{			
			if(ArgC < 4)
			{
				UE_LOG(LogPakFile,Error,TEXT("Insufficient arguments."));
			}
			else
			{
				FString PakFilename = GetPakPath(ArgV[1],false);
				Result = ExportDependencies(*PakFilename,ArgV[2],ArgV[3],*ExportDependencyFilename);					
			}
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("Diff")))
		{
			FString PakFilename1 = GetPakPath(ArgV[1], false);
			FString PakFilename2 = GetPakPath(ArgV[2], false);
			Result = DiffFilesInPaks(*PakFilename1, *PakFilename2) ? 0 : 1;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("Extract")))
		{
			FString PakFilename = GetPakPath(ArgV[1], false);
			if (ArgC < 4)
			{
				UE_LOG(LogPakFile, Error, TEXT("No extraction path specified."));
				Result = 1;
			}
			else
			{
				FString DestPath = (ArgV[2][0] == '-') ? ArgV[3] : ArgV[2];
				TMap<FString, FFileInfo> EmptyMap;
				Result = ExtractFilesFromPak(*PakFilename, EmptyMap, *DestPath) ? 0 : 1;
			}
		}
		else
		{
			// since this is for creation, we pass true to make it not look in LaunchDir
			FString PakFilename = GetPakPath(ArgV[1], true);

			// List of all items to add to pak file
			TArray<FPakInputPair> Entries;
			ProcessCommandLine(ArgC, ArgV, Entries, CmdLineParameters);
			TMap<FString, uint64> OrderMap;
			ProcessOrderFile(ArgC, ArgV, OrderMap);

			if (Entries.Num() == 0)
			{
				UE_LOG(LogPakFile, Error, TEXT("No files specified to add to pak file."));
				Result = 1;
			}
			else
			{
				TMap<FString, FFileInfo> SourceFileHashes;

				if ( CmdLineParameters.GeneratePatch )
				{
					FString OutputPath;
					if (!FParse::Value(FCommandLine::Get(), TEXT("TempFiles="), OutputPath))
					{
						OutputPath = FPaths::GetPath(PakFilename) / FString(TEXT("TempFiles"));
					}

					IFileManager::Get().DeleteDirectory(*OutputPath);

					UE_LOG(LogPakFile, Display, TEXT("Generating patch from %s."), *CmdLineParameters.SourcePatchPakFilename, true );

					if ( !GenerateHashesFromPak(*CmdLineParameters.SourcePatchPakFilename, *PakFilename, SourceFileHashes, true) )
					{
						if ( ExtractFilesFromPak( *CmdLineParameters.SourcePatchPakFilename, SourceFileHashes, *OutputPath, true ) == false )
						{
							UE_LOG(LogPakFile, Warning, TEXT("Unable to extract files from source pak file for patch") );
						}
						else
						{
							CmdLineParameters.SourcePatchDiffDirectory = OutputPath;
						}
					}
				}


				// Start collecting files
				TArray<FPakInputPair> FilesToAdd;
				CollectFilesToAdd(FilesToAdd, Entries, OrderMap);

				if ( CmdLineParameters.GeneratePatch )
				{
					// if we are generating a patch here we remove files which are already shipped...
					RemoveIdenticalFiles(FilesToAdd, CmdLineParameters.SourcePatchDiffDirectory, SourceFileHashes);
				}


				Result = CreatePakFile(*PakFilename, FilesToAdd, CmdLineParameters) ? 0 : 1;

				if (CmdLineParameters.GeneratePatch)
				{
					FString OutputPath = FPaths::GetPath(PakFilename) / FString(TEXT("TempFiles"));
					// delete the temporary directory
					IFileManager::Get().DeleteDirectory(*OutputPath, false, true);
				}
			}
		}
	}

	UE_LOG(LogPakFile, Display, TEXT("Unreal pak executed in %f seconds"), FPlatformTime::Seconds() - StartTime );

	GLog->Flush();

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return Result;
}
