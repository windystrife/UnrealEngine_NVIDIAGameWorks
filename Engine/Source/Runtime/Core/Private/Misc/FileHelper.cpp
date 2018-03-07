// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/FileHelper.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/ByteSwap.h"
#include "Misc/CoreMisc.h"
#include "Misc/Paths.h"
#include "Math/IntRect.h"
#include "Misc/OutputDeviceFile.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"

#define LOCTEXT_NAMESPACE "FileHelper"

static const FString InvalidFilenames[] = {
	TEXT("CON"), TEXT("PRN"), TEXT("AUX"), TEXT("CLOCK$"), TEXT("NUL"), TEXT("NONE"),
	TEXT("COM1"), TEXT("COM2"), TEXT("COM3"), TEXT("COM4"), TEXT("COM5"), TEXT("COM6"), TEXT("COM7"), TEXT("COM8"), TEXT("COM9"),
	TEXT("LPT1"), TEXT("LPT2"), TEXT("LPT3"), TEXT("LPT4"), TEXT("LPT5"), TEXT("LPT6"), TEXT("LPT7"), TEXT("LPT8"), TEXT("LPT9")
};

/*-----------------------------------------------------------------------------
	FFileHelper
-----------------------------------------------------------------------------*/

/**
 * Load a binary file to a dynamic array.
 */
bool FFileHelper::LoadFileToArray( TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags )
{
	FScopedLoadingState ScopedLoadingState(Filename);

	FArchive* Reader = IFileManager::Get().CreateFileReader( Filename, Flags );
	if( !Reader )
	{
		if (!(Flags & FILEREAD_Silent))
		{
			UE_LOG(LogStreaming,Warning,TEXT("Failed to read file '%s' error."),Filename);
		}
		return 0;
	}
	int64 TotalSize = Reader->TotalSize();
	Result.Reset( TotalSize );
	Result.AddUninitialized( TotalSize );
	Reader->Serialize(Result.GetData(), Result.Num());
	bool Success = Reader->Close();
	delete Reader;
	return Success;
}

/**
 * Converts an arbitrary text buffer to an FString.
 * Supports all combination of ANSI/Unicode files and platforms.
 */
void FFileHelper::BufferToString( FString& Result, const uint8* Buffer, int32 Size )
{
	TArray<TCHAR>& ResultArray = Result.GetCharArray();
	ResultArray.Empty();

	if( Size >= 2 && !( Size & 1 ) && Buffer[0] == 0xff && Buffer[1] == 0xfe )
	{
		// Unicode Intel byte order. Less 1 for the FFFE header, additional 1 for null terminator.
		ResultArray.AddUninitialized( Size / 2 );
		for( int32 i = 0; i < ( Size / 2 ) - 1; i++ )
		{
			ResultArray[ i ] = CharCast<TCHAR>( (UCS2CHAR)(( uint16 )Buffer[i * 2 + 2] + ( uint16 )Buffer[i * 2 + 3] * 256) );
		}
	}
	else if( Size >= 2 && !( Size & 1 ) && Buffer[0] == 0xfe && Buffer[1] == 0xff )
	{
		// Unicode non-Intel byte order. Less 1 for the FFFE header, additional 1 for null terminator.
		ResultArray.AddUninitialized( Size / 2 );
		for( int32 i = 0; i < ( Size / 2 ) - 1; i++ )
		{
			ResultArray[ i ] = CharCast<TCHAR>( (UCS2CHAR)(( uint16 )Buffer[i * 2 + 3] + ( uint16 )Buffer[i * 2 + 2] * 256) );
		}
	}
	else
	{
		if ( Size >= 3 && Buffer[0] == 0xef && Buffer[1] == 0xbb && Buffer[2] == 0xbf )
		{
			// Skip over UTF-8 BOM if there is one
			Buffer += 3;
			Size   -= 3;
		}

		FUTF8ToTCHAR Conv((const ANSICHAR*)Buffer, Size);
		int32 Length = Conv.Length();
		ResultArray.AddUninitialized(Length + 1); // For the null terminator
		CopyAssignItems(ResultArray.GetData(), Conv.Get(), Length);
	}

	if (ResultArray.Num() == 1)
	{
		// If it's only a zero terminator then make the result actually empty
		ResultArray.Empty();
	}
	else
	{
		// Else ensure null terminator is present
		ResultArray.Last() = 0;
	}
}

