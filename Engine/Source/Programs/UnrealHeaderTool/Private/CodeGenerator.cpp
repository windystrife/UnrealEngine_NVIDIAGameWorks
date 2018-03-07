// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "CoreMinimal.h"
#include "Misc/AssertionMacros.h"
#include "HAL/PlatformProcess.h"
#include "Templates/UnrealTemplate.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Delegates/Delegate.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "UObject/ErrorException.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/Interface.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "Misc/PackageName.h"
#include "UnrealHeaderToolGlobals.h"

#include "ParserClass.h"
#include "Scope.h"
#include "HeaderProvider.h"
#include "GeneratedCodeVersion.h"
#include "SimplifiedParsingClassInfo.h"
#include "UnrealSourceFile.h"
#include "ParserHelper.h"
#include "Classes.h"
#include "NativeClassExporter.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "HeaderParser.h"
#include "IScriptGeneratorPluginInterface.h"
#include "Manifest.h"
#include "StringUtils.h"
#include "Features/IModularFeatures.h"
#include "Algo/Sort.h"
#include "Algo/Reverse.h"
#include "Misc/ScopeExit.h"
#include "UnrealTypeDefinitionInfo.h"

#include "FileLineException.h"

/////////////////////////////////////////////////////
// Globals

FManifest GManifest;

double GMacroizeTime = 0.0;

static TArray<FString> ChangeMessages;
static bool bWriteContents = false;
static bool bVerifyContents = false;

static TSharedRef<FUnrealSourceFile> PerformInitialParseOnHeader(UPackage* InParent, const TCHAR* FileName, EObjectFlags Flags, const TCHAR* Buffer);

FCompilerMetadataManager GScriptHelper;

/** C++ name lookup helper */
FNameLookupCPP NameLookupCPP;

namespace
{
	static FString AsTEXT(FString InStr)
	{
		return FString::Printf(TEXT("TEXT(\"%s\")"), *InStr);
	}

	const TCHAR HeaderCopyright[] =
		TEXT("// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.\r\n")
		TEXT("/*===========================================================================\r\n")
		TEXT("\tGenerated code exported from UnrealHeaderTool.\r\n")
		TEXT("\tDO NOT modify this manually! Edit the corresponding .h files instead!\r\n")
		TEXT("===========================================================================*/\r\n")
		LINE_TERMINATOR;

	const TCHAR RequiredCPPIncludes[] = TEXT("#include \"GeneratedCppIncludes.h\"") LINE_TERMINATOR;

	// A struct which emits #if and #endif blocks as appropriate when invoked.
	struct FMacroBlockEmitter
	{
		explicit FMacroBlockEmitter(FOutputDevice& InOutput, const TCHAR* InMacro)
			: Output(InOutput)
			, bEmittedIf(false)
			, Macro(InMacro)
		{
		}

		~FMacroBlockEmitter()
		{
			if (bEmittedIf)
			{
				Output.Logf(TEXT("#endif // %s\r\n"), Macro);
			}
		}

		void operator()(bool bInBlock)
		{
			if (!bEmittedIf && bInBlock)
			{
				Output.Logf(TEXT("#if %s\r\n"), Macro);
				bEmittedIf = true;
			}
			else if (bEmittedIf && !bInBlock)
			{
				Output.Logf(TEXT("#endif // %s\r\n"), Macro);
				bEmittedIf = false;
			}
		}

		FMacroBlockEmitter(const FMacroBlockEmitter&) = delete;
		FMacroBlockEmitter& operator=(const FMacroBlockEmitter&) = delete;

	private:
		FOutputDevice& Output;
		bool bEmittedIf;
		const TCHAR* Macro;
	};

	/** Guard that should be put at the start editor only generated code */
	const TCHAR BeginEditorOnlyGuard[] = TEXT("#if WITH_EDITOR") LINE_TERMINATOR;

	/** Guard that should be put at the end of editor only generated code */
	const TCHAR EndEditorOnlyGuard[] = TEXT("#endif //WITH_EDITOR") LINE_TERMINATOR;
}

#define BEGIN_WRAP_EDITOR_ONLY(DoWrap) DoWrap ? BeginEditorOnlyGuard : TEXT("")
#define END_WRAP_EDITOR_ONLY(DoWrap) DoWrap ? EndEditorOnlyGuard : TEXT("")

/**
 * Finds exact match of Identifier in string. Returns nullptr if none is found.
 *
 * @param StringBegin Start of string to search.
 * @param StringEnd End of string to search.
 * @param Identifier Identifier to find.
 * @return Pointer to Identifier match within string. nullptr if none found.
 */
const TCHAR* FindIdentifierExactMatch(const TCHAR* StringBegin, const TCHAR* StringEnd, const FString& Identifier)
{
	int32 StringLen = StringEnd - StringBegin;

	// Check for exact match first.
	if (FCString::Strncmp(StringBegin, *Identifier, StringLen) == 0)
	{
		return StringBegin;
	}

	int32        FindLen        = Identifier.Len();
	const TCHAR* StringToSearch = StringBegin;

	for (;;)
	{
		const TCHAR* IdentifierStart = FCString::Strstr(StringToSearch, *Identifier);
		if (IdentifierStart == nullptr)
		{
			// Not found.
			return nullptr;
		}

		if (IdentifierStart > StringEnd || IdentifierStart + FindLen + 1 > StringEnd)
		{
			// Found match is out of string range.
			return nullptr;
		}

		if (IdentifierStart == StringBegin && !FChar::IsIdentifier(*(IdentifierStart + FindLen + 1)))
		{
			// Found match is at the beginning of string.
			return IdentifierStart;
		}

		if (IdentifierStart + FindLen == StringEnd && !FChar::IsIdentifier(*(IdentifierStart - 1)))
		{
			// Found match ends with end of string.
			return IdentifierStart;
		}

		if (!FChar::IsIdentifier(*(IdentifierStart + FindLen)) && !FChar::IsIdentifier(*(IdentifierStart - 1)))
		{
			// Found match is in the middle of string
			return IdentifierStart;
		}

		// Didn't find exact match, nor got to end of search string. Keep on searching.
		StringToSearch = IdentifierStart + FindLen;
	}

	// We should never get here.
	checkNoEntry();
	return nullptr;
}

/**
 * Finds exact match of Identifier in string. Returns nullptr if none is found.
 *
 * @param String String to search.
 * @param Identifier Identifier to find.
 * @return Index to Identifier match within String. INDEX_NONE if none found.
 */
int32 FindIdentifierExactMatch(const FString& String, const FString& Identifier)
{
	const TCHAR* IdentifierPtr = FindIdentifierExactMatch(*String, *String + String.Len(), Identifier);
	if (IdentifierPtr == nullptr)
	{
		return INDEX_NONE;
	}

	return IdentifierPtr - *String;
}

/**
* Checks if exact match of Identifier is in String.
*
* @param StringBegin Start of string to search.
* @param StringEnd End of string to search.
* @param Identifier Identifier to find.
* @return true if Identifier is within string, false otherwise.
*/
bool HasIdentifierExactMatch(const TCHAR* StringBegin, const TCHAR* StringEnd, const FString& Find)
{
	return FindIdentifierExactMatch(StringBegin, StringEnd, Find) != nullptr;
}

/**
* Checks if exact match of Identifier is in String.
*
* @param String String to search.
* @param Identifier Identifier to find.
* @return true if Identifier is within String, false otherwise.
*/
bool HasIdentifierExactMatch(const FString &String, const FString& Identifier)
{
	return FindIdentifierExactMatch(String, Identifier) != INDEX_NONE;
}

void ConvertToBuildIncludePath(const UPackage* Package, FString& LocalPath)
{
	FPaths::MakePathRelativeTo(LocalPath, *GPackageToManifestModuleMap.FindChecked(Package)->IncludeBase);
}

/**
 *	Helper function for finding the location of a package
 *	This is required as source now lives in several possible directories
 *
 *	@param	InPackage		The name of the package of interest
 *	@param	OutLocation		The location of the given package, if found
 *  @param	OutHeaderLocation	The directory where generated headers should be placed
 *
 *	@return	bool			true if found, false if not
 */
bool FindPackageLocation(const TCHAR* InPackage, FString& OutLocation, FString& OutHeaderLocation)
{
	// Mapping of processed packages to their locations
	// An empty location string means it was processed but not found
	static TMap<FString, FManifestModule*> CheckedPackageList;

	FString CheckPackage(InPackage);

	FManifestModule* ModuleInfoPtr = CheckedPackageList.FindRef(CheckPackage);

	if (!ModuleInfoPtr)
	{
		FManifestModule* ModuleInfoPtr2 = GManifest.Modules.FindByPredicate([&](FManifestModule& Module) { return Module.Name == CheckPackage; });
		if (ModuleInfoPtr2 && IFileManager::Get().DirectoryExists(*ModuleInfoPtr2->BaseDirectory))
		{
			ModuleInfoPtr = ModuleInfoPtr2;
			CheckedPackageList.Add(CheckPackage, ModuleInfoPtr);
		}
	}

	if (!ModuleInfoPtr)
	{
		return false;
	}

	OutLocation       = ModuleInfoPtr->BaseDirectory;
	OutHeaderLocation = ModuleInfoPtr->GeneratedIncludeDirectory;
	return true;
}


FString Macroize(const TCHAR* MacroName, const TCHAR* StringToMacroize)
{
	FScopedDurationTimer Tracker(GMacroizeTime);

	FString Result = StringToMacroize;
	if (Result.Len())
	{
		Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("\n"), TEXT(" \\\n"), ESearchCase::CaseSensitive);
		checkSlow(Result.EndsWith(TEXT(" \\\n"), ESearchCase::CaseSensitive));

		if (Result.Len() >= 3)
		{
			for (int32 Index = Result.Len() - 3; Index < Result.Len(); ++Index)
			{
				Result[Index] = TEXT('\n');
			}
		}
		else
		{
			Result = TEXT("\n\n\n");
		}
		Result.ReplaceInline(TEXT("\n"), TEXT("\r\n"), ESearchCase::CaseSensitive);
	}
	return FString::Printf(TEXT("#define %s%s\r\n"), MacroName, Result.Len() ? TEXT(" \\") : TEXT("")) + Result;
}

/** Generates a CRC tag string for the specified field */
static FString GetGeneratedCodeCRCTag(UField* Field)
{
	FString Tag;
	const uint32* FieldCrc = GGeneratedCodeCRCs.Find(Field);
	if (FieldCrc)
	{
		Tag = FString::Printf(TEXT(" // %u"), *FieldCrc);
	}
	return Tag;
}

struct FParmsAndReturnProperties
{
	FParmsAndReturnProperties()
		: Return(NULL)
	{
	}

	#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

		FParmsAndReturnProperties(FParmsAndReturnProperties&&) = default;
		FParmsAndReturnProperties(const FParmsAndReturnProperties&) = default;
		FParmsAndReturnProperties& operator=(FParmsAndReturnProperties&&) = default;
		FParmsAndReturnProperties& operator=(const FParmsAndReturnProperties&) = default;

	#else

		FParmsAndReturnProperties(      FParmsAndReturnProperties&& Other) : Parms(MoveTemp(Other.Parms)), Return(MoveTemp(Other.Return)) {}
		FParmsAndReturnProperties(const FParmsAndReturnProperties&  Other) : Parms(         Other.Parms ), Return(         Other.Return ) {}
		FParmsAndReturnProperties& operator=(      FParmsAndReturnProperties&& Other) { Parms = MoveTemp(Other.Parms); Return = MoveTemp(Other.Return); return *this; }
		FParmsAndReturnProperties& operator=(const FParmsAndReturnProperties&  Other) { Parms =          Other.Parms ; Return =          Other.Return ; return *this; }

	#endif

	bool HasParms() const
	{
		return Parms.Num() || Return;
	}

	TArray<UProperty*> Parms;
	UProperty*         Return;
};

/**
 * Get parameters and return type for a given function.
 *
 * @param  Function The function to get the parameters for.
 * @return An aggregate containing the parameters and return type of that function.
 */
FParmsAndReturnProperties GetFunctionParmsAndReturn(UFunction* Function)
{
	FParmsAndReturnProperties Result;
	for ( TFieldIterator<UProperty> It(Function); It; ++It)
	{
		UProperty* Field = *It;

		if ((It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm)
		{
			Result.Parms.Add(Field);
		}
		else if (It->PropertyFlags & CPF_ReturnParm)
		{
			Result.Return = Field;
		}
	}
	return Result;
}

/**
 * Determines whether the glue version of the specified native function
 * should be exported
 *
 * @param	Function	the function to check
 * @return	true if the glue version of the function should be exported.
 */
bool ShouldExportUFunction(UFunction* Function)
{
	// export any script stubs for native functions declared in interface classes
	bool bIsBlueprintNativeEvent = (Function->FunctionFlags & FUNC_BlueprintEvent) && (Function->FunctionFlags & FUNC_Native);
	if (Function->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface) && !bIsBlueprintNativeEvent)
	{
		return true;
	}

	// always export if the function is static
	if (Function->FunctionFlags & FUNC_Static)
	{
		return true;
	}

	// don't export the function if this is not the original declaration and there is
	// at least one parent version of the function that is declared native
	for (UFunction* ParentFunction = Function->GetSuperFunction(); ParentFunction; ParentFunction = ParentFunction->GetSuperFunction())
	{
		if (ParentFunction->FunctionFlags & FUNC_Native)
		{
			return false;
		}
	}

	return true;
}

FString CreateLiteralString(const FString& Str)
{
	FString Result;

	// Have a reasonable guess at reserving the right size
	Result.Reserve(Str.Len() + 8);
	Result += TEXT("TEXT(\"");

	bool bPreviousCharacterWasHex = false;

	const TCHAR* Ptr = *Str;
	while (TCHAR Ch = *Ptr++)
	{
		switch (Ch)
		{
			case TEXT('\r'): continue;
			case TEXT('\n'): Result += TEXT("\\n");  bPreviousCharacterWasHex = false; break;
			case TEXT('\\'): Result += TEXT("\\\\"); bPreviousCharacterWasHex = false; break;
			case TEXT('\"'): Result += TEXT("\\\""); bPreviousCharacterWasHex = false; break;
			default:
				if (Ch < 31 || Ch >= 128)
				{
					Result += FString::Printf(TEXT("\\x%04x"), Ch);
					bPreviousCharacterWasHex = true;
				}
				else
				{
					// We close and open the literal (with TEXT) here in order to ensure that successive hex characters aren't appended to the hex sequence, causing a different number
					if (bPreviousCharacterWasHex && FCharWide::IsHexDigit(Ch))
					{
						Result += "\")TEXT(\"";
					}
					bPreviousCharacterWasHex = false;
					Result += Ch;
				}
				break;
		}
	}

	Result += TEXT("\")");
	return Result;
}

FString CreateUTF8LiteralString(const FString& Str)
{
	FString Result;

	// Have a reasonable guess at reserving the right size
	Result.Reserve(Str.Len() + 2);
	Result += TEXT("\"");

	bool bPreviousCharacterWasHex = false;

	FTCHARToUTF8 StrUTF8(*Str);

	const char* Ptr = StrUTF8.Get();
	while (char Ch = *Ptr++)
	{
		switch (Ch)
		{
			case '\r': continue;
			case '\n': Result += TEXT("\\n");  bPreviousCharacterWasHex = false; break;
			case '\\': Result += TEXT("\\\\"); bPreviousCharacterWasHex = false; break;
			case '\"': Result += TEXT("\\\""); bPreviousCharacterWasHex = false; break;
			default:
				if (Ch < 31 || Ch >= 128)
				{
					Result += FString::Printf(TEXT("\\x%02x"), (uint8)Ch);
					bPreviousCharacterWasHex = true;
				}
				else
				{
					// We close and open the literal here in order to ensure that successive hex characters aren't appended to the hex sequence, causing a different number
					if (bPreviousCharacterWasHex && FCharWide::IsHexDigit(Ch))
					{
						Result += "\"\"";
					}
					bPreviousCharacterWasHex = false;
					Result += Ch;
				}
				break;
		}
	}

	Result += TEXT("\"");
	return Result;
}

// Returns the METADATA_PARAMS for this output
static FString OutputMetaDataCodeForObject(FOutputDevice& Out, const UObject* Object, const TCHAR* MetaDataBlockName, const TCHAR* Spaces)
{
	TMap<FName, FString>* MetaData = UMetaData::GetMapForObject(Object);

	FUHTStringBuilder MetaDataOutput;
	if (MetaData && MetaData->Num())
	{
		typedef TKeyValuePair<FName, FString> KVPType;
		TArray<KVPType> KVPs;
		for (TPair<FName, FString>& KVP : *MetaData)
		{
			KVPs.Add(KVPType(KVP.Key, KVP.Value));
		}

		// We sort the metadata here so that we can get consistent output across multiple runs
		// even when metadata is added in a different order
		Algo::SortBy(KVPs, &KVPType::Key);

		for (const KVPType& KVP : KVPs)
		{
			MetaDataOutput.Logf(TEXT("%s\t{ %s, %s },\r\n"), Spaces, *CreateUTF8LiteralString(KVP.Key.ToString()), *CreateUTF8LiteralString(KVP.Value));
		}
	}

	FString Result;
	if (MetaDataOutput.Len())
	{
		Out.Log (TEXT("#if WITH_METADATA\r\n"));
		Out.Logf(TEXT("%sstatic const UE4CodeGen_Private::FMetaDataPairParam %s[] = {\r\n"), Spaces, MetaDataBlockName);
		Out.Log (*MetaDataOutput);
		Out.Logf(TEXT("%s};\r\n"), Spaces);
		Out.Log (TEXT("#endif\r\n"));

		Result = FString::Printf(TEXT("METADATA_PARAMS(%s, ARRAY_COUNT(%s))"), MetaDataBlockName, MetaDataBlockName);
	}
	else
	{
		Result = TEXT("METADATA_PARAMS(nullptr, 0)");
	}

	return Result;
}

void FNativeClassHeaderGenerator::ExportProperties(FOutputDevice& Out, UStruct* Struct, int32 TextIndent)
{
	UProperty*	Previous			= NULL;
	UProperty*	PreviousNonEditorOnly = NULL;
	UProperty*	LastInSuper			= NULL;
	UStruct*	InheritanceSuper	= Struct->GetInheritanceSuper();

	// Find last property in the lowest base class that has any properties
	UStruct* CurrentSuper = InheritanceSuper;
	while (LastInSuper == NULL && CurrentSuper)
	{
		for( TFieldIterator<UProperty> It(CurrentSuper,EFieldIteratorFlags::ExcludeSuper); It; ++It )
		{
			UProperty* Current = *It;

			// Disregard properties with 0 size like functions.
			if( It.GetStruct() == CurrentSuper && Current->ElementSize )
			{
				LastInSuper = Current;
			}
		}
		// go up a layer in the hierarchy
		CurrentSuper = CurrentSuper->GetSuperStruct();
	}

	FMacroBlockEmitter WithEditorOnlyData(Out, TEXT("WITH_EDITORONLY_DATA"));

	// Iterate over all properties in this struct.
	for( TFieldIterator<UProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It )
	{
		UProperty* Current = *It;

		// Disregard properties with 0 size like functions.
		if (It.GetStruct() == Struct)
		{
			WithEditorOnlyData(Current->IsEditorOnlyProperty());

			// Export property specifiers
			// Indent code and export CPP text.
			{
				FUHTStringBuilder JustPropertyDecl;

				const FString* Dim = GArrayDimensions.Find(Current);
				Current->ExportCppDeclaration( JustPropertyDecl, EExportedDeclaration::Member, Dim ? **Dim : NULL);
				ApplyAlternatePropertyExportText(*It, JustPropertyDecl, EExportingState::TypeEraseDelegates);

				// Finish up line.
				Out.Logf(TEXT("%s%s;\r\n"), FCString::Tab(TextIndent + 1), *JustPropertyDecl);
			}

			LastInSuper	= NULL;
			Previous = Current;
			if (!Current->IsEditorOnlyProperty())
			{
				PreviousNonEditorOnly = Current;
			}
		}
	}
}

/**
 * Class that is representing a type singleton.
 */
struct FTypeSingleton
{
public:
	/** Constructor */
	FTypeSingleton(FString InName, UField* InType)
		: Name(MoveTemp(InName)), Type(InType) {}

	/**
	 * Gets this singleton's name.
	 */
	const FString& GetName() const
	{
		return Name;
	}

	/**
	 * Gets this singleton's extern declaration.
	 */
	const FString& GetExternDecl() const
	{
		if (ExternDecl.IsEmpty())
		{
			ExternDecl = GenerateExternDecl(Type, GetName());
		}

		return ExternDecl;
	}

private:
	/**
	 * Extern declaration generator.
	 */
	static FString GenerateExternDecl(UField* InType, const FString& InName)
	{
		const TCHAR* TypeStr = nullptr;

		if (InType->GetClass() == UClass::StaticClass())
		{
			TypeStr = TEXT("UClass");
		}
		else if (InType->GetClass() == UFunction::StaticClass() || InType->GetClass() == UDelegateFunction::StaticClass())
		{
			TypeStr = TEXT("UFunction");
		}
		else if (InType->GetClass() == UScriptStruct::StaticClass())
		{
			TypeStr = TEXT("UScriptStruct");
		}
		else if (InType->GetClass() == UEnum::StaticClass())
		{
			TypeStr = TEXT("UEnum");
		}
		else
		{
			FError::Throwf(TEXT("Unsupported item type to get extern for."));
		}

		return FString::Printf(
			TEXT("\t%s_API %s* %s;\r\n"),
			*FPackageName::GetShortName(InType->GetOutermost()).ToUpper(),
			TypeStr,
			*InName
		);
	}

	/** Field that stores this singleton name. */
	FString Name;

	/** Cached field that stores this singleton extern declaration. */
	mutable FString ExternDecl;

	/** Type of the singleton */
	UField* Type;
};

/**
 * Class that represents type singleton cache.
 */
class FTypeSingletonCache
{
public:
	/**
	 * Gets type singleton from cache.
	 *
	 * @param Type Singleton type.
	 * @param bRequiresValidObject Does it require a valid object?
	 */
	static const FTypeSingleton& Get(UField* Type, bool bRequiresValidObject = true)
	{
		static TMap<FTypeSingletonCacheKey, FTypeSingleton> CacheData;

		FTypeSingletonCacheKey Key(Type, bRequiresValidObject);
		if (FTypeSingleton* SingletonPtr = CacheData.Find(Key))
		{
			return *SingletonPtr;
		}

		return CacheData.Add(Key,
			FTypeSingleton(GenerateSingletonName(Type, bRequiresValidObject), Type)
		);
	}

private:
	/**
	 * Private type that represents cache map key.
	 */
	struct FTypeSingletonCacheKey
	{
		/** FTypeSingleton type */
		UField* Type;

		/** If this type singleton requires valid object. */
		bool bRequiresValidObject;

		/* Constructor */
		FTypeSingletonCacheKey(UField* InType, bool bInRequiresValidObject)
			: Type(InType), bRequiresValidObject(bInRequiresValidObject)
		{}

		/**
		 * Equality operator.
		 *
		 * @param Other Other key.
		 *
		 * @returns True if this is equal to Other. False otherwise.
		 */
		bool operator==(const FTypeSingletonCacheKey& Other) const
		{
			return Type == Other.Type && bRequiresValidObject == Other.bRequiresValidObject;
		}

		/**
		 * Gets hash value for this object.
		 */
		friend uint32 GetTypeHash(const FTypeSingletonCacheKey& Object)
		{
			return HashCombine(
				GetTypeHash(Object.Type),
				GetTypeHash(Object.bRequiresValidObject)
			);
		}
	};

	/**
	 * Generates singleton name.
	 */
	static FString GenerateSingletonName(UField* Item, bool bRequiresValidObject)
	{
		check(Item);

		FString Suffix;
		if (UClass* ItemClass = Cast<UClass>(Item))
		{
			if (!bRequiresValidObject && !ItemClass->HasAllClassFlags(CLASS_Intrinsic))
			{
				Suffix = TEXT("_NoRegister");
			}
		}

		FString Result;
		for (UObject* Outer = Item; Outer; Outer = Outer->GetOuter())
		{
			if (!Result.IsEmpty())
			{
				Result = TEXT("_") + Result;
			}

			if (Cast<UClass>(Outer) || Cast<UScriptStruct>(Outer))
			{
				FString OuterName = NameLookupCPP.GetNameCPP(Cast<UStruct>(Outer));
				Result = OuterName + Result;

				// Structs can also have UPackage outer.
				if (Cast<UClass>(Outer) || Cast<UPackage>(Outer->GetOuter()))
				{
					break;
				}
			}
			else
			{
				Result = Outer->GetName() + Result;
			}
		}

		// Can't use long package names in function names.
		if (Result.StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive))
		{
			Result = FPackageName::GetShortName(Result);
		}

		FString ClassString = NameLookupCPP.GetNameCPP(Item->GetClass());
		return FString(TEXT("Z_Construct_")) + ClassString + TEXT("_") + Result + Suffix + TEXT("()");
	}
};

FString FNativeClassHeaderGenerator::GetSingletonName(UField* Item, bool bRequiresValidObject)
{
	const FTypeSingleton& Cache = FTypeSingletonCache::Get(Item, bRequiresValidObject);

	FString Result = Cache.GetName();

	if (UniqueCrossModuleReferences)
	{
		const FString& Extern = Cache.GetExternDecl();
		UniqueCrossModuleReferences->Add(*Extern);
	}

	return Result;
}

FString FNativeClassHeaderGenerator::GetSingletonNameFuncAddr(UField* Item, bool bRequiresValidObject)
{
	FString Result;
	if (!Item)
	{
		Result = TEXT("nullptr");
	}
	else
	{
		Result = this->GetSingletonName(Item, bRequiresValidObject).LeftChop(2);
	}
	return Result;
}

FString FNativeClassHeaderGenerator::GetOverriddenName(const UField* Item)
{
	const FString& OverriddenName = Item->GetMetaData(TEXT("OverrideNativeName"));
	if (!OverriddenName.IsEmpty())
	{
		return OverriddenName.ReplaceCharWithEscapedChar();
	}
	return Item->GetName();
}

FName FNativeClassHeaderGenerator::GetOverriddenFName(const UField* Item)
{
	FString OverriddenName = Item->GetMetaData(TEXT("OverrideNativeName"));
	if (!OverriddenName.IsEmpty())
	{
		return FName(*OverriddenName);
	}
	return Item->GetFName();
}

FString FNativeClassHeaderGenerator::GetOverriddenPathName(const UField* Item)
{
	return FString::Printf(TEXT("%s.%s"), *FClass::GetTypePackageName(Item), *GetOverriddenName(Item));
}

FString FNativeClassHeaderGenerator::GetOverriddenNameForLiteral(const UField* Item)
{
	const FString& OverriddenName = Item->GetMetaData(TEXT("OverrideNativeName"));
	if (!OverriddenName.IsEmpty())
	{
		return TEXT("TEXT(\"") + OverriddenName + TEXT("\")");
	}
	return TEXT("\"") + Item->GetName() + TEXT("\"");
}

FString FNativeClassHeaderGenerator::GetUTF8OverriddenNameForLiteral(const UField* Item)
{
	const FString& OverriddenName = Item->GetMetaData(TEXT("OverrideNativeName"));
	if (!OverriddenName.IsEmpty())
	{
		return CreateUTF8LiteralString(OverriddenName);
	}
	return CreateUTF8LiteralString(Item->GetName());
}

