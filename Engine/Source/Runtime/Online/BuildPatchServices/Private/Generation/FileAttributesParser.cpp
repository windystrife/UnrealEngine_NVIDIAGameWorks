// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "Generation/FileAttributesParser.h"
#include "Templates/ScopedPointer.h"
#include "Misc/FileHelper.h"
#include "UniquePtr.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFileAttributesParser, Log, All);
DEFINE_LOG_CATEGORY(LogFileAttributesParser);

namespace BuildPatchServices
{
	FFileAttributes::FFileAttributes()
		: bReadOnly(false)
		, bCompressed(false)
		, bUnixExecutable(false)
	{}

	class FFileAttributesParserImpl
		: public FFileAttributesParser
	{
		typedef void(*SetKeyValueAttributeFunction)(FFileAttributes&, FString);

	public:
		FFileAttributesParserImpl(IPlatformFile& PlatformFile);
		virtual ~FFileAttributesParserImpl();

		virtual bool ParseFileAttributes(const FString& MetaFilename, TMap<FString, FFileAttributes>& FileAttributes) override;

	private:
		bool FileAttributesMetaToMap(const FString& AttributesList, TMap<FString, FFileAttributes>& FileAttributesMap);

	private:
		IPlatformFile& PlatformFile;
		TMap<FString, SetKeyValueAttributeFunction> AttributeSetters;
	};

	FFileAttributesParserImpl::FFileAttributesParserImpl(IPlatformFile& InPlatformFile)
		: PlatformFile(InPlatformFile)
	{
		AttributeSetters.Add(TEXT("readonly"),   [](FFileAttributes& Attributes, FString Value){ Attributes.bReadOnly = true; });
		AttributeSetters.Add(TEXT("compressed"), [](FFileAttributes& Attributes, FString Value){ Attributes.bCompressed = true; });
		AttributeSetters.Add(TEXT("executable"), [](FFileAttributes& Attributes, FString Value){ Attributes.bUnixExecutable = true; });
		AttributeSetters.Add(TEXT("tag"),        [](FFileAttributes& Attributes, FString Value){ Attributes.InstallTags.Add(MoveTemp(Value)); });
	}

	FFileAttributesParserImpl::~FFileAttributesParserImpl()
	{
	}

	bool FFileAttributesParserImpl::ParseFileAttributes(const FString& MetaFilename, TMap<FString, FFileAttributes>& FileAttributes)
	{
		TUniquePtr<IFileHandle> Handle(PlatformFile.OpenRead(*MetaFilename));
		if (Handle)
		{
			TArray<uint8> FileData;
			FileData.AddUninitialized(Handle->Size());
			if (Handle->Read(FileData.GetData(), FileData.Num()))
			{
				FString FileDataString;
				FFileHelper::BufferToString(FileDataString, FileData.GetData(), FileData.Num());
				return FileAttributesMetaToMap(FileDataString, FileAttributes);
			}
			else
			{
				UE_LOG(LogFileAttributesParser, Error, TEXT("Could not read meta file %s"), *MetaFilename);
			}
		}
		else
		{
			UE_LOG(LogFileAttributesParser, Error, TEXT("Could not open meta file %s"), *MetaFilename);
		}

		return false;
	}

	bool FFileAttributesParserImpl::FileAttributesMetaToMap(const FString& AttributesList, TMap<FString, FFileAttributes>& FileAttributesMap)
	{
		const TCHAR Quote = TEXT('\"');
		const TCHAR EOFile = TEXT('\0');
		const TCHAR EOLine = TEXT('\n');

		bool Successful = true;
		bool FoundFilename = false;

		const TCHAR* CharPtr = *AttributesList;
		while (*CharPtr != EOFile)
		{
			// Parse filename
			while (*CharPtr != Quote && *CharPtr != EOFile){ ++CharPtr; }
			if (*CharPtr == EOFile)
			{
				if (!FoundFilename)
				{
					UE_LOG(LogFileAttributesParser, Error, TEXT("Did not find opening quote for filename!"));
					return false;
				}
				break;
			}
			const TCHAR* FilenameStart = ++CharPtr;
			while (*CharPtr != Quote && *CharPtr != EOFile && *CharPtr != EOLine){ ++CharPtr; }
			// Check we didn't run out of file
			if (*CharPtr == EOFile)
			{
				UE_LOG(LogFileAttributesParser, Error, TEXT("Unexpected end of file before next quote! Pos:%d"), CharPtr - *AttributesList);
				return false;
			}
			// Check we didn't run out of line
			if(*CharPtr == EOLine)
			{
				UE_LOG(LogFileAttributesParser, Error, TEXT("Unexpected end of line before next quote! Pos:%d"), CharPtr - *AttributesList);
				return false;
			}
			// Save positions
			const TCHAR* FilenameEnd = CharPtr++;
			const TCHAR* AttributesStart = CharPtr;
			// Parse keywords
			while (*CharPtr != Quote && *CharPtr != EOFile && *CharPtr != EOLine){ ++CharPtr; }
			// Check we hit the end of the line or file, another quote it wrong
			if (*CharPtr == Quote)
			{
				UE_LOG(LogFileAttributesParser, Error, TEXT("Unexpected Quote before end of keywords! Pos:%d"), CharPtr - *AttributesList);
				return false;
			}
			FoundFilename = true;
			// Save position
			const TCHAR* EndOfLine = CharPtr;
			// Grab info
			FString Filename = FString(FilenameEnd - FilenameStart, FilenameStart).Replace(TEXT("\\"), TEXT("/"));
			FFileAttributes& FileAttributes = FileAttributesMap.FindOrAdd(Filename);
			TArray<FString> AttributeParamsArray;
			FString AttributeParams(EndOfLine - AttributesStart, AttributesStart);
			AttributeParams.ParseIntoArrayWS(AttributeParamsArray);
			for (const FString& AttributeParam : AttributeParamsArray)
			{
				FString Key, Value;
				if(!AttributeParam.Split(TEXT(":"), &Key, &Value))
				{
					Key = AttributeParam;
				}
				if (AttributeSetters.Contains(Key))
				{
					AttributeSetters[Key](FileAttributes, MoveTemp(Value));
				}
				else
				{
					UE_LOG(LogFileAttributesParser, Error, TEXT("Unrecognised attribute %s for %s"), *AttributeParam, *Filename);
					Successful = false;
				}
			}
		}

		return Successful;
	}

	FFileAttributesParserRef FFileAttributesParserFactory::Create(IPlatformFile& PlatformFile)
	{
		return MakeShareable(new FFileAttributesParserImpl(PlatformFile));
	}
}
