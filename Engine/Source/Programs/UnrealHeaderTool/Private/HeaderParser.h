// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ParserHelper.h"
#include "BaseParser.h"
#include "Misc/CompilationResult.h"
#include "Scope.h"

class UClass;
enum class EGeneratedCodeVersion : uint8;
class FFeedbackContext;
class UPackage;
struct FManifestModule;
class IScriptGeneratorPluginInterface;
class FStringOutputDevice;
class UProperty;
class FUnrealSourceFile;
class UFunction;
class UEnum;
class UScriptStruct;
class UDelegateFunction;
class UStruct;
class FClass;
class FClasses;
class FScope;
class FHeaderProvider;

extern double GPluginOverheadTime;
extern double GHeaderCodeGenTime;

/*-----------------------------------------------------------------------------
	Constants & types.
-----------------------------------------------------------------------------*/

enum {MAX_NEST_LEVELS = 16};

/* Code nesting types. */
enum class ENestType
{
	GlobalScope,
	Class,
	FunctionDeclaration,
	Interface,
	NativeInterface
};

/** Types of statements to allow within a particular nesting block. */
enum class ENestAllowFlags
{
	None                 = 0,
	Function             = 1,  // Allow Event declarations at this level.
	VarDecl              = 2,  // Allow variable declarations at this level.
	Class                = 4,  // Allow class definition heading.
	Return               = 8,  // Allow 'return' within a function.
	TypeDecl             = 16, // Allow declarations which do not affect memory layout, such as structs, enums, and consts, but not implicit delegates
	ImplicitDelegateDecl = 32, // Allow implicit delegates (i.e. those not decorated with UDELEGATE) to be declared
};

ENUM_CLASS_FLAGS(ENestAllowFlags)

namespace EDelegateSpecifierAction
{
	enum Type
	{
		DontParse,
		Parse
	};
}

/** The category of variable declaration being parsed */
namespace EVariableCategory
{
	enum Type
	{
		RegularParameter,
		ReplicatedParameter,
		Return,
		Member
	};
}

/** Information for a particular nesting level. */
class FNestInfo
{
	/** Link to the stack node. */
	FScope* Scope;

public:
	/**
	 * Gets nesting scope.
	 */
	FScope* GetScope() const
	{
		return Scope;
	}

	/**
	 * Sets nesting scope.
	 */
	void SetScope(FScope* InScope)
	{
		this->Scope = InScope;
	}

	/** Statement that caused the nesting. */
	ENestType NestType;

	/** Types of statements to allow at this nesting level. */
	ENestAllowFlags Allow;
};

struct FIndexRange
{
	int32 StartIndex;
	int32 Count;
};


struct ClassDefinitionRange
{
	ClassDefinitionRange(const TCHAR* InStart, const TCHAR* InEnd)
		: Start(InStart)
		, End(InEnd)
		, bHasGeneratedBody(false)
	{ }

	ClassDefinitionRange()
		: Start(nullptr)
		, End(nullptr)
		, bHasGeneratedBody(false)
	{ }

	void Validate()
	{
		if (End <= Start)
		{
			FError::Throwf(TEXT("The class definition range is invalid. Most probably caused by previous parsing error."));
		}
	}

	const TCHAR* Start;
	const TCHAR* End;
	bool bHasGeneratedBody;
};

extern TMap<UClass*, ClassDefinitionRange> ClassDefinitionRanges;

/////////////////////////////////////////////////////
// FHeaderParser

//
// Header parser class.  Extracts metadata from annotated C++ headers and gathers enough
// information to autogenerate additional headers and other boilerplate code.
//
class FHeaderParser : public FBaseParser, public FContextSupplier
{
public:
	// Default version of generated code. Defaults to oldest possible, unless specified otherwise in config.
	static EGeneratedCodeVersion DefaultGeneratedCodeVersion;

	// Compute the function parameter size and save the return offset
	static void ComputeFunctionParametersSize(UClass* InClass);