void FNativeClassHeaderGenerator::PropertyNew(FOutputDevice& Out, UProperty* Prop, const TCHAR* OffsetStr, const TCHAR* Name, const TCHAR* Spaces, const TCHAR* SourceStruct)
{
	FString      PropName             = CreateUTF8LiteralString(FNativeClassHeaderGenerator::GetOverriddenName(Prop));
	FString      PropNameDep          = Prop->HasAllPropertyFlags(CPF_Deprecated) ? Prop->GetName() + TEXT("_DEPRECATED") : Prop->GetName();
	const TCHAR* UPropertyObjectFlags = FClass::IsOwnedByDynamicType(Prop) ? TEXT("RF_Public|RF_Transient") : TEXT("RF_Public|RF_Transient|RF_MarkAsNative");
	uint64       PropFlags            = Prop->PropertyFlags & ~CPF_ComputedFlags;

	FString PropTag        = GetGeneratedCodeCRCTag(Prop);
	FString PropNotifyFunc = (Prop->RepNotifyFunc != NAME_None) ? CreateUTF8LiteralString(*Prop->RepNotifyFunc.ToString()) : TEXT("nullptr");

	FString ArrayDim = (Prop->ArrayDim != 1) ? FString::Printf(TEXT("CPP_ARRAY_DIM(%s, %s)"), *PropNameDep, SourceStruct) : TEXT("1");

	FString MetaDataParams = OutputMetaDataCodeForObject(Out, Prop, *FString::Printf(TEXT("%s_MetaData"), Name), Spaces);

	if (UByteProperty* TypedProp = Cast<UByteProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FBytePropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Byte, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->Enum),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UInt8Property* TypedProp = Cast<UInt8Property>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FInt8PropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Int8, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UInt16Property* TypedProp = Cast<UInt16Property>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FInt16PropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Int16, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UIntProperty* TypedProp = Cast<UIntProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::%s %s = { UE4CodeGen_Private::EPropertyClass::Int, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			GUnsizedProperties.Contains(TypedProp) ? TEXT("FUnsizedIntPropertyParams") : TEXT("FIntPropertyParams"),
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UInt64Property* TypedProp = Cast<UInt64Property>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FInt64PropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Int64, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UUInt16Property* TypedProp = Cast<UUInt16Property>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FUInt16PropertyParams %s = { UE4CodeGen_Private::EPropertyClass::UInt16, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UUInt32Property* TypedProp = Cast<UUInt32Property>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::%s %s = { UE4CodeGen_Private::EPropertyClass::UInt32, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			GUnsizedProperties.Contains(TypedProp) ? TEXT("FUnsizedUIntPropertyParams") : TEXT("FUInt32PropertyParams"),
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UUInt64Property* TypedProp = Cast<UUInt64Property>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FUInt64PropertyParams %s = { UE4CodeGen_Private::EPropertyClass::UInt64, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UFloatProperty* TypedProp = Cast<UFloatProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FFloatPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Float, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UDoubleProperty* TypedProp = Cast<UDoubleProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FDoublePropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Double, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UBoolProperty* TypedProp = Cast<UBoolProperty>(Prop))
	{
		UObject* PropOuter = Prop->GetOuter();
		FString OuterSize;
		FString Setter;
		if (bool bOuterIsContainer = PropOuter->IsA<UArrayProperty>() || PropOuter->IsA<UMapProperty>() || PropOuter->IsA<USetProperty>())
		{
			OuterSize = TEXT("0");
			Setter    = TEXT("nullptr");
		}
		else
		{
			OuterSize = FString::Printf(TEXT("sizeof(%s)"), SourceStruct);
			Out.Logf(
				TEXT("%sauto %s_SetBit = [](void* Obj){ ((%s*)Obj)->%s%s = 1; };\r\n"),
				Spaces,
				Name,
				SourceStruct,
				*Prop->GetName(),
				Prop->HasAllPropertyFlags(CPF_Deprecated) ? TEXT("_DEPRECATED") : TEXT("")
			);
			Setter = FString::Printf(TEXT("&UE4CodeGen_Private::TBoolSetBitWrapper<decltype(%s_SetBit)>::SetBit"), Name);
		}

		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FBoolPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Bool, %s, %s, 0x%016llx, %s, %s, sizeof(%s), %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			*TypedProp->GetCPPType(nullptr, 0),
			TypedProp->IsNativeBool() ? TEXT("UE4CodeGen_Private::ENativeBool::Native") : TEXT("UE4CodeGen_Private::ENativeBool::NotNative"),
			*OuterSize,
			*Setter,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (USoftClassProperty* TypedProp = Cast<USoftClassProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FSoftClassPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::SoftClass, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->MetaClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UWeakObjectProperty* TypedProp = Cast<UWeakObjectProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FWeakObjectPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::WeakObject, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->PropertyClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (ULazyObjectProperty* TypedProp = Cast<ULazyObjectProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FLazyObjectPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::LazyObject, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->PropertyClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (USoftObjectProperty* TypedProp = Cast<USoftObjectProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FSoftObjectPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::SoftObject, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->PropertyClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UClassProperty* TypedProp = Cast<UClassProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FClassPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Class, %s, %s, 0x%016llx, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->MetaClass, false),
			*GetSingletonNameFuncAddr(TypedProp->PropertyClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UObjectProperty* TypedProp = Cast<UObjectProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FObjectPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Object, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->PropertyClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UInterfaceProperty* TypedProp = Cast<UInterfaceProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FInterfacePropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Interface, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->InterfaceClass, false),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UNameProperty* TypedProp = Cast<UNameProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FNamePropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Name, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UStrProperty* TypedProp = Cast<UStrProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FStrPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Str, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UArrayProperty* TypedProp = Cast<UArrayProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FArrayPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Array, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UMapProperty* TypedProp = Cast<UMapProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FMapPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Map, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (USetProperty* TypedProp = Cast<USetProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FSetPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Set, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UStructProperty* TypedProp = Cast<UStructProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FStructPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Struct, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->Struct),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UDelegateProperty* TypedProp = Cast<UDelegateProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FDelegatePropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Delegate, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->SignatureFunction),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UMulticastDelegateProperty* TypedProp = Cast<UMulticastDelegateProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FMulticastDelegatePropertyParams %s = { UE4CodeGen_Private::EPropertyClass::MulticastDelegate, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->SignatureFunction),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UTextProperty* TypedProp = Cast<UTextProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FTextPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Text, %s, %s, 0x%016llx, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	if (UEnumProperty* TypedProp = Cast<UEnumProperty>(Prop))
	{
		Out.Logf(
			TEXT("%sstatic const UE4CodeGen_Private::FEnumPropertyParams %s = { UE4CodeGen_Private::EPropertyClass::Enum, %s, %s, 0x%016llx, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			Name,
			*PropName,
			UPropertyObjectFlags,
			PropFlags,
			*ArrayDim,
			*PropNotifyFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(TypedProp->Enum),
			*MetaDataParams,
			*PropTag
		);

		return;
	}

	// Unhandled type
	check(false);
}

bool IsEditorOnlyDataProperty(UProperty* Prop)
{
	while (Prop)
	{
		if (Prop->IsEditorOnlyProperty())
		{
			return true;
		}

		Prop = Cast<UProperty>(Prop->GetOuter());
	}

	return false;
}

void FNativeClassHeaderGenerator::OutputProperties(FOutputDevice& OutputDevice, FString& OutPropertyRange, const TArray<UProperty*>& Properties, const TCHAR* Spaces)
{
	if (Properties.Num() == 0)
	{
		OutPropertyRange = TEXT("nullptr, 0");
		return;
	}

	TArray<FPropertyNamePointerPair> PropertyNamesAndPointers;
	bool bHasAllEditorOnlyDataProperties = true;

	{
		FMacroBlockEmitter WithEditorOnlyMacroEmitter(OutputDevice, TEXT("WITH_EDITORONLY_DATA"));

		for (int32 Index = Properties.Num() - 1; Index >= 0; Index--)
		{
			bool bRequiresHasEditorOnlyMacro = IsEditorOnlyDataProperty(Properties[Index]);
			if (!bRequiresHasEditorOnlyMacro)
			{
				bHasAllEditorOnlyDataProperties = false;
			}

			WithEditorOnlyMacroEmitter(bRequiresHasEditorOnlyMacro);
			OutputProperty(OutputDevice, PropertyNamesAndPointers, Properties[Index], Spaces);
		}

		WithEditorOnlyMacroEmitter(bHasAllEditorOnlyDataProperties);
		OutputDevice.Logf(TEXT("%sstatic const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[] = {\r\n"), Spaces);

		for (const FPropertyNamePointerPair& PropNameAndPtr : PropertyNamesAndPointers)
		{
			bool bRequiresHasEditorOnlyMacro = IsEditorOnlyDataProperty(PropNameAndPtr.Prop);

			WithEditorOnlyMacroEmitter(bRequiresHasEditorOnlyMacro);
			OutputDevice.Logf(TEXT("%s\t(const UE4CodeGen_Private::FPropertyParamsBase*)&%s,\r\n"), Spaces, *PropNameAndPtr.Name);
		}

		WithEditorOnlyMacroEmitter(bHasAllEditorOnlyDataProperties);
		OutputDevice.Logf(TEXT("%s};\r\n"), Spaces);
	}

	if (bHasAllEditorOnlyDataProperties)
	{
		OutPropertyRange = TEXT("IF_WITH_EDITORONLY_DATA(PropPointers, nullptr), IF_WITH_EDITORONLY_DATA(ARRAY_COUNT(PropPointers), 0)");
	}
	else
	{
		OutPropertyRange = TEXT("PropPointers, ARRAY_COUNT(PropPointers)");
	}
}

inline FString GetEventStructParamsName(UObject* Outer, const TCHAR* FunctionName)
{
	FString OuterName;
	if (Outer->IsA<UClass>())
	{
		OuterName = ((UClass*)Outer)->GetName();
	}
	else if (Outer->IsA<UPackage>())
	{
		OuterName = ((UPackage*)Outer)->GetName();
		OuterName.ReplaceInline(TEXT("/"), TEXT("_"), ESearchCase::CaseSensitive);
	}
	else
	{
		FError::Throwf(TEXT("Unrecognized outer type"));
	}

	FString Result = FString::Printf(TEXT("%s_event%s_Parms"), *OuterName, FunctionName);
	if (Result.Len() && FChar::IsDigit(Result[0]))
	{
		Result.InsertAt(0, TCHAR('_'));
	}
	return Result;
}

void FNativeClassHeaderGenerator::OutputProperty(FOutputDevice& OutputDevice, TArray<FPropertyNamePointerPair>& PropertyNamesAndPointers, UProperty* Prop, const TCHAR* Spaces)
{
	FString PropName = Prop->GetName();

	FString PropVariableName = FString::Printf(TEXT("NewProp_%s"), *PropName);

	// Helper to handle the creation of the underlying properties if they're enum properties
	auto HandleUnderlyingEnumProperty = [this, &PropertyNamesAndPointers, &OutputDevice, Spaces](UProperty* LocalProp, const FString& OuterName)
	{
		if (UEnumProperty* EnumProp = Cast<UEnumProperty>(LocalProp))
		{
			FString PropVarName = OuterName + TEXT("_Underlying");
			PropertyNew(OutputDevice, EnumProp->UnderlyingProp, TEXT("0"), *PropVarName, Spaces);
			PropertyNamesAndPointers.Emplace(MoveTemp(PropVarName), EnumProp->UnderlyingProp);
		}
	};

	{
		FString SourceStruct;
		if (UFunction* Function = Cast<UFunction>(Prop->GetOuter()))
		{
			while (Function->GetSuperFunction())
			{
				Function = Function->GetSuperFunction();
			}
			FString FunctionName = Function->GetName();
			if( Function->HasAnyFunctionFlags( FUNC_Delegate ) )
			{
				FunctionName = FunctionName.LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );
			}

			SourceStruct = GetEventStructParamsName(Function->GetOuter(), *FunctionName);
		}
		else
		{
			SourceStruct = NameLookupCPP.GetNameCPP(CastChecked<UStruct>(Prop->GetOuter()));
		}

		FString PropNameDep = PropName;
		if (Prop->HasAllPropertyFlags(CPF_Deprecated))
		{
			 PropNameDep += TEXT("_DEPRECATED");
		}

		FString PropMacroOuterClass = FString::Printf(TEXT("STRUCT_OFFSET(%s, %s)"), *SourceStruct, *PropNameDep);

		PropertyNew(OutputDevice, Prop, *PropMacroOuterClass, *PropVariableName, Spaces, *SourceStruct);
		PropertyNamesAndPointers.Emplace(PropVariableName, Prop);
		HandleUnderlyingEnumProperty(Prop, PropVariableName);
	}

	if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Prop))
	{
		FString InnerVariableName = FString::Printf(TEXT("NewProp_%s_Inner"), *ArrayProperty->Inner->GetName());

		PropertyNew(OutputDevice, ArrayProperty->Inner, TEXT("0"), *InnerVariableName, Spaces);
		PropertyNamesAndPointers.Emplace(InnerVariableName, ArrayProperty->Inner);
		HandleUnderlyingEnumProperty(ArrayProperty->Inner, InnerVariableName);
	}

	else if (UMapProperty* MapProperty = Cast<UMapProperty>(Prop))
	{
		UProperty* Key   = MapProperty->KeyProp;
		UProperty* Value = MapProperty->ValueProp;

		FString KeyVariableName   = FString::Printf(TEXT("NewProp_%s_KeyProp"), *Key->GetName());
		FString ValueVariableName = FString::Printf(TEXT("NewProp_%s_ValueProp"), *Value->GetName());

		PropertyNew(OutputDevice, Key, TEXT("0"), *KeyVariableName, Spaces);
		PropertyNamesAndPointers.Emplace(KeyVariableName, Key);
		HandleUnderlyingEnumProperty(Key, KeyVariableName);

		PropertyNew(OutputDevice, Value, TEXT("1"), *ValueVariableName, Spaces);
		PropertyNamesAndPointers.Emplace(ValueVariableName, Value);
		HandleUnderlyingEnumProperty(Value, ValueVariableName);
	}

	else if (USetProperty* SetProperty = Cast<USetProperty>(Prop))
	{
		UProperty* Inner = SetProperty->ElementProp;

		FString ElementVariableName = FString::Printf(TEXT("NewProp_%s_ElementProp"), *Inner->GetName());

		PropertyNew(OutputDevice, Inner, TEXT("0"), *ElementVariableName, Spaces);
		PropertyNamesAndPointers.Emplace(ElementVariableName, Inner);
		HandleUnderlyingEnumProperty(Inner, ElementVariableName);
	}
}

static bool IsAlwaysAccessible(UScriptStruct* Script)
{
	FName ToTest = Script->GetFName();
	if (ToTest == NAME_Matrix)
	{
		return false; // special case, the C++ FMatrix does not have the same members.
	}
	bool Result = Script->HasDefaults(); // if we have cpp struct ops in it for UHT, then we can assume it is always accessible
	if( ToTest == NAME_Plane
		||	ToTest == NAME_Vector
		||	ToTest == NAME_Vector4
		||	ToTest == NAME_Quat
		||	ToTest == NAME_Color
		)
	{
		check(Result);
	}
	return Result;
}

static void FindNoExportStructsRecursive(TArray<UScriptStruct*>& Structs, UStruct* Start)
{
	while (Start)
	{
		if (UScriptStruct* StartScript = Cast<UScriptStruct>(Start))
		{
			if (StartScript->StructFlags & STRUCT_Native)
			{
				break;
			}

			if (!IsAlwaysAccessible(StartScript)) // these are a special cases that already exists and if wrong if exported naively
			{
				// this will topologically sort them in reverse order
				Structs.Remove(StartScript);
				Structs.Add(StartScript);
			}
		}

		for (UProperty* Prop : TFieldRange<UProperty>(Start, EFieldIteratorFlags::ExcludeSuper))
		{
			if (UStructProperty* StructProp = Cast<UStructProperty>(Prop))
			{
				FindNoExportStructsRecursive(Structs, StructProp->Struct);
			}
			else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop))
			{
				if (UStructProperty* InnerStructProp = Cast<UStructProperty>(ArrayProp->Inner))
				{
					FindNoExportStructsRecursive(Structs, InnerStructProp->Struct);
				}
			}
			else if (UMapProperty* MapProp = Cast<UMapProperty>(Prop))
			{
				if (UStructProperty* KeyStructProp = Cast<UStructProperty>(MapProp->KeyProp))
				{
					FindNoExportStructsRecursive(Structs, KeyStructProp->Struct);
				}
				if (UStructProperty* ValueStructProp = Cast<UStructProperty>(MapProp->ValueProp))
				{
					FindNoExportStructsRecursive(Structs, ValueStructProp->Struct);
				}
			}
			else if (USetProperty* SetProp = Cast<USetProperty>(Prop))
			{
				if (UStructProperty* ElementStructProp = Cast<UStructProperty>(SetProp->ElementProp))
				{
					FindNoExportStructsRecursive(Structs, ElementStructProp->Struct);
				}
			}
		}
		Start = Start->GetSuperStruct();
	}
}

static TArray<UScriptStruct*> FindNoExportStructs(UStruct* Start)
{
	TArray<UScriptStruct*> Result;
	FindNoExportStructsRecursive(Result, Start);

	// These come out in reverse order of topology so reverse them
	Algo::Reverse(Result);

	return Result;
}

FString FNativeClassHeaderGenerator::GetPackageSingletonName(const UPackage* InPackage)
{
	static FString ClassString = NameLookupCPP.GetNameCPP(UPackage::StaticClass());

	FString Result = TEXT("Z_Construct_") + ClassString + TEXT("_") + InPackage->GetName().Replace(TEXT("/"), TEXT("_")) + TEXT("()");

	if (UniqueCrossModuleReferences)
	{
		UniqueCrossModuleReferences->Add(FString::Printf(TEXT("\tUPackage* %s;\r\n"), *Result));
	}

	return Result;
}

void FNativeClassHeaderGenerator::ExportGeneratedPackageInitCode(FOutputDevice& Out, const TCHAR* InDeclarations, const UPackage* InPackage, uint32 CRC)
{
	FString ApiString = GetAPIString();
	FString SingletonName = GetPackageSingletonName(InPackage);

	TArray<UField*> SingletonsToOutput;
	for (UField* ScriptType : TObjectRange<UField>())
	{
		if (ScriptType->GetOutermost() != InPackage)
		{
			continue;
		}

		if (ScriptType->IsA<UDelegateFunction>() || (ScriptType->IsA<UScriptStruct>() && (((UScriptStruct*)ScriptType)->StructFlags & STRUCT_NoExport)))
		{
			UField* FieldOuter = Cast<UField>(ScriptType->GetOuter());
			if (!FieldOuter || !FClass::IsDynamic(FieldOuter))
			{
				SingletonsToOutput.Add(ScriptType);
			}
		}
	}

	for (UField* ScriptType : SingletonsToOutput)
	{
		const FString& Extern = FTypeSingletonCache::Get(ScriptType, true).GetExternDecl();

		Out.Logf(TEXT("%s"), *Extern);
	}

	FString MetaDataParams = OutputMetaDataCodeForObject(Out, InPackage, TEXT("Package_MetaDataParams"), TEXT("\t\t\t"));

	Out.Logf(TEXT("\tUPackage* %s\r\n"), *SingletonName);
	Out.Logf(TEXT("\t{\r\n"));
	Out.Logf(TEXT("\t\tstatic UPackage* ReturnPackage = nullptr;\r\n"));
	Out.Logf(TEXT("\t\tif (!ReturnPackage)\r\n"));
	Out.Logf(TEXT("\t\t{\r\n"));

	FString SingletonRange;
	if (SingletonsToOutput.Num() > 0)
	{
		Out.Logf(TEXT("\t\t\tstatic UObject* (*const SingletonFuncArray[])() = {\r\n"));
		for (UField* ScriptType : SingletonsToOutput)
		{
			const FString& Name = FTypeSingletonCache::Get(ScriptType, true).GetName().LeftChop(2);

			Out.Logf(TEXT("\t\t\t\t(UObject* (*)())%s,\r\n"), *Name);
		}
		Out.Logf(TEXT("\t\t\t};\r\n"));

		SingletonRange = TEXT("SingletonFuncArray, ARRAY_COUNT(SingletonFuncArray)");
	}
	else
	{
		SingletonRange = TEXT("nullptr, 0");
	}

	Out.Logf(TEXT("\t\t\tstatic const UE4CodeGen_Private::FPackageParams PackageParams = {\r\n"));
	Out.Logf(TEXT("\t\t\t\t%s,\r\n"), *CreateUTF8LiteralString(InPackage->GetName()));
	Out.Logf(TEXT("\t\t\t\tPKG_CompiledIn | 0x%08X,\r\n"), InPackage->GetPackageFlags() & (PKG_ClientOptional | PKG_ServerSideOnly | PKG_EditorOnly | PKG_Developer));
	Out.Logf(TEXT("\t\t\t\t0x%08X,\r\n"), CRC);
	Out.Logf(TEXT("\t\t\t\t0x%08X,\r\n"), GenerateTextCRC(InDeclarations));
	Out.Logf(TEXT("\t\t\t\t%s,\r\n"), *SingletonRange);
	Out.Logf(TEXT("\t\t\t\t%s\r\n"), *MetaDataParams);
	Out.Logf(TEXT("\t\t\t};\r\n"));
	Out.Logf(TEXT("\t\t\tUE4CodeGen_Private::ConstructUPackage(ReturnPackage, PackageParams);\r\n"));
	Out.Logf(TEXT("\t\t}\r\n"));
	Out.Logf(TEXT("\t\treturn ReturnPackage;\r\n"));
	Out.Logf(TEXT("\t}\r\n"));
}

void FNativeClassHeaderGenerator::ExportNativeGeneratedInitCode(FOutputDevice& Out, FOutputDevice& OutDeclarations, const FUnrealSourceFile& SourceFile, FClass* Class, FUHTStringBuilder& OutFriendText)
{
	check(!OutFriendText.Len());

	const bool   bIsNoExport  = Class->HasAnyClassFlags(CLASS_NoExport);
	const bool   bIsDynamic   = FClass::IsDynamic(Class);
	const TCHAR* ClassNameCPP = NameLookupCPP.GetNameCPP(Class);

	FUHTStringBuilder BodyText;
	FString ApiString = GetAPIString();

	TSet<FName> AlreadyIncludedNames;
	TArray<UFunction*> FunctionsToExport;
	bool bAllEditorOnlyFunctions = true;
	for (UFunction* LocalFunc : TFieldRange<UFunction>(Class,EFieldIteratorFlags::ExcludeSuper))
	{
		FName TrueName = FNativeClassHeaderGenerator::GetOverriddenFName(LocalFunc);
		bool bAlreadyIncluded = false;
		AlreadyIncludedNames.Add(TrueName, &bAlreadyIncluded);
		if (bAlreadyIncluded)
		{
			// In a dynamic class the same function signature may be used for a Multi- and a Single-cast delegate.
			if (!LocalFunc->IsA<UDelegateFunction>() || !bIsDynamic)
			{
				FError::Throwf(TEXT("The same function linked twice. Function: %s Class: %s"), *LocalFunc->GetName(), *Class->GetName());
			}
			continue;
		}
		if (!LocalFunc->IsA<UDelegateFunction>())
		{
			bAllEditorOnlyFunctions &= LocalFunc->HasAnyFunctionFlags(FUNC_EditorOnly);
		}
		FunctionsToExport.Add(LocalFunc);
	}

	// Sort the list of functions
	FunctionsToExport.Sort();

	FUHTStringBuilder GeneratedClassRegisterFunctionText;

	// The class itself.
	{
		// simple ::StaticClass wrapper to avoid header, link and DLL hell
		{
			FString SingletonNameNoRegister = GetSingletonName(Class, false);

			OutDeclarations.Log(FTypeSingletonCache::Get(Class, false).GetExternDecl());

			GeneratedClassRegisterFunctionText.Logf(TEXT("\tUClass* %s\r\n"), *SingletonNameNoRegister);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t{\r\n"));
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\treturn %s::StaticClass();\r\n"), ClassNameCPP);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t}\r\n"));
		}
		FString SingletonName = GetSingletonName(Class);

		OutFriendText.Logf(TEXT("\tfriend %sclass UClass* %s;\r\n"), *ApiString, *SingletonName);
		OutDeclarations.Log(FTypeSingletonCache::Get(Class).GetExternDecl());

		GeneratedClassRegisterFunctionText.Logf(TEXT("\tUClass* %s\r\n"), *SingletonName);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t{\r\n"));
		if (!bIsDynamic)
		{
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tstatic UClass* OuterClass = nullptr;\r\n"));
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tif (!OuterClass)\r\n"));
		}
		else
		{
			const FString DynamicClassPackageName = FClass::GetTypePackageName(Class);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tUPackage* OuterPackage = FindOrConstructDynamicTypePackage(TEXT(\"%s\"));\r\n"), *DynamicClassPackageName);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tUClass* OuterClass = Cast<UClass>(StaticFindObjectFast(UClass::StaticClass(), OuterPackage, TEXT(\"%s\")));\r\n"), *FNativeClassHeaderGenerator::GetOverriddenName(Class));
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tif (!OuterClass || !(OuterClass->ClassFlags & CLASS_Constructed))\r\n"));
		}

		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t{\r\n"));

		FUHTStringBuilder Singletons;
		UClass* SuperClass = Class->GetSuperClass();
		if (SuperClass && SuperClass != Class)
		{
			OutDeclarations.Log(FTypeSingletonCache::Get(SuperClass).GetExternDecl());
			Singletons.Logf(TEXT("\t\t\t\t(UObject* (*)())%s,\r\n"), *GetSingletonName(SuperClass).LeftChop(2));
		}
		if (!bIsDynamic)
		{
			FString PackageSingletonName = GetPackageSingletonName(Class->GetOutermost());

			OutDeclarations.Logf(TEXT("\t%s_API UPackage* %s;\r\n"), *ApiString, *PackageSingletonName);
			Singletons.Logf(TEXT("\t\t\t\t(UObject* (*)())%s,\r\n"), *PackageSingletonName.LeftChop(2));
		}

		if (Singletons.Len() != 0)
		{
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\tstatic UObject* (*const DependentSingletons[])() = {\r\n"));
			GeneratedClassRegisterFunctionText.Log (*Singletons);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t};\r\n"));
		}

		if (FunctionsToExport.Num() != 0)
		{
			GeneratedClassRegisterFunctionText.Log(BEGIN_WRAP_EDITOR_ONLY(bAllEditorOnlyFunctions));
			GeneratedClassRegisterFunctionText.Log(TEXT("\t\t\tstatic const FClassFunctionLinkInfo FuncInfo[] = {\r\n"));

			for (UFunction* Function : FunctionsToExport)
			{
				const bool bIsEditorOnlyFunction = Function->HasAnyFunctionFlags(FUNC_EditorOnly);

				if (!Function->IsA<UDelegateFunction>())
				{
					OutDeclarations.Logf(
						TEXT("%s%s%s"),
						BEGIN_WRAP_EDITOR_ONLY(bIsEditorOnlyFunction),
						*FTypeSingletonCache::Get(Function).GetExternDecl(),
						END_WRAP_EDITOR_ONLY(bIsEditorOnlyFunction)
					);
					ExportFunction(Out, SourceFile, Function, bIsNoExport);
				}

				GeneratedClassRegisterFunctionText.Logf(
					TEXT("%s\t\t\t\t{ &%s, %s },%s\r\n%s"),
					BEGIN_WRAP_EDITOR_ONLY(bIsEditorOnlyFunction),
					*GetSingletonNameFuncAddr(Function),
					*FNativeClassHeaderGenerator::GetUTF8OverriddenNameForLiteral(Function),
					*GetGeneratedCodeCRCTag(Function),
					END_WRAP_EDITOR_ONLY(bIsEditorOnlyFunction)
				);
			}

			GeneratedClassRegisterFunctionText.Log(TEXT("\t\t\t};\r\n"));
			GeneratedClassRegisterFunctionText.Log(END_WRAP_EDITOR_ONLY(bAllEditorOnlyFunctions));
		}

		TMap<FName, FString>* MetaDataMap = UMetaData::GetMapForObject(Class);
		if (MetaDataMap)
		{
			FClassMetaData* ClassMetaData = GScriptHelper.FindClassData(Class);
			if (ClassMetaData && ClassMetaData->bObjectInitializerConstructorDeclared)
			{
				MetaDataMap->Add(FName(TEXT("ObjectInitializerConstructorDeclared")), TEXT(""));
			}
		}
		FString MetaDataParams = OutputMetaDataCodeForObject(GeneratedClassRegisterFunctionText, Class, TEXT("Class_MetaDataParams"), TEXT("\t\t\t"));

		TArray<UProperty*> Props;
		for (UProperty* Prop : TFieldRange<UProperty>(Class, EFieldIteratorFlags::ExcludeSuper))
		{
			Props.Add(Prop);
		}

		UE_CLOG(Class->ClassGeneratedBy, LogCompile, Fatal, TEXT("For intrinsic and compiled-in classes, ClassGeneratedBy should always be null"));

		FString PropertyRange;
		OutputProperties(GeneratedClassRegisterFunctionText, PropertyRange, Props, TEXT("\t\t\t"));

		FString InterfaceRange;
		if (Class->Interfaces.Num() > 0)
		{
			GeneratedClassRegisterFunctionText.Log(TEXT("\t\t\tstatic const UE4CodeGen_Private::FImplementedInterfaceParams InterfaceParams[] = {\r\n"));
			for (const FImplementedInterface& Inter : Class->Interfaces)
			{
				check(Inter.Class);
				FString OffsetString;
				if (Inter.PointerOffset)
				{
					OffsetString = FString::Printf(TEXT("(int32)VTABLE_OFFSET(%s, %s)"), ClassNameCPP, NameLookupCPP.GetNameCPP(Inter.Class, true));
				}
				else
				{
					OffsetString = TEXT("0");
				}
				GeneratedClassRegisterFunctionText.Logf(
					TEXT("\t\t\t\t{ %s, %s, %s },\r\n"),
					*GetSingletonName(Inter.Class, false).LeftChop(2),
					*OffsetString,
					Inter.bImplementedByK2 ? TEXT("true") : TEXT("false")
				);
			}
			GeneratedClassRegisterFunctionText.Log(TEXT("\t\t\t};\r\n"));

			InterfaceRange = TEXT("InterfaceParams, ARRAY_COUNT(InterfaceParams)");
		}
		else
		{
			InterfaceRange = TEXT("nullptr, 0");
		}

		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\tstatic const FCppClassTypeInfoStatic StaticCppClassTypeInfo = {\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\tTCppClassTypeTraits<%s>::IsAbstract,\r\n"), NameLookupCPP.GetNameCPP(Class, Class->HasAllClassFlags(CLASS_Interface)));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t};\r\n"));

		GeneratedClassRegisterFunctionText.Log (TEXT("\t\t\tstatic const UE4CodeGen_Private::FClassParams ClassParams = {\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t&%s::StaticClass,\r\n"), ClassNameCPP);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), (Singletons.Len() != 0) ? TEXT("DependentSingletons, ARRAY_COUNT(DependentSingletons)") : TEXT("nullptr, 0"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t0x%08Xu,\r\n"), (uint32)(Class->ClassFlags & CLASS_SaveInCompiledInClasses));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), (FunctionsToExport.Num() != 0) ? (bAllEditorOnlyFunctions ? TEXT("IF_WITH_EDITOR(FuncInfo, nullptr), IF_WITH_EDITOR(ARRAY_COUNT(FuncInfo), 0)") : TEXT("FuncInfo, ARRAY_COUNT(FuncInfo)")) : TEXT("nullptr, 0"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *PropertyRange);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), (Class->ClassConfigName != NAME_None) ? *CreateUTF8LiteralString(Class->ClassConfigName.ToString()) : TEXT("nullptr"));
		GeneratedClassRegisterFunctionText.Log (TEXT("\t\t\t\t&StaticCppClassTypeInfo,\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *InterfaceRange);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s\r\n"), *MetaDataParams);
		GeneratedClassRegisterFunctionText.Log (TEXT("\t\t\t};\r\n"));
		GeneratedClassRegisterFunctionText.Log (TEXT("\t\t\tUE4CodeGen_Private::ConstructUClass(OuterClass, ClassParams);\r\n"));

		if (bIsDynamic)
		{
			FString* CustomDynamicClassInitializationMD = MetaDataMap ? MetaDataMap->Find(TEXT("CustomDynamicClassInitialization")) : nullptr;
			if (CustomDynamicClassInitializationMD)
			{
				GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t\t%s(CastChecked<UDynamicClass>(OuterClass));\n"), *(*CustomDynamicClassInitializationMD));
			}
		}

		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t}\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\treturn OuterClass;\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t}\r\n"));

		Out.Logf(TEXT("%s"), *GeneratedClassRegisterFunctionText);
	}

	if (OutFriendText.Len() && bIsNoExport)
	{
		Out.Logf(TEXT("\t/* friend declarations for pasting into noexport class %s\r\n"), ClassNameCPP);
		Out.Log(OutFriendText);
		Out.Logf(TEXT("\t*/\r\n"));
		OutFriendText.Reset();
	}

	FString SingletonName = GetSingletonName(Class);
	SingletonName.ReplaceInline(TEXT("()"), TEXT(""), ESearchCase::CaseSensitive); // function address

	FString OverriddenClassName = *FNativeClassHeaderGenerator::GetOverriddenName(Class);

	const FString EmptyString = FString();
	const FString& InitSearchableValuesFunctionName = bIsDynamic ? Class->GetMetaData(TEXT("InitializeStaticSearchableValues")) : EmptyString;
	const FString InitSearchableValuesFunctionParam = InitSearchableValuesFunctionName.IsEmpty() ? FString(TEXT("nullptr")) :
		FString::Printf(TEXT("&%s::%s"), ClassNameCPP, *InitSearchableValuesFunctionName);

	// Append base class' CRC at the end of the generated code, this will force update derived classes
	// when base class changes during hot-reload.
	uint32 BaseClassCRC = 0;
	if (Class->GetSuperClass() && !Class->GetSuperClass()->HasAnyClassFlags(CLASS_Intrinsic))
	{
		BaseClassCRC = GGeneratedCodeCRCs.FindChecked(Class->GetSuperClass());
	}
	GeneratedClassRegisterFunctionText.Logf(TEXT("\r\n// %u\r\n"), BaseClassCRC);

	// Calculate generated class initialization code CRC so that we know when it changes after hot-reload
	uint32 ClassCrc = GenerateTextCRC(*GeneratedClassRegisterFunctionText);
	GGeneratedCodeCRCs.Add(Class, ClassCrc);
	// Emit the IMPLEMENT_CLASS macro to go in the generated cpp file.
	if (!bIsDynamic)
	{
		Out.Logf(TEXT("\tIMPLEMENT_CLASS(%s, %u);\r\n"), ClassNameCPP, ClassCrc);
	}
	else
	{
		Out.Logf(TEXT("\tIMPLEMENT_DYNAMIC_CLASS(%s, TEXT(\"%s\"), %u);\r\n"), ClassNameCPP, *OverriddenClassName, ClassCrc);
	}

	Out.Logf(TEXT("\tstatic FCompiledInDefer Z_CompiledInDefer_UClass_%s(%s, &%s::StaticClass, TEXT(\"%s\"), TEXT(\"%s\"), %s, %s, %s, %s);\r\n"),
		ClassNameCPP,
		*SingletonName,
		ClassNameCPP,
		bIsDynamic ? *FClass::GetTypePackageName(Class) : *Class->GetOutermost()->GetName(),
		bIsDynamic ? *OverriddenClassName : ClassNameCPP,
		bIsDynamic ? TEXT("true") : TEXT("false"),
		bIsDynamic ? *AsTEXT(FClass::GetTypePackageName(Class)) : TEXT("nullptr"),
		bIsDynamic ? *AsTEXT(FNativeClassHeaderGenerator::GetOverriddenPathName(Class)) : TEXT("nullptr"),
		*InitSearchableValuesFunctionParam);
}