/**
 * Load a text file to an FString.
 * Supports all combination of ANSI/Unicode files and platforms.
 * @param Result string representation of the loaded file
 * @param Filename name of the file to load
 * @param VerifyFlags flags controlling the hash verification behavior ( see EHashOptions )
 */
bool FFileHelper::LoadFileToString( FString& Result, const TCHAR* Filename, EHashOptions VerifyFlags )
{
	FScopedLoadingState ScopedLoadingState(Filename);

	TUniquePtr<FArchive> Reader( IFileManager::Get().CreateFileReader( Filename ) );
	if( !Reader )
	{
		return 0;
	}
	
	int32 Size = Reader->TotalSize();
	if( !Size )
	{
		Result.Empty();
		return true;
	}

	uint8* Ch = (uint8*)FMemory::Malloc(Size);
	Reader->Serialize( Ch, Size );
	bool Success = Reader->Close();
	Reader = nullptr;
	BufferToString( Result, Ch, Size );

	// handle SHA verify of the file
	if( EnumHasAnyFlags(VerifyFlags, EHashOptions::EnableVerify) && ( EnumHasAnyFlags(VerifyFlags, EHashOptions::ErrorMissingHash) || FSHA1::GetFileSHAHash(Filename, NULL) ) )
	{
		// kick off SHA verify task. this frees the buffer on close
		FBufferReaderWithSHA Ar( Ch, Size, true, Filename, false, true );
	}
	else
	{
		// free manually since not running SHA task
		FMemory::Free(Ch);
	}

	return Success;
}

bool FFileHelper::LoadFileToStringArray( TArray<FString>& Result, const TCHAR* Filename, EHashOptions VerifyFlags )
{
	Result.Empty();

	FString Buffer;
	if(!LoadFileToString(Buffer, Filename, VerifyFlags))
	{
		return false;
	}

	for(const TCHAR* Pos = *Buffer; *Pos != 0; )
	{
		const TCHAR* LineStart = Pos;
		while(*Pos != 0 && *Pos != '\r' && *Pos != '\n')
		{
			Pos++;
		}

		Result.Add(FString(Pos - LineStart, LineStart));

		if(*Pos == '\r')
		{
			Pos++;
		}
		if(*Pos == '\n')
		{
			Pos++;
		}
	}

	return true;
}

/**
 * Save a binary array to a file.
 */
bool FFileHelper::SaveArrayToFile(TArrayView<const uint8> Array, const TCHAR* Filename, IFileManager* FileManager /*= &IFileManager::Get()*/, uint32 WriteFlags)
{
	FArchive* Ar = FileManager->CreateFileWriter( Filename, WriteFlags );
	if( !Ar )
	{
		return 0;
	}
	Ar->Serialize(const_cast<uint8*>(Array.GetData()), Array.Num());
	delete Ar;
	return true;
}

/**
 * Write the FString to a file.
 * Supports all combination of ANSI/Unicode files and platforms.
 */
bool FFileHelper::SaveStringToFile( const FString& String, const TCHAR* Filename,  EEncodingOptions EncodingOptions, IFileManager* FileManager /*= &IFileManager::Get()*/, uint32 WriteFlags )
{
	// max size of the string is a UCS2CHAR for each character and some UNICODE magic 
	auto Ar = TUniquePtr<FArchive>( FileManager->CreateFileWriter( Filename, WriteFlags ) );
	if( !Ar )
		return false;

	if( String.IsEmpty() )
		return true;

	const TCHAR* StrPtr = *String;

	bool SaveAsUnicode = EncodingOptions == EEncodingOptions::ForceUnicode || ( EncodingOptions == EEncodingOptions::AutoDetect && !FCString::IsPureAnsi(StrPtr) );
	if( EncodingOptions == EEncodingOptions::ForceUTF8 )
	{
		UTF8CHAR UTF8BOM[] = { 0xEF, 0xBB, 0xBF };
		Ar->Serialize( &UTF8BOM, ARRAY_COUNT(UTF8BOM) * sizeof(UTF8CHAR) );

		FTCHARToUTF8 UTF8String(StrPtr);
		Ar->Serialize( (UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR) );
	}
	else if ( EncodingOptions == EEncodingOptions::ForceUTF8WithoutBOM )
	{
		FTCHARToUTF8 UTF8String(StrPtr);
		Ar->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR));
	}
	else if (SaveAsUnicode)
	{
		UCS2CHAR BOM = UNICODE_BOM;
		Ar->Serialize( &BOM, sizeof(UCS2CHAR) );

		auto Src = StringCast<UCS2CHAR>(StrPtr, String.Len());
		Ar->Serialize( (UCS2CHAR*)Src.Get(), Src.Length() * sizeof(UCS2CHAR) );
	}
	else
	{
		auto Src = StringCast<ANSICHAR>(StrPtr, String.Len());
		Ar->Serialize( (ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR) );
	}

	return true;
}