	// Parse all headers for classes that are inside LimitOuter.
	static ECompilationResult::Type ParseAllHeadersInside(
		FClasses& ModuleClasses,
		FFeedbackContext* Warn,
		UPackage* LimitOuter,
		const FManifestModule& Module,
		TArray<class IScriptGeneratorPluginInterface*>& ScriptPlugins
	);

	// Performs a preliminary parse of the text in the specified buffer, pulling out:
	//   Class name and parent class name
	//   Is it an interface
	//   The list of other classes/interfaces it is dependent on
	//   
	//  It also splits the buffer up into:
	//   ScriptText (text outside of #if CPP and #if DEFAULTS blocks)
	static void SimplifiedClassParse(const TCHAR* Filename, const TCHAR* Buffer, TArray<FSimplifiedParsingClassInfo>& OutParsedClassArray, TArray<FHeaderProvider>& DependentOn, FStringOutputDevice& ScriptText);

	/**
	 * Returns True if the given class name includes a valid Unreal prefix and matches up with the given original class Name.
	 *
	 * @param InNameToCheck - Name w/ potential prefix to check
	 * @param OriginalClassName - Name of class w/ no prefix to check against
	 */
	static bool ClassNameHasValidPrefix(const FString InNameToCheck, const FString OriginalClassName);

	/**
	 * Tries to convert the header file name to a class name (with 'U' prefix)
	 *
	 * @param HeaderFilename Filename.
	 * @param OutClass The resulting class name (if successfull)
	 * @return true if the filename was a header filename (.h), false otherwise (in which case OutClassName is unmodified).
	 */
	static bool DependentClassNameFromHeader(const TCHAR* HeaderFilename, FString& OutClassName);

	/**
	 * Transforms CPP-formated string containing default value, to inner formated string
	 * If it cannot be transformed empty string is returned.
	 *
	 * @param Property The property that owns the default value.
	 * @param CppForm A CPP-formated string.
	 * @param out InnerForm Inner formated string
	 * @return true on success, false otherwise.
	 */
	static bool DefaultValueStringCppFormatToInnerFormat(const UProperty* Property, const FString& CppForm, FString &InnerForm);

	/**
	 * Parse Class's annotated headers and optionally its child classes.  Marks the class as CLASS_Parsed.
	 *
	 * @param	AllClasses			the class tree containing all classes in the current package
	 * @param	HeaderParser		the header parser
	 * @param	SourceFile			Source file info.
	 *
	 * @return	Result enumeration.
	 */
	static ECompilationResult::Type ParseHeaders(FClasses& AllClasses, FHeaderParser& HeaderParser, FUnrealSourceFile* SourceFile);

protected:
	friend struct FScriptLocation;

	// For compiling messages and errors.
	FFeedbackContext* Warn;

	// Filename currently being parsed
	FString Filename;

	// Was the first include in the file a validly formed auto-generated header include?
	bool bSpottedAutogeneratedHeaderInclude;

	// Current nest level, starts at 0.
	int32 NestLevel;

	// Top nesting level.
	FNestInfo* TopNest;

	/**
	 * Gets current nesting scope.
	 */
	FScope* GetCurrentScope() const
	{
		return TopNest->GetScope();
	}

	/**
	 * Gets current file scope.
	 */
	FFileScope* GetCurrentFileScope() const
	{
		int32 Index = 0;
		if (!TopNest)
		{
			check(!NestLevel);
			return nullptr;
		}
		while (TopNest[Index].NestType != ENestType::GlobalScope)
		{
			--Index;
		}

		return (FFileScope*)TopNest[Index].GetScope();
	}

	/**
	 * Gets current source file.
	 */
	FUnrealSourceFile* GetCurrentSourceFile() const
	{
		return CurrentSourceFile;
	}

	void SetCurrentSourceFile(FUnrealSourceFile* UnrealSourceFile)
	{
		CurrentSourceFile = UnrealSourceFile;
	}