void FNativeClassHeaderGenerator::ExportFunction(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UFunction* Function, bool bIsNoExport)
{
	UFunction* SuperFunction = Function->GetSuperFunction();

	const bool bIsEditorOnlyFunction = Function->HasAnyFunctionFlags(FUNC_EditorOnly);

	bool bIsDelegate = Function->HasAnyFunctionFlags(FUNC_Delegate);

	const FString SingletonName = GetSingletonName(Function);

	FUHTStringBuilder CurrentFunctionText;

	// Begin wrapping editor only functions.  Note: This should always be the first step!
	if (bIsEditorOnlyFunction)
	{
		CurrentFunctionText.Logf(BeginEditorOnlyGuard);
	}

	CurrentFunctionText.Logf(TEXT("\tUFunction* %s\r\n"), *SingletonName);
	CurrentFunctionText.Logf(TEXT("\t{\r\n"));

	if (bIsNoExport || !(Function->FunctionFlags&FUNC_Event))  // non-events do not export a params struct, so lets do that locally for offset determination
	{
		TArray<UScriptStruct*> Structs = FindNoExportStructs(Function);
		for (UScriptStruct* Struct : Structs)
		{
			ExportMirrorsForNoexportStruct(CurrentFunctionText, Struct, /*Indent=*/ 2);
		}

		ExportEventParm(CurrentFunctionText, ForwardDeclarations, Function, /*Indent=*/ 2, /*bOutputConstructor=*/ false, EExportingState::TypeEraseDelegates);
	}

	UField* FieldOuter = Cast<UField>(Function->GetOuter());
	const bool bIsDynamic = (FieldOuter && FClass::IsDynamic(FieldOuter));

	FString OuterFunc;
	if (UObject* Outer = Function->GetOuter())
	{
		OuterFunc = Outer->IsA<UPackage>() ? GetPackageSingletonName((UPackage*)Outer).LeftChop(2) : GetSingletonNameFuncAddr(Function->GetOwnerClass());
	}
	else
	{
		OuterFunc = TEXT("nullptr");
	}

	if (!bIsDynamic)
	{
		CurrentFunctionText.Logf(TEXT("\t\tstatic UFunction* ReturnFunction = nullptr;\r\n"));
	}
	else
	{
		FString FunctionName = FNativeClassHeaderGenerator::GetOverriddenNameForLiteral(Function);
		CurrentFunctionText.Logf(TEXT("\t\tUObject* Outer = %s();\r\n"), *OuterFunc);
		CurrentFunctionText.Logf(TEXT("\t\tUFunction* ReturnFunction = static_cast<UFunction*>(StaticFindObjectFast( UFunction::StaticClass(), Outer, %s ));\r\n"), *FunctionName);
	}

	CurrentFunctionText.Logf(TEXT("\t\tif (!ReturnFunction)\r\n"));
	CurrentFunctionText.Logf(TEXT("\t\t{\r\n"));

	TArray<UProperty*> Props;
	for (TFieldIterator<UProperty> ItInner(Function, EFieldIteratorFlags::ExcludeSuper); ItInner; ++ItInner)
	{
		Props.Add(*ItInner);
	}

	FString StructureSize;
	if (Props.Num())
	{
		UFunction* TempFunction = Function;
		while (TempFunction->GetSuperFunction())
		{
			TempFunction = TempFunction->GetSuperFunction();
		}
		FString FunctionName = TempFunction->GetName();
		if (TempFunction->HasAnyFunctionFlags(FUNC_Delegate))
		{
			FunctionName = FunctionName.LeftChop(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len());
		}

		StructureSize = FString::Printf(TEXT("sizeof(%s)"), *GetEventStructParamsName(TempFunction->GetOuter(), *FunctionName));
	}
	else
	{
		StructureSize = TEXT("0");
	}

	const TCHAR* UFunctionType = bIsDelegate ? TEXT("UDelegateFunction") : TEXT("UFunction");
	const TCHAR* UFunctionObjectFlags = FClass::IsOwnedByDynamicType(Function) ? TEXT("RF_Public|RF_Transient") : TEXT("RF_Public|RF_Transient|RF_MarkAsNative");

	FString PropertyRange;
	OutputProperties(CurrentFunctionText, PropertyRange, Props, TEXT("\t\t\t"));

	const FFunctionData* CompilerInfo = FFunctionData::FindForFunction(Function);
	const FFuncInfo&     FunctionData = CompilerInfo->GetFunctionData();
	const bool           bIsNet       = !!(FunctionData.FunctionFlags & (FUNC_NetRequest | FUNC_NetResponse));

	FString MetaDataParams = OutputMetaDataCodeForObject(CurrentFunctionText, Function, TEXT("Function_MetaDataParams"), TEXT("\t\t\t"));

	CurrentFunctionText.Logf(
		TEXT("\t\t\tstatic const UE4CodeGen_Private::FFunctionParams FuncParams = { (UObject*(*)())%s, %s, %s, %s, (EFunctionFlags)0x%08X, %s, %s, %d, %d, %s };\r\n"),
		*OuterFunc,
		*CreateUTF8LiteralString(FNativeClassHeaderGenerator::GetOverriddenName(Function)),
		UFunctionObjectFlags,
		*GetSingletonNameFuncAddr(SuperFunction),
		(uint32)Function->FunctionFlags,
		*StructureSize,
		*PropertyRange,
		bIsNet ? FunctionData.RPCId : 0,
		bIsNet ? FunctionData.RPCResponseId : 0,
		*MetaDataParams
	);

	CurrentFunctionText.Log(TEXT("\t\t\tUE4CodeGen_Private::ConstructUFunction(ReturnFunction, FuncParams);\r\n"));
	CurrentFunctionText.Logf(TEXT("\t\t}\r\n"));
	CurrentFunctionText.Logf(TEXT("\t\treturn ReturnFunction;\r\n"));
	CurrentFunctionText.Logf(TEXT("\t}\r\n"));

	// End wrapping editor only functions.  Note: This should always be the last step!
	if (bIsEditorOnlyFunction)
	{
		CurrentFunctionText.Logf(EndEditorOnlyGuard);
	}

	uint32 FunctionCrc = GenerateTextCRC(*CurrentFunctionText);
	GGeneratedCodeCRCs.Add(Function, FunctionCrc);
	Out.Log(CurrentFunctionText);
}

void FNativeClassHeaderGenerator::ExportNatives(FOutputDevice& Out, FClass* Class)
{
	const TCHAR* ClassCPPName = NameLookupCPP.GetNameCPP(Class);
	FString TypeName = Class->HasAnyClassFlags(CLASS_Interface) ? *FString::Printf(TEXT("I%s"), *Class->GetName()) : ClassCPPName;

	Out.Logf(TEXT("\tvoid %s::StaticRegisterNatives%s()\r\n"), ClassCPPName, ClassCPPName);
	Out.Log(TEXT("\t{\r\n"));

	{
		bool bAllEditorOnly = true;

		TArray<TTuple<UFunction*, FString>> NamedFunctionsToExport;
		for (UFunction* Function : TFieldRange<UFunction>(Class, EFieldIteratorFlags::ExcludeSuper))
		{
			if ((Function->FunctionFlags & (FUNC_Native | FUNC_NetRequest)) == FUNC_Native)
			{
				FString OverriddenName = FNativeClassHeaderGenerator::GetUTF8OverriddenNameForLiteral(Function);
				NamedFunctionsToExport.Emplace(Function, MoveTemp(OverriddenName));

				if (!Function->HasAnyFunctionFlags(FUNC_EditorOnly))
				{
					bAllEditorOnly = false;
				}
			}
		}

		Algo::SortBy(NamedFunctionsToExport, [](const TTuple<UFunction*, FString>& Pair){ return Pair.Get<0>()->GetFName(); });

		if (NamedFunctionsToExport.Num() > 0)
		{
			FMacroBlockEmitter EditorOnly(Out, TEXT("WITH_EDITOR"));
			EditorOnly(bAllEditorOnly);

			Out.Logf(TEXT("\t\tUClass* Class = %s::StaticClass();\r\n"), ClassCPPName);
			Out.Log(TEXT("\t\tstatic const FNameNativePtrPair Funcs[] = {\r\n"));

			for (const TTuple<UFunction*, FString>& Func : NamedFunctionsToExport)
			{
				UFunction* Function = Func.Get<0>();

				EditorOnly(Function->HasAnyFunctionFlags(FUNC_EditorOnly));

				Out.Logf(
					TEXT("\t\t\t{ %s, (Native)&%s::exec%s },\r\n"),
					*Func.Get<1>(),
					*TypeName,
					*Function->GetName()
				);
			}

			EditorOnly(bAllEditorOnly);

			Out.Log(TEXT("\t\t};\r\n"));
			Out.Logf(TEXT("\t\tFNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, ARRAY_COUNT(Funcs));\r\n"));
		}
	}

	for (UScriptStruct* Struct : TFieldRange<UScriptStruct>(Class, EFieldIteratorFlags::ExcludeSuper))
	{
		if (Struct->StructFlags & STRUCT_Native)
		{
			Out.Logf( TEXT("\t\tUScriptStruct::DeferCppStructOps(FName(TEXT(\"%s\")),new UScriptStruct::TCppStructOps<%s%s>);\r\n"), *Struct->GetName(), Struct->GetPrefixCPP(), *Struct->GetName() );
		}
	}

	Out.Logf(TEXT("\t}\r\n"));
}

void FNativeClassHeaderGenerator::ExportInterfaceCallFunctions(FOutputDevice& OutCpp, FUHTStringBuilder& Out, const TArray<UFunction*>& CallbackFunctions, const TCHAR* ClassName)
{
	FString APIString = GetAPIString();

	for (UFunction* Function : CallbackFunctions)
	{
		FString FunctionName = Function->GetName();

		auto* CompilerInfo = FFunctionData::FindForFunction(Function);

		const FFuncInfo& FunctionData = CompilerInfo->GetFunctionData();
		const TCHAR* ConstQualifier = FunctionData.FunctionReference->HasAllFunctionFlags(FUNC_Const) ? TEXT("const ") : TEXT("");
		FString ExtraParam = FString::Printf(TEXT("%sUObject* O"), ConstQualifier);

		ExportNativeFunctionHeader(Out, ForwardDeclarations, FunctionData, EExportFunctionType::Interface, EExportFunctionHeaderStyle::Declaration, *ExtraParam, *APIString);
		Out.Logf( TEXT(";") LINE_TERMINATOR );

		FString FunctionNameName = FString::Printf(TEXT("NAME_%s_%s"), NameLookupCPP.GetNameCPP(CastChecked<UStruct>(Function->GetOuter())), *FunctionName);
		OutCpp.Logf(TEXT("\tstatic FName %s = FName(TEXT(\"%s\"));") LINE_TERMINATOR, *FunctionNameName, *GetOverriddenFName(Function).ToString());

		ExportNativeFunctionHeader(OutCpp, ForwardDeclarations, FunctionData, EExportFunctionType::Interface, EExportFunctionHeaderStyle::Definition, *ExtraParam, *APIString);
		OutCpp.Logf( LINE_TERMINATOR TEXT("\t{") LINE_TERMINATOR );

		OutCpp.Logf(TEXT("\t\tcheck(O != NULL);") LINE_TERMINATOR);
		OutCpp.Logf(TEXT("\t\tcheck(O->GetClass()->ImplementsInterface(U%s::StaticClass()));") LINE_TERMINATOR, ClassName);

		auto Parameters = GetFunctionParmsAndReturn(FunctionData.FunctionReference);

		// See if we need to create Parms struct
		const bool bHasParms = Parameters.HasParms();
		if (bHasParms)
		{
			FString EventParmStructName = GetEventStructParamsName(Function->GetOuter(), *FunctionName);
			OutCpp.Logf(TEXT("\t\t%s Parms;") LINE_TERMINATOR, *EventParmStructName);
		}

		OutCpp.Logf(TEXT("\t\tUFunction* const Func = O->FindFunction(%s);") LINE_TERMINATOR, *FunctionNameName);
		OutCpp.Log(TEXT("\t\tif (Func)") LINE_TERMINATOR);
		OutCpp.Log(TEXT("\t\t{") LINE_TERMINATOR);

		// code to populate Parms struct
		for (auto It = Parameters.Parms.CreateConstIterator(); It; ++It)
		{
			UProperty* Param = *It;

			OutCpp.Logf(TEXT("\t\t\tParms.%s=%s;") LINE_TERMINATOR, *Param->GetName(), *Param->GetName());
		}

		const FString ObjectRef = FunctionData.FunctionReference->HasAllFunctionFlags(FUNC_Const) ? FString::Printf(TEXT("const_cast<UObject*>(O)")) : TEXT("O");
		OutCpp.Logf(TEXT("\t\t\t%s->ProcessEvent(Func, %s);") LINE_TERMINATOR, *ObjectRef, bHasParms ? TEXT("&Parms") : TEXT("NULL"));

		for (auto It = Parameters.Parms.CreateConstIterator(); It; ++It)
		{
			UProperty* Param = *It;

			if( Param->HasAllPropertyFlags(CPF_OutParm) && !Param->HasAnyPropertyFlags(CPF_ConstParm|CPF_ReturnParm))
			{
				OutCpp.Logf(TEXT("\t\t\t%s=Parms.%s;") LINE_TERMINATOR, *Param->GetName(), *Param->GetName());
			}
		}

		OutCpp.Log(TEXT("\t\t}") LINE_TERMINATOR);


		// else clause to call back into native if it's a BlueprintNativeEvent
		if (Function->FunctionFlags & FUNC_Native)
		{
			OutCpp.Logf(TEXT("\t\telse if (auto I = (%sI%s*)(O->GetNativeInterfaceAddress(U%s::StaticClass())))") LINE_TERMINATOR, ConstQualifier, ClassName, ClassName);
			OutCpp.Log(TEXT("\t\t{") LINE_TERMINATOR);

			OutCpp.Log(TEXT("\t\t\t"));
			if (Parameters.Return)
			{
				OutCpp.Log(TEXT("Parms.ReturnValue = "));
			}

			OutCpp.Logf(TEXT("I->%s_Implementation("), *FunctionName);

			bool First = true;
			for (auto It = Parameters.Parms.CreateConstIterator(); It; ++It)
			{
				UProperty* Param = *It;

				if (!First)
				{
					OutCpp.Logf(TEXT(","));
				}
				First = false;

				OutCpp.Logf(TEXT("%s"), *Param->GetName());
			}

			OutCpp.Logf(TEXT(");") LINE_TERMINATOR);

			OutCpp.Logf(TEXT("\t\t}") LINE_TERMINATOR);
		}

		if (Parameters.Return)
		{
			OutCpp.Logf(TEXT("\t\treturn Parms.ReturnValue;") LINE_TERMINATOR);
		}

		OutCpp.Logf(TEXT("\t}") LINE_TERMINATOR);
	}
}

/**
 * Gets preprocessor string to emit GENERATED_U*_BODY() macro is deprecated.
 *
 * @param MacroName Name of the macro to be deprecated.
 *
 * @returns Preprocessor string to emit the message.
 */
FString GetGeneratedMacroDeprecationWarning(const TCHAR* MacroName)
{
	// Deprecation warning is disabled right now. After people get familiar with the new macro it should be re-enabled.
	//return FString() + TEXT("EMIT_DEPRECATED_WARNING_MESSAGE(\"") + MacroName + TEXT("() macro is deprecated. Please use GENERATED_BODY() macro instead.\")") LINE_TERMINATOR;
	return TEXT("");
}

/**
 * Returns a string with access specifier that was met before parsing GENERATED_BODY() macro to preserve it.
 *
 * @param Class Class for which to return the access specifier.
 *
 * @returns Access specifier string.
 */
FString GetPreservedAccessSpecifierString(FClass* Class)
{
	FString PreservedAccessSpecifier;
	if (FClassMetaData* Data = GScriptHelper.FindClassData(Class))
	{
		switch (Data->GeneratedBodyMacroAccessSpecifier)
		{
		case EAccessSpecifier::ACCESS_Private:
			PreservedAccessSpecifier = "private:";
			break;
		case EAccessSpecifier::ACCESS_Protected:
			PreservedAccessSpecifier = "protected:";
			break;
		case EAccessSpecifier::ACCESS_Public:
			PreservedAccessSpecifier = "public:";
			break;
		case EAccessSpecifier::ACCESS_NotAnAccessSpecifier :
			PreservedAccessSpecifier = FString::Printf(TEXT("static_assert(false, \"Unknown access specifier for GENERATED_BODY() macro in class %s.\");"), *GetNameSafe(Class));
			break;
		}
	}

	return PreservedAccessSpecifier + LINE_TERMINATOR;
}

void WriteMacro(FOutputDevice& Output, const FString& MacroName, const FString& MacroContent)
{
	Output.Log(*Macroize(*MacroName, *MacroContent));
}

static FString PrivatePropertiesOffsetGetters(const UStruct* Struct, const FString& StructCppName)
{
	check(Struct);

	FUHTStringBuilder Result;
	for (const UProperty* Property : TFieldRange<UProperty>(Struct, EFieldIteratorFlags::ExcludeSuper))
	{
		if (Property && Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected) && !Property->HasAnyPropertyFlags(CPF_EditorOnly))
		{
			const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(Property);
			if (BoolProperty && !BoolProperty->IsNativeBool()) // if it's a bitfield
			{
				continue;
			}

			FString PropertyName = Property->GetName();
			if (Property->HasAllPropertyFlags(CPF_Deprecated))
			{
				PropertyName += TEXT("_DEPRECATED");
			}
			Result.Logf(TEXT("\tFORCEINLINE static uint32 __PPO__%s() { return STRUCT_OFFSET(%s, %s); }") LINE_TERMINATOR,
				*PropertyName, *StructCppName, *PropertyName);
		}
	}

	return Result;
}