bool FFileHelper::SaveStringArrayToFile( const TArray<FString>& Lines, const TCHAR* Filename, EEncodingOptions EncodingOptions, IFileManager* FileManager, uint32 WriteFlags )
{
	int32 Length = 10;
	for(const FString& Line : Lines)
	{
		Length += Line.Len() + ARRAY_COUNT(LINE_TERMINATOR);
	}
	
	FString CombinedString;
	CombinedString.Reserve(Length);

	for(const FString& Line : Lines)
	{
		CombinedString += Line;
		CombinedString += LINE_TERMINATOR;
	}

	return SaveStringToFile(CombinedString, Filename, EncodingOptions, FileManager, WriteFlags);
}

/**
 * Generates the next unique bitmap filename with a specified extension
 * 
 * @param Pattern		Filename with path, but without extension.
 * @oaran Extension		File extension to be appended
 * @param OutFilename	Reference to an FString where the newly generated filename will be placed
 * @param FileManager	Reference to a IFileManager (or the global instance by default)
 *
 * @return true if success
 */
bool FFileHelper::GenerateNextBitmapFilename( const FString& Pattern, const FString& Extension, FString& OutFilename, IFileManager* FileManager /*= &IFileManager::Get()*/ )
{
	FString File;
	OutFilename = "";
	bool bSuccess = false;

	//
	// As an optimization for sequential screenshots using the same pattern, we track the last index used and check if that exists 
	// for the provided pattern. If it does we start checking from that index
	// 
	// If a file with the last used index does not exist we it's a different pattern so start at 0 to find the next free name.

	static int32 LastScreenShotIndex = 0;
	int32 SearchIndex = 0;

	File = FString::Printf(TEXT("%s%05i.%s"), *Pattern, LastScreenShotIndex, *Extension);

	if (FileManager->FileExists(*File))
	{
		SearchIndex = LastScreenShotIndex+1;
	}
	
	for( int32 TestBitmapIndex = SearchIndex; TestBitmapIndex < 100000; ++TestBitmapIndex )
	{
		File = FString::Printf(TEXT("%s%05i.%s"), *Pattern, TestBitmapIndex, *Extension);
		if( FileManager->FileExists(*File) == false)
		{
			LastScreenShotIndex = TestBitmapIndex;
			OutFilename = File;
			bSuccess = true;
			break;
		}
	}

	return bSuccess;
}

/**
 * Saves a 24Bit BMP file to disk
 * 
 * @param Pattern filename with path, must not be 0, if with "bmp" extension (e.g. "out.bmp") the filename stays like this, if without (e.g. "out") automatic index numbers are addended (e.g. "out00002.bmp")
 * @param DataWidth - Width of the bitmap supplied in Data >0
 * @param DataHeight - Height of the bitmap supplied in Data >0
 * @param Data must not be 0
 * @param SubRectangle optional, specifies a sub-rectangle of the source image to save out. If NULL, the whole bitmap is saved
 * @param FileManager must not be 0
 * @param OutFilename optional, if specified filename will be output
 *
 * @return true if success
 */