	/**
	 * Gets current class scope.
	 */
	FStructScope* GetCurrentClassScope() const
	{
		check(TopNest->NestType == ENestType::Class || TopNest->NestType == ENestType::Interface || TopNest->NestType == ENestType::NativeInterface);

		return (FStructScope*)TopNest->GetScope();
	}

	/**
	 * Tells if parser is currently in a class.
	 */
	bool IsInAClass() const
	{
		int32 Index = 0;
		while (TopNest[Index].NestType != ENestType::GlobalScope)
		{
			if (TopNest[Index].NestType == ENestType::Class || TopNest->NestType == ENestType::Interface || TopNest->NestType == ENestType::NativeInterface)
			{
				return true;
			}

			--Index;
		}

		return false;
	}

	/**
	 * Gets current class.
	 */
	UClass* GetCurrentClass() const
	{
		return (UClass*)GetCurrentClassScope()->GetStruct();
	}

	/**
	 * Gets current class's metadata.
	 */
	FClassMetaData* GetCurrentClassData()
	{
		return GScriptHelper.FindClassData(GetCurrentClass());
	}

	// Information about all nesting levels.
	FNestInfo Nest[MAX_NEST_LEVELS];

	// enum for complier directives used to build up the directive stack
	struct ECompilerDirective
	{
		enum Type 
		{
			// this directive is insignificant and does not change the code generation at all
			Insignificant			= 0,
			// this indicates we are in a WITH_EDITOR #if-Block
			WithEditor				= 1<<0,
			// this indicates we are in a WITH_EDITORONLY_DATA #if-Block
			WithEditorOnlyData		= 1<<1,
		};
	};

	/** 
	 * Compiler directive nest in which the parser currently is
	 * NOTE: compiler directives are combined when more are added onto the stack, so
	 * checking the only the top of stack is enough to determine in which #if-Block(s) the current code
	 * is.
	 *
	 * ex. Stack.Num() == 1 while entering #if WITH_EDITOR: 
	 *		CompilerDirectiveStack[1] == CompilerDirectiveStack[0] | ECompilerDirective::WithEditor == 
	 *		CompilerDirecitveStack[1] == CompilerDirectiveStack.Num()-1 | ECompilerDirective::WithEditor
	 *
	 * ex. Stack.Num() == 2 while entering #if WITH_EDITOR: 
	 *		CompilerDirectiveStack[3] == CompilerDirectiveStack[0] | CompilerDirectiveStack[1] | CompilerDirectiveStack[2] | ECompilerDirective::WithEditor ==
	 *		CompilerDirecitveStack[3] == CompilerDirectiveStack.Num()-1 | ECompilerDirective::WithEditor
	 */
	TArray<uint32> CompilerDirectiveStack;

	// Pushes the Directive specified to the CompilerDirectiveStack according to the rules described above
	void FORCEINLINE PushCompilerDirective(ECompilerDirective::Type Directive)
	{
		CompilerDirectiveStack.Push(CompilerDirectiveStack.Num()>0 ? (CompilerDirectiveStack[CompilerDirectiveStack.Num()-1] | Directive) : Directive);
	}

	/**
	 * The starting class flags (i.e. the class flags that were set before the
	 * CLASS_RecompilerClear mask was applied) for the class currently being compiled
	 */
	uint32 PreviousClassFlags;

	// For new-style classes, used to keep track of an unmatched {} pair
	bool bEncounteredNewStyleClass_UnmatchedBrackets;

	// Indicates that UCLASS/USTRUCT/UINTERFACE has already been parsed in this .h file..
	bool bHaveSeenUClass;

	// Indicates that a GENERATED_UCLASS_BODY or GENERATED_BODY has been found in the UClass.
	bool bClassHasGeneratedBody;

	// Indicates that a GENERATED_UINTERFACE_BODY has been found in the UClass.
	bool bClassHasGeneratedUInterfaceBody;