void FNativeClassHeaderGenerator::ExportClassFromSourceFileInner(
	FOutputDevice&           OutGeneratedHeaderText,
	FOutputDevice&           OutCpp,
	FOutputDevice&           OutDeclarations,
	FClass*                  Class,
	const FUnrealSourceFile& SourceFile
)
{
	FUHTStringBuilder StandardUObjectConstructorsMacroCall;
	FUHTStringBuilder EnhancedUObjectConstructorsMacroCall;

	FClassMetaData* ClassData = GScriptHelper.FindClassData(Class);
	check(ClassData);

	// C++ -> VM stubs (native function execs)
	FUHTStringBuilder ClassMacroCalls;
	FUHTStringBuilder ClassNoPureDeclsMacroCalls;
	ExportNativeFunctions(OutGeneratedHeaderText, ClassMacroCalls, ClassNoPureDeclsMacroCalls, SourceFile, Class, ClassData);

	// Get Callback functions
	TArray<UFunction*> CallbackFunctions;
	for (UFunction* Function : TFieldRange<UFunction>(Class, EFieldIteratorFlags::ExcludeSuper))
	{
		if ((Function->FunctionFlags & FUNC_Event) && Function->GetSuperFunction() == nullptr)
		{
			CallbackFunctions.Add(Function);
		}
	}

	FUHTStringBuilder PrologMacroCalls;
	if (CallbackFunctions.Num() != 0)
	{
		Algo::SortBy(CallbackFunctions, [](UObject* Obj) { return Obj->GetName(); });

		FUHTStringBuilder UClassMacroContent;

		// export parameters structs for all events and delegates
		for (UFunction* Function : CallbackFunctions)
		{
			ExportEventParm(UClassMacroContent, ForwardDeclarations, Function, /*Indent=*/ 1, /*bOutputConstructor=*/ true, EExportingState::Normal);
		}

		FString MacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_EVENT_PARMS"));
		WriteMacro(OutGeneratedHeaderText, MacroName, UClassMacroContent);
		PrologMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		// VM -> C++ proxies (events and delegates).
		FUHTStringBuilder NullOutput;
		FOutputDevice& CallbackOut = Class->HasAnyClassFlags(CLASS_NoExport) ? NullOutput : OutCpp;
		FString CallbackWrappersMacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_CALLBACK_WRAPPERS"));
		ExportCallbackFunctions(
			OutGeneratedHeaderText,
			CallbackOut,
			ForwardDeclarations,
			CallbackFunctions,
			*CallbackWrappersMacroName,
			(Class->ClassFlags & CLASS_Interface) ? EExportCallbackType::Interface : EExportCallbackType::Class,
			*GetAPIString()
		);

		ClassMacroCalls.Logf(TEXT("\t%s\r\n"), *CallbackWrappersMacroName);
		ClassNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *CallbackWrappersMacroName);
	}

	// Class definition.
	if (!Class->HasAnyClassFlags(CLASS_NoExport))
	{
		ExportNatives(OutCpp, Class);
	}

	FUHTStringBuilder FriendText;
	ExportNativeGeneratedInitCode(OutCpp, OutDeclarations, SourceFile, Class, FriendText);

	FClass* SuperClass = Class->GetSuperClass();

	// the name for the C++ version of the UClass
	const TCHAR* ClassCPPName = NameLookupCPP.GetNameCPP(Class);
	const TCHAR* SuperClassCPPName = (SuperClass != nullptr) ? NameLookupCPP.GetNameCPP(SuperClass) : nullptr;

	FString APIArg = API;
	if (!Class->HasAnyClassFlags(CLASS_MinimalAPI))
	{
		APIArg = TEXT("NO");
	}

	FString PPOMacroName;

	// Replication, add in the declaration for GetLifetimeReplicatedProps() automatically if there are any net flagged properties
	bool bNeedsRep = false;
	for (TFieldIterator<UProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if ((It->PropertyFlags & CPF_Net) != 0)
		{
			bNeedsRep = true;
			break;
		}
	}

	ClassDefinitionRange ClassRange;
	if (ClassDefinitionRange* FoundRange = ClassDefinitionRanges.Find(Class))
	{
		ClassRange = *FoundRange;
		ClassRange.Validate();
	}

	bool bHasGetLifetimeReplicatedProps = HasIdentifierExactMatch(ClassRange.Start, ClassRange.End, TEXT("GetLifetimeReplicatedProps"));

	{
		FUHTStringBuilder Boilerplate;

		// Export the class's native function registration.
		Boilerplate.Logf(TEXT("private:\r\n"));
		Boilerplate.Logf(TEXT("\tstatic void StaticRegisterNatives%s();\r\n"), ClassCPPName);
		Boilerplate.Log(*FriendText);
		Boilerplate.Logf(TEXT("public:\r\n"));

		const bool bCastedClass = Class->HasAnyCastFlag(CASTCLASS_AllFlags) && SuperClass && Class->ClassCastFlags != SuperClass->ClassCastFlags;

		Boilerplate.Logf(TEXT("\tDECLARE_CLASS(%s, %s, COMPILED_IN_FLAGS(%s%s), %s, TEXT(\"%s\"), %s_API)\r\n"),
			ClassCPPName,
			SuperClassCPPName ? SuperClassCPPName : TEXT("None"),
			Class->HasAnyClassFlags(CLASS_Abstract) ? TEXT("CLASS_Abstract") : TEXT("0"),
			*GetClassFlagExportText(Class),
			bCastedClass ? *FString::Printf(TEXT("CASTCLASS_%s"), ClassCPPName) : TEXT("0"),
			*FClass::GetTypePackageName(Class),
			*APIArg);

		Boilerplate.Logf(TEXT("\tDECLARE_SERIALIZER(%s)\r\n"), ClassCPPName);
		Boilerplate.Log(TEXT("\tenum {IsIntrinsic=COMPILED_IN_INTRINSIC};\r\n"));

		if (SuperClass && Class->ClassWithin != SuperClass->ClassWithin)
		{
			Boilerplate.Logf(TEXT("\tDECLARE_WITHIN(%s)\r\n"), NameLookupCPP.GetNameCPP(Class->GetClassWithin()));
		}

		if (Class->HasAnyClassFlags(CLASS_Interface))
		{
			ExportConstructorsMacros(OutGeneratedHeaderText, OutCpp, StandardUObjectConstructorsMacroCall, EnhancedUObjectConstructorsMacroCall, SourceFile.GetGeneratedMacroName(ClassData), Class, *APIArg);

			FString InterfaceMacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_GENERATED_UINTERFACE_BODY"));
			OutGeneratedHeaderText.Log(Macroize(*(InterfaceMacroName + TEXT("()")), *Boilerplate));

			int32 ClassGeneratedBodyLine = ClassData->GetGeneratedBodyLine();

			FString DeprecationWarning = GetGeneratedMacroDeprecationWarning(TEXT("GENERATED_UINTERFACE_BODY"));

			const TCHAR* DeprecationPushString = TEXT("PRAGMA_DISABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR;
			const TCHAR* DeprecationPopString = TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR;
			const TCHAR* Offset = TEXT("\t");

			OutGeneratedHeaderText.Logf(
				TEXT("%s"),
				*Macroize(
					*SourceFile.GetGeneratedBodyMacroName(ClassGeneratedBodyLine, true),
					*(
						FString() +
						Offset + DeprecationWarning +
						Offset + DeprecationPushString +
						Offset + InterfaceMacroName + TEXT("()") LINE_TERMINATOR +
						StandardUObjectConstructorsMacroCall +
						Offset + DeprecationPopString
					)
				)
			);

			OutGeneratedHeaderText.Logf(
				TEXT("%s"),
				*Macroize(
					*SourceFile.GetGeneratedBodyMacroName(ClassGeneratedBodyLine),
					*(
						FString() +
						Offset + DeprecationPushString +
						Offset + InterfaceMacroName + TEXT("()") LINE_TERMINATOR +
						EnhancedUObjectConstructorsMacroCall +
						GetPreservedAccessSpecifierString(Class) +
						Offset + DeprecationPopString
					)
				)
			);

			// =============================================
			// Export the pure interface version of the class

			// the name of the pure interface class
			FString InterfaceCPPName = FString::Printf(TEXT("I%s"), *Class->GetName());
			FString SuperInterfaceCPPName;
			if (SuperClass != NULL)
			{
				SuperInterfaceCPPName = FString::Printf(TEXT("I%s"), *SuperClass->GetName());
			}

			// Thunk functions
			FUHTStringBuilder InterfaceBoilerplate;

			InterfaceBoilerplate.Logf(TEXT("protected:\r\n\tvirtual ~%s() {}\r\npublic:\r\n"), *InterfaceCPPName);
			InterfaceBoilerplate.Logf(TEXT("\ttypedef %s UClassType;\r\n"), ClassCPPName);

			ExportInterfaceCallFunctions(OutCpp, InterfaceBoilerplate, CallbackFunctions, *Class->GetName());

			// we'll need a way to get to the UObject portion of a native interface, so that we can safely pass native interfaces
			// to script VM functions
			if (SuperClass->IsChildOf(UInterface::StaticClass()))
			{
				// Note: This used to be declared as a pure virtual function, but it was changed here in order to allow the Blueprint nativization process
				// to detect C++ interface classes that explicitly declare pure virtual functions via type traits. This code will no longer trigger that check.
				InterfaceBoilerplate.Logf(TEXT("\tvirtual UObject* _getUObject() const { check(0 && \"Missing required implementation.\"); return nullptr; }\r\n"));
			}

			if (bNeedsRep && !bHasGetLifetimeReplicatedProps)
			{
				if (SourceFile.GetGeneratedCodeVersionForStruct(Class) == EGeneratedCodeVersion::V1)
				{
					InterfaceBoilerplate.Logf(TEXT("\tvoid GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;\r\n"));
				}
				else
				{
					FError::Throwf(TEXT("Class %s has Net flagged properties and should declare member function: void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override"), ClassCPPName);
				}
			}

			FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_INCLASS_IINTERFACE_NO_PURE_DECLS"));
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, InterfaceBoilerplate);
			ClassNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);

			FString MacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_INCLASS_IINTERFACE"));
			WriteMacro(OutGeneratedHeaderText, MacroName, InterfaceBoilerplate);
			ClassMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);
		}
		else
		{
			// export the class's config name
			if (SuperClass && Class->ClassConfigName != NAME_None && Class->ClassConfigName != SuperClass->ClassConfigName)
			{
				Boilerplate.Logf(TEXT("\tstatic const TCHAR* StaticConfigName() {return TEXT(\"%s\");}\r\n\r\n"), *Class->ClassConfigName.ToString());
			}

			// export implementation of _getUObject for classes that implement interfaces
			if (Class->Interfaces.Num() > 0)
			{
				Boilerplate.Logf(TEXT("\tvirtual UObject* _getUObject() const override { return const_cast<%s*>(this); }\r\n"), ClassCPPName);
			}

			if (bNeedsRep && !bHasGetLifetimeReplicatedProps)
			{
				// Default version autogenerates declarations.
				if (SourceFile.GetGeneratedCodeVersionForStruct(Class) == EGeneratedCodeVersion::V1)
				{
					Boilerplate.Logf(TEXT("\tvoid GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;\r\n"));
				}
				else
				{
					FError::Throwf(TEXT("Class %s has Net flagged properties and should declare member function: void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override"), ClassCPPName);
				}
			}
			{
				FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_INCLASS_NO_PURE_DECLS"));
				WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, Boilerplate);
				ClassNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);

				FString MacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_INCLASS"));
				WriteMacro(OutGeneratedHeaderText, MacroName, Boilerplate);
				ClassMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

				ExportConstructorsMacros(OutGeneratedHeaderText, OutCpp, StandardUObjectConstructorsMacroCall, EnhancedUObjectConstructorsMacroCall, SourceFile.GetGeneratedMacroName(ClassData), Class, *APIArg);
			}
			{
				const FString PrivatePropertiesOffsets = PrivatePropertiesOffsetGetters(Class, ClassCPPName);
				const FString PPOMacroNameRaw = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_PRIVATE_PROPERTY_OFFSET"));
				PPOMacroName = FString::Printf(TEXT("\t%s\r\n"), *PPOMacroNameRaw);
				WriteMacro(OutGeneratedHeaderText, PPOMacroNameRaw, PrivatePropertiesOffsets);
			}
		}
	}

	{
		FString MacroName = SourceFile.GetGeneratedMacroName(ClassData->GetPrologLine(), TEXT("_PROLOG"));
		WriteMacro(OutGeneratedHeaderText, MacroName, PrologMacroCalls);
	}

	{
		bool bIsIInterface = Class->HasAnyClassFlags(CLASS_Interface);

		auto MacroName = FString::Printf(TEXT("GENERATED_%s_BODY()"), bIsIInterface ? TEXT("IINTERFACE") : TEXT("UCLASS"));

		auto DeprecationWarning = bIsIInterface ? FString(TEXT("")) : GetGeneratedMacroDeprecationWarning(*MacroName);

		auto DeprecationPushString = TEXT("PRAGMA_DISABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR;
		auto DeprecationPopString = TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR;

		auto Public = TEXT("public:" LINE_TERMINATOR);

		auto GeneratedBodyLine = bIsIInterface ? ClassData->GetInterfaceGeneratedBodyLine() : ClassData->GetGeneratedBodyLine();
		auto LegacyGeneratedBody = FString(bIsIInterface ? TEXT("") : PPOMacroName)
			+ ClassMacroCalls
			+ (bIsIInterface ? TEXT("") : StandardUObjectConstructorsMacroCall);
		auto GeneratedBody = FString(bIsIInterface ? TEXT("") : PPOMacroName)
			+ ClassNoPureDeclsMacroCalls
			+ (bIsIInterface ? TEXT("") : EnhancedUObjectConstructorsMacroCall);

		auto WrappedLegacyGeneratedBody = DeprecationWarning + DeprecationPushString + Public + LegacyGeneratedBody + Public + DeprecationPopString;
		auto WrappedGeneratedBody = FString(DeprecationPushString) + Public + GeneratedBody + GetPreservedAccessSpecifierString(Class) + DeprecationPopString;

		auto BodyMacros = Macroize(*SourceFile.GetGeneratedBodyMacroName(GeneratedBodyLine, true), *WrappedLegacyGeneratedBody) +
			Macroize(*SourceFile.GetGeneratedBodyMacroName(GeneratedBodyLine, false), *WrappedGeneratedBody);

		OutGeneratedHeaderText.Log(*BodyMacros);
	}
}

/**
* Generates private copy-constructor declaration.
*
* @param Out Output device to generate to.
* @param Class Class to generate constructor for.
* @param API API string for this constructor.
*/
void ExportCopyConstructorDefinition(FOutputDevice& Out, const TCHAR* API, const TCHAR* ClassCPPName)
{
	Out.Logf(TEXT("private:\r\n"));
	Out.Logf(TEXT("\t/** Private move- and copy-constructors, should never be used */\r\n"));
	Out.Logf(TEXT("\t%s_API %s(%s&&);\r\n"), API, ClassCPPName, ClassCPPName);
	Out.Logf(TEXT("\t%s_API %s(const %s&);\r\n"), API, ClassCPPName, ClassCPPName);
	Out.Logf(TEXT("public:\r\n"));
}

/**
 * Generates vtable helper caller and eventual constructor body.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate for.
 * @param API API string.
 */
void ExportVTableHelperCtorAndCaller(FOutputDevice& Out, FClassMetaData* ClassData, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!ClassData->bCustomVTableHelperConstructorDeclared)
	{
		Out.Logf(TEXT("\tDECLARE_VTABLE_PTR_HELPER_CTOR(%s_API, %s);" LINE_TERMINATOR), API, ClassCPPName);
	}
	Out.Logf(TEXT("DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(%s);" LINE_TERMINATOR), ClassCPPName);
}

/**
 * Generates standard constructor declaration.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor for.
 * @param API API string for this constructor.
 */
void ExportStandardConstructorsMacro(FOutputDevice& Out, FClass* Class, FClassMetaData* ClassData, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!Class->HasAnyClassFlags(CLASS_CustomConstructor))
	{
		Out.Logf(TEXT("\t/** Standard constructor, called after all reflected properties have been initialized */\r\n"));
		Out.Logf(TEXT("\t%s_API %s(const FObjectInitializer& ObjectInitializer%s);\r\n"), API, ClassCPPName,
			ClassData->bDefaultConstructorDeclared ? TEXT("") : TEXT(" = FObjectInitializer::Get()"));
	}
	Out.Logf(TEXT("\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);

	ExportVTableHelperCtorAndCaller(Out, ClassData, API, ClassCPPName);
	ExportCopyConstructorDefinition(Out, API, ClassCPPName);
}

/**
 * Generates constructor definition.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor for.
 * @param API API string for this constructor.
 */
void ExportConstructorDefinition(FOutputDevice& Out, FClass* Class, FClassMetaData* ClassData, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!ClassData->bConstructorDeclared)
	{
		Out.Logf(TEXT("\t/** Standard constructor, called after all reflected properties have been initialized */\r\n"));

		// Assume super class has OI constructor, this may not always be true but we should always be able to check this.
		// In any case, it will default to old behaviour before we even checked this.
		bool bSuperClassObjectInitializerConstructorDeclared = true;
		FClass* SuperClass = Class->GetSuperClass();
		if (SuperClass != nullptr)
		{
			FClassMetaData* SuperClassData = GScriptHelper.FindClassData(SuperClass);
			if (SuperClassData)
			{
				bSuperClassObjectInitializerConstructorDeclared = SuperClassData->bObjectInitializerConstructorDeclared;
			}
		}
		if (bSuperClassObjectInitializerConstructorDeclared)
		{
			Out.Logf(TEXT("\t%s_API %s(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { };\r\n"), API, ClassCPPName);
			ClassData->bObjectInitializerConstructorDeclared = true;
		}
		else
		{
			Out.Logf(TEXT("\t%s_API %s() { };\r\n"), API, ClassCPPName);
			ClassData->bDefaultConstructorDeclared = true;
		}

		ClassData->bConstructorDeclared = true;
	}
	ExportCopyConstructorDefinition(Out, API, ClassCPPName);
}

/**
 * Generates constructor call definition.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor call definition for.
 */
void ExportDefaultConstructorCallDefinition(FOutputDevice& Out, FClassMetaData* ClassData, const TCHAR* ClassCPPName)
{
	if (ClassData->bObjectInitializerConstructorDeclared)
	{
		Out.Logf(TEXT("\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
	}
	else if (ClassData->bDefaultConstructorDeclared)
	{
		Out.Logf(TEXT("\tDEFINE_DEFAULT_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
	}
	else
	{
		Out.Logf(TEXT("\tDEFINE_FORBIDDEN_DEFAULT_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
	}
}

/**
 * Generates enhanced constructor declaration.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor for.
 * @param API API string for this constructor.
 */
void ExportEnhancedConstructorsMacro(FOutputDevice& Out, FClass* Class, FClassMetaData* ClassData, const TCHAR* API, const TCHAR* ClassCPPName)
{
	ExportConstructorDefinition(Out, Class, ClassData, API, ClassCPPName);
	ExportVTableHelperCtorAndCaller(Out, ClassData, API, ClassCPPName);
	ExportDefaultConstructorCallDefinition(Out, ClassData, ClassCPPName);
}

/**
 * Gets a package relative inclusion path of the given source file for build.
 *
 * @param SourceFile Given source file.
 *
 * @returns Inclusion path.
 */
FString GetBuildPath(FUnrealSourceFile& SourceFile)
{
	FString Out = SourceFile.GetFilename();

	ConvertToBuildIncludePath(SourceFile.GetPackage(), Out);

	return Out;
}

void FNativeClassHeaderGenerator::ExportConstructorsMacros(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& Out, FOutputDevice& StandardUObjectConstructorsMacroCall, FOutputDevice& EnhancedUObjectConstructorsMacroCall, const FString& ConstructorsMacroPrefix, FClass* Class, const TCHAR* APIArg)
{
	const TCHAR* ClassCPPName = NameLookupCPP.GetNameCPP(Class);

	FClassMetaData* ClassData = GScriptHelper.FindClassData(Class);
	check(ClassData);

	FUHTStringBuilder StdMacro;
	FUHTStringBuilder EnhMacro;
	FString StdMacroName = ConstructorsMacroPrefix + TEXT("_STANDARD_CONSTRUCTORS");
	FString EnhMacroName = ConstructorsMacroPrefix + TEXT("_ENHANCED_CONSTRUCTORS");

	ExportStandardConstructorsMacro(StdMacro, Class, ClassData, APIArg, ClassCPPName);
	ExportEnhancedConstructorsMacro(EnhMacro, Class, ClassData, APIArg, ClassCPPName);

	if (!ClassData->bCustomVTableHelperConstructorDeclared)
	{
		Out.Logf(TEXT("\tDEFINE_VTABLE_PTR_HELPER_CTOR(%s);" LINE_TERMINATOR), ClassCPPName);
	}

	OutGeneratedHeaderText.Log(*Macroize(*StdMacroName, *StdMacro));
	OutGeneratedHeaderText.Log(*Macroize(*EnhMacroName, *EnhMacro));

	StandardUObjectConstructorsMacroCall.Logf(TEXT("\t%s\r\n"), *StdMacroName);
	EnhancedUObjectConstructorsMacroCall.Logf(TEXT("\t%s\r\n"), *EnhMacroName);
}

bool FNativeClassHeaderGenerator::WriteHeader(const TCHAR* Path, const FString& InBodyText, const TSet<FString>& InFwdDecl)
{
	FUHTStringBuilder GeneratedHeaderTextWithCopyright;
	GeneratedHeaderTextWithCopyright.Logf(TEXT("%s"), HeaderCopyright);
	GeneratedHeaderTextWithCopyright.Log(TEXT("#include \"ObjectMacros.h\"\r\n"));
	GeneratedHeaderTextWithCopyright.Log(TEXT("#include \"ScriptMacros.h\"\r\n"));
	GeneratedHeaderTextWithCopyright.Log(LINE_TERMINATOR);
	GeneratedHeaderTextWithCopyright.Log(TEXT("PRAGMA_DISABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR);

	for (const FString& FWDecl : InFwdDecl)
	{
		if (FWDecl.Len() > 0)
		{
			GeneratedHeaderTextWithCopyright.Logf(TEXT("%s\r\n"), *FWDecl);
		}
	}

	GeneratedHeaderTextWithCopyright.Log(*InBodyText);
	GeneratedHeaderTextWithCopyright.Log(TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR);

	bool bHasChanged = SaveHeaderIfChanged(Path, *GeneratedHeaderTextWithCopyright);
	return bHasChanged;
}

/**
 * Returns a string in the format CLASS_Something|CLASS_Something which represents all class flags that are set for the specified
 * class which need to be exported as part of the DECLARE_CLASS macro
 */
FString FNativeClassHeaderGenerator::GetClassFlagExportText( UClass* Class )
{
	FString StaticClassFlagText;

	check(Class);
	if ( Class->HasAnyClassFlags(CLASS_Transient) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Transient");
	}
	if( Class->HasAnyClassFlags(CLASS_DefaultConfig) )
	{
		StaticClassFlagText += TEXT(" | CLASS_DefaultConfig");
	}
	if( Class->HasAnyClassFlags(CLASS_GlobalUserConfig) )
	{
		StaticClassFlagText += TEXT(" | CLASS_GlobalUserConfig");
	}
	if( Class->HasAnyClassFlags(CLASS_Config) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Config");
	}
	if ( Class->HasAnyClassFlags(CLASS_Interface) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Interface");
	}
	if ( Class->HasAnyClassFlags(CLASS_Deprecated) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Deprecated");
	}

	return StaticClassFlagText;
}

/**
* Exports the header text for the list of enums specified
*
* @param	Enums	the enums to export
*/
void FNativeClassHeaderGenerator::ExportEnum(FOutputDevice& Out, UEnum* Enum)
{
	// Export FOREACH macro
	Out.Logf( TEXT("#define FOREACH_ENUM_%s(op) "), *Enum->GetName().ToUpper() );
	for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
	{
		const FString QualifiedEnumValue = Enum->GetNameByIndex(i).ToString();
		Out.Logf( TEXT("\\\r\n\top(%s) "), *QualifiedEnumValue );
	}
	Out.Logf( TEXT("\r\n") );
}

// Exports the header text for the list of structs specified (GENERATED_BODY impls)
void FNativeClassHeaderGenerator::ExportGeneratedStructBodyMacros(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UScriptStruct* Struct)
{
	const bool bIsDynamic = FClass::IsDynamic(Struct);
	const FString ActualStructName = FNativeClassHeaderGenerator::GetOverriddenName(Struct);
	const FString FriendApiString  = GetAPIString();

	UStruct* BaseStruct = Struct->GetSuperStruct();

	const TCHAR* StructNameCPP = NameLookupCPP.GetNameCPP(Struct);

	// Export struct.
	if (Struct->StructFlags & STRUCT_Native)
	{
		check(Struct->StructMacroDeclaredLineNumber != INDEX_NONE);

		const FString StaticConstructionString = GetSingletonName(Struct);

		FString RequiredAPI;
		if (!(Struct->StructFlags & STRUCT_RequiredAPI))
		{
			RequiredAPI = FriendApiString;
		}

		const FString FriendLine = FString::Printf(TEXT("\tfriend %sclass UScriptStruct* %s;\r\n"), *FriendApiString, *StaticConstructionString);
		const FString StaticClassLine = FString::Printf(TEXT("\t%sstatic class UScriptStruct* StaticStruct();\r\n"), *RequiredAPI);
		const FString PrivatePropertiesOffset = PrivatePropertiesOffsetGetters(Struct, StructNameCPP);
		const FString SuperTypedef = BaseStruct ? FString::Printf(TEXT("\ttypedef %s Super;\r\n"), NameLookupCPP.GetNameCPP(BaseStruct)) : FString();

		const FString CombinedLine = FriendLine + StaticClassLine + PrivatePropertiesOffset + SuperTypedef;
		const FString MacroName = SourceFile.GetGeneratedBodyMacroName(Struct->StructMacroDeclaredLineNumber);

		const FString Macroized = Macroize(*MacroName, *CombinedLine);
		OutGeneratedHeaderText.Log(*Macroized);

		FString SingletonName = StaticConstructionString.Replace(TEXT("()"), TEXT(""), ESearchCase::CaseSensitive); // function address
		FString GetCRCName = FString::Printf(TEXT("Get_%s_CRC"), *SingletonName);

		Out.Logf(TEXT("class UScriptStruct* %s::StaticStruct()\r\n"), StructNameCPP);
		Out.Logf(TEXT("{\r\n"));

		// UStructs can have UClass or UPackage outer (if declared in non-UClass headers).
		FString OuterName;
		if (!bIsDynamic)
		{
			OuterName = GetPackageSingletonName(CastChecked<UPackage>(Struct->GetOuter()));
			Out.Logf(TEXT("\tstatic class UScriptStruct* Singleton = NULL;\r\n"));
		}
		else
		{
			OuterName = TEXT("StructPackage");
			Out.Logf(TEXT("\tclass UPackage* %s = FindOrConstructDynamicTypePackage(TEXT(\"%s\"));\r\n"), *OuterName, *FClass::GetTypePackageName(Struct));
			Out.Logf(TEXT("\tclass UScriptStruct* Singleton = Cast<UScriptStruct>(StaticFindObjectFast(UScriptStruct::StaticClass(), %s, TEXT(\"%s\")));\r\n"), *OuterName, *ActualStructName);
		}

		Out.Logf(TEXT("\tif (!Singleton)\r\n"));
		Out.Logf(TEXT("\t{\r\n"));
		Out.Logf(TEXT("\t\textern %suint32 %s();\r\n"), *FriendApiString, *GetCRCName);

		Out.Logf(TEXT("\t\tSingleton = GetStaticStruct(%s, %s, TEXT(\"%s\"), sizeof(%s), %s());\r\n"),
			*SingletonName, *OuterName, *ActualStructName, StructNameCPP, *GetCRCName);

		Out.Logf(TEXT("\t}\r\n"));
		Out.Logf(TEXT("\treturn Singleton;\r\n"));
		Out.Logf(TEXT("}\r\n"));

		Out.Logf(TEXT("static FCompiledInDeferStruct Z_CompiledInDeferStruct_UScriptStruct_%s(%s::StaticStruct, TEXT(\"%s\"), TEXT(\"%s\"), %s, %s, %s);\r\n"),
			StructNameCPP,
			StructNameCPP,
			bIsDynamic ? *FClass::GetTypePackageName(Struct) : *Struct->GetOutermost()->GetName(),
			*ActualStructName,
			bIsDynamic ? TEXT("true") : TEXT("false"),
			bIsDynamic ? *AsTEXT(FClass::GetTypePackageName(Struct)) : TEXT("nullptr"),
			bIsDynamic ? *AsTEXT(FNativeClassHeaderGenerator::GetOverriddenPathName(Struct)) : TEXT("nullptr"));

		// Generate StaticRegisterNatives equivalent for structs without classes.
		if (!Struct->GetOuter()->IsA(UStruct::StaticClass()))
		{
			const FString ShortPackageName = FPackageName::GetShortName(Struct->GetOuter()->GetName());
			Out.Logf(TEXT("static struct FScriptStruct_%s_StaticRegisterNatives%s\r\n"), *ShortPackageName, StructNameCPP);
			Out.Logf(TEXT("{\r\n"));
			Out.Logf(TEXT("\tFScriptStruct_%s_StaticRegisterNatives%s()\r\n"), *ShortPackageName, StructNameCPP);
			Out.Logf(TEXT("\t{\r\n"));

			Out.Logf(TEXT("\t\tUScriptStruct::DeferCppStructOps(FName(TEXT(\"%s\")),new UScriptStruct::TCppStructOps<%s>);\r\n"), *ActualStructName, StructNameCPP);

			Out.Logf(TEXT("\t}\r\n"));
			Out.Logf(TEXT("} ScriptStruct_%s_StaticRegisterNatives%s;\r\n"), *ShortPackageName, StructNameCPP);
		}
	}

	const FString SingletonName = GetSingletonName(Struct);

	FUHTStringBuilder GeneratedStructRegisterFunctionText;

	GeneratedStructRegisterFunctionText.Logf(TEXT("\tUScriptStruct* %s\r\n"), *SingletonName);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t{\r\n"));

	// if this is a no export struct, we will put a local struct here for offset determination
	TArray<UScriptStruct*> Structs = FindNoExportStructs(Struct);
	for (UScriptStruct* NoExportStruct : Structs)
	{
		ExportMirrorsForNoexportStruct(GeneratedStructRegisterFunctionText, NoExportStruct, /*Indent=*/ 2);
	}

	FString CRCFuncName = FString::Printf(TEXT("Get_%s_CRC"), *SingletonName.Replace(TEXT("()"), TEXT(""), ESearchCase::CaseSensitive));
	// Structs can either have a UClass or UPackage as outer (if declared in non-UClass header).
	if (!bIsDynamic)
	{
		GeneratedStructRegisterFunctionText.Logf(TEXT("#if WITH_HOT_RELOAD\r\n"));
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\textern uint32 %s();\r\n"), *CRCFuncName);
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tUPackage* Outer = %s;\r\n"), *GetPackageSingletonName(CastChecked<UPackage>(Struct->GetOuter())));
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tstatic UScriptStruct* ReturnStruct = FindExistingStructIfHotReloadOrDynamic(Outer, TEXT(\"%s\"), sizeof(%s), %s(), false);\r\n"), *ActualStructName, StructNameCPP, *CRCFuncName);
		GeneratedStructRegisterFunctionText.Logf(TEXT("#else\r\n"));
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tstatic UScriptStruct* ReturnStruct = nullptr;\r\n"));
		GeneratedStructRegisterFunctionText.Logf(TEXT("#endif\r\n"));
	}
	else
	{
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\textern uint32 %s();\r\n"), *CRCFuncName);
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tUPackage* Outer = FindOrConstructDynamicTypePackage(TEXT(\"%s\"));\r\n"), *FClass::GetTypePackageName(Struct));
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tUScriptStruct* ReturnStruct = FindExistingStructIfHotReloadOrDynamic(Outer, TEXT(\"%s\"), sizeof(%s), %s(), true);\r\n"), *ActualStructName, StructNameCPP, *CRCFuncName);
	}
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tif (!ReturnStruct)\r\n"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t{\r\n"));

	FString BaseStructString;
	if (BaseStruct)
	{
		CastChecked<UScriptStruct>(BaseStruct); // this better actually be a script struct
		BaseStructString = GetSingletonName(BaseStruct);
	}
	else
	{
		BaseStructString = TEXT("nullptr");
	}

	FString CppStructOpsString;
	if (Struct->StructFlags & STRUCT_Native)
	{
		//@todo .u we don't need the auto register versions of these (except for hotreload, which should be fixed)
		CppStructOpsString = FString::Printf(TEXT("new UScriptStruct::TCppStructOps<%s>"), StructNameCPP);
	}
	else
	{
		CppStructOpsString = TEXT("nullptr");
	}

	EStructFlags UncomputedFlags = (EStructFlags)(Struct->StructFlags & ~STRUCT_ComputedFlags);

	FString OuterFunc;
	if (!bIsDynamic)
	{
		OuterFunc = GetPackageSingletonName(CastChecked<UPackage>(Struct->GetOuter())).LeftChop(2);
	}
	else
	{
		OuterFunc = FString::Printf(TEXT("[](){ return (UObject*)FindOrConstructDynamicTypePackage(TEXT(\"%s\")); }"), *FClass::GetTypePackageName(Struct));
	}

	FString MetaDataParams = OutputMetaDataCodeForObject(GeneratedStructRegisterFunctionText, Struct, TEXT("Struct_MetaDataParams"), TEXT("\t\t\t"));

	TArray<UProperty*> Props;
	for (UProperty* Prop : TFieldRange<UProperty>(Struct, EFieldIteratorFlags::ExcludeSuper))
	{
		Props.Add(Prop);
	}

	FString NewStructOps;
	if (Struct->StructFlags & STRUCT_Native)
	{
		GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\tauto NewStructOpsLambda = []() -> void* { return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<%s>(); };\r\n"), StructNameCPP);
		NewStructOps = TEXT("&UE4CodeGen_Private::TNewCppStructOpsWrapper<decltype(NewStructOpsLambda)>::NewCppStructOps");
	}
	else
	{
		NewStructOps = TEXT("nullptr");
	}

	FString PropertyRange;
	OutputProperties(GeneratedStructRegisterFunctionText, PropertyRange, Props, TEXT("\t\t\t"));

	GeneratedStructRegisterFunctionText.Log (TEXT("\t\t\tstatic const UE4CodeGen_Private::FStructParams ReturnStructParams = {\r\n"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t(UObject* (*)())%s,\r\n"), *OuterFunc);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *GetSingletonNameFuncAddr(BaseStruct));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *NewStructOps);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *CreateUTF8LiteralString(ActualStructName));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), bIsDynamic ? TEXT("RF_Public|RF_Transient") : TEXT("RF_Public|RF_Transient|RF_MarkAsNative"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\tEStructFlags(0x%08X),\r\n"), (uint32)UncomputedFlags);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\tsizeof(%s),\r\n"), StructNameCPP);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\talignof(%s),\r\n"), StructNameCPP);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *PropertyRange);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\t\t%s\r\n"), *MetaDataParams);
	GeneratedStructRegisterFunctionText.Log (TEXT("\t\t\t};\r\n"));
	GeneratedStructRegisterFunctionText.Log (TEXT("\t\t\tUE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, ReturnStructParams);\r\n"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t}\r\n"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\treturn ReturnStruct;\r\n"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t}\r\n"));

	uint32 StructCrc = GenerateTextCRC(*GeneratedStructRegisterFunctionText);
	GGeneratedCodeCRCs.Add(Struct, StructCrc);

	Out.Log(GeneratedStructRegisterFunctionText);
	Out.Logf(TEXT("\tuint32 %s() { return %uU; }\r\n"), *CRCFuncName, StructCrc);
}

void FNativeClassHeaderGenerator::ExportGeneratedEnumInitCode(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UEnum* Enum)
{
	const bool    bIsDynamic            = FClass::IsDynamic(Enum);
	const FString SingletonName         = GetSingletonNameFuncAddr(Enum);
	const FString EnumNameCpp           = Enum->GetName(); //UserDefinedEnum should already have a valid cpp name.
	const FString OverriddenEnumNameCpp = FNativeClassHeaderGenerator::GetOverriddenName(Enum);

	const bool bIsEditorOnlyDataType = GEditorOnlyDataTypes.Contains(Enum);

	FMacroBlockEmitter EditorOnlyData(Out, TEXT("WITH_EDITORONLY_DATA"));
	EditorOnlyData(bIsEditorOnlyDataType);

	FString PackageSingletonName;
	if (!bIsDynamic)
	{
		PackageSingletonName = GetPackageSingletonName(CastChecked<UPackage>(Enum->GetOuter()));
	}
	else
	{
		PackageSingletonName = FClass::GetTypePackageName(Enum);
	}

	Out.Logf(TEXT("\tstatic UEnum* %s_StaticEnum()\r\n"), *Enum->GetName());
	Out.Logf(TEXT("\t{\r\n"));

	if (!bIsDynamic)
	{
		Out.Logf(TEXT("\t\tstatic UEnum* Singleton = nullptr;\r\n"));
	}
	else
	{
		Out.Logf(TEXT("\t\tclass UPackage* EnumPackage = FindOrConstructDynamicTypePackage(TEXT(\"%s\"));\r\n"), *PackageSingletonName);
		Out.Logf(TEXT("\t\tclass UEnum* Singleton = Cast<UEnum>(StaticFindObjectFast(UEnum::StaticClass(), EnumPackage, TEXT(\"%s\")));\r\n"), *OverriddenEnumNameCpp);
	}
	Out.Logf(TEXT("\t\tif (!Singleton)\r\n"));
	Out.Logf(TEXT("\t\t{\r\n"));
	if (!bIsDynamic)
	{
		Out.Logf(TEXT("\t\t\tSingleton = GetStaticEnum(%s, %s, TEXT(\"%s\"));\r\n"), *SingletonName, *PackageSingletonName, *Enum->GetName());
	}
	else
	{
		Out.Logf(TEXT("\t\t\tSingleton = GetStaticEnum(%s, EnumPackage, TEXT(\"%s\"));\r\n"), *SingletonName, *OverriddenEnumNameCpp);
	}

	Out.Logf(TEXT("\t\t}\r\n"));
	Out.Logf(TEXT("\t\treturn Singleton;\r\n"));
	Out.Logf(TEXT("\t}\r\n"));

	Out.Logf(
		TEXT("\tstatic FCompiledInDeferEnum Z_CompiledInDeferEnum_UEnum_%s(%s_StaticEnum, TEXT(\"%s\"), TEXT(\"%s\"), %s, %s, %s);\r\n"),
		*EnumNameCpp,
		*EnumNameCpp,
		bIsDynamic ? *FClass::GetTypePackageName(Enum) : *Enum->GetOutermost()->GetName(),
		*OverriddenEnumNameCpp,
		bIsDynamic ? TEXT("true") : TEXT("false"),
		bIsDynamic ? *AsTEXT(FClass::GetTypePackageName(Enum)) : TEXT("nullptr"),
		bIsDynamic ? *AsTEXT(FNativeClassHeaderGenerator::GetOverriddenPathName(Enum)) : TEXT("nullptr")
	);

	const FString EnumSingletonName = GetSingletonName(Enum);
	const FString CRCFuncName       = FString::Printf(TEXT("Get_%s_CRC"), *SingletonName);

	FUHTStringBuilder GeneratedEnumRegisterFunctionText;

	GeneratedEnumRegisterFunctionText.Logf(TEXT("\tUEnum* %s\r\n"), *EnumSingletonName);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t{\r\n"));

	// Enums can either have a UClass or UPackage as outer (if declared in non-UClass header).
	FString OuterString;
	if (!bIsDynamic)
	{
		OuterString = PackageSingletonName;
		GeneratedEnumRegisterFunctionText.Logf(TEXT("#if WITH_HOT_RELOAD\r\n"));
		GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tUPackage* Outer = %s;\r\n"), *OuterString);
		GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tstatic UEnum* ReturnEnum = FindExistingEnumIfHotReloadOrDynamic(Outer, TEXT(\"%s\"), 0, %s(), false);\r\n"), *EnumNameCpp, *CRCFuncName);
		GeneratedEnumRegisterFunctionText.Logf(TEXT("#else\r\n"));
		GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tstatic UEnum* ReturnEnum = nullptr;\r\n"));
		GeneratedEnumRegisterFunctionText.Logf(TEXT("#endif // WITH_HOT_RELOAD\r\n"));
	}
	else
	{
		OuterString = FString::Printf(TEXT("[](){ return (UObject*)FindOrConstructDynamicTypePackage(TEXT(\"%s\")); }()"), *PackageSingletonName);
		GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tUPackage* Outer = FindOrConstructDynamicTypePackage(TEXT(\"%s\"));"), *PackageSingletonName);
		GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tUEnum* ReturnEnum = FindExistingEnumIfHotReloadOrDynamic(Outer, TEXT(\"%s\"), 0, %s(), true);\r\n"), *OverriddenEnumNameCpp, *CRCFuncName);
	}
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tif (!ReturnEnum)\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t{\r\n"));

	const TCHAR* UEnumObjectFlags = bIsDynamic ? TEXT("RF_Public|RF_Transient") : TEXT("RF_Public|RF_Transient|RF_MarkAsNative");

	FString EnumFormStr;
	switch (Enum->GetCppForm())
	{
		case UEnum::ECppForm::Regular:    EnumFormStr = TEXT("UEnum::ECppForm::Regular");    break;
		case UEnum::ECppForm::Namespaced: EnumFormStr = TEXT("UEnum::ECppForm::Namespaced"); break;
		case UEnum::ECppForm::EnumClass:  EnumFormStr = TEXT("UEnum::ECppForm::EnumClass");  break;
	}

	const FString& EnumDisplayNameFn = Enum->GetMetaData(TEXT("EnumDisplayNameFn"));

	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\tstatic const UE4CodeGen_Private::FEnumeratorParam Enumerators[] = {\r\n"));
	for (int32 Index = 0; Index != Enum->NumEnums(); ++Index)
	{
		const TCHAR* OverridenNameMetaDatakey = TEXT("OverrideName");
		const FString KeyName = Enum->HasMetaData(OverridenNameMetaDatakey, Index) ? Enum->GetMetaData(OverridenNameMetaDatakey, Index) : Enum->GetNameByIndex(Index).ToString();
		GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t{ %s, (int64)%s },\r\n"), *CreateUTF8LiteralString(KeyName), *Enum->GetNameByIndex(Index).ToString());
	}
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t};\r\n"));

	FString MetaDataParams = OutputMetaDataCodeForObject(GeneratedEnumRegisterFunctionText, Enum, TEXT("Enum_MetaDataParams"), TEXT("\t\t\t"));

	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\tstatic const UE4CodeGen_Private::FEnumParams EnumParams = {\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t(UObject*(*)())%s,\r\n"), *OuterString.LeftChop(2));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\tUE4CodeGen_Private::EDynamicType::%s,\r\n"), bIsDynamic ? TEXT("Dynamic") : TEXT("NotDynamic"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *CreateUTF8LiteralString(OverriddenEnumNameCpp));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), UEnumObjectFlags);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), EnumDisplayNameFn.IsEmpty() ? TEXT("nullptr") : *EnumDisplayNameFn);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t(uint8)%s,\r\n"), *EnumFormStr);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t%s,\r\n"), *CreateUTF8LiteralString(Enum->CppType));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\tEnumerators,\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\tARRAY_COUNT(Enumerators),\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t\t%s\r\n"), *MetaDataParams);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\t};\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\tUE4CodeGen_Private::ConstructUEnum(ReturnEnum, EnumParams);\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t}\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\treturn ReturnEnum;\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t}\r\n"));

	uint32 EnumCrc = GenerateTextCRC(*GeneratedEnumRegisterFunctionText);
	GGeneratedCodeCRCs.Add(Enum, EnumCrc);
	Out.Logf(TEXT("\tuint32 %s() { return %uU; }\r\n"), *CRCFuncName, EnumCrc);
	Out.Log(GeneratedEnumRegisterFunctionText);
}