bool FFileHelper::CreateBitmap( const TCHAR* Pattern, int32 SourceWidth, int32 SourceHeight, const FColor* Data, struct FIntRect* SubRectangle, IFileManager* FileManager /*= &IFileManager::Get()*/, FString* OutFilename /*= NULL*/, bool bInWriteAlpha /*= false*/ )
{
#if ALLOW_DEBUG_FILES
	FIntRect Src(0, 0, SourceWidth, SourceHeight);
	if (SubRectangle == NULL || SubRectangle->Area() == 0)
	{
		SubRectangle = &Src;
	}

	FString File;
	// if the Pattern already has a .bmp extension, then use that the file to write to
	if (FPaths::GetExtension(Pattern) == TEXT("bmp"))
	{
		File = Pattern;
	}
	else
	{
		if (GenerateNextBitmapFilename(Pattern, TEXT("bmp"), File, FileManager))
		{
			if ( OutFilename )
			{
				*OutFilename = File;
			}
		}
		else
		{
			return false;
		}
	}

	FArchive* Ar = FileManager->CreateDebugFileWriter( *File );
	if( Ar )
	{
		// Types.
		#if PLATFORM_SUPPORTS_PRAGMA_PACK
			#pragma pack (push,1)
		#endif
		struct BITMAPFILEHEADER
		{
			uint16 bfType GCC_PACK(1);
			uint32 bfSize GCC_PACK(1);
			uint16 bfReserved1 GCC_PACK(1); 
			uint16 bfReserved2 GCC_PACK(1);
			uint32 bfOffBits GCC_PACK(1);
		} FH; 
		struct BITMAPINFOHEADER
		{
			uint32 biSize GCC_PACK(1); 
			int32  biWidth GCC_PACK(1);
			int32  biHeight GCC_PACK(1);
			uint16 biPlanes GCC_PACK(1);
			uint16 biBitCount GCC_PACK(1);
			uint32 biCompression GCC_PACK(1);
			uint32 biSizeImage GCC_PACK(1);
			int32  biXPelsPerMeter GCC_PACK(1); 
			int32  biYPelsPerMeter GCC_PACK(1);
			uint32 biClrUsed GCC_PACK(1);
			uint32 biClrImportant GCC_PACK(1); 
		} IH;
		struct BITMAPV4HEADER
		{
			uint32 bV4RedMask GCC_PACK(1);
			uint32 bV4GreenMask GCC_PACK(1);
			uint32 bV4BlueMask GCC_PACK(1);
			uint32 bV4AlphaMask GCC_PACK(1);
			uint32 bV4CSType GCC_PACK(1);
			uint32 bV4EndpointR[3] GCC_PACK(1);
			uint32 bV4EndpointG[3] GCC_PACK(1);
			uint32 bV4EndpointB[3] GCC_PACK(1);
			uint32 bV4GammaRed GCC_PACK(1);
			uint32 bV4GammaGreen GCC_PACK(1);
			uint32 bV4GammaBlue GCC_PACK(1);
		} IHV4;
		#if PLATFORM_SUPPORTS_PRAGMA_PACK
			#pragma pack (pop)
		#endif

		int32 Width = SubRectangle->Width();
		int32 Height = SubRectangle->Height();
		uint32 BytesPerPixel = bInWriteAlpha ? 4 : 3;
		uint32 BytesPerLine = Align(Width * BytesPerPixel, 4);

		uint32 InfoHeaderSize = sizeof(BITMAPINFOHEADER) + (bInWriteAlpha ? sizeof(BITMAPV4HEADER) : 0);

		// File header.
		FH.bfType       		= INTEL_ORDER16((uint16) ('B' + 256*'M'));
		FH.bfSize       		= INTEL_ORDER32((uint32) (sizeof(BITMAPFILEHEADER) + InfoHeaderSize + BytesPerLine * Height));
		FH.bfReserved1  		= INTEL_ORDER16((uint16) 0);
		FH.bfReserved2  		= INTEL_ORDER16((uint16) 0);
		FH.bfOffBits    		= INTEL_ORDER32((uint32) (sizeof(BITMAPFILEHEADER) + InfoHeaderSize));
		Ar->Serialize( &FH, sizeof(FH) );

		// Info header.
		IH.biSize               = INTEL_ORDER32((uint32) InfoHeaderSize);
		IH.biWidth              = INTEL_ORDER32((uint32) Width);
		IH.biHeight             = INTEL_ORDER32((uint32) Height);
		IH.biPlanes             = INTEL_ORDER16((uint16) 1);
		IH.biBitCount           = INTEL_ORDER16((uint16) BytesPerPixel * 8);
		if(bInWriteAlpha)
		{
			IH.biCompression    = INTEL_ORDER32((uint32) 3); //BI_BITFIELDS
		}
		else
		{
			IH.biCompression    = INTEL_ORDER32((uint32) 0); //BI_RGB
		}
		IH.biSizeImage          = INTEL_ORDER32((uint32) BytesPerLine * Height);
		IH.biXPelsPerMeter      = INTEL_ORDER32((uint32) 0);
		IH.biYPelsPerMeter      = INTEL_ORDER32((uint32) 0);
		IH.biClrUsed            = INTEL_ORDER32((uint32) 0);
		IH.biClrImportant       = INTEL_ORDER32((uint32) 0);
		Ar->Serialize( &IH, sizeof(IH) );

		// If we're writing alpha, we need to write the extra portion of the V4 header
		if (bInWriteAlpha)
		{
			IHV4.bV4RedMask     = INTEL_ORDER32((uint32) 0x00ff0000);
			IHV4.bV4GreenMask   = INTEL_ORDER32((uint32) 0x0000ff00);
			IHV4.bV4BlueMask    = INTEL_ORDER32((uint32) 0x000000ff);
			IHV4.bV4AlphaMask   = INTEL_ORDER32((uint32) 0xff000000);
			IHV4.bV4CSType      = INTEL_ORDER32((uint32) 'Win ');
			IHV4.bV4GammaRed    = INTEL_ORDER32((uint32) 0);
			IHV4.bV4GammaGreen  = INTEL_ORDER32((uint32) 0);
			IHV4.bV4GammaBlue   = INTEL_ORDER32((uint32) 0);
			Ar->Serialize( &IHV4, sizeof(IHV4) );
		}

		// Colors.
		for( int32 i = SubRectangle->Max.Y - 1; i >= SubRectangle->Min.Y; i-- )
		{
			for( int32 j = SubRectangle->Min.X; j < SubRectangle->Max.X; j++ )
			{
				Ar->Serialize( (void *)&Data[i*SourceWidth+j].B, 1 );
				Ar->Serialize( (void *)&Data[i*SourceWidth+j].G, 1 );
				Ar->Serialize( (void *)&Data[i*SourceWidth+j].R, 1 );

				if (bInWriteAlpha)
				{
					Ar->Serialize( (void *)&Data[i * SourceWidth + j].A, 1 );
				}
			}

			// Pad each row's length to be a multiple of 4 bytes.

			for(uint32 PadIndex = Width * BytesPerPixel; PadIndex < BytesPerLine; PadIndex++)
			{
				uint8 B = 0;
				Ar->Serialize(&B, 1);
			}
		}

		// Success.
		delete Ar;
		if (!GIsEditor)
		{
			SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), File );
		}
	}
	else 
	{
		return 0;
	}