	// Indicates that a GENERATED_IINTERFACE_BODY has been found in the UClass.
	bool bClassHasGeneratedIInterfaceBody;

	// public, private, etc at the current parse spot
	EAccessSpecifier CurrentAccessSpecifier;

	////////////////////////////////////////////////////

	// Special parsed struct names that do not require a prefix
	static TArray<FString> StructsWithNoPrefix;
	
	// Special parsed struct names that have a 'T' prefix
	static TArray<FString> StructsWithTPrefix;

	// Mapping from 'human-readable' macro substring to # of parameters for delegate declarations
	// Index 0 is 1 parameter, Index 1 is 2, etc...
	static TArray<FString> DelegateParameterCountStrings;

	// Types that have been renamed, treat the old deprecated name as the new name for code generation
	static TMap<FString, FString> TypeRedirectMap;

	// List of all used identifiers for net service function declarations (every function must be unique)
	TMap<int32, FString> UsedRPCIds;
	// List of all net service functions with undeclared response functions 
	TMap<int32, FString> RPCsNeedingHookup;

	// Constructor.
	explicit FHeaderParser(FFeedbackContext* InWarn, const FManifestModule& InModule);

	virtual ~FHeaderParser()
	{
		if ( FScriptLocation::Compiler == this )
		{
			FScriptLocation::Compiler = NULL;
		}
	}

	// Returns true if the token is a dynamic delegate declaration
	bool IsValidDelegateDeclaration(const FToken& Token) const;

	// Returns true if the current token is a bitfield type
	bool IsBitfieldProperty();

	// Parse the parameter list of a function or delegate declaration
	void ParseParameterList(FClasses& AllClasses, UFunction* Function, bool bExpectCommaBeforeName = false, TMap<FName, FString>* MetaData = NULL);

	// Modify token to fix redirected types if needed
	void RedirectTypeIdentifier(FToken& Token) const;

public:
	// Throws if a specifier value wasn't provided
	static void RequireSpecifierValue(const FPropertySpecifier& Specifier, bool bRequireExactlyOne = false);
	static FString RequireExactlyOneSpecifierValue(const FPropertySpecifier& Specifier);

protected:

	/**
	 * Parse rest of the module's source files.
	 *
	 * @param	AllClasses The class tree containing all classes in the current package.
	 * @param	ModulePackage Current package.
	 * @param	HeaderParser The header parser.
	 *
	 * @return	Result enumeration.
	 */
	static ECompilationResult::Type ParseRestOfModulesSourceFiles(FClasses& AllClasses, UPackage* ModulePackage, FHeaderParser& HeaderParser);

	//@TODO: Remove this method
	static void ParseClassName(const TCHAR* Temp, FString& ClassName);

	/**
	 * @param		Input		An input string, expected to be a script comment.
	 * @return					The input string, reformatted in such a way as to be appropriate for use as a tooltip.
	 */
	static FString FormatCommentForToolTip(const FString& Input);
	
	/**
	 * Begins the process of exporting C++ class declarations for native classes in the specified package
	 * 
	 * @param CurrentPackage The package being compiled.
	 * @param AllClasses The class tree for CurrentPackage.
	 * @param Module Currently exported module.
	 */
	static void ExportNativeHeaders(
		UPackage* CurrentPackage,
		FClasses& AllClasses,
		bool bAllowSaveExportedHeaders,
		const FManifestModule& Module
	);

	// FContextSupplier interface.
	virtual FString GetContext() override;
	// End of FContextSupplier interface.

	// High-level compiling functions.
	/**
	 * Parses given source file.
	 *
	 * @param AllClasses The class tree for current package.
	 * @param SourceFile Source file to parse.
	 *
	 * @returns Compilation result enum.
	 */
	ECompilationResult::Type ParseHeader(FClasses& AllClasses, FUnrealSourceFile* SourceFile);
	void CompileDirective(FClasses& AllClasses);
	void FinalizeScriptExposedFunctions(UClass* Class);
	UEnum* CompileEnum();
	UScriptStruct* CompileStructDeclaration(FClasses& AllClasses);
	bool CompileDeclaration(FClasses& AllClasses, TArray<UDelegateFunction*>& DelegatesToFixup, FToken& Token);