void FNativeClassHeaderGenerator::ExportMirrorsForNoexportStruct(FOutputDevice& Out, UScriptStruct* Struct, int32 TextIndent)
{
	// Export struct.
	const TCHAR* StructName = NameLookupCPP.GetNameCPP(Struct);
	Out.Logf(TEXT("%sstruct %s"), FCString::Tab(TextIndent), StructName);
	if (Struct->GetSuperStruct() != NULL)
	{
		Out.Logf(TEXT(" : public %s"), NameLookupCPP.GetNameCPP(Struct->GetSuperStruct()));
	}
	Out.Logf(TEXT("\r\n%s{\r\n"), FCString::Tab(TextIndent));

	// Export the struct's CPP properties.
	ExportProperties(Out, Struct, TextIndent);

	Out.Logf(TEXT("%s};\r\n\r\n"), FCString::Tab(TextIndent));
}

bool FNativeClassHeaderGenerator::WillExportEventParms( UFunction* Function )
{
  TFieldIterator<UProperty> It(Function);
  return It && (It->PropertyFlags&CPF_Parm);
}

void WriteEventFunctionPrologue(FOutputDevice& Output, int32 Indent, const FParmsAndReturnProperties& Parameters, UObject* FunctionOuter, const TCHAR* FunctionName)
{
	// now the body - first we need to declare a struct which will hold the parameters for the event/delegate call
	Output.Logf(TEXT("\r\n%s{\r\n"), FCString::Tab(Indent));

	// declare and zero-initialize the parameters and return value, if applicable
	if (!Parameters.HasParms())
		return;

	FString EventStructName = GetEventStructParamsName(FunctionOuter, FunctionName);

	Output.Logf(TEXT("%s%s Parms;\r\n"), FCString::Tab(Indent + 1), *EventStructName );

	// Declare a parameter struct for this event/delegate and assign the struct members using the values passed into the event/delegate call.
	for (auto It = Parameters.Parms.CreateConstIterator(); It; ++It)
	{
		UProperty* Prop = *It;

		const FString PropertyName = Prop->GetName();
		if (Prop->ArrayDim > 1)
		{
			Output.Logf(TEXT("%sFMemory::Memcpy(Parms.%s,%s,sizeof(Parms.%s));\r\n"), FCString::Tab(Indent + 1), *PropertyName, *PropertyName, *PropertyName);
		}
		else
		{
			FString ValueAssignmentText = PropertyName;
			if (Prop->IsA<UBoolProperty>())
			{
				ValueAssignmentText += TEXT(" ? true : false");
			}

			Output.Logf(TEXT("%sParms.%s=%s;\r\n"), FCString::Tab(Indent + 1), *PropertyName, *ValueAssignmentText);
		}
	}
}

void WriteEventFunctionEpilogue(FOutputDevice& Output, int32 Indent, const FParmsAndReturnProperties& Parameters)
{
	// Out parm copying.
	for (auto It = Parameters.Parms.CreateConstIterator(); It; ++It)
	{
		UProperty* Prop = *It;

		if ((Prop->PropertyFlags & (CPF_OutParm | CPF_ConstParm)) == CPF_OutParm)
		{
			const FString PropertyName = Prop->GetName();
			if ( Prop->ArrayDim > 1 )
			{
				Output.Logf(TEXT("%sFMemory::Memcpy(&%s,&Parms.%s,sizeof(%s));\r\n"), FCString::Tab(Indent + 1), *PropertyName, *PropertyName, *PropertyName);
			}
			else
			{
				Output.Logf(TEXT("%s%s=Parms.%s;\r\n"), FCString::Tab(Indent + 1), *PropertyName, *PropertyName);
			}
		}
	}

	// Return value.
	if (Parameters.Return)
	{
		// Make sure uint32 -> bool is supported
		bool bBoolProperty = Parameters.Return->IsA(UBoolProperty::StaticClass());
		Output.Logf(TEXT("%sreturn %sParms.%s;\r\n"), FCString::Tab(Indent + 1), bBoolProperty ? TEXT("!!") : TEXT(""), *Parameters.Return->GetName());
	}
	Output.Logf(TEXT("%s}\r\n"), FCString::Tab(Indent));
}

void FNativeClassHeaderGenerator::ExportDelegateDeclaration(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UFunction* Function)
{
	static const TCHAR DelegateStr[] = TEXT("delegate");

	check(Function->HasAnyFunctionFlags(FUNC_Delegate));

	const bool bIsMulticastDelegate = Function->HasAnyFunctionFlags( FUNC_MulticastDelegate );

	// Unmangle the function name
	const FString DelegateName = Function->GetName().LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );

	const FFunctionData* CompilerInfo = FFunctionData::FindForFunction(Function);

	FFuncInfo FunctionData = CompilerInfo->GetFunctionData();

	// Add class name to beginning of function, to avoid collisions with other classes with the same delegate name in this scope
	check(FunctionData.MarshallAndCallName.StartsWith(DelegateStr));
	FString ShortName = *FunctionData.MarshallAndCallName + ARRAY_COUNT(DelegateStr) - 1;
	FunctionData.MarshallAndCallName = FString::Printf( TEXT( "F%s_DelegateWrapper" ), *ShortName );

	// Setup delegate parameter
	const FString ExtraParam = FString::Printf(
		TEXT( "const %s& %s" ),
		bIsMulticastDelegate ? TEXT( "FMulticastScriptDelegate" ) : TEXT( "FScriptDelegate" ),
		*DelegateName
	);

	FUHTStringBuilder DelegateOutput;
	DelegateOutput.Log(TEXT("static "));

	// export the line that looks like: int32 Main(const FString& Parms)
	ExportNativeFunctionHeader(DelegateOutput, ForwardDeclarations, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Declaration, *ExtraParam, *GetAPIString());

	// Only exporting function prototype
	DelegateOutput.Logf(TEXT(";\r\n"));

	ExportFunction(Out, SourceFile, Function, false);
}

void FNativeClassHeaderGenerator::ExportDelegateDefinition(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UFunction* Function)
{
	static const TCHAR DelegateStr[] = TEXT("delegate");

	check(Function->HasAnyFunctionFlags(FUNC_Delegate));

	// Export parameters structs for all delegates.  We'll need these to declare our delegate execution function.
	FUHTStringBuilder DelegateOutput;
	ExportEventParm(DelegateOutput, ForwardDeclarations, Function, /*Indent=*/ 0, /*bOutputConstructor=*/ true, EExportingState::Normal);

	const bool bIsMulticastDelegate = Function->HasAnyFunctionFlags( FUNC_MulticastDelegate );

	// Unmangle the function name
	const FString DelegateName = Function->GetName().LeftChop( FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX ).Len() );

	const FFunctionData* CompilerInfo = FFunctionData::FindForFunction(Function);

	FFuncInfo FunctionData = CompilerInfo->GetFunctionData();

	// Always export delegate wrapper functions as inline
	FunctionData.FunctionExportFlags |= FUNCEXPORT_Inline;

	// Add class name to beginning of function, to avoid collisions with other classes with the same delegate name in this scope
	check(FunctionData.MarshallAndCallName.StartsWith(DelegateStr));
	FString ShortName = *FunctionData.MarshallAndCallName + ARRAY_COUNT(DelegateStr) - 1;
	FunctionData.MarshallAndCallName = FString::Printf( TEXT( "F%s_DelegateWrapper" ), *ShortName );

	// Setup delegate parameter
	const FString ExtraParam = FString::Printf(
		TEXT( "const %s& %s" ),
		bIsMulticastDelegate ? TEXT( "FMulticastScriptDelegate" ) : TEXT( "FScriptDelegate" ),
		*DelegateName
	);

	DelegateOutput.Log(TEXT("static "));

	// export the line that looks like: int32 Main(const FString& Parms)
	ExportNativeFunctionHeader(DelegateOutput, ForwardDeclarations, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Declaration, *ExtraParam, *GetAPIString());

	FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(FunctionData.FunctionReference);

	WriteEventFunctionPrologue(DelegateOutput, 0, Parameters, Function->GetOuter(), *DelegateName);
	{
		const TCHAR* DelegateType = bIsMulticastDelegate ? TEXT( "ProcessMulticastDelegate" ) : TEXT( "ProcessDelegate" );
		const TCHAR* DelegateArg  = Parameters.HasParms() ? TEXT("&Parms") : TEXT("NULL");
		DelegateOutput.Logf(TEXT("\t%s.%s<UObject>(%s);\r\n"), *DelegateName, DelegateType, DelegateArg);
	}
	WriteEventFunctionEpilogue(DelegateOutput, 0, Parameters);

	FString MacroName = SourceFile.GetGeneratedMacroName(FunctionData.MacroLine, TEXT("_DELEGATE"));
	WriteMacro(Out, MacroName, DelegateOutput);
}

void FNativeClassHeaderGenerator::ExportEventParm(FUHTStringBuilder& Out, TSet<FString>& PropertyFwd, UFunction* Function, int32 Indent, bool bOutputConstructor, EExportingState ExportingState)
{
	if (!WillExportEventParms(Function))
	{
		return;
	}

	FString FunctionName = Function->GetName();
	if (Function->HasAnyFunctionFlags(FUNC_Delegate))
	{
		FunctionName = FunctionName.LeftChop(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len());
	}

	FString EventParmStructName = GetEventStructParamsName(Function->GetOuter(), *FunctionName);
	Out.Logf(TEXT("%sstruct %s\r\n"), FCString::Tab(Indent), *EventParmStructName);
	Out.Logf(TEXT("%s{\r\n"), FCString::Tab(Indent));

	for (UProperty* Prop : TFieldRange<UProperty>(Function))
	{
		if (!(Prop->PropertyFlags & CPF_Parm))
		{
			continue;
		}

		PropertyFwd.Add(Prop->GetCPPTypeForwardDeclaration());

		FUHTStringBuilder PropertyText;
		PropertyText.Log(FCString::Tab(Indent + 1));

		bool bEmitConst = Prop->HasAnyPropertyFlags(CPF_ConstParm) && Prop->IsA<UObjectProperty>();

		//@TODO: UCREMOVAL: This is awful code duplication to avoid a double-const
		{
			// export 'const' for parameters
			const bool bIsConstParam = (Prop->IsA(UInterfaceProperty::StaticClass()) && !Prop->HasAllPropertyFlags(CPF_OutParm)); //@TODO: This should be const once that flag exists
			const bool bIsOnConstClass = (Prop->IsA(UObjectProperty::StaticClass()) && ((UObjectProperty*)Prop)->PropertyClass != NULL && ((UObjectProperty*)Prop)->PropertyClass->HasAnyClassFlags(CLASS_Const));

			if (bIsConstParam || bIsOnConstClass)
			{
				bEmitConst = false; // ExportCppDeclaration will do it for us
			}
		}

		if (bEmitConst)
		{
			PropertyText.Logf(TEXT("const "));
		}

		const FString* Dim = GArrayDimensions.Find(Prop);
		Prop->ExportCppDeclaration(PropertyText, EExportedDeclaration::Local, Dim ? **Dim : NULL);
		ApplyAlternatePropertyExportText(Prop, PropertyText, ExportingState);

		PropertyText.Log(TEXT(";\r\n"));
		Out += *PropertyText;

	}
	// constructor must initialize the return property if it needs it
	UProperty* Prop = Function->GetReturnProperty();
	if (Prop && bOutputConstructor)
	{
		FUHTStringBuilder InitializationAr;

		UStructProperty* InnerStruct = Cast<UStructProperty>(Prop);
		bool bNeedsOutput = true;
		if (InnerStruct)
		{
			bNeedsOutput = InnerStruct->HasNoOpConstructor();
		}
		else if (
			Cast<UNameProperty>(Prop) ||
			Cast<UDelegateProperty>(Prop) ||
			Cast<UMulticastDelegateProperty>(Prop) ||
			Cast<UStrProperty>(Prop) ||
			Cast<UTextProperty>(Prop) ||
			Cast<UArrayProperty>(Prop) ||
			Cast<UMapProperty>(Prop) ||
			Cast<USetProperty>(Prop) ||
			Cast<UInterfaceProperty>(Prop)
			)
		{
			bNeedsOutput = false;
		}
		if (bNeedsOutput)
		{
			check(Prop->ArrayDim == 1); // can't return arrays
			Out.Logf(TEXT("\r\n%s/** Constructor, initializes return property only **/\r\n"), FCString::Tab(Indent + 1));
			Out.Logf(TEXT("%s%s()\r\n"), FCString::Tab(Indent + 1), *EventParmStructName);
			Out.Logf(TEXT("%s%s %s(%s)\r\n"), FCString::Tab(Indent + 2), TEXT(":"), *Prop->GetName(), *GetNullParameterValue(Prop, true));
			Out.Logf(TEXT("%s{\r\n"), FCString::Tab(Indent + 1));
			Out.Logf(TEXT("%s}\r\n"), FCString::Tab(Indent + 1));
		}
	}
	Out.Logf(TEXT("%s};\r\n"), FCString::Tab(Indent));
}

/**
 * Get the intrinsic null value for this property
 *
 * @param	Prop				the property to get the null value for
 * @param	bMacroContext		true when exporting the P_GET* macro, false when exporting the friendly C++ function header
 *
 * @return	the intrinsic null value for the property (0 for ints, TEXT("") for strings, etc.)
 */
FString FNativeClassHeaderGenerator::GetNullParameterValue( UProperty* Prop, bool bInitializer/*=false*/ )
{
	UClass* PropClass = Prop->GetClass();
	UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Prop);
	if (PropClass == UByteProperty::StaticClass())
	{
		UByteProperty* ByteProp = (UByteProperty*)Prop;

		// if it's an enum class then we need an explicit cast
		if( ByteProp->Enum && ByteProp->Enum->GetCppForm() == UEnum::ECppForm::EnumClass )
		{
			return FString::Printf(TEXT("(%s)0"), *ByteProp->GetCPPType());
		}

		return TEXT("0");
	}
	else if (PropClass == UEnumProperty::StaticClass())
	{
		UEnumProperty* EnumProp = (UEnumProperty*)Prop;

		return FString::Printf(TEXT("(%s)0"), *EnumProp->Enum->GetName());
	}
	else if ( PropClass == UBoolProperty::StaticClass() )
	{
		return TEXT("false");
	}
	else if ( PropClass == UIntProperty::StaticClass()
	||	PropClass == UFloatProperty::StaticClass()
	||	PropClass == UDoubleProperty::StaticClass())
	{
		return TEXT("0");
	}
	else if ( PropClass == UNameProperty::StaticClass() )
	{
		return TEXT("NAME_None");
	}
	else if ( PropClass == UStrProperty::StaticClass() )
	{
		return TEXT("TEXT(\"\")");
	}
	else if ( PropClass == UTextProperty::StaticClass() )
	{
		return TEXT("FText::GetEmpty()");
	}
	else if ( PropClass == UArrayProperty::StaticClass()
		||    PropClass == UMapProperty::StaticClass()
		||    PropClass == USetProperty::StaticClass()
		||    PropClass == UDelegateProperty::StaticClass()
		||    PropClass == UMulticastDelegateProperty::StaticClass() )
	{
		FString Type, ExtendedType;
		Type = Prop->GetCPPType(&ExtendedType,CPPF_OptionalValue);
		return Type + ExtendedType + TEXT("()");
	}
	else if ( PropClass == UStructProperty::StaticClass() )
	{
		bool bHasNoOpConstuctor = CastChecked<UStructProperty>(Prop)->HasNoOpConstructor();
		if (bInitializer && bHasNoOpConstuctor)
		{
			return TEXT("ForceInit");
		}

		FString Type, ExtendedType;
		Type = Prop->GetCPPType(&ExtendedType,CPPF_OptionalValue);
		return Type + ExtendedType + (bHasNoOpConstuctor ? TEXT("(ForceInit)") : TEXT("()"));
	}
	else if (ObjectProperty)
	{
		return TEXT("NULL");
	}
	else if ( PropClass == UInterfaceProperty::StaticClass() )
	{
		return TEXT("NULL");
	}

	UE_LOG(LogCompile, Fatal,TEXT("GetNullParameterValue - Unhandled property type '%s': %s"), *PropClass->GetName(), *Prop->GetPathName());
	return TEXT("");
}


FString FNativeClassHeaderGenerator::GetFunctionReturnString(UFunction* Function)
{
	if (UProperty* Return = Function->GetReturnProperty())
	{
		FString ExtendedReturnType;
		ForwardDeclarations.Add(Return->GetCPPTypeForwardDeclaration());
		FString ReturnType = Return->GetCPPType(&ExtendedReturnType, CPPF_ArgumentOrReturnValue);
		FUHTStringBuilder ReplacementText;
		ReplacementText += ReturnType;
		ApplyAlternatePropertyExportText(Return, ReplacementText, EExportingState::Normal);
		return ReplacementText + ExtendedReturnType;
	}

	return TEXT("void");
}

/**
* Gets string with function const modifier type.
*
* @param Function Function to get const modifier of.
* @return Empty FString if function is non-const, FString("const") if function is const.
*/
FString GetFunctionConstModifierString(UFunction* Function)
{
	if (Function->HasAllFunctionFlags(FUNC_Const))
	{
		return TEXT("const");
	}

	return FString();
}

/**
 * Converts Position within File to Line and Column.
 *
 * @param File File contents.
 * @param Position Position in string to convert.
 * @param OutLine Result line.
 * @param OutColumn Result column.
 */
void GetLineAndColumnFromPositionInFile(const FString& File, int32 Position, int32& OutLine, int32& OutColumn)
{
	OutLine = 1;
	OutColumn = 1;

	int32 i;
	for (i = 1; i <= Position; ++i)
	{
		if (File[i] == '\n')
		{
			++OutLine;
			OutColumn = 0;
		}
		else
		{
			++OutColumn;
		}
	}
}