#endif
	// Success.
	return 1;
}

/**
 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
 *	Intended for use in simple text parsing actions
 *
 *	@param	InFilename			The text file to read, full path
 *	@param	InFileManager		The filemanager to use - NULL will use &IFileManager::Get()
 *	@param	OutStrings			The array of FStrings to fill in
 *
 *	@return	bool				true if successful, false if not
 */
bool FFileHelper::LoadANSITextFileToStrings(const TCHAR* InFilename, IFileManager* InFileManager, TArray<FString>& OutStrings)
{
	FScopedLoadingState ScopedLoadingState(InFilename);

	IFileManager* FileManager = (InFileManager != NULL) ? InFileManager : &IFileManager::Get();
	// Read and parse the file, adding the pawns and their sounds to the list
	FArchive* TextFile = FileManager->CreateFileReader(InFilename, 0);
	if (TextFile != NULL)
	{
		// get the size of the file
		int32 Size = TextFile->TotalSize();
		// read the file
		TArray<uint8> Buffer;
		Buffer.Empty(Size);
		Buffer.AddUninitialized(Size);
		TextFile->Serialize(Buffer.GetData(), Size);
		// zero terminate it
		Buffer.Add(0);
		// Release the file
		delete TextFile;

		// Now read it
		// init traveling pointer
		ANSICHAR* Ptr = (ANSICHAR*)Buffer.GetData();

		// iterate over the lines until complete
		bool bIsDone = false;
		while (!bIsDone)
		{
			// Store the location of the first character of this line
			ANSICHAR* Start = Ptr;

			// Advance the char pointer until we hit a newline character
			while (*Ptr && *Ptr!='\r' && *Ptr!='\n')
			{
				Ptr++;
			}

			// If this is the end of the file, we're done
			if (*Ptr == 0)
			{
				bIsDone = 1;
			}
			// Handle different line endings. If \r\n then NULL and advance 2, otherwise NULL and advance 1
			// This handles \r, \n, or \r\n
			else if ( *Ptr=='\r' && *(Ptr+1)=='\n' )
			{
				// This was \r\n. Terminate the current line, and advance the pointer forward 2 characters in the stream
				*Ptr++ = 0;
				*Ptr++ = 0;
			}
			else
			{
				// Terminate the current line, and advance the pointer to the next character in the stream
				*Ptr++ = 0;
			}

			FString CurrLine = ANSI_TO_TCHAR(Start);
			OutStrings.Add(CurrLine);
		}

		return true;
	}
	else
	{
		UE_LOG(LogStreaming, Warning, TEXT("Failed to open ANSI TEXT file %s"), InFilename);
		return false;
	}
}