	/** Skip C++ (noexport) declaration. */
	bool SkipDeclaration(FToken& Token);
	/** Similar to MatchSymbol() but will return to the exact location as on entry if the symbol was not found. */
	bool SafeMatchSymbol(const TCHAR* Match);
	void HandleOneInheritedClass(FClasses& AllClasses, UClass* Class, FString InterfaceName);
	FClass* ParseClassNameDeclaration(FClasses& AllClasses, FString& DeclaredClassName, FString& RequiredAPIMacroIfPresent);

	/** The property style of a variable declaration being parsed */
	struct EPropertyDeclarationStyle
	{
		enum Type
		{
			None,
			UPROPERTY
		};
	};

	/**
	 * Resets current class data back to its defaults.
	 */
	void ResetClassData();

	/**
	 * Create new function object based on given info structure.
	 */
	UFunction* CreateFunction(const FFuncInfo &FuncInfo) const;

	/**
	 * Create new delegate function object based on given info structure.
	 */
	UDelegateFunction* CreateDelegateFunction(const FFuncInfo &FuncInfo) const;	

	UClass* CompileClassDeclaration(FClasses& AllClasses);
	UDelegateFunction* CompileDelegateDeclaration(FClasses& AllClasses, const TCHAR* DelegateIdentifier, EDelegateSpecifierAction::Type SpecifierAction = EDelegateSpecifierAction::DontParse);
	void CompileFunctionDeclaration(FClasses& AllClasses);
	void CompileVariableDeclaration (FClasses& AllClasses, UStruct* Struct);
	void CompileInterfaceDeclaration(FClasses& AllClasses);

	FClass* ParseInterfaceNameDeclaration(FClasses& AllClasses, FString& DeclaredInterfaceName, FString& RequiredAPIMacroIfPresent);
	bool TryParseIInterfaceClass(FClasses& AllClasses);

	bool CompileStatement(FClasses& AllClasses, TArray<UDelegateFunction*>& DelegatesToFixup);

	// Checks to see if a particular kind of command is allowed on this nesting level.
	bool IsAllowedInThisNesting(ENestAllowFlags AllowFlags);
	
	// Make sure that a particular kind of command is allowed on this nesting level.
	// If it's not, issues a compiler error referring to the token and the current
	// nesting level.
	void CheckAllow(const TCHAR* Thing, ENestAllowFlags AllowFlags);

	UStruct* GetSuperScope( UStruct* CurrentScope, const FName& SearchName );

	/**
	 * Find a field in the specified context.  Starts with the specified scope, then iterates
	 * through the Outer chain until the field is found.
	 * 
	 * @param	InScope				scope to start searching for the field in 
	 * @param	InIdentifier		name of the field we're searching for
	 * @param	bIncludeParents		whether to allow searching in the scope of a parent struct
	 * @param	FieldClass			class of the field to search for.  used to e.g. search for functions only
	 * @param	Thing				hint text that will be used in the error message if an error is encountered
	 *
	 * @return	a pointer to a UField with a name matching InIdentifier, or NULL if it wasn't found
	 */
	UField* FindField( UStruct* InScope, const TCHAR* InIdentifier, bool bIncludeParents=true, UClass* FieldClass=UField::StaticClass(), const TCHAR* Thing=NULL );
	void SkipStatements( int32 SubCount, const TCHAR* ErrorTag );