bool FNativeClassHeaderGenerator::IsMissingVirtualSpecifier(const FString& SourceFile, int32 FunctionNamePosition)
{
	auto IsEndOfSearchChar = [](TCHAR C) { return (C == TEXT('}')) || (C == TEXT('{')) || (C == TEXT(';')); };

	// Find first occurrence of "}", ";", "{" going backwards from ImplementationPosition.
	int32 EndOfSearchCharIndex = SourceFile.FindLastCharByPredicate(IsEndOfSearchChar, FunctionNamePosition);
	check(EndOfSearchCharIndex != INDEX_NONE);

	// Then find if there is "virtual" keyword starting from position of found character to ImplementationPosition
	return !HasIdentifierExactMatch(&SourceFile[EndOfSearchCharIndex], &SourceFile[FunctionNamePosition], TEXT("virtual"));
}

FString CreateClickableErrorMessage(const FString& Filename, int32 Line, int32 Column)
{
	return FString::Printf(TEXT("%s(%d,%d): error: "), *Filename, Line, Column);
}

void FNativeClassHeaderGenerator::CheckRPCFunctions(const FFuncInfo& FunctionData, const FString& ClassName, int32 ImplementationPosition, int32 ValidatePosition, const FUnrealSourceFile& SourceFile)
{
	bool bHasImplementation = ImplementationPosition != INDEX_NONE;
	bool bHasValidate = ValidatePosition != INDEX_NONE;

	auto Function = FunctionData.FunctionReference;
	auto FunctionReturnType = GetFunctionReturnString(Function);
	auto ConstModifier = GetFunctionConstModifierString(Function) + TEXT(" ");

	auto bIsNative = Function->HasAllFunctionFlags(FUNC_Native);
	auto bIsNet = Function->HasAllFunctionFlags(FUNC_Net);
	auto bIsNetValidate = Function->HasAllFunctionFlags(FUNC_NetValidate);
	auto bIsNetResponse = Function->HasAllFunctionFlags(FUNC_NetResponse);
	auto bIsBlueprintEvent = Function->HasAllFunctionFlags(FUNC_BlueprintEvent);

	bool bNeedsImplementation = (bIsNet && !bIsNetResponse) || bIsBlueprintEvent || bIsNative;
	bool bNeedsValidate = (bIsNative || bIsNet) && !bIsNetResponse && bIsNetValidate;

	check(bNeedsImplementation || bNeedsValidate);

	auto ParameterString = GetFunctionParameterString(Function);
	const auto& Filename = SourceFile.GetFilename();
	const auto& FileContent = SourceFile.GetContent();

	//
	// Get string with function specifiers, listing why we need _Implementation or _Validate functions.
	//
	TArray<FString> FunctionSpecifiers;
	FunctionSpecifiers.Reserve(4);
	if (bIsNative)			{ FunctionSpecifiers.Add(TEXT("Native"));			}
	if (bIsNet)				{ FunctionSpecifiers.Add(TEXT("Net"));				}
	if (bIsBlueprintEvent)	{ FunctionSpecifiers.Add(TEXT("BlueprintEvent"));	}
	if (bIsNetValidate)		{ FunctionSpecifiers.Add(TEXT("NetValidate"));		}

	check(FunctionSpecifiers.Num() > 0);

	//
	// Coin static_assert message
	//
	FUHTStringBuilder AssertMessage;
	AssertMessage.Logf(TEXT("Function %s was marked as %s"), *(Function->GetName()), *FunctionSpecifiers[0]);
	for (int32 i = 1; i < FunctionSpecifiers.Num(); ++i)
	{
		AssertMessage.Logf(TEXT(", %s"), *FunctionSpecifiers[i]);
	}

	AssertMessage.Logf(TEXT("."));

	//
	// Check if functions are missing.
	//
	int32 Line;
	int32 Column;
	GetLineAndColumnFromPositionInFile(FileContent, FunctionData.InputPos, Line, Column);
	if (bNeedsImplementation && !bHasImplementation)
	{
		FString ErrorPosition = CreateClickableErrorMessage(Filename, Line, Column);
		FString FunctionDecl = FString::Printf(TEXT("virtual %s %s::%s(%s) %s"), *FunctionReturnType, *ClassName, *FunctionData.CppImplName, *ParameterString, *ConstModifier);
		FError::Throwf(TEXT("%s%s Declare function %s"), *ErrorPosition, *AssertMessage, *FunctionDecl);
	}

	if (bNeedsValidate && !bHasValidate)
	{
		FString ErrorPosition = CreateClickableErrorMessage(Filename, Line, Column);
		FString FunctionDecl = FString::Printf(TEXT("virtual bool %s::%s(%s) %s"), *ClassName, *FunctionData.CppValidationImplName, *ParameterString, *ConstModifier);
		FError::Throwf(TEXT("%s%s Declare function %s"), *ErrorPosition, *AssertMessage, *FunctionDecl);
	}

	//
	// If all needed functions are declared, check if they have virtual specifiers.
	//
	if (bNeedsImplementation && bHasImplementation && IsMissingVirtualSpecifier(FileContent, ImplementationPosition))
	{
		GetLineAndColumnFromPositionInFile(FileContent, ImplementationPosition, Line, Column);
		FString ErrorPosition = CreateClickableErrorMessage(Filename, Line, Column);
		FString FunctionDecl = FString::Printf(TEXT("%s %s::%s(%s) %s"), *FunctionReturnType, *ClassName, *FunctionData.CppImplName, *ParameterString, *ConstModifier);
		FError::Throwf(TEXT("%sDeclared function %sis not marked as virtual."), *ErrorPosition, *FunctionDecl);
	}

	if (bNeedsValidate && bHasValidate && IsMissingVirtualSpecifier(FileContent, ValidatePosition))
	{
		GetLineAndColumnFromPositionInFile(FileContent, ValidatePosition, Line, Column);
		FString ErrorPosition = CreateClickableErrorMessage(Filename, Line, Column);
		FString FunctionDecl = FString::Printf(TEXT("bool %s::%s(%s) %s"), *ClassName, *FunctionData.CppValidationImplName, *ParameterString, *ConstModifier);
		FError::Throwf(TEXT("%sDeclared function %sis not marked as virtual."), *ErrorPosition, *FunctionDecl);
	}
}

void FNativeClassHeaderGenerator::ExportNativeFunctionHeader(
	FOutputDevice&                   Out,
	TSet<FString>&                   OutFwdDecls,
	const FFuncInfo&                 FunctionData,
	EExportFunctionType::Type        FunctionType,
	EExportFunctionHeaderStyle::Type FunctionHeaderStyle,
	const TCHAR*                     ExtraParam,
	const TCHAR*                     APIString
)
{
	UFunction* Function = FunctionData.FunctionReference;

	const bool bIsDelegate   = Function->HasAnyFunctionFlags( FUNC_Delegate );
	const bool bIsInterface  = !bIsDelegate && Function->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface);
	const bool bIsK2Override = Function->HasAnyFunctionFlags( FUNC_BlueprintEvent );

	if (!bIsDelegate)
	{
		Out.Log(TEXT("\t"));
	}

	if (FunctionHeaderStyle == EExportFunctionHeaderStyle::Declaration)
	{
		// cpp implementation of functions never have these appendages

		// If the function was marked as 'RequiredAPI', then add the *_API macro prefix.  Note that if the class itself
		// was marked 'RequiredAPI', this is not needed as C++ will exports all methods automatically.
		if (FunctionType != EExportFunctionType::Event &&
			!Function->GetOwnerClass()->HasAnyClassFlags(CLASS_RequiredAPI) &&
			(FunctionData.FunctionExportFlags & FUNCEXPORT_RequiredAPI))
		{
			Out.Log(APIString);
		}

		if(FunctionType == EExportFunctionType::Interface)
		{
			Out.Log(TEXT("static "));
		}
		else if (bIsK2Override)
		{
			Out.Log(TEXT("virtual "));
		}
		// if the owning class is an interface class
		else if ( bIsInterface )
		{
			Out.Log(TEXT("virtual "));
		}
		// this is not an event, the function is not a static function and the function is not marked final
		else if ( FunctionType != EExportFunctionType::Event && !Function->HasAnyFunctionFlags(FUNC_Static) && !(FunctionData.FunctionExportFlags & FUNCEXPORT_Final) )
		{
			Out.Log(TEXT("virtual "));
		}
		else if( FunctionData.FunctionExportFlags & FUNCEXPORT_Inline )
		{
			Out.Log(TEXT("inline "));
		}
	}

	if (UProperty* Return = Function->GetReturnProperty())
	{
		FString ExtendedReturnType;
		FString ReturnType = Return->GetCPPType(&ExtendedReturnType, (FunctionHeaderStyle == EExportFunctionHeaderStyle::Definition && (FunctionType != EExportFunctionType::Interface) ? CPPF_Implementation : 0) | CPPF_ArgumentOrReturnValue);
		OutFwdDecls.Add(Return->GetCPPTypeForwardDeclaration());
		FUHTStringBuilder ReplacementText;
		ReplacementText += ReturnType;
		ApplyAlternatePropertyExportText(Return, ReplacementText, EExportingState::Normal);
		Out.Logf(TEXT("%s%s"), *ReplacementText, *ExtendedReturnType);
	}
	else
	{
		Out.Log( TEXT("void") );
	}

	FString FunctionName;
	if (FunctionHeaderStyle == EExportFunctionHeaderStyle::Definition)
	{
		FunctionName = FString(NameLookupCPP.GetNameCPP(CastChecked<UClass>(Function->GetOuter()), bIsInterface || FunctionType == EExportFunctionType::Interface)) + TEXT("::");
	}

	if (FunctionType == EExportFunctionType::Interface)
	{
		FunctionName += FString::Printf(TEXT("Execute_%s"), *Function->GetName());
	}
	else if (FunctionType == EExportFunctionType::Event)
	{
		FunctionName += FunctionData.MarshallAndCallName;
	}
	else
	{
		FunctionName += FunctionData.CppImplName;
	}

	Out.Logf(TEXT(" %s("), *FunctionName);

	int32 ParmCount=0;

	// Emit extra parameter if we have one
	if( ExtraParam )
	{
		Out.Logf(TEXT("%s"), ExtraParam);
		++ParmCount;
	}

	for (UProperty* Property : TFieldRange<UProperty>(Function))
	{
		if ((Property->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) != CPF_Parm)
		{
			continue;
		}

		OutFwdDecls.Add(Property->GetCPPTypeForwardDeclaration());

		if( ParmCount++ )
		{
			Out.Log(TEXT(", "));
		}

		FUHTStringBuilder PropertyText;

		const FString* Dim = GArrayDimensions.Find(Property);
		Property->ExportCppDeclaration( PropertyText, EExportedDeclaration::Parameter, Dim ? **Dim : NULL );
		ApplyAlternatePropertyExportText(Property, PropertyText, EExportingState::Normal);

		Out.Logf(TEXT("%s"), *PropertyText);
	}

	Out.Log( TEXT(")") );
	if (FunctionType != EExportFunctionType::Interface)
	{
		if (!bIsDelegate && Function->HasAllFunctionFlags(FUNC_Const))
		{
			Out.Log( TEXT(" const") );
		}

		if (bIsInterface && FunctionHeaderStyle == EExportFunctionHeaderStyle::Declaration)
		{
			// all methods in interface classes are pure virtuals
			Out.Log(TEXT("=0"));
		}
	}
}

/**
 * Export the actual internals to a standard thunk function
 *
 * @param RPCWrappers output device for writing
 * @param FunctionData function data for the current function
 * @param Parameters list of parameters in the function
 * @param Return return parameter for the function
 * @param DeprecationWarningOutputDevice Device to output deprecation warnings for _Validate and _Implementation functions.
 */
void FNativeClassHeaderGenerator::ExportFunctionThunk(FUHTStringBuilder& RPCWrappers, UFunction* Function, const FFuncInfo& FunctionData, const TArray<UProperty*>& Parameters, UProperty* Return)
{
	// export the GET macro for this parameter
	FString ParameterList;
	for (int32 ParameterIndex = 0; ParameterIndex < Parameters.Num(); ParameterIndex++)
	{
		UProperty* Param = Parameters[ParameterIndex];
		ForwardDeclarations.Add(Param->GetCPPTypeForwardDeclaration());

		FString EvalBaseText = TEXT("P_GET_");	// e.g. P_GET_STR
		FString EvalModifierText;				// e.g. _REF
		FString EvalParameterText;				// e.g. (UObject*,NULL)

		FString TypeText;

		if (Param->ArrayDim > 1)
		{
			EvalBaseText += TEXT("ARRAY");
			TypeText = Param->GetCPPType();
		}
		else
		{
			EvalBaseText += Param->GetCPPMacroType(TypeText);

			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Param);
			if (ArrayProperty)
			{
				UInterfaceProperty* InterfaceProperty = Cast<UInterfaceProperty>(ArrayProperty->Inner);
				if (InterfaceProperty)
				{
					FString InterfaceTypeText;
					InterfaceProperty->GetCPPMacroType(InterfaceTypeText);
					TypeText += FString::Printf(TEXT("<%s>"), *InterfaceTypeText);
				}
			}
		}

		bool bPassAsNoPtr = Param->HasAllPropertyFlags(CPF_UObjectWrapper | CPF_OutParm) && Param->IsA(UClassProperty::StaticClass());
		if (bPassAsNoPtr)
		{
			TypeText = Param->GetCPPType();
		}

		FUHTStringBuilder ReplacementText;
		ReplacementText += TypeText;

		ApplyAlternatePropertyExportText(Param, ReplacementText, EExportingState::Normal);
		TypeText = ReplacementText;

		FString DefaultValueText;
		FString ParamPrefix = TEXT("Z_Param_");

		// if this property is an out parm, add the REF tag
		if (Param->PropertyFlags & CPF_OutParm)
		{
			if (!bPassAsNoPtr)
			{
				EvalModifierText += TEXT("_REF");
			}
			else
			{
				// Parameters passed as TSubclassOf<Class>& shouldn't have asterisk added.
				EvalModifierText += TEXT("_REF_NO_PTR");
			}

			ParamPrefix += TEXT("Out_");
		}

		// if this property requires a specialization, add a comma to the type name so we can print it out easily
		if (TypeText != TEXT(""))
		{
			TypeText += TCHAR(',');
		}

		FString ParamName = ParamPrefix + Param->GetName();

		EvalParameterText = FString::Printf(TEXT("(%s%s%s)"), *TypeText, *ParamName, *DefaultValueText);

		RPCWrappers.Logf(TEXT("\t\t%s%s%s;") LINE_TERMINATOR, *EvalBaseText, *EvalModifierText, *EvalParameterText);

		// add this property to the parameter list string
		if (ParameterList.Len())
		{
			ParameterList += TCHAR(',');
		}

		{
			UDelegateProperty* DelegateProp = Cast< UDelegateProperty >(Param);
			if (DelegateProp != NULL)
			{
				// For delegates, add an explicit conversion to the specific type of delegate before passing it along
				const FString FunctionName = DelegateProp->SignatureFunction->GetName().LeftChop(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len());
				const FString CPPDelegateName = FString(TEXT("F")) + FunctionName;
				ParamName = FString::Printf(TEXT("%s(%s)"), *CPPDelegateName, *ParamName);
			}
		}

		{
			UMulticastDelegateProperty* MulticastDelegateProp = Cast< UMulticastDelegateProperty >(Param);
			if (MulticastDelegateProp != NULL)
			{
				// For delegates, add an explicit conversion to the specific type of delegate before passing it along
				const FString FunctionName = MulticastDelegateProp->SignatureFunction->GetName().LeftChop(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len());
				const FString CPPDelegateName = FString(TEXT("F")) + FunctionName;
				ParamName = FString::Printf(TEXT("%s(%s)"), *CPPDelegateName, *ParamName);
			}
		}

		UEnum* Enum = nullptr;
		UByteProperty* ByteProp = Cast<UByteProperty>(Param);
		if (ByteProp && ByteProp->Enum)
		{
			Enum = ByteProp->Enum;
		}
		else if (Param->IsA<UEnumProperty>())
		{
			Enum = ((UEnumProperty*)Param)->Enum;
		}

		if (Enum)
		{
			// For enums, add an explicit conversion
			if (!(Param->PropertyFlags & CPF_OutParm))
			{
				ParamName = FString::Printf(TEXT("%s(%s)"), *Enum->CppType, *ParamName);
			}
			else
			{
				if (Enum->GetCppForm() == UEnum::ECppForm::EnumClass)
				{
					// If we're an enum class don't require the wrapper
					ParamName = FString::Printf(TEXT("(%s&)(%s)"), *Enum->CppType, *ParamName);
				}
				else
				{
					ParamName = FString::Printf(TEXT("(TEnumAsByte<%s>&)(%s)"), *Enum->CppType, *ParamName);
				}
			}
		}

		ParameterList += ParamName;
	}

	RPCWrappers += TEXT("\t\tP_FINISH;") LINE_TERMINATOR;
	RPCWrappers += TEXT("\t\tP_NATIVE_BEGIN;") LINE_TERMINATOR;

	ClassDefinitionRange ClassRange;
	if (ClassDefinitionRanges.Contains(Function->GetOwnerClass()))
	{
		ClassRange = ClassDefinitionRanges[Function->GetOwnerClass()];
		ClassRange.Validate();
	}

	const TCHAR* ClassStart = ClassRange.Start;
	const TCHAR* ClassEnd   = ClassRange.End;
	FString      ClassName  = Function->GetOwnerClass()->GetName();

	FString ClassDefinition(ClassEnd - ClassStart, ClassStart);

	bool bHasImplementation = HasIdentifierExactMatch(ClassDefinition, FunctionData.CppImplName);
	bool bHasValidate = HasIdentifierExactMatch(ClassDefinition, FunctionData.CppValidationImplName);

	bool bShouldEnableImplementationDeprecation =
		// Enable deprecation warnings only if GENERATED_BODY is used inside class or interface (not GENERATED_UCLASS_BODY etc.)
		ClassRange.bHasGeneratedBody
		// and implementation function is called, but not the one declared by user
		&& (FunctionData.CppImplName != Function->GetName() && !bHasImplementation);

	bool bShouldEnableValidateDeprecation =
		// Enable deprecation warnings only if GENERATED_BODY is used inside class or interface (not GENERATED_UCLASS_BODY etc.)
		ClassRange.bHasGeneratedBody
		// and validation function is called
		&& (FunctionData.FunctionFlags & FUNC_NetValidate) && !bHasValidate;

	//Emit warning here if necessary
	FUHTStringBuilder FunctionDeclaration;
	ExportNativeFunctionHeader(FunctionDeclaration, ForwardDeclarations, FunctionData, EExportFunctionType::Function, EExportFunctionHeaderStyle::Declaration, nullptr, *GetAPIString());

	// Call the validate function if there is one
	if (!(FunctionData.FunctionExportFlags & FUNCEXPORT_CppStatic) && (FunctionData.FunctionFlags & FUNC_NetValidate))
	{
		RPCWrappers.Logf(TEXT("\t\tif (!this->%s(%s))") LINE_TERMINATOR, *FunctionData.CppValidationImplName, *ParameterList);
		RPCWrappers.Logf(TEXT("\t\t{") LINE_TERMINATOR);
		RPCWrappers.Logf(TEXT("\t\t\tRPC_ValidateFailed(TEXT(\"%s\"));") LINE_TERMINATOR, *FunctionData.CppValidationImplName);
		RPCWrappers.Logf(TEXT("\t\t\treturn;") LINE_TERMINATOR);	// If we got here, the validation function check failed
		RPCWrappers.Logf(TEXT("\t\t}") LINE_TERMINATOR);
	}

	// write out the return value
	RPCWrappers.Log(TEXT("\t\t"));
	if (Return)
	{
		ForwardDeclarations.Add(Return->GetCPPTypeForwardDeclaration());

		FUHTStringBuilder ReplacementText;
		FString ReturnExtendedType;
		ReplacementText += Return->GetCPPType(&ReturnExtendedType);
		ApplyAlternatePropertyExportText(Return, ReplacementText, EExportingState::Normal);

		FString ReturnType = ReplacementText;
		RPCWrappers.Logf(TEXT("*(%s%s*)") TEXT(PREPROCESSOR_TO_STRING(RESULT_PARAM)) TEXT("="), *ReturnType, *ReturnExtendedType);
	}

	// export the call to the C++ version
	if (FunctionData.FunctionExportFlags & FUNCEXPORT_CppStatic)
	{
		RPCWrappers.Logf(TEXT("%s::%s(%s);") LINE_TERMINATOR, NameLookupCPP.GetNameCPP(Function->GetOwnerClass()), *FunctionData.CppImplName, *ParameterList);
	}
	else
	{
		RPCWrappers.Logf(TEXT("this->%s(%s);") LINE_TERMINATOR, *FunctionData.CppImplName, *ParameterList);
	}
	RPCWrappers += TEXT("\t\tP_NATIVE_END;") LINE_TERMINATOR;
}

FString FNativeClassHeaderGenerator::GetFunctionParameterString(UFunction* Function)
{
	FString ParameterList;
	FUHTStringBuilder PropertyText;

	for (UProperty* Property : TFieldRange<UProperty>(Function))
	{
		ForwardDeclarations.Add(Property->GetCPPTypeForwardDeclaration());

		if ((Property->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) != CPF_Parm)
		{
			break;
		}

		if (ParameterList.Len())
		{
			ParameterList += TEXT(", ");
		}

		auto Dim = GArrayDimensions.Find(Property);
		Property->ExportCppDeclaration(PropertyText, EExportedDeclaration::Parameter, Dim ? **Dim : NULL, 0, true);
		ApplyAlternatePropertyExportText(Property, PropertyText, EExportingState::Normal);

		ParameterList += PropertyText;
		PropertyText.Reset();
	}

	return ParameterList;
}

struct FNativeFunctionStringBuilder
{
	FUHTStringBuilder RPCWrappers;
	FUHTStringBuilder AutogeneratedBlueprintFunctionDeclarations;
	FUHTStringBuilder AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared;
};

void FNativeClassHeaderGenerator::ExportNativeFunctions(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& OutMacroCalls, FOutputDevice& OutNoPureDeclsMacroCalls, const FUnrealSourceFile& SourceFile, UClass* Class, FClassMetaData* ClassData)
{
	FNativeFunctionStringBuilder RuntimeStringBuilders;
	FNativeFunctionStringBuilder EditorStringBuilders;

	FString ClassName = Class->GetName();

	ClassDefinitionRange ClassRange;
	if (ClassDefinitionRanges.Contains(Class))
	{
		ClassRange = ClassDefinitionRanges[Class];
		ClassRange.Validate();
	}

	// export the C++ stubs

	for (UFunction* Function : TFieldRange<UFunction>(Class, EFieldIteratorFlags::ExcludeSuper))
	{
		if (!(Function->FunctionFlags & FUNC_Native))
		{
			continue;
		}

		const bool bEditorOnlyFunc = Function->HasAnyFunctionFlags(FUNC_EditorOnly);
		FNativeFunctionStringBuilder& FuncStringBuilders = bEditorOnlyFunc ? EditorStringBuilders : RuntimeStringBuilders;

		FFunctionData* CompilerInfo = FFunctionData::FindForFunction(Function);

		const FFuncInfo& FunctionData = CompilerInfo->GetFunctionData();

		// Custom thunks don't get any C++ stub function generated
		if (FunctionData.FunctionExportFlags & FUNCEXPORT_CustomThunk)
		{
			continue;
		}

		// Should we emit these to RPC wrappers or just ignore them?
		const bool bWillBeProgrammerTyped = FunctionData.CppImplName == Function->GetName();

		if (!bWillBeProgrammerTyped)
		{
			const TCHAR* ClassStart = ClassRange.Start;
			const TCHAR* ClassEnd   = ClassRange.End;
			FString ClassDefinition(ClassEnd - ClassStart, ClassStart);

			FString FunctionName = Function->GetName();
			int32 ClassDefinitionStartPosition = ClassStart - *SourceFile.GetContent();

			int32 ImplementationPosition = FindIdentifierExactMatch(ClassDefinition, FunctionData.CppImplName);
			bool bHasImplementation = ImplementationPosition != INDEX_NONE;
			if (bHasImplementation)
			{
				ImplementationPosition += ClassDefinitionStartPosition;
			}

			int32 ValidatePosition = FindIdentifierExactMatch(ClassDefinition, FunctionData.CppValidationImplName);
			bool bHasValidate = ValidatePosition != INDEX_NONE;
			if (bHasValidate)
			{
				ValidatePosition += ClassDefinitionStartPosition;
			}

			//Emit warning here if necessary
			FUHTStringBuilder FunctionDeclaration;
			ExportNativeFunctionHeader(FunctionDeclaration, ForwardDeclarations, FunctionData, EExportFunctionType::Function, EExportFunctionHeaderStyle::Declaration, nullptr, *GetAPIString());
			FunctionDeclaration.Log(TEXT(";\r\n"));

			// Declare validation function if needed
			if (FunctionData.FunctionFlags & FUNC_NetValidate)
			{
				FString ParameterList = GetFunctionParameterString(Function);

				const TCHAR* Virtual = (!FunctionData.FunctionReference->HasAnyFunctionFlags(FUNC_Static) && !(FunctionData.FunctionExportFlags & FUNCEXPORT_Final)) ? TEXT("virtual") : TEXT("");
				FStringOutputDevice ValidDecl;
				ValidDecl.Logf(TEXT("\t%s bool %s(%s);\r\n"), Virtual, *FunctionData.CppValidationImplName, *ParameterList);
				FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarations.Log(*ValidDecl);
				if (!bHasValidate)
				{
					FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared.Logf(TEXT("%s"), *ValidDecl);
				}
			}

			FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarations.Log(*FunctionDeclaration);
			if (!bHasImplementation && FunctionData.CppImplName != FunctionName)
			{
				FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared.Log(*FunctionDeclaration);
			}

			// Versions that skip function autodeclaration throw an error when a function is missing.
			if (ClassRange.bHasGeneratedBody && (SourceFile.GetGeneratedCodeVersionForStruct(Class) > EGeneratedCodeVersion::V1))
			{
				FString Name = Class->HasAnyClassFlags(CLASS_Interface) ? TEXT("I") + ClassName : FString(NameLookupCPP.GetNameCPP(Class));
				CheckRPCFunctions(FunctionData, *Name, ImplementationPosition, ValidatePosition, SourceFile);
			}
		}

		FuncStringBuilders.RPCWrappers.Log(TEXT("\r\n"));

		// if this function was originally declared in a base class, and it isn't a static function,
		// only the C++ function header will be exported
		if (!ShouldExportUFunction(Function))
		{
			continue;
		}

		// export the script wrappers
		FuncStringBuilders.RPCWrappers.Logf(TEXT("\tDECLARE_FUNCTION(%s)"), *FunctionData.UnMarshallAndCallName);
		FuncStringBuilders.RPCWrappers += LINE_TERMINATOR TEXT("\t{") LINE_TERMINATOR;

		FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(FunctionData.FunctionReference);
		ExportFunctionThunk(FuncStringBuilders.RPCWrappers, Function, FunctionData, Parameters.Parms, Parameters.Return);

		FuncStringBuilders.RPCWrappers += TEXT("\t}") LINE_TERMINATOR;
	}

	// Write runtime wrappers
	{
		FString MacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_RPC_WRAPPERS"));

		WriteMacro(OutGeneratedHeaderText, MacroName, RuntimeStringBuilders.AutogeneratedBlueprintFunctionDeclarations + RuntimeStringBuilders.RPCWrappers);
		OutMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		// Put static checks before RPCWrappers to get proper messages from static asserts before compiler errors.
		FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_RPC_WRAPPERS_NO_PURE_DECLS"));
		if (SourceFile.GetGeneratedCodeVersionForStruct(Class) > EGeneratedCodeVersion::V1)
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, RuntimeStringBuilders.RPCWrappers);
		}
		else
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, RuntimeStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared + RuntimeStringBuilders.RPCWrappers);
		}

		OutNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);
	}

	// Write editor only RPC wrappers if they exist
	if (EditorStringBuilders.RPCWrappers.Len() > 0)
	{
		OutGeneratedHeaderText.Log( BeginEditorOnlyGuard );

		FString MacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_EDITOR_ONLY_RPC_WRAPPERS"));


		WriteMacro(OutGeneratedHeaderText, MacroName, EditorStringBuilders.AutogeneratedBlueprintFunctionDeclarations + EditorStringBuilders.RPCWrappers);
		OutMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		// Put static checks before RPCWrappers to get proper messages from static asserts before compiler errors.
		FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassData, TEXT("_EDITOR_ONLY_RPC_WRAPPERS_NO_PURE_DECLS"));
		if (SourceFile.GetGeneratedCodeVersionForStruct(Class) > EGeneratedCodeVersion::V1)
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, EditorStringBuilders.RPCWrappers);
		}
		else
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, EditorStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared + EditorStringBuilders.RPCWrappers);
		}

		// write out an else preprocessor block for when not compiling for the editor.  The generated macros should be empty then since the functions are compiled out
		{
			OutGeneratedHeaderText.Log(TEXT("#else\r\n"));

			WriteMacro(OutGeneratedHeaderText, MacroName, TEXT(""));
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, TEXT(""));

			OutGeneratedHeaderText.Log(EndEditorOnlyGuard);
		}

		OutNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);
	}
}