/**
* Checks to see if a filename is valid for saving.
* A filename must be under MAX_UNREAL_FILENAME_LENGTH to be saved
*
* @param Filename	Filename, with or without path information, to check.
* @param OutError	If an error occurs, this is the reason why
*/
bool FFileHelper::IsFilenameValidForSaving(const FString& Filename, FText& OutError)
{
	bool bFilenameIsValid = false;

	// Get the clean filename (filename with extension but without path )
	const FString BaseFilename = FPaths::GetBaseFilename(Filename);

	// Check length of the filename
	if (BaseFilename.Len() > 0)
	{
		if (BaseFilename.Len() <= MAX_UNREAL_FILENAME_LENGTH)
		{
			bFilenameIsValid = true;

			/*
			// Check that the name isn't the name of a UClass
			for ( TObjectIterator<UClass> It; It; ++It )
			{
			UClass* Class = *It;
			if ( Class->GetName() == BaseFilename )
			{
			bFilenameIsValid = false;
			break;
			}
			}
			*/

			for (const FString& InvalidFilename : InvalidFilenames)
			{
				if (BaseFilename.Equals(InvalidFilename, ESearchCase::IgnoreCase))
				{
					OutError = NSLOCTEXT("UnrealEd", "Error_InvalidFilename", "A file/folder may not match any of the following : \nCON, PRN, AUX, CLOCK$, NUL, NONE, \nCOM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9, \nLPT1, LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, or LPT9.");
					return false;
				}
			}

			if (FName(*BaseFilename).IsNone())
			{
				OutError = FText::Format(NSLOCTEXT("UnrealEd", "Error_NoneFilename", "Filename '{0}' resolves to 'None' and cannot be used"), FText::FromString(BaseFilename));
				return false;
			}

			// Check for invalid characters in the filename
			if (bFilenameIsValid &&
				(BaseFilename.Contains(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
					BaseFilename.Contains(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd)))
			{
				bFilenameIsValid = false;
			}

			if (!bFilenameIsValid)
			{
				OutError = FText::Format(NSLOCTEXT("UnrealEd", "Error_FilenameDisallowed", "Filename '{0}' is disallowed."), FText::FromString(BaseFilename));
			}
		}
		else
		{
			OutError = FText::Format(NSLOCTEXT("UnrealEd", "Error_FilenameIsTooLongForCooking", "Filename '{0}' is too long; this may interfere with cooking for consoles.  Unreal filenames should be no longer than {1} characters."),
				FText::FromString(BaseFilename), FText::AsNumber(MAX_UNREAL_FILENAME_LENGTH));
		}
	}
	else
	{
		OutError = LOCTEXT("Error_FilenameIsTooShort", "Please provide a filename for the asset.");
	}

	return bFilenameIsValid;
}

/*-----------------------------------------------------------------------------
	FMaintenance
-----------------------------------------------------------------------------*/
void FMaintenance::DeleteOldLogs()
{
	int32 PurgeLogsDays = -1; // -1 means don't delete old files
	int32 MaxLogFilesOnDisk = -1; // -1 means keep all files

	GConfig->GetInt(TEXT("LogFiles"), TEXT("PurgeLogsDays"), PurgeLogsDays, GEngineIni);
	GConfig->GetInt(TEXT("LogFiles"), TEXT("MaxLogFilesOnDisk"), MaxLogFilesOnDisk, GEngineIni);

	if (PurgeLogsDays >= 0 || MaxLogFilesOnDisk >= 0)
	{
		// get list of files in the log directory (grouped by log name)
		TMap<FString, TArray<FString>> LogToPaths;
		{
			TArray<FString> Files;
			IFileManager::Get().FindFiles(Files, *FString::Printf(TEXT("%s*.*"), *FPaths::ProjectLogDir()), true, false);

			for (FString& Filename : Files)
			{
				const int32 BackupPostfixIndex = Filename.Find(BACKUP_LOG_FILENAME_POSTFIX);

				if (BackupPostfixIndex >= 0)
				{
					const FString LogName = Filename.Left(BackupPostfixIndex);
					TArray<FString>& FilePaths = LogToPaths.FindOrAdd(LogName);
					FilePaths.Add(FPaths::ProjectLogDir() / Filename);
				}
			}
		}

		// delete old log files in each group
		double MaxFileAgeSeconds = 60.0 * 60.0 * 24.0 * double(PurgeLogsDays);

		struct FSortByDateNewestFirst
		{
			bool operator()(const FString& A, const FString& B) const
			{
				const FDateTime TimestampA = IFileManager::Get().GetTimeStamp(*A);
				const FDateTime TimestampB = IFileManager::Get().GetTimeStamp(*B);
				return TimestampB < TimestampA;
			}
		};

		for (TPair<FString, TArray<FString>>& Pair : LogToPaths)
		{
			TArray<FString>& FilePaths = Pair.Value;

			// sort the file paths by date
			FilePaths.Sort(FSortByDateNewestFirst());

			// delete files that are older than the desired number of days
			for (int32 PathIndex = FilePaths.Num() - 1; PathIndex >= 0; --PathIndex)
			{
				const FString& FilePath = FilePaths[PathIndex];

				if (IFileManager::Get().GetFileAgeSeconds(*FilePath) > MaxFileAgeSeconds)
				{
					UE_LOG(LogStreaming, Log, TEXT("Deleting old log file %s"), *FilePath);
					IFileManager::Get().Delete(*FilePath);
					FilePaths.RemoveAt(PathIndex);
				}
			}

			// trim the number of files on disk if desired
			if (MaxLogFilesOnDisk >= 0 && FilePaths.Num() > MaxLogFilesOnDisk)
			{
				for (int32 PathIndex = FilePaths.Num() - 1; PathIndex >= 0 && FilePaths.Num() > MaxLogFilesOnDisk; --PathIndex)
				{
					if (FOutputDeviceFile::IsBackupCopy(*FilePaths[PathIndex]))
					{
						IFileManager::Get().Delete(*FilePaths[PathIndex]);
						FilePaths.RemoveAt(PathIndex);
					}
				}
			}
		}
	}

	// Remove all legacy UE4 crash contexts (regardless of age and purge settings, these are deprecated)
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *FString::Printf(TEXT("%s/UE4CC*"), *FPaths::ProjectLogDir()), false, true);

	for (const FString& Dir : Directories)
	{
		const FString CrashConfigDirectory = FPaths::ProjectLogDir() / Dir;
		IFileManager::Get().DeleteDirectory(*CrashConfigDirectory, false, true);
	}
}

#undef LOCTEXT_NAMESPACE