	/**
	 * Parses a variable or return value declaration and determines the variable type and property flags.
	 *
	 * @param   AllClasses                the class tree for CurrentPackage
	 * @param   Scope                     struct to create the property in
	 * @param   VarProperty               will be filled in with type and property flag data for the property declaration that was parsed
	 * @param   Disallow                  contains a mask of variable modifiers that are disallowed in this context
	 * @param   OuterPropertyType         only specified when compiling the inner properties for arrays or maps.  corresponds to the FToken for the outer property declaration.
	 * @param   PropertyDeclarationStyle  if the variable is defined with a UPROPERTY
	 * @param   VariableCategory          what kind of variable is being parsed
	 * @param   ParsedVarIndexRange       The source text [Start, End) index range for the parsed type.
	 */
	void GetVarType(
		FClasses&                       AllClasses,
		FScope*							Scope,
		FPropertyBase&                  VarProperty,
		uint64                          Disallow,
		FToken*                         OuterPropertyType,
		EPropertyDeclarationStyle::Type PropertyDeclarationStyle,
		EVariableCategory::Type         VariableCategory,
		FIndexRange*                    ParsedVarIndexRange = nullptr);

	/**
	 * Parses a variable name declaration and creates a new UProperty object.
	 *
	 * @param	Scope				struct to create the property in
	 * @param	VarProperty			type and propertyflag info for the new property (inout)
	 * @param   VariableCategory	what kind of variable is being created
	 *
	 * @return	a pointer to the new UProperty if successful, or NULL if there was no property to parse
	 */
	UProperty* GetVarNameAndDim(
		UStruct* Struct,
		FToken& VarProperty,
		EVariableCategory::Type VariableCategory);
	
	/**
	 * Returns whether the specified class can be referenced from the class currently being compiled.
	 *
	 * @param	Scope		The scope we are currently parsing.
	 * @param	CheckClass	The class we want to reference.
	 *
	 * @return	true if the specified class is an intrinsic type or if the class has successfully been parsed
	 */
	bool AllowReferenceToClass(UStruct* Scope, UClass* CheckClass) const;

	/**
	 * @return	true if Scope has UProperty objects in its list of fields
	 */
	static bool HasMemberProperties( const UStruct* Scope );

	/**
	 * Parses optional metadata text.
	 *
	 * @param	MetaData	the metadata map to store parsed metadata in
	 * @param	FieldName	the field being parsed (used for logging)
	 *
	 * @return	true if metadata was specified
	 */
	void ParseFieldMetaData(TMap<FName, FString>& MetaData, const TCHAR* FieldName);

	/**
	 * Formats the current comment, if any, and adds it to the metadata as a tooltip.
	 *
	 * @param	MetaData	the metadata map to store the tooltip in
	 */
	void AddFormattedPrevCommentAsTooltipMetaData(TMap<FName, FString>& MetaData);

	/** 
	 * Tries to parse the token as an access protection specifier (public:, protected:, or private:)
	 *
	 * @return	EAccessSpecifier this is, or zero if it is none
	 */
	EAccessSpecifier ParseAccessProtectionSpecifier(FToken& Token);

	const TCHAR* NestTypeName( ENestType NestType );

	FClass* GetQualifiedClass(const FClasses& AllClasses, const TCHAR* Thing);

	/**
	 * Increase the nesting level, setting the new top nesting level to
	 * the one specified.  If pushing a function or state and it overrides a similar
	 * thing declared on a lower nesting level, verifies that the override is legal.
	 *
	 * @param	NestType	the new nesting type
	 * @param	InNode		@todo
	 */
	void PushNest(ENestType NestType, UStruct* InNode, FUnrealSourceFile* SourceFile = nullptr);
	void PopNest(ENestType NestType, const TCHAR* Descr);

	/**
	 * Tasks that need to be done after popping function declaration
	 * from parsing stack.
	 *
	 * @param AllClasses The class tree for current package.
	 * @param PoppedFunction Function that have just been popped.
	 */
	void PostPopFunctionDeclaration(FClasses& AllClasses, UFunction* PoppedFunction);

	/**
	 * Tasks that need to be done after popping interface definition
	 * from parsing stack.
	 *
	 * @param AllClasses The class tree for current package.
	 * @param CurrentInterface Interface that have just been popped.
	 */
	void PostPopNestInterface(FClasses& AllClasses, UClass* CurrentInterface);