/**
 * Exports the methods which trigger UnrealScript events and delegates.
 *
 * @param	CallbackFunctions	the functions to export
 */
void FNativeClassHeaderGenerator::ExportCallbackFunctions(
	FOutputDevice&            OutGeneratedHeaderText,
	FOutputDevice&            OutCpp,
	TSet<FString>&            OutFwdDecls,
	const TArray<UFunction*>& CallbackFunctions,
	const TCHAR*              CallbackWrappersMacroName,
	EExportCallbackType       ExportCallbackType,
	const TCHAR*              APIString
)
{
	FUHTStringBuilder RPCWrappers;
	for (UFunction* Function : CallbackFunctions)
	{
		// Never expecting to export delegate functions this way
		check(!Function->HasAnyFunctionFlags(FUNC_Delegate));

		FFunctionData*   CompilerInfo = FFunctionData::FindForFunction(Function);
		const FFuncInfo& FunctionData = CompilerInfo->GetFunctionData();
		FString          FunctionName = Function->GetName();
		UClass*          Class        = CastChecked<UClass>(Function->GetOuter());
		const TCHAR*     ClassName    = NameLookupCPP.GetNameCPP(Class);

		if (FunctionData.FunctionFlags & FUNC_NetResponse)
		{
			// Net response functions don't go into the VM
			continue;
		}

		const bool bWillBeProgrammerTyped = FunctionName == FunctionData.MarshallAndCallName;

		// Emit the declaration if the programmer isn't responsible for declaring this wrapper
		if (!bWillBeProgrammerTyped)
		{
			// export the line that looks like: int32 Main(const FString& Parms)
			ExportNativeFunctionHeader(RPCWrappers, OutFwdDecls, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Declaration, nullptr, APIString);

			RPCWrappers.Log(TEXT(";\r\n"));
			RPCWrappers.Log(TEXT("\r\n"));
		}

		FString FunctionNameName;
		if (ExportCallbackType != EExportCallbackType::Interface)
		{
			FunctionNameName = FString::Printf(TEXT("NAME_%s_%s"), ClassName, *FunctionName);
			OutCpp.Logf(TEXT("\tstatic FName %s = FName(TEXT(\"%s\"));") LINE_TERMINATOR, *FunctionNameName, *GetOverriddenFName(Function).ToString());
		}

		// Emit the thunk implementation
		ExportNativeFunctionHeader(OutCpp, OutFwdDecls, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Definition, nullptr, APIString);

		FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(FunctionData.FunctionReference);

		if (ExportCallbackType != EExportCallbackType::Interface)
		{
			WriteEventFunctionPrologue(OutCpp, /*Indent=*/ 1, Parameters, Class, *FunctionName);
			{
				// Cast away const just in case, because ProcessEvent isn't const
				OutCpp.Logf(
					TEXT("\t\t%sProcessEvent(FindFunctionChecked(%s),%s);\r\n"),
					(Function->HasAllFunctionFlags(FUNC_Const)) ? *FString::Printf(TEXT("const_cast<%s*>(this)->"), ClassName) : TEXT(""),
					*FunctionNameName,
					Parameters.HasParms() ? TEXT("&Parms") : TEXT("NULL")
				);
			}
			WriteEventFunctionEpilogue(OutCpp, /*Indent=*/ 1, Parameters);
		}
		else
		{
			OutCpp.Log(LINE_TERMINATOR);
			OutCpp.Log(TEXT("\t{") LINE_TERMINATOR);

			// assert if this is ever called directly
			OutCpp.Logf(TEXT("\t\tcheck(0 && \"Do not directly call Event functions in Interfaces. Call Execute_%s instead.\");") LINE_TERMINATOR, *FunctionName);

			// satisfy compiler if it's expecting a return value
			if (Parameters.Return)
			{
				FString EventParmStructName = GetEventStructParamsName(Class, *FunctionName);
				OutCpp.Logf(TEXT("\t\t%s Parms;") LINE_TERMINATOR, *EventParmStructName);
				OutCpp.Log(TEXT("\t\treturn Parms.ReturnValue;") LINE_TERMINATOR);
			}
			OutCpp.Log(TEXT("\t}") LINE_TERMINATOR);
		}
	}

	WriteMacro(OutGeneratedHeaderText, CallbackWrappersMacroName, RPCWrappers);
}


/**
 * Determines if the property has alternate export text associated with it and if so replaces the text in PropertyText with the
 * alternate version. (for example, structs or properties that specify a native type using export-text).  Should be called immediately
 * after ExportCppDeclaration()
 *
 * @param	Prop			the property that is being exported
 * @param	PropertyText	the string containing the text exported from ExportCppDeclaration
 */
void FNativeClassHeaderGenerator::ApplyAlternatePropertyExportText(UProperty* Prop, FUHTStringBuilder& PropertyText, EExportingState ExportingState)
{
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Prop);
	UProperty* InnerProperty = ArrayProperty ? ArrayProperty->Inner : nullptr;
	if (InnerProperty && (
			(InnerProperty->IsA<UByteProperty>() && ((UByteProperty*)InnerProperty)->Enum && FClass::IsDynamic(((UByteProperty*)InnerProperty)->Enum)) ||
			(InnerProperty->IsA<UEnumProperty>()                                          && FClass::IsDynamic(((UEnumProperty*)InnerProperty)->Enum))
		)
	)
	{
		const FString Original = InnerProperty->GetCPPType();
		const FString RawByte = InnerProperty->GetCPPType(nullptr, EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
		if (Original != RawByte)
		{
			PropertyText.ReplaceInline(*Original, *RawByte, ESearchCase::CaseSensitive);
		}
		return;
	}

	if (ExportingState == EExportingState::TypeEraseDelegates)
	{
		UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(Prop);
		UMulticastDelegateProperty* MulticastDelegateProperty = Cast<UMulticastDelegateProperty>(Prop);
		if (DelegateProperty || MulticastDelegateProperty)
		{
			FString Original = Prop->GetCPPType();
			FString PlaceholderOfSameSizeAndAlignemnt;
			if (DelegateProperty)
			{
				PlaceholderOfSameSizeAndAlignemnt = TEXT("FScriptDelegate");
			}
			else
			{
				PlaceholderOfSameSizeAndAlignemnt = TEXT("FMulticastScriptDelegate");
			}
			PropertyText.ReplaceInline(*Original, *PlaceholderOfSameSizeAndAlignemnt, ESearchCase::CaseSensitive);
		}
	}
}

void GetSourceFilesInDependencyOrderRecursive(TArray<FUnrealSourceFile*>& OutTest, const UPackage* Package, FUnrealSourceFile* SourceFile, TSet<const FUnrealSourceFile*>& VisitedSet, bool bCheckDependenciesOnly, const TSet<FUnrealSourceFile*>& Ignore)
{
	// Check if the Class has already been exported, after we've checked for circular header dependencies.
	if (OutTest.Contains(SourceFile) || Ignore.Contains(SourceFile))
	{
		return;
	}

	// Check for circular dependencies.
	if (VisitedSet.Contains(SourceFile))
	{
		UE_LOG(LogCompile, Error, TEXT("Circular dependency detected for filename %s!"), *SourceFile->GetFilename());
		return;
	}

	// Check for circular header dependencies between export classes.
	bCheckDependenciesOnly = bCheckDependenciesOnly || SourceFile->GetPackage() != Package;

	VisitedSet.Add(SourceFile);
	for (FHeaderProvider& Include : SourceFile->GetIncludes())
	{
		if (FUnrealSourceFile* IncludeFile = Include.Resolve())
		{
			GetSourceFilesInDependencyOrderRecursive(OutTest, Package, IncludeFile, VisitedSet, bCheckDependenciesOnly, Ignore);
		}
	}
	VisitedSet.Remove(SourceFile);

	if (!bCheckDependenciesOnly)
	{
		OutTest.Add(SourceFile);
	}
}

TArray<FUnrealSourceFile*> GetSourceFilesInDependencyOrder(const UPackage* Package, const TArray<FUnrealSourceFile*>& SourceFiles, const TSet<FUnrealSourceFile*>& Ignore)
{
	TArray<FUnrealSourceFile*> Result;
	TSet<const FUnrealSourceFile*>	VisitedSet;
	for (FUnrealSourceFile* SourceFile : SourceFiles)
	{
		if (SourceFile->GetPackage() == Package)
		{
			GetSourceFilesInDependencyOrderRecursive(Result, Package, SourceFile, VisitedSet, false, Ignore);
		}
	}

	return Result;
}

TMap<UClass*, FUnrealSourceFile*> GClassToSourceFileMap;

// Constructor.
FNativeClassHeaderGenerator::FNativeClassHeaderGenerator(
	const UPackage* InPackage,
	const TArray<FUnrealSourceFile*>& SourceFiles,
	FClasses& AllClasses,
	bool InAllowSaveExportedHeaders
)
	: API                        (FPackageName::GetShortName(InPackage).ToUpper())
	, Package                    (InPackage)
	, UniqueCrossModuleReferences(nullptr)
	, bAllowSaveExportedHeaders  (InAllowSaveExportedHeaders)
	, bFailIfGeneratedCodeChanges(FParse::Param(FCommandLine::Get(), TEXT("FailIfGeneratedCodeChanges")))
{
	const FString PackageName = FPackageName::GetShortName(Package);

	FString PkgDir;
	FString GeneratedIncludeDirectory;
	if (!FindPackageLocation(*PackageName, PkgDir, GeneratedIncludeDirectory))
	{
		UE_LOG(LogCompile, Error, TEXT("Failed to find path for package %s"), *PackageName);
	}

	bool bWriteClassesH = false;
	const bool bPackageHasAnyExportClasses = AllClasses.GetClassesInPackage(Package).ContainsByPredicate([](FClass* Class)
	{
		return Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_NoExport | CLASS_Intrinsic);
	});
	if (bPackageHasAnyExportClasses)
	{
		for (FUnrealSourceFile* SourceFile : SourceFiles)
		{
			TArray<UClass*> DefinedClasses = SourceFile->GetDefinedClasses();
			for (UClass* Class : DefinedClasses)
			{
				if (!Class->HasAnyClassFlags(CLASS_Native))
				{
					Class->UnMark(EObjectMark(OBJECTMARK_TagImp | OBJECTMARK_TagExp));
				}
				else if (GTypeDefinitionInfoMap.Contains(Class) && !Class->HasAnyClassFlags(CLASS_NoExport))
				{
					bWriteClassesH = true;
					Class->UnMark(OBJECTMARK_TagImp);
					Class->Mark(OBJECTMARK_TagExp);
				}
			}
		}
	}

	// Export an include line for each header
	TArray<FUnrealSourceFile*> PublicHeaderGroupIncludes;
	FUHTStringBuilder GeneratedFunctionDeclarations;

	TArray<FUnrealSourceFile*> Exported;
	{
		// Get source files and ignore them next time round
		static TSet<FUnrealSourceFile*> ExportedSourceFiles;
		Exported = GetSourceFilesInDependencyOrder(Package, SourceFiles, ExportedSourceFiles);
		ExportedSourceFiles.Append(Exported);
	}

	struct FGeneratedCPP
	{
		explicit FGeneratedCPP(FString&& InGeneratedCppFullFilename)
			: GeneratedCppFullFilename(MoveTemp(InGeneratedCppFullFilename))
		{
		}

		FString                      GeneratedCppFullFilename;
		TArray<FString>              RelativeIncludes;
		FUHTStringBuilderLineCounter GeneratedText;
		TSet<FString>                CrossModuleReferences;
	};

	TMap<FUnrealSourceFile*, FGeneratedCPP> GeneratedCPPs;
	for (FUnrealSourceFile* SourceFile : Exported)
	{
		FString ModuleRelativeFilename = SourceFile->GetFilename();
		ConvertToBuildIncludePath(Package, ModuleRelativeFilename);

		FString StrippedName       = FPaths::GetBaseFilename(ModuleRelativeFilename);
		FString BaseSourceFilename = GeneratedIncludeDirectory / StrippedName;

		FUHTStringBuilder GeneratedHeaderText;
		FGeneratedCPP& GeneratedCPP = GeneratedCPPs.Emplace(SourceFile, BaseSourceFilename + TEXT(".gen.cpp"));
		GeneratedCPP.RelativeIncludes.Add(MoveTemp(ModuleRelativeFilename));

		UniqueCrossModuleReferences = &GeneratedCPP.CrossModuleReferences;
		ON_SCOPE_EXIT
		{
			UniqueCrossModuleReferences = nullptr;
		};

		FUHTStringBuilderLineCounter& OutText = GeneratedCPP.GeneratedText;

		NameLookupCPP.SetCurrentSourceFile(SourceFile);

		TArray<UEnum*>             Enums;
		TArray<UScriptStruct*>     Structs;
		TArray<UDelegateFunction*> DelegateFunctions;
		SourceFile->GetScope()->SplitTypesIntoArrays(Enums, Structs, DelegateFunctions);

		// Reverse the containers as they come out in the reverse order of declaration
		Algo::Reverse(Enums);
		Algo::Reverse(Structs);
		Algo::Reverse(DelegateFunctions);

		GeneratedHeaderText.Logf(
			TEXT("#ifdef %s")																	LINE_TERMINATOR
			TEXT("#error \"%s.generated.h already included, missing '#pragma once' in %s.h\"")	LINE_TERMINATOR
			TEXT("#endif")																		LINE_TERMINATOR
			TEXT("#define %s")																	LINE_TERMINATOR
			LINE_TERMINATOR,
			*SourceFile->GetFileDefineName(), *SourceFile->GetStrippedFilename(), *SourceFile->GetStrippedFilename(), *SourceFile->GetFileDefineName());

		// export delegate definitions
		for (UDelegateFunction* Func : DelegateFunctions)
		{
			GeneratedFunctionDeclarations.Log(FTypeSingletonCache::Get(Func).GetExternDecl());
			ExportDelegateDeclaration(OutText, *SourceFile, Func);
		}

		// Export enums declared in non-UClass headers.
		for (UEnum* Enum : Enums)
		{
			// Is this ever not the case?
			if (Enum->GetOuter()->IsA(UPackage::StaticClass()))
			{
				GeneratedFunctionDeclarations.Log(FTypeSingletonCache::Get(Enum).GetExternDecl());
				ExportGeneratedEnumInitCode(OutText, *SourceFile, Enum);
			}
		}

		// export boilerplate macros for structs
		// reverse the order.
		for (UScriptStruct* Struct : Structs)
		{
			GeneratedFunctionDeclarations.Log(FTypeSingletonCache::Get(Struct).GetExternDecl());
			ExportGeneratedStructBodyMacros(GeneratedHeaderText, OutText, *SourceFile, Struct);
		}

		// export delegate wrapper function implementations
		for (UDelegateFunction* Func : DelegateFunctions)
		{
			ExportDelegateDefinition(GeneratedHeaderText, *SourceFile, Func);
		}

		TArray<UClass*> DefinedClasses = SourceFile->GetDefinedClasses();
		for (UClass* Class : DefinedClasses)
		{
			if (!(Class->ClassFlags & CLASS_Intrinsic))
			{
				ExportClassFromSourceFileInner(GeneratedHeaderText, OutText, GeneratedFunctionDeclarations, (FClass*)Class, *SourceFile);
			}

			GClassToSourceFileMap.Add(Class, SourceFile);
		}

		GeneratedHeaderText.Log(TEXT("#undef CURRENT_FILE_ID\r\n"));
		GeneratedHeaderText.Logf(TEXT("#define CURRENT_FILE_ID %s\r\n\r\n\r\n"), *SourceFile->GetFileId());

		for (UEnum* Enum : Enums)
		{
			ExportEnum(GeneratedHeaderText, Enum);
		}

		FString HeaderPath = BaseSourceFilename + TEXT(".generated.h");
		bool bHasChanged = WriteHeader(*HeaderPath, GeneratedHeaderText, ForwardDeclarations);

		SourceFile->SetGeneratedFilename(HeaderPath);
		SourceFile->SetHasChanged(bHasChanged);

		ForwardDeclarations.Reset();

		if (GPublicSourceFileSet.Contains(SourceFile))
		{
			PublicHeaderGroupIncludes.AddUnique(SourceFile);
		}
	}

	// Add includes for 'Within' classes
	for (FUnrealSourceFile* SourceFile : Exported)
	{
		TArray<UClass*> DefinedClasses = SourceFile->GetDefinedClasses();
		for (UClass* Class : DefinedClasses)
		{
			if (Class->ClassWithin && Class->ClassWithin != UObject::StaticClass())
			{
				if (FUnrealSourceFile** WithinSourceFile = GClassToSourceFileMap.Find(Class->ClassWithin))
				{
					FString Header = GetBuildPath(**WithinSourceFile);
					TArray<FString>& RelativeIncludes = GeneratedCPPs[SourceFile].RelativeIncludes;
					RelativeIncludes.AddUnique(MoveTemp(Header));
				}
			}
		}
	}

	if (bWriteClassesH)
	{
		// Write the classes and enums header prefixes.

		FUHTStringBuilder ClassesHText;
		ClassesHText.Log(HeaderCopyright);
		ClassesHText.Log(TEXT("#pragma once\r\n"));
		ClassesHText.Log(TEXT("\r\n"));
		ClassesHText.Log(TEXT("\r\n"));

		// Fill with the rest source files from this package.
		for (FUnrealSourceFile* SourceFile : GPublicSourceFileSet)
		{
			if (SourceFile->GetPackage() == InPackage)
			{
				PublicHeaderGroupIncludes.AddUnique(SourceFile);
			}
		}

		for (FUnrealSourceFile* SourceFile : PublicHeaderGroupIncludes)
		{
			ClassesHText.Logf(TEXT("#include \"%s\"") LINE_TERMINATOR, *GetBuildPath(*SourceFile));
		}

		ClassesHText.Log(LINE_TERMINATOR);

		// Save the classes header if it has changed.
		FString ClassesHeaderPath = GeneratedIncludeDirectory / (PackageName + TEXT("Classes.h"));
		SaveHeaderIfChanged(*ClassesHeaderPath, *ClassesHText);
	}

	// now export the names for the functions in this package
	// notice we always export this file (as opposed to only exporting if we have any marked names)
	// because there would be no way to know when the file was created otherwise
	UE_LOG(LogCompile, Log, TEXT("Generating code for module '%s'"), *PackageName);

	if (GeneratedFunctionDeclarations.Len())
	{
		uint32 CombinedCRC = 0;
		for (const TPair<FUnrealSourceFile*, FGeneratedCPP>& GeneratedCPP : GeneratedCPPs)
		{
			uint32 SplitCRC = GenerateTextCRC(*GeneratedCPP.Value.GeneratedText);
			if (CombinedCRC == 0)
			{
				// Don't combine in the first case because it keeps GUID backwards compatibility
				CombinedCRC = SplitCRC;
			}
			else
			{
				CombinedCRC = HashCombine(SplitCRC, CombinedCRC);
			}
		}

		FGeneratedCPP& GeneratedCPP = GeneratedCPPs.Emplace(nullptr, GeneratedIncludeDirectory / FString::Printf(TEXT("%s.init.gen.cpp"), *PackageName));
		ExportGeneratedPackageInitCode(GeneratedCPP.GeneratedText, *GeneratedFunctionDeclarations, Package, CombinedCRC);
	}

	const FManifestModule* ModuleInfo = GPackageToManifestModuleMap.FindChecked(Package);

	// Find other includes to put at the top of the .cpp
	FUHTStringBuilder OtherIncludes;
	if (ModuleInfo->PCH.Len())
	{
		FString PCH = ModuleInfo->PCH;
		ConvertToBuildIncludePath(Package, PCH);
		OtherIncludes.Logf(TEXT("#include \"%s\"") LINE_TERMINATOR, *PCH);
	}

	// Generate CPP files
	TArray<FString> GeneratedCPPNames;
	for (const TPair<FUnrealSourceFile*, FGeneratedCPP>& GeneratedCPP : GeneratedCPPs)
	{
		FUHTStringBuilder FileText;

		FString GeneratedIncludes = OtherIncludes;
		for (const FString& RelativeInclude : GeneratedCPP.Value.RelativeIncludes)
		{
			GeneratedIncludes += FString::Printf(TEXT("#include \"%s\"\r\n"), *RelativeInclude);
		}

		ExportGeneratedCPP(
			FileText,
			GeneratedCPP.Value.CrossModuleReferences,
			*FPaths::GetCleanFilename(GeneratedCPP.Value.GeneratedCppFullFilename).Replace(TEXT(".gen.cpp"), TEXT("")).Replace(TEXT("."), TEXT("_")),
			*GeneratedCPP.Value.GeneratedText,
			*GeneratedIncludes
		);

		SaveHeaderIfChanged(*GeneratedCPP.Value.GeneratedCppFullFilename, *FileText);

		GeneratedCPPNames.Add(FPaths::GetCleanFilename(*GeneratedCPP.Value.GeneratedCppFullFilename));
	}

	if (bAllowSaveExportedHeaders)
	{
		// Delete old generated .cpp files which we don't need because we generated less code than last time.
		TArray<FString> FoundFiles;
		FString BaseDir = FPaths::GetPath(ModuleInfo->GeneratedCPPFilenameBase);
		IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(BaseDir, TEXT("*.generated.cpp")), true, false);
		IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(BaseDir, TEXT("*.generated.*.cpp")), true, false);
		IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(BaseDir, TEXT("*.gen.cpp")), true, false);
		IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(BaseDir, TEXT("*.gen.*.cpp")), true, false);
		for (FString& File : FoundFiles)
		{
			if (!GeneratedCPPNames.Contains(File))
			{
				IFileManager::Get().Delete(*FPaths::Combine(*BaseDir, *File));
			}
		}
	}

	// Export all changed headers from their temp files to the .h files
	ExportUpdatedHeaders(PackageName);

	// Delete stale *.generated.h files
	DeleteUnusedGeneratedHeaders();
}

