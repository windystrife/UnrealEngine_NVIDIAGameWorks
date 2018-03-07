// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Framework/Commands/Commands.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "Internationalization/StringTableCore.h"
#include "GatherTextFromSourceCommandlet.generated.h"

class Error;

/**
 *	UGatherTextFromSourceCommandlet: Localization commandlet that collects all text to be localized from the source code.
 */
UCLASS()
class UGatherTextFromSourceCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

private:
#define LOC_DEFINE_REGION

	enum class EMacroBlockState : uint8
	{
		Normal,
		EditorOnly,
	};

	struct FSourceLocation
	{
		FSourceLocation()
			: File()
			, Line(INDEX_NONE)
		{
		}

		FSourceLocation(FString InFile, const int32 InLine)
			: File(MoveTemp(InFile))
			, Line(InLine)
		{
		}

		FString ToString() const
		{
			return (Line == INDEX_NONE) ? File : FString::Printf(TEXT("%s - line %d"), *File, Line);
		}

		FString File;
		int32 Line;
	};

	struct FParsedStringTableEntry
	{
		FString SourceString;
		FSourceLocation SourceLocation;
		bool bIsEditorOnly;
	};

	struct FParsedStringTableEntryMetaData
	{
		FString MetaData;
		FSourceLocation SourceLocation;
		bool bIsEditorOnly;
	};

	typedef TMap<FName, FParsedStringTableEntryMetaData> FParsedStringTableEntryMetaDataMap;

	struct FParsedStringTable
	{
		FString TableNamespace;
		FSourceLocation SourceLocation;
		TMap<FString, FParsedStringTableEntry, FDefaultSetAllocator, FLocKeyMapFuncs<FParsedStringTableEntry>> TableEntries;
		TMap<FString, FParsedStringTableEntryMetaDataMap, FDefaultSetAllocator, FLocKeyMapFuncs<FParsedStringTableEntryMetaDataMap>> MetaDataEntries;
	};

	struct FSourceFileParseContext
	{
		bool AddManifestText( const FString& Token, const FString& Namespace, const FString& SourceText, const FManifestContext& Context );

		void PushMacroBlock( const FString& InBlockCtx );

		void PopMacroBlock();

		void FlushMacroStack();

		EMacroBlockState EvaluateMacroStack() const;

		void SetDefine( const FString& InDefineCtx );

		void RemoveDefine( const FString& InDefineCtx );

		void AddStringTable( const FName InTableId, const FString& InTableNamespace );

		void AddStringTableFromFile( const FName InTableId, const FString& InTableNamespace, const FString& InTableFilename, const FString& InRootPath );

		void AddStringTableEntry( const FName InTableId, const FString& InKey, const FString& InSourceString );

		void AddStringTableEntryMetaData( const FName InTableId, const FString& InKey, const FName InMetaDataId, const FString& InMetaData );

		//Working data
		FString Filename;
		int32 LineNumber;
		FString LineText;
		FString Namespace;
		bool ExcludedRegion;
		bool EndParsingCurrentLine;
		bool WithinBlockComment;
		bool WithinLineComment;
		bool WithinStringLiteral;
		bool WithinNamespaceDefine;
		FString WithinStartingLine;

		//Should editor-only data be included in this gather?
		bool ShouldGatherFromEditorOnlyData;

		//Destination location of the parsed FLocTextEntrys
		TSharedPtr< FLocTextHelper > GatherManifestHelper;

		//Discovered string table data from all files
		TMap<FName, FParsedStringTable> ParsedStringTables;

		FSourceFileParseContext()
			: Filename()
			, LineNumber(0)
			, LineText()
			, Namespace()
			, ExcludedRegion(false)
			, EndParsingCurrentLine(false)
			, WithinBlockComment(false)
			, WithinLineComment(false)
			, WithinStringLiteral(false)
			, WithinNamespaceDefine(false)
			, WithinStartingLine()
			, ShouldGatherFromEditorOnlyData(false)
			, GatherManifestHelper()
			, MacroBlockStack()
			, CachedMacroBlockState()
		{

		}

	private:
		bool AddStringTableImpl( const FName InTableId, const FString& InTableNamespace );
		bool AddStringTableEntryImpl( const FName InTableId, const FString& InKey, const FString& InSourceString, const FSourceLocation& InSourceLocation );
		bool AddStringTableEntryMetaDataImpl( const FName InTableId, const FString& InKey, const FName InMetaDataId, const FString& InMetaData, const FSourceLocation& InSourceLocation );

		//Working data
		TArray<FString> MacroBlockStack;
		mutable TOptional<EMacroBlockState> CachedMacroBlockState;
	};

	class FParsableDescriptor
	{
	public:
		FParsableDescriptor():bOverridesLongerTokens(false){}
		FParsableDescriptor(bool bOverride):bOverridesLongerTokens(bOverride){}
		virtual ~FParsableDescriptor(){}
		virtual const FString& GetToken() const = 0;
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const = 0;

		bool OverridesLongerTokens(){ return bOverridesLongerTokens; }
	protected:
		bool bOverridesLongerTokens;
	};

	class FPreProcessorDescriptor : public FParsableDescriptor
	{
	public:
		FPreProcessorDescriptor():FParsableDescriptor(true){}
	protected:
		static const FString DefineString;
		static const FString UndefString;
		static const FString IfString;
		static const FString IfDefString;
		static const FString ElIfString;
		static const FString ElseString;
		static const FString EndIfString;
		static const FString DefinedString;
		static const FString IniNamespaceString;
	};

	class FDefineDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::DefineString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FUndefDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::UndefString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FIfDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::IfString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FIfDefDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::IfDefString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FElIfDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::ElIfString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FElseDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::ElseString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FEndIfDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::EndIfString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FMacroDescriptor : public FParsableDescriptor
	{
	public:
		static const FString TextMacroString;

		FMacroDescriptor(FString InName) : Name(MoveTemp(InName)) {}

		virtual const FString& GetToken() const override { return Name; }

	protected:
		bool ParseArgsFromMacro(const FString& Text, TArray<FString>& Args, FSourceFileParseContext& Context) const;

		static bool PrepareArgument(FString& Argument, bool IsAutoText, const FString& IdentForLogging, bool& OutHasQuotes);

	private:
		FString Name;
	};

	class FCommandMacroDescriptor : public FMacroDescriptor
	{
	public:
		FCommandMacroDescriptor() : FMacroDescriptor(TEXT("UI_COMMAND")) {}

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FStringMacroDescriptor : public FMacroDescriptor
	{
	public:
		enum EMacroArgSemantic
		{
			MAS_Namespace,
			MAS_Identifier,
			MAS_SourceText,
		};

		struct FMacroArg
		{
			EMacroArgSemantic Semantic;
			bool IsAutoText;

			FMacroArg(EMacroArgSemantic InSema, bool InIsAutoText) : Semantic(InSema), IsAutoText(InIsAutoText) {}
		};

		FStringMacroDescriptor(FString InName, FMacroArg Arg0, FMacroArg Arg1, FMacroArg Arg2) : FMacroDescriptor(InName)
		{
			Arguments.Add(Arg0);
			Arguments.Add(Arg1);
			Arguments.Add(Arg2);
		}

		FStringMacroDescriptor(FString InName, FMacroArg Arg0, FMacroArg Arg1) : FMacroDescriptor(InName)
		{
			Arguments.Add(Arg0);
			Arguments.Add(Arg1);
		}

		FStringMacroDescriptor(FString InName, FMacroArg Arg0) : FMacroDescriptor(InName)
		{
			Arguments.Add(Arg0);
		}

		virtual void TryParse(const FString& LineText, FSourceFileParseContext& Context) const override;

	private:
		TArray<FMacroArg> Arguments;
	};

	class FStringTableMacroDescriptor : public FMacroDescriptor
	{
	public:
		FStringTableMacroDescriptor() : FMacroDescriptor(TEXT("LOCTABLE_NEW")) {}

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FStringTableFromFileMacroDescriptor : public FMacroDescriptor
	{
	public:
		FStringTableFromFileMacroDescriptor(FString InName, FString InRootPath) : FMacroDescriptor(MoveTemp(InName)), RootPath(MoveTemp(InRootPath)) {}

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;

	private:
		FString RootPath;
	};

	class FStringTableEntryMacroDescriptor : public FMacroDescriptor
	{
	public:
		FStringTableEntryMacroDescriptor() : FMacroDescriptor(TEXT("LOCTABLE_SETSTRING")) {}

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FStringTableEntryMetaDataMacroDescriptor : public FMacroDescriptor
	{
	public:
		FStringTableEntryMetaDataMacroDescriptor() : FMacroDescriptor(TEXT("LOCTABLE_SETMETA")) {}

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FIniNamespaceDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::IniNamespaceString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	static const FString ChangelistName;

	static FString UnescapeLiteralCharacterEscapeSequences(const FString& InString);
	static FString RemoveStringFromTextMacro(const FString& TextMacro, const FString& IdentForLogging, bool& Error);
	static FString StripCommentsFromToken(const FString& InToken, FSourceFileParseContext& Context);
	static bool ParseSourceText(const FString& Text, const TArray<FParsableDescriptor*>& Parsables, FSourceFileParseContext& ParseCtxt);

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

#undef LOC_DEFINE_REGION
};