	/**
	 * Tasks that need to be done after popping class definition
	 * from parsing stack.
	 *
	 * @param CurrentClass Class that have just been popped.
	 */
	void PostPopNestClass(UClass* CurrentClass);

	/**
	 * Binds all delegate properties declared in ValidationScope the delegate functions specified in the variable declaration, verifying that the function is a valid delegate
	 * within the current scope.  This must be done once the entire class has been parsed because instance delegate properties must be declared before the delegate declaration itself.
	 *
	 * @todo: this function will no longer be required once the post-parse fixup phase is added (TTPRO #13256)
	 *
	 * @param	AllClasses			the class tree for CurrentPackage
	 * @param	Struct				the struct to validate delegate properties for
	 * @param	Scope				the current scope
	 * @param	DelegateCache		cached map of delegates that have already been found; used for faster lookup.
	 */
	void FixupDelegateProperties(FClasses& AllClasses, UStruct* ValidationScope, FScope& Scope, TMap<FName, UFunction*>& DelegateCache);

	// Retry functions.
	void InitScriptLocation( FScriptLocation& Retry );
	void ReturnToLocation( const FScriptLocation& Retry, bool Binary=1, bool Text=1 );

	/**
	 * If the property has already been seen during compilation, then return add. If not,
	 * then return replace so that INI files don't mess with header exporting
	 *
	 * @param PropertyName the string token for the property
	 *
	 * @return FNAME_Replace_Not_Safe_For_Threading or FNAME_Add
	 */
	EFindName GetFindFlagForPropertyName(const TCHAR* PropertyName);

	static void ValidatePropertyIsDeprecatedIfNecessary(FPropertyBase& VarProperty, FToken* OuterPropertyType);

private:
	// Source file currently parsed by UHT.
	FUnrealSourceFile* CurrentSourceFile;

	// Module currently parsed by UHT.
	const FManifestModule* CurrentlyParsedModule;

	// True if the module currently being parsed is part of the engine, as opposed to being part of a game
	bool bIsCurrentModulePartOfEngine;

	/**
	 * Tries to match constructor parameter list. Assumes that constructor
	 * name is already matched.
	 *
	 * If fails it reverts all parsing done.
	 *
	 * @param Token Token to start parsing from.
	 *
	 * @returns True if matched. False otherwise.
	 */
	bool TryToMatchConstructorParameterList(FToken Token);
	void SkipDeprecatedMacroIfNecessary();

	// Parses possible version declaration in generated code, e.g. GENERATED_BODY(<some_version>).
	void CompileVersionDeclaration(UStruct* Struct);

	// Verifies that all specified class's UProperties with function associations have valid targets
	void VerifyPropertyMarkups( UClass* TargetClass );

	// Verifies the target function meets the criteria for a blueprint property getter
	void VerifyBlueprintPropertyGetter(UProperty* Property, UFunction* TargetFunction);

	// Verifies the target function meets the criteria for a blueprint property setter
	void VerifyBlueprintPropertySetter(UProperty* Property, UFunction* TargetFunction);

	// Verifies the target function meets the criteria for a replication notify callback
	void VerifyRepNotifyCallback(UProperty* Property, UFunction* TargetFunction);
};

/////////////////////////////////////////////////////
// FHeaderPreParser

class FHeaderPreParser : public FBaseParser
{
public:
	FHeaderPreParser()
	{
	}

	void ParseClassDeclaration(
		const TCHAR* Filename,
		const TCHAR* InputText,
		int32 InLineNumber,
		const TCHAR*
		StartingMatchID,
		FName& out_StrippedClassName,
		FString& out_ClassName,
		FString& out_BaseClassName,
		TArray<FHeaderProvider>& out_ClassNames,
		const TArray<FSimplifiedParsingClassInfo>& ParsedClassArray
	);
};