void FNativeClassHeaderGenerator::DeleteUnusedGeneratedHeaders()
{
	TSet<FString> AllIntermediateFolders;
	TSet<FString> PackageHeaderPathSet(PackageHeaderPaths);

	for (const FString& PackageHeader : PackageHeaderPaths)
	{
		const FString IntermediatePath = FPaths::GetPath(PackageHeader);

		if (AllIntermediateFolders.Contains(IntermediatePath))
		{
			continue;
		}

		AllIntermediateFolders.Add( IntermediatePath );

		TArray<FString> AllHeaders;
		IFileManager::Get().FindFiles( AllHeaders, *(IntermediatePath / TEXT("*.generated.h")), true, false );

		for (const FString& Header : AllHeaders)
		{
			const FString HeaderPath = IntermediatePath / Header;

			if (PackageHeaderPathSet.Contains(HeaderPath))
			{
				continue;
			}

			// Check intrinsic classes. Get the class name from file name by removing .generated.h.
			const FString HeaderFilename = FPaths::GetBaseFilename(HeaderPath);
			const int32   GeneratedIndex = HeaderFilename.Find(TEXT(".generated"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			const FString ClassName      = HeaderFilename.Mid(0, GeneratedIndex);
			UClass* IntrinsicClass       = FindObject<UClass>(ANY_PACKAGE, *ClassName);
			if (!IntrinsicClass || !IntrinsicClass->HasAnyClassFlags(CLASS_Intrinsic))
			{
				IFileManager::Get().Delete(*HeaderPath);
			}
		}
	}
}

/**
 * Dirty hack global variable to allow different result codes passed through
 * exceptions. Needs to be fixed in future versions of UHT.
 */
ECompilationResult::Type GCompilationResult = ECompilationResult::OtherCompilationError;

bool FNativeClassHeaderGenerator::SaveHeaderIfChanged(const TCHAR* HeaderPath, const TCHAR* InNewHeaderContents)
{
	if ( !bAllowSaveExportedHeaders )
	{
		// Return false indicating that the header did not need updating
		return false;
	}

	const TCHAR* NewHeaderContents = InNewHeaderContents;
	static bool bTestedCmdLine = false;
	if (!bTestedCmdLine)
	{
		bTestedCmdLine = true;

		const FString ReferenceGeneratedCodePath = FPaths::ProjectSavedDir() / TEXT("ReferenceGeneratedCode/");
		const FString VerifyGeneratedCodePath = FPaths::ProjectSavedDir() / TEXT("VerifyGeneratedCode/");

		if (FParse::Param(FCommandLine::Get(), TEXT("WRITEREF")))
		{
			bWriteContents = true;
			UE_LOG(LogCompile, Log, TEXT("********************************* Writing reference generated code to %s."), *ReferenceGeneratedCodePath);
			UE_LOG(LogCompile, Log, TEXT("********************************* Deleting all files in ReferenceGeneratedCode."));
			IFileManager::Get().DeleteDirectory(*ReferenceGeneratedCodePath, false, true);
			IFileManager::Get().MakeDirectory(*ReferenceGeneratedCodePath);
		}
		else if (FParse::Param( FCommandLine::Get(), TEXT("VERIFYREF")))
		{
			bVerifyContents = true;
			UE_LOG(LogCompile, Log, TEXT("********************************* Writing generated code to %s and comparing to %s"), *VerifyGeneratedCodePath, *ReferenceGeneratedCodePath);
			UE_LOG(LogCompile, Log, TEXT("********************************* Deleting all files in VerifyGeneratedCode."));
			IFileManager::Get().DeleteDirectory(*VerifyGeneratedCodePath, false, true);
			IFileManager::Get().MakeDirectory(*VerifyGeneratedCodePath);
		}
	}

	if (bWriteContents || bVerifyContents)
	{
		FString Ref    = FPaths::ProjectSavedDir() / TEXT("ReferenceGeneratedCode") / FPaths::GetCleanFilename(HeaderPath);
		FString Verify = FPaths::ProjectSavedDir() / TEXT("VerifyGeneratedCode") / FPaths::GetCleanFilename(HeaderPath);

		if (bWriteContents)
		{
			int32 i;
			for (i = 0 ;i < 10; i++)
			{
				if (FFileHelper::SaveStringToFile(NewHeaderContents, *Ref))
				{
					break;
				}
				FPlatformProcess::Sleep(1.0f); // I don't know why this fails after we delete the directory
			}
			check(i<10);
		}
		else
		{
			int32 i;
			for (i = 0 ;i < 10; i++)
			{
				if (FFileHelper::SaveStringToFile(NewHeaderContents, *Verify))
				{
					break;
				}
				FPlatformProcess::Sleep(1.0f); // I don't know why this fails after we delete the directory
			}
			check(i<10);
			FString RefHeader;
			FString Message;
			if (!FFileHelper::LoadFileToString(RefHeader, *Ref))
			{
				Message = FString::Printf(TEXT("********************************* %s appears to be a new generated file."), *FPaths::GetCleanFilename(HeaderPath));
			}
			else
			{
				if (FCString::Strcmp(NewHeaderContents, *RefHeader) != 0)
				{
					Message = FString::Printf(TEXT("********************************* %s has changed."), *FPaths::GetCleanFilename(HeaderPath));
				}
			}
			if (Message.Len())
			{
				UE_LOG(LogCompile, Log, TEXT("%s"), *Message);
				ChangeMessages.AddUnique(Message);
			}
		}
	}


	FString OriginalHeaderLocal;
	FFileHelper::LoadFileToString(OriginalHeaderLocal, HeaderPath);

	const bool bHasChanged = OriginalHeaderLocal.Len() == 0 || FCString::Strcmp(*OriginalHeaderLocal, NewHeaderContents);
	if (bHasChanged)
	{
		if (bFailIfGeneratedCodeChanges)
		{
			FString ConflictPath = FString(HeaderPath) + TEXT(".conflict");
			FFileHelper::SaveStringToFile(NewHeaderContents, *ConflictPath);

			GCompilationResult = ECompilationResult::FailedDueToHeaderChange;
			FError::Throwf(TEXT("ERROR: '%s': Changes to generated code are not allowed - conflicts written to '%s'"), HeaderPath, *ConflictPath);
		}

		// save the updated version to a tmp file so that the user can see what will be changing
		const FString TmpHeaderFilename = GenerateTempHeaderName( HeaderPath, false );

		// delete any existing temp file
		IFileManager::Get().Delete( *TmpHeaderFilename, false, true );
		if ( !FFileHelper::SaveStringToFile(NewHeaderContents, *TmpHeaderFilename) )
		{
			UE_LOG_WARNING_UHT(TEXT("Failed to save header export preview: '%s'"), *TmpHeaderFilename);
		}

		TempHeaderPaths.Add(TmpHeaderFilename);
	}

	// Remember this header filename to be able to check for any old (unused) headers later.
	PackageHeaderPaths.Add(FString(HeaderPath).Replace(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive));

	return bHasChanged;
}

/**
* Create a temp header file name from the header name
*
* @param	CurrentFilename		The filename off of which the current filename will be generated
* @param	bReverseOperation	Get the header from the temp file name instead
*
* @return	The generated string
*/
FString FNativeClassHeaderGenerator::GenerateTempHeaderName( FString CurrentFilename, bool bReverseOperation )
{
	return bReverseOperation
		? CurrentFilename.Replace(TEXT(".tmp"), TEXT(""))
		: CurrentFilename + TEXT(".tmp");
}

/**
* Exports the temp header files into the .h files, then deletes the temp files.
*
* @param	PackageName	Name of the package being saved
*/
void FNativeClassHeaderGenerator::ExportUpdatedHeaders(FString PackageName)
{
	for (const FString& TmpFilename : TempHeaderPaths)
	{
		FString Filename = GenerateTempHeaderName( TmpFilename, true );
		if (!IFileManager::Get().Move(*Filename, *TmpFilename, true, true))
		{
			UE_LOG(LogCompile, Error, TEXT("Error exporting %s: couldn't write file '%s'"), *PackageName, *Filename);
		}
		else
		{
			UE_LOG(LogCompile, Log, TEXT("Exported updated C++ header: %s"), *Filename);
		}
	}
}

/**
 * Exports C++ definitions for boilerplate that was generated for a package.
 */
void FNativeClassHeaderGenerator::ExportGeneratedCPP(FOutputDevice& Out, const TSet<FString>& InCrossModuleReferences, const TCHAR* EmptyLinkFunctionPostfix, const TCHAR* Body, const TCHAR* OtherIncludes)
{
	static const TCHAR EnableDeprecationWarnings [] = TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR;
	static const TCHAR DisableDeprecationWarnings[] = TEXT("PRAGMA_DISABLE_DEPRECATION_WARNINGS") LINE_TERMINATOR;
	static const TCHAR DisableWarning4883        [] = TEXT("#ifdef _MSC_VER") LINE_TERMINATOR TEXT("#pragma warning (push)") LINE_TERMINATOR TEXT("#pragma warning (disable : 4883)") LINE_TERMINATOR TEXT("#endif") LINE_TERMINATOR;
	static const TCHAR EnableWarning4883         [] = TEXT("#ifdef _MSC_VER") LINE_TERMINATOR TEXT("#pragma warning (pop)") LINE_TERMINATOR TEXT("#endif") LINE_TERMINATOR;

	Out.Log(HeaderCopyright);
	Out.Log(RequiredCPPIncludes);
	Out.Log(OtherIncludes);
	Out.Log(DisableWarning4883);
	Out.Log(DisableDeprecationWarnings);

	Out.Logf(TEXT("void EmptyLinkFunctionForGeneratedCode%s() {}") LINE_TERMINATOR, EmptyLinkFunctionPostfix);

	if (InCrossModuleReferences.Num() > 0)
	{
		Out.Logf(TEXT("// Cross Module References\r\n"));
		for (const FString& Ref : InCrossModuleReferences)
		{
			Out.Log(*Ref);
		}
		Out.Logf(TEXT("// End Cross Module References\r\n"));
	}
	Out.Log(Body);
	Out.Log(EnableDeprecationWarnings);
	Out.Log(EnableWarning4883);
}

/** Get all script plugins based on ini setting */
void GetScriptPlugins(TArray<IScriptGeneratorPluginInterface*>& ScriptPlugins)
{
	FScopedDurationTimer PluginTimeTracker(GPluginOverheadTime);

	ScriptPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IScriptGeneratorPluginInterface>(TEXT("ScriptGenerator"));
	UE_LOG(LogCompile, Log, TEXT("Found %d script generator plugins."), ScriptPlugins.Num());

	// Check if we can use these plugins and initialize them
	for (int32 PluginIndex = ScriptPlugins.Num() - 1; PluginIndex >= 0; --PluginIndex)
	{
		auto ScriptGenerator = ScriptPlugins[PluginIndex];
		bool bSupportedPlugin = ScriptGenerator->SupportsTarget(GManifest.TargetName);
		if (bSupportedPlugin)
		{
			// Find the right output directory for this plugin base on its target (Engine-side) plugin name.
			FString GeneratedCodeModuleName = ScriptGenerator->GetGeneratedCodeModuleName();
			const FManifestModule* GeneratedCodeModule = NULL;
			FString OutputDirectory;
			FString IncludeBase;
			for (const FManifestModule& Module : GManifest.Modules)
			{
				if (Module.Name == GeneratedCodeModuleName)
				{
					GeneratedCodeModule = &Module;
				}
			}
			if (GeneratedCodeModule)
			{
				UE_LOG(LogCompile, Log, TEXT("Initializing script generator \'%s\'"), *ScriptGenerator->GetGeneratorName());
				ScriptGenerator->Initialize(GManifest.RootLocalPath, GManifest.RootBuildPath, GeneratedCodeModule->GeneratedIncludeDirectory, GeneratedCodeModule->IncludeBase);
			}
			else
			{
				// Can't use this plugin
				UE_LOG(LogCompile, Log, TEXT("Unable to determine output directory for %s. Cannot export script glue with \'%s\'"), *GeneratedCodeModuleName, *ScriptGenerator->GetGeneratorName());
				bSupportedPlugin = false;
			}
		}
		if (!bSupportedPlugin)
		{
			UE_LOG(LogCompile, Log, TEXT("Script generator \'%s\' not supported for target: %s"), *ScriptGenerator->GetGeneratorName(), *GManifest.TargetName);
			ScriptPlugins.RemoveAt(PluginIndex);
		}
	}
}

/**
 * Tries to resolve super classes for classes defined in the given
 * module.
 *
 * @param Package Modules package.
 */
void ResolveSuperClasses(UPackage* Package)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(Package, Objects);

	for (UObject* Object : Objects)
	{
		if (!Object->IsA<UClass>())
		{
			continue;
		}

		UClass* DefinedClass = Cast<UClass>(Object);

		if (DefinedClass->HasAnyClassFlags(CLASS_Intrinsic | CLASS_NoExport))
		{
			continue;
		}

		const FSimplifiedParsingClassInfo& ParsingInfo = GTypeDefinitionInfoMap[DefinedClass]->GetUnrealSourceFile().GetDefinedClassParsingInfo(DefinedClass);

		const FString& BaseClassName         = ParsingInfo.GetBaseClassName();
		const FString& BaseClassNameStripped = GetClassNameWithPrefixRemoved(BaseClassName);

		if (!BaseClassNameStripped.IsEmpty() && !DefinedClass->GetSuperClass())
		{
			UClass* FoundBaseClass = FindObject<UClass>(Package, *BaseClassNameStripped);

			if (FoundBaseClass == nullptr)
			{
				FoundBaseClass = FindObject<UClass>(ANY_PACKAGE, *BaseClassNameStripped);
			}

			if (FoundBaseClass == nullptr)
			{
				// Don't know its parent class. Raise error.
				FError::Throwf(TEXT("Couldn't find parent type for '%s' named '%s' in current module or any other module parsed so far."), *DefinedClass->GetName(), *BaseClassName);
			}

			DefinedClass->SetSuperStruct(FoundBaseClass);
			DefinedClass->ClassCastFlags |= FoundBaseClass->ClassCastFlags;
		}
	}
}

ECompilationResult::Type PreparseModules(const FString& ModuleInfoPath, int32& NumFailures)
{
	// Three passes.  1) Public 'Classes' headers (legacy)  2) Public headers   3) Private headers
	enum EHeaderFolderTypes
	{
		PublicClassesHeaders = 0,
		PublicHeaders = 1,
		PrivateHeaders,

		FolderType_Count
	};

	ECompilationResult::Type Result = ECompilationResult::Succeeded;
	for (FManifestModule& Module : GManifest.Modules)
	{
		if (Result != ECompilationResult::Succeeded)
		{
			break;
		}

		// Force regeneration of all subsequent modules, otherwise data will get corrupted.
		Module.ForceRegeneration();

		UPackage* Package = Cast<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), NULL, FName(*Module.LongPackageName), false, false));
		if (Package == NULL)
		{
			Package = CreatePackage(NULL, *Module.LongPackageName);
		}
		// Set some package flags for indicating that this package contains script
		// NOTE: We do this even if we didn't have to create the package, because CoreUObject is compiled into UnrealHeaderTool and we still
		//       want to make sure our flags get set
		Package->SetPackageFlags(PKG_ContainsScript | PKG_Compiling);
		Package->ClearPackageFlags(PKG_ClientOptional | PKG_ServerSideOnly);
		if (Module.ModuleType == EBuildModuleType::GameEditor || Module.ModuleType == EBuildModuleType::EngineEditor)
		{
			Package->SetPackageFlags(PKG_EditorOnly);
		}

		if (Module.ModuleType == EBuildModuleType::GameDeveloper || Module.ModuleType == EBuildModuleType::EngineDeveloper)
		{
			Package->SetPackageFlags(Package->GetPackageFlags() | PKG_Developer);
		}

		// Add new module or overwrite whatever we had loaded, that data is obsolete.
		GPackageToManifestModuleMap.Add(Package, &Module);

		double ThisModulePreparseTime = 0.0;
		int32 NumHeadersPreparsed = 0;
		FDurationTimer ThisModuleTimer(ThisModulePreparseTime);
		ThisModuleTimer.Start();

		// Pre-parse the headers
		for (int32 PassIndex = 0; PassIndex < FolderType_Count && Result == ECompilationResult::Succeeded; ++PassIndex)
		{
			EHeaderFolderTypes CurrentlyProcessing = (EHeaderFolderTypes)PassIndex;

			// We'll make an ordered list of all UObject headers we care about.
			// @todo uht: Ideally 'dependson' would not be allowed from public -> private, or NOT at all for new style headers
			const TArray<FString>& UObjectHeaders =
				(CurrentlyProcessing == PublicClassesHeaders) ? Module.PublicUObjectClassesHeaders :
				(CurrentlyProcessing == PublicHeaders       ) ? Module.PublicUObjectHeaders        :
				                                                Module.PrivateUObjectHeaders;
			if (!UObjectHeaders.Num())
			{
				continue;
			}

			NumHeadersPreparsed += UObjectHeaders.Num();

			for (const FString& RawFilename : UObjectHeaders)
			{
#if !PLATFORM_EXCEPTIONS_DISABLED
				try
#endif
				{
					// Import class.
					const FString FullFilename = FPaths::ConvertRelativePathToFull(ModuleInfoPath, RawFilename);

					FString HeaderFile;
					if (!FFileHelper::LoadFileToString(HeaderFile, *FullFilename))
					{
						FError::Throwf(TEXT("UnrealHeaderTool was unable to load source file '%s'"), *FullFilename);
					}

					TSharedRef<FUnrealSourceFile> UnrealSourceFile = PerformInitialParseOnHeader(Package, *RawFilename, RF_Public | RF_Standalone, *HeaderFile);
					FUnrealSourceFile* UnrealSourceFilePtr = &UnrealSourceFile.Get();
					TArray<UClass*> DefinedClasses = UnrealSourceFile->GetDefinedClasses();
					GUnrealSourceFilesMap.Add(RawFilename, UnrealSourceFile);

					if (CurrentlyProcessing == PublicClassesHeaders)
					{
						GPublicSourceFileSet.Add(UnrealSourceFilePtr);
					}

					// Save metadata for the class path, both for it's include path and relative to the module base directory
					if (FullFilename.StartsWith(Module.BaseDirectory))
					{
						// Get the path relative to the module directory
						const TCHAR* ModuleRelativePath = *FullFilename + Module.BaseDirectory.Len();

						UnrealSourceFile->SetModuleRelativePath(ModuleRelativePath);

						// Calculate the include path
						const TCHAR* IncludePath = ModuleRelativePath;

						// Walk over the first potential slash
						if (*IncludePath == TEXT('/'))
						{
							IncludePath++;
						}

						// Does this module path start with a known include path location? If so, we can cut that part out of the include path
						static const TCHAR PublicFolderName[]  = TEXT("Public/");
						static const TCHAR PrivateFolderName[] = TEXT("Private/");
						static const TCHAR ClassesFolderName[] = TEXT("Classes/");
						if (FCString::Strnicmp(IncludePath, PublicFolderName, ARRAY_COUNT(PublicFolderName) - 1) == 0)
						{
							IncludePath += (ARRAY_COUNT(PublicFolderName) - 1);
						}
						else if (FCString::Strnicmp(IncludePath, PrivateFolderName, ARRAY_COUNT(PrivateFolderName) - 1) == 0)
						{
							IncludePath += (ARRAY_COUNT(PrivateFolderName) - 1);
						}
						else if (FCString::Strnicmp(IncludePath, ClassesFolderName, ARRAY_COUNT(ClassesFolderName) - 1) == 0)
						{
							IncludePath += (ARRAY_COUNT(ClassesFolderName) - 1);
						}

						// Add the include path
						if (*IncludePath != 0)
						{
							UnrealSourceFile->SetIncludePath(MoveTemp(IncludePath));
						}
					}
				}
#if !PLATFORM_EXCEPTIONS_DISABLED
				catch (const FFileLineException& Ex)
				{
					TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

					FString AbsFilename           = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Ex.Filename);
					FString Prefix                = FString::Printf(TEXT("%s(%d): "), *AbsFilename, Ex.Line);
					FString FormattedErrorMessage = FString::Printf(TEXT("%sError: %s\r\n"), *Prefix, *Ex.Message);
					Result = GCompilationResult;

					UE_LOG(LogCompile, Log, TEXT("%s"), *FormattedErrorMessage);
					GWarn->Log(ELogVerbosity::Error, FormattedErrorMessage);

					++NumFailures;
				}
				catch (TCHAR* ErrorMsg)
				{
					TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

					FString AbsFilename           = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RawFilename);
					FString Prefix                = FString::Printf(TEXT("%s(1): "), *AbsFilename);
					FString FormattedErrorMessage = FString::Printf(TEXT("%sError: %s\r\n"), *Prefix, ErrorMsg);
					Result = GCompilationResult;

					UE_LOG(LogCompile, Log, TEXT("%s"), *FormattedErrorMessage);
					GWarn->Log(ELogVerbosity::Error, FormattedErrorMessage);

					++NumFailures;
				}
#endif
			}
			if (Result == ECompilationResult::Succeeded && NumFailures != 0)
			{
				Result = ECompilationResult::OtherCompilationError;
			}
		}

		// Don't resolve superclasses for module when loading from makefile.
		// Data is only partially loaded at this point.
#if !PLATFORM_EXCEPTIONS_DISABLED
		try
#endif
		{
			ResolveSuperClasses(Package);
		}
#if !PLATFORM_EXCEPTIONS_DISABLED
		catch (TCHAR* ErrorMsg)
		{
			TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

			FString FormattedErrorMessage = FString::Printf(TEXT("Error: %s\r\n"), ErrorMsg);

			Result = GCompilationResult;

			UE_LOG(LogCompile, Log, TEXT("%s"), *FormattedErrorMessage);
			GWarn->Log(ELogVerbosity::Error, FormattedErrorMessage);

			++NumFailures;
		}
#endif

		ThisModuleTimer.Stop();
		UE_LOG(LogCompile, Log, TEXT("Preparsed module %s containing %i files(s) in %.2f secs."), *Module.LongPackageName, NumHeadersPreparsed, ThisModulePreparseTime);
	}

	return Result;
}

ECompilationResult::Type UnrealHeaderTool_Main(const FString& ModuleInfoFilename)
{
	check(GIsUCCMakeStandaloneHeaderGenerator);
	ECompilationResult::Type Result = ECompilationResult::Succeeded;

	FString ModuleInfoPath = FPaths::GetPath(ModuleInfoFilename);

	// Load the manifest file, giving a list of all modules to be processed, pre-sorted by dependency ordering
#if !PLATFORM_EXCEPTIONS_DISABLED
	try
#endif
	{
		GManifest = FManifest::LoadFromFile(ModuleInfoFilename);
	}
#if !PLATFORM_EXCEPTIONS_DISABLED
	catch (const TCHAR* Ex)
	{
		UE_LOG(LogCompile, Error, TEXT("Failed to load manifest file '%s': %s"), *ModuleInfoFilename, Ex);
		return GCompilationResult;
	}
#endif

	// Counters.
	int32 NumFailures = 0;
	double TotalModulePreparseTime = 0.0;
	double TotalParseAndCodegenTime = 0.0;

	{
		FDurationTimer TotalModulePreparseTimer(TotalModulePreparseTime);
		TotalModulePreparseTimer.Start();
		Result = PreparseModules(ModuleInfoPath, NumFailures);
		TotalModulePreparseTimer.Stop();
	}
	// Do the actual parse of the headers and generate for them
	if (Result == ECompilationResult::Succeeded)
	{
		FScopedDurationTimer ParseAndCodeGenTimer(TotalParseAndCodegenTime);

		// Verify that all script declared superclasses exist.
		for (const UClass* ScriptClass : TObjectRange<UClass>())
		{
			const UClass* ScriptSuperClass = ScriptClass->GetSuperClass();

			if (ScriptSuperClass && !ScriptSuperClass->HasAnyClassFlags(CLASS_Intrinsic) && GTypeDefinitionInfoMap.Contains(ScriptClass) && !GTypeDefinitionInfoMap.Contains(ScriptSuperClass))
			{
				class FSuperClassContextSupplier : public FContextSupplier
				{
				public:
					FSuperClassContextSupplier(const UClass* Class)
						: DefinitionInfo(GTypeDefinitionInfoMap[Class])
					{ }

					virtual FString GetContext() override
					{
						FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*DefinitionInfo->GetUnrealSourceFile().GetFilename());
						int32 LineNumber = DefinitionInfo->GetLineNumber();
						return FString::Printf(TEXT("%s(%i)"), *Filename, LineNumber);
					}
				private:
					TSharedRef<FUnrealTypeDefinitionInfo> DefinitionInfo;
				} ContextSupplier(ScriptClass);

				auto OldContext = GWarn->GetContext();

				TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

				GWarn->SetContext(&ContextSupplier);
				GWarn->Log(ELogVerbosity::Error, FString::Printf(TEXT("Error: Superclass %s of class %s not found"), *ScriptSuperClass->GetName(), *ScriptClass->GetName()));
				GWarn->SetContext(OldContext);

				Result = ECompilationResult::OtherCompilationError;
				++NumFailures;
			}
		}

		if (Result == ECompilationResult::Succeeded)
		{
			TArray<IScriptGeneratorPluginInterface*> ScriptPlugins;
			// Can only export scripts for game targets
			if (GManifest.IsGameTarget)
			{
				GetScriptPlugins(ScriptPlugins);
			}

			for (const FManifestModule& Module : GManifest.Modules)
			{
				if (UPackage* Package = Cast<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), NULL, FName(*Module.LongPackageName), false, false)))
				{
					// Object which represents all parsed classes
					FClasses AllClasses(Package);
					AllClasses.Validate();

					Result = FHeaderParser::ParseAllHeadersInside(AllClasses, GWarn, Package, Module, ScriptPlugins);
					if (Result != ECompilationResult::Succeeded)
					{
						++NumFailures;
						break;
					}
				}
			}

			{
				FScopedDurationTimer PluginTimeTracker(GPluginOverheadTime);
				for (IScriptGeneratorPluginInterface* ScriptGenerator : ScriptPlugins)
				{
					ScriptGenerator->FinishExport();
				}
			}

			// Get a list of external dependencies from each enabled plugin
			FString ExternalDependencies;
			for (IScriptGeneratorPluginInterface* ScriptPlugin : ScriptPlugins)
			{
				TArray<FString> PluginExternalDependencies;
				ScriptPlugin->GetExternalDependencies(PluginExternalDependencies);

				for (const FString& PluginExternalDependency : PluginExternalDependencies)
				{
					ExternalDependencies += PluginExternalDependency + LINE_TERMINATOR;
				}
			}
			FFileHelper::SaveStringToFile(ExternalDependencies, *GManifest.ExternalDependenciesFile);
		}
	}

	// Avoid TArray slack for meta data.
	GScriptHelper.Shrink();

	UE_LOG(LogCompile, Log, TEXT("Preparsing %i modules took %.2f seconds"), GManifest.Modules.Num(), TotalModulePreparseTime);
	UE_LOG(LogCompile, Log, TEXT("Parsing took %.2f seconds"), TotalParseAndCodegenTime - GHeaderCodeGenTime);
	UE_LOG(LogCompile, Log, TEXT("Code generation took %.2f seconds"), GHeaderCodeGenTime);
	UE_LOG(LogCompile, Log, TEXT("ScriptPlugin overhead was %.2f seconds"), GPluginOverheadTime);
	UE_LOG(LogCompile, Log, TEXT("Macroize time was %.2f seconds"), GMacroizeTime);

	if (bWriteContents)
	{
		UE_LOG(LogCompile, Log, TEXT("********************************* Wrote reference generated code to ReferenceGeneratedCode."));
	}
	else if (bVerifyContents)
	{
		UE_LOG(LogCompile, Log, TEXT("********************************* Wrote generated code to VerifyGeneratedCode and compared to ReferenceGeneratedCode"));
		for (FString& Msg : ChangeMessages)
		{
			UE_LOG(LogCompile, Error, TEXT("%s"), *Msg);
		}
		TArray<FString> RefFileNames;
		IFileManager::Get().FindFiles( RefFileNames, *(FPaths::ProjectSavedDir() / TEXT("ReferenceGeneratedCode/*.*")), true, false );
		TArray<FString> VerFileNames;
		IFileManager::Get().FindFiles( VerFileNames, *(FPaths::ProjectSavedDir() / TEXT("VerifyGeneratedCode/*.*")), true, false );
		if (RefFileNames.Num() != VerFileNames.Num())
		{
			UE_LOG(LogCompile, Error, TEXT("Number of generated files mismatch ref=%d, ver=%d"), RefFileNames.Num(), VerFileNames.Num());
		}
	}

	GIsRequestingExit = true;

	if (Result != ECompilationResult::Succeeded || NumFailures > 0)
	{
		return ECompilationResult::OtherCompilationError;
	}

	return Result;
}

UClass* ProcessParsedClass(bool bClassIsAnInterface, TArray<FHeaderProvider>& DependentOn, const FString& ClassName, const FString& BaseClassName, UObject* InParent, EObjectFlags Flags)
{
	FString ClassNameStripped = GetClassNameWithPrefixRemoved(*ClassName);

	// All classes must start with a valid unreal prefix
	if (!FHeaderParser::ClassNameHasValidPrefix(ClassName, ClassNameStripped))
	{
		FError::Throwf(TEXT("Invalid class name '%s'. The class name must have an appropriate prefix added (A for Actors, U for other classes)."), *ClassName);
	}

	// Ensure the base class has any valid prefix and exists as a valid class. Checking for the 'correct' prefix will occur during compilation
	FString BaseClassNameStripped;
	if (!BaseClassName.IsEmpty())
	{
		BaseClassNameStripped = GetClassNameWithPrefixRemoved(BaseClassName);
		if (!FHeaderParser::ClassNameHasValidPrefix(BaseClassName, BaseClassNameStripped))
		{
			FError::Throwf(TEXT("No prefix or invalid identifier for base class %s.\nClass names must match Unreal prefix specifications (e.g., \"UObject\" or \"AActor\")"), *BaseClassName);
		}

		if (DependentOn.ContainsByPredicate([&](const FHeaderProvider& Dependency){ FString DependencyStr = Dependency.GetId(); return !DependencyStr.Contains(TEXT(".generated.h")) && FPaths::GetBaseFilename(DependencyStr) == ClassNameStripped; }))
		{
			FError::Throwf(TEXT("Class '%s' contains a dependency (#include or base class) to itself"), *ClassName);
		}
	}

	//UE_LOG(LogCompile, Log, TEXT("Class: %s extends %s"),*ClassName,*BaseClassName);
	// Handle failure and non-class headers.
	if (BaseClassName.IsEmpty() && (ClassName != TEXT("UObject")))
	{
		FError::Throwf(TEXT("Class '%s' must inherit UObject or a UObject-derived class"), *ClassName);
	}

	if (ClassName == BaseClassName)
	{
		FError::Throwf(TEXT("Class '%s' cannot inherit from itself"), *ClassName);
	}

	// In case the file system and the class disagree on the case of the
	// class name replace the fname with the one from the script class file
	// This is needed because not all source control systems respect the
	// original filename's case
	FName ClassNameReplace(*ClassName, FNAME_Replace_Not_Safe_For_Threading);

	// Use stripped class name for processing and replace as we did above
	FName ClassNameStrippedReplace(*ClassNameStripped, FNAME_Replace_Not_Safe_For_Threading);

	UClass* ResultClass = FindObject<UClass>(InParent, *ClassNameStripped);

	// if we aren't generating headers, then we shouldn't set misaligned object, since it won't get cleared

	const static bool bVerboseOutput = FParse::Param(FCommandLine::Get(), TEXT("VERBOSE"));

	if (ResultClass == nullptr || !ResultClass->IsNative())
	{
		// detect if the same class name is used in multiple packages
		if (ResultClass == nullptr)
		{
			UClass* ConflictingClass = FindObject<UClass>(ANY_PACKAGE, *ClassNameStripped, true);
			if (ConflictingClass != nullptr)
			{
				UE_LOG_WARNING_UHT(TEXT("Duplicate class name: %s also exists in file %s"), *ClassName, *ConflictingClass->GetOutermost()->GetName());
			}
		}

		// Create new class.
		ResultClass = new(EC_InternalUseOnlyConstructor, InParent, *ClassNameStripped, Flags) UClass(FObjectInitializer(), nullptr);
		GClassHeaderNameWithNoPathMap.Add(ResultClass, ClassNameStripped);

		// add CLASS_Interface flag if the class is an interface
		// NOTE: at this pre-parsing/importing stage, we cannot know if our super class is an interface or not,
		// we leave the validation to the main header parser
		if (bClassIsAnInterface)
		{
			ResultClass->ClassFlags |= CLASS_Interface;
		}

		if (bVerboseOutput)
		{
			UE_LOG(LogCompile, Log, TEXT("Imported: %s"), *ResultClass->GetFullName());
		}
	}

	if (bVerboseOutput)
	{
		for (const auto& Dependency : DependentOn)
		{
			UE_LOG(LogCompile, Log, TEXT("\tAdding %s as a dependency"), *Dependency.ToString());
		}
	}

	return ResultClass;
}


TSharedRef<FUnrealSourceFile> PerformInitialParseOnHeader(UPackage* InParent, const TCHAR* FileName, EObjectFlags Flags, const TCHAR* Buffer)
{
	const TCHAR* InBuffer = Buffer;

	// is the parsed class name an interface?
	bool bClassIsAnInterface = false;

	TArray<FHeaderProvider> DependsOn;

	// Parse the header to extract the information needed
	FUHTStringBuilder ClassHeaderTextStrippedOfCppText;
	TArray<FSimplifiedParsingClassInfo> ParsedClassArray;
	FHeaderParser::SimplifiedClassParse(FileName, Buffer, /*out*/ ParsedClassArray, /*out*/ DependsOn, ClassHeaderTextStrippedOfCppText);

	FUnrealSourceFile* UnrealSourceFilePtr = new FUnrealSourceFile(InParent, FileName, MoveTemp(ClassHeaderTextStrippedOfCppText));
	TSharedRef<FUnrealSourceFile> UnrealSourceFile = MakeShareable(UnrealSourceFilePtr);
	for (auto& ParsedClassInfo : ParsedClassArray)
	{
		UClass* ResultClass = ProcessParsedClass(ParsedClassInfo.IsInterface(), DependsOn, ParsedClassInfo.GetClassName(), ParsedClassInfo.GetBaseClassName(), InParent, Flags);
		GStructToSourceLine.Add(ResultClass, MakeTuple(UnrealSourceFile, ParsedClassInfo.GetClassDefLine()));

		FScope::AddTypeScope(ResultClass, &UnrealSourceFile->GetScope().Get());

		GTypeDefinitionInfoMap.Add(ResultClass, MakeShared<FUnrealTypeDefinitionInfo>(*UnrealSourceFilePtr, ParsedClassInfo.GetClassDefLine()));
		UnrealSourceFile->AddDefinedClass(ResultClass, MoveTemp(ParsedClassInfo));
	}

	for (auto& DependsOnElement : DependsOn)
	{
		UnrealSourceFile->GetIncludes().AddUnique(DependsOnElement);
	}

	return UnrealSourceFile;
}
