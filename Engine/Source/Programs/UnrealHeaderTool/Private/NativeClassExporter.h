// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Classes.h"

class FUnrealSourceFile;
class UPackage;
class UProperty;
class UFunction;
class UStruct;
class UField;
class UClass;
class UEnum;
class UScriptStruct;
class UDelegateFunction;
class FClassMetaData;
struct FFuncInfo;
class FOutputDevice;

//
//	FNativeClassHeaderGenerator
//

namespace EExportFunctionHeaderStyle
{
	enum Type
	{
		Definition,
		Declaration
	};
}

namespace EExportFunctionType
{
	enum Type
	{
		Interface,
		Function,
		Event
	};
}

class FClass;
class FClasses;
class FModuleClasses;
class FScope;

// These are declared in this way to allow swapping out the classes for something more optimized in the future
typedef FStringOutputDevice FUHTStringBuilder;
typedef FStringOutputDeviceCountLines FUHTStringBuilderLineCounter;

enum class EExportingState
{
	Normal,
	TypeEraseDelegates
};

enum class EExportCallbackType
{
	Interface,
	Class
};

struct FPropertyNamePointerPair
{
	FPropertyNamePointerPair(FString InName, UProperty* InProp)
		: Name(MoveTemp(InName))
		, Prop(InProp)
	{
	}

	FString Name;
	UProperty* Prop;
};

struct FNativeClassHeaderGenerator
{
private:
	FString API;

	/**
	 * Gets API string for this header.
	 */
	FString GetAPIString()
	{
		return FString::Printf(TEXT("%s_API "), *API);
	}

	const UPackage* Package;

	/** Set of already exported cross-module references, to prevent duplicates */
	TSet<FString>* UniqueCrossModuleReferences;

	/** the existing disk version of the header for this package's names */
	FString OriginalNamesHeader;

	/** Array of temp filenames that for files to overwrite headers */
	TArray<FString> TempHeaderPaths;

	/** Array of all header filenames from the current package. */
	TArray<FString> PackageHeaderPaths;

	/** If false, exported headers will not be saved to disk */
	bool bAllowSaveExportedHeaders;

	/** If true, any change in the generated headers will result in UHT failure. */
	bool bFailIfGeneratedCodeChanges;

	/** Forward declarations that we need. */
	TSet<FString> ForwardDeclarations;

	/**
	 * Exports the struct's C++ properties to the HeaderText output device and adds special
	 * compiler directives for GCC to pack as we expect.
	 *
	 * @param	Out				alternate output device
	 * @param	Struct			UStruct to export properties
	 * @param	TextIndent		Current text indentation
	 */
	static void ExportProperties(FOutputDevice& Out, UStruct* Struct, int32 TextIndent);

	/** Return the name of the singleton function that returns the UObject for Item */
	FString GetPackageSingletonName(const UPackage* Item);

	/** Return the name of the singleton function that returns the UObject for Item */
	FString GetSingletonName(UField* Item, bool bRequiresValidObject=true);

	/** Return the address of the singleton function - handles nullptr */
	FString GetSingletonNameFuncAddr(UField* Item, bool bRequiresValidObject=true);

	/**
	 * Returns the name (overridden if marked up) with TEXT("") or "" wrappers for use in a string literal.
	 */
	static FString GetOverriddenNameForLiteral(const UField* Item);

	/**
	 * Returns the name (overridden if marked up) or "" wrappers for use in a string literal.
	 */
	static FString GetUTF8OverriddenNameForLiteral(const UField* Item);

	/**
	 * Export functions used to find and call C++ or script implementation of a script function in the interface 
	 */
	void ExportInterfaceCallFunctions(FOutputDevice& OutCpp, FUHTStringBuilder& Out, const TArray<UFunction*>& CallbackFunctions, const TCHAR* ClassName);

	/**
	 * Export UInterface boilerplate.
	 *
	 * @param UInterfaceBoilerplate Device to export to.
	 * @param Class Interface to export.
	 * @param FriendText Friend text for this boilerplate.
	 */
	static void ExportUInterfaceBoilerplate(FUHTStringBuilder& UInterfaceBoilerplate, FClass* Class, const FString& FriendText);

	/**
	 * After all of the dependency checking, and setup for isolating the generated code, actually export the class
	 *
	 * @param	OutEnums			Output device for enums declarations.
	 * @param	OutputGetter		The function to call to get the output.
	 * @param	SourceFile			Source file to export.
	 */
	void ExportClassFromSourceFileInner(
		FOutputDevice&           OutGeneratedHeaderText,
		FOutputDevice&           OutputGetter,
		FOutputDevice&           OutDeclarations,
		FClass*                  Class,
		const FUnrealSourceFile& SourceFile
	);

	/**
	 * After all of the dependency checking, but before actually exporting the class, set up the generated code
	 *
	 * @param	SourceFile			Source file to export.
	 */
	bool WriteHeader(const TCHAR* Path, const FString& InBodyText, const TSet<FString>& InFwdDecl);

	/**
	 * Returns a string in the format CLASS_Something|CLASS_Something which represents all class flags that are set for the specified
	 * class which need to be exported as part of the DECLARE_CLASS macro
	 */
	static FString GetClassFlagExportText( UClass* Class );

	/**
	 * Exports the header text for the enum specified
	 * 
	 * @param	Out		the output device for the mirror struct
	 * @param	Enums	the enum to export
	 */
	static void ExportEnum(FOutputDevice& Out, UEnum* Enum);

	/**
	 * Exports the inl text for enums declared in non-UClass headers.
	 * 
	 * @param	OutputGetter	The function to call to get the output.
	 * @param	Enum			the enum to export
	 */
	void ExportGeneratedEnumInitCode(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UEnum* Enum);

	/**
	 * Exports the macro declarations for GENERATED_BODY() for each Foo in the struct specified
	 * 
	 * @param	Out				output device
	 * @param	Struct			The struct to export
	 */
	void ExportGeneratedStructBodyMacros(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UScriptStruct* Struct);

	/**
	 * Exports a local mirror of the specified struct; used to get offsets
	 * 
	 * @param	Out			the output device for the mirror struct
	 * @param	Structs		the struct to export
	 * @param	TextIndent	the current indentation of the header exporter
	 */
	static void ExportMirrorsForNoexportStruct(FOutputDevice& Out, UScriptStruct* Struct, int32 TextIndent);

	/**heade
	 * Exports the parameter struct declarations for the list of functions specified
	 * 
	 * @param	Function	the function that (may) have parameters which need to be exported
	 * @return	true		if the structure generated is not completely empty
	 */
	static bool WillExportEventParms( UFunction* Function );

	/**
	 * Exports C++ type declarations for delegates
	 *
	 * @param	Out					output device
	 * @param	SourceFile			Source file of the delegate.
	 * @param	DelegateFunctions	the functions that have parameters which need to be exported
	 */
	void ExportDelegateDeclaration(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UFunction* Function);

	/**
	 * Exports C++ type definitions for delegates
	 *
	 * @param	Out					output device
	 * @param	SourceFile			Source file of the delegate.
	 * @param	DelegateFunctions	the functions that have parameters which need to be exported
	 */
	void ExportDelegateDefinition(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UFunction* Function);

	/**
	 * Exports the parameter struct declarations for the given function.
	 *
	 * @param	Out					output device
	 * @param	Function			the function that have parameters which need to be exported
	 * @param	Indent				number of spaces to put before each line
	 * @param	bOutputConstructor	If true, output a constructor for the param struct
	 */
	static void ExportEventParm(FUHTStringBuilder& Out, TSet<FString>& PropertyFwd, UFunction* Function, int32 Indent, bool bOutputConstructor, EExportingState ExportingState);

	/** 
	* Exports the temp header files into the .h files, then deletes the temp files.
	* 
	* @param	PackageName	Name of the package being saved
	*/
	void ExportUpdatedHeaders( FString PackageName  );

	/**
	 * Exports the generated cpp file for all functions/events/delegates in package.
	 */
	static void ExportGeneratedCPP(FOutputDevice& Out, const TSet<FString>& InCrossModuleReferences, const TCHAR* EmptyLinkFunctionPostfix, const TCHAR* Body, const TCHAR* OtherIncludes);

	/**
	 * Get the intrinsic null value for this property
	 * 
	 * @param	Prop				the property to get the null value for
	 * @param	bMacroContext		true when exporting the P_GET* macro, false when exporting the friendly C++ function header
	 * @param	bInitializer		if true, will just return ForceInit instead of FStruct(ForceInit)
	 *
	 * @return	the intrinsic null value for the property (0 for ints, TEXT("") for strings, etc.)
	 */
	static FString GetNullParameterValue( UProperty* Prop, bool bInitializer = false );

	/**
	 * Exports a native function prototype
	 * 
	 * @param	Out					Where to write the exported function prototype to.
	 * @param	FunctionData		data representing the function to export
	 * @param	FunctionType		Whether to export this function prototype as an event stub, an interface or a native function stub.
	 * @param	FunctionHeaderStyle	Whether we're outputting a declaration or definition.
	 * @param	ExtraParam			Optional extra parameter that will be added to the declaration as the first argument
	 */
	static void ExportNativeFunctionHeader(
		FOutputDevice&                   Out,
		TSet<FString>&                   OutFwdDecls,
		const FFuncInfo&                 FunctionData,
		EExportFunctionType::Type        FunctionType,
		EExportFunctionHeaderStyle::Type FunctionHeaderStyle,
		const TCHAR*                     ExtraParam,
		const TCHAR*                     APIString
	);

	/**
	* Runs checks whether necessary RPC functions exist for function described by FunctionData.
	*
	* @param	FunctionData			Data representing the function to export.
	* @param	ClassName				Name of currently parsed class.
	* @param	ImplementationPosition	Position in source file of _Implementation function for function described by FunctionData.
	* @param	ValidatePosition		Position in source file of _Validate function for function described by FunctionData.
	* @param	SourceFile				Currently analyzed source file.
	*/
	void CheckRPCFunctions(const FFuncInfo& FunctionData, const FString& ClassName, int32 ImplementationPosition, int32 ValidatePosition, const FUnrealSourceFile& SourceFile);

	/**
	 * Exports the native stubs for the list of functions specified
	 * 
	 * @param SourceFile	current source file
	 * @param Class			class
	 * @param ClassData		class data
	 */
	void ExportNativeFunctions(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& OutMacroCalls, FOutputDevice& OutNoPureDeclsMacroCalls, const FUnrealSourceFile& SourceFile, UClass* Class, FClassMetaData* ClassData);

	/**
	 * Export the actual internals to a standard thunk function
	 *
	 * @param RPCWrappers output device for writing
	 * @param Function given function
	 * @param FunctionData function data for the current function
	 * @param Parameters list of parameters in the function
	 * @param Return return parameter for the function
	 */
	void ExportFunctionThunk(FUHTStringBuilder& RPCWrappers, UFunction* Function, const FFuncInfo& FunctionData, const TArray<UProperty*>& Parameters, UProperty* Return);

	/** Exports the native function registration code for the given class. */
	static void ExportNatives(FOutputDevice& Out, FClass* Class);

	/**
	 * Exports generated singleton functions for UObjects that used to be stored in .u files.
	 * 
	 * @param	Out				The destination to write to.
	 * @param	SourceFile		The source file being processed.
	 * @param	Class			Class to export
	 * @param	OutFriendText	(Output parameter) Friend text
	 */
	void ExportNativeGeneratedInitCode(FOutputDevice& Out, FOutputDevice& OutDeclarations, const FUnrealSourceFile& SourceFile, FClass* Class, FUHTStringBuilder& OutFriendText);

	/**
	 * Export given function.
	 *
	 * @param	Out				The destination to write to.
	 * @param	Function		Given function.
	 * @param	bIsNoExport		Is in NoExport class.
	 */
	void ExportFunction(FOutputDevice& Out, const FUnrealSourceFile& SourceFile, UFunction* Function, bool bIsNoExport);

	/**
	 * Exports a generated singleton function to setup the package for compiled-in classes.
	 * 
	 * @param	Out			The destination to write to.
	 * @param	Package		Package to export code for.
	**/
	void ExportGeneratedPackageInitCode(FOutputDevice& Out, const TCHAR* InDeclarations, const UPackage* Package, uint32 CRC);

	/**
	 * Function to output the C++ code necessary to set up the given array of properties
	 * 
	 * @param	Meta			Returned string of meta data generator code
	 * @param	OutputDevice	String output device to send the generated code to
	 * @param	Properties		Array of properties to export
	 * @param	Spaces			String of spaces to use as an indent
	 */
	void OutputProperties(FOutputDevice& OutputDevice, FString& OutPropertyRange, const TArray<UProperty*>& Properties, const TCHAR* Spaces);

	/**
	 * Function to output the C++ code necessary to set up a property
	 * 
	 * @param	Meta			Returned string of meta data generator code
	 * @param	OutputDevice	String output device to send the generated code to
	 * @param	Prop			Property to export
	 * @param	Spaces			String of spaces to use as an indent
	**/
	void OutputProperty(FOutputDevice& OutputDevice, TArray<FPropertyNamePointerPair>& PropertyNamesAndPointers, UProperty* Prop, const TCHAR* Spaces);

	/**
	 * Function to output the C++ code necessary to set up a property, including an array property and its inner, array dimensions, etc.
	 *
	 * @param	Out				The destination to write to.
	 * @param	Prop			Property to export
	 * @param	OffsetStr		String specifying the property offset
	 * @param	Name			Name for the generated variable
	 * @param	Spaces			String of spaces to use as an indent
	 * @param	SourceStruct	Structure that the property offset is relative to
	**/
	void PropertyNew(FOutputDevice& Out, UProperty* Prop, const TCHAR* OffsetStr, const TCHAR* Name, const TCHAR* Spaces, const TCHAR* SourceStruct = NULL);

	/**
	 * Exports the proxy definitions for the list of enums specified
	 * 
	 * @param SourceFile	current source file
	 */
	static void ExportCallbackFunctions(
		FOutputDevice&            OutGeneratedHeaderText,
		FOutputDevice&            Out,
		TSet<FString>&            OutFwdDecls,
		const TArray<UFunction*>& CallbackFunctions,
		const TCHAR*              CallbackWrappersMacroName,
		EExportCallbackType       ExportCallbackType,
		const TCHAR*              APIString
	);

	/**
	 * Determines if the property has alternate export text associated with it and if so replaces the text in PropertyText with the
	 * alternate version. (for example, structs or properties that specify a native type using export-text).  Should be called immediately
	 * after ExportCppDeclaration()
	 *
	 * @param	Prop			the property that is being exported
	 * @param	PropertyText	the string containing the text exported from ExportCppDeclaration
	 */
	static void ApplyAlternatePropertyExportText(UProperty* Prop, FUHTStringBuilder& PropertyText, EExportingState ExportingState);

	/**
	* Create a temp header file name from the header name
	*
	* @param	CurrentFilename		The filename off of which the current filename will be generated
	* @param	bReverseOperation	Get the header from the temp file name instead
	*
	* @return	The generated string
	*/
	static FString GenerateTempHeaderName( FString CurrentFilename, bool bReverseOperation = false );

	/**
	 * Saves a generated header if it has changed. 
	 *
	 * @param HeaderPath	Header Filename
	 * @param NewHeaderContents	Contents of the generated header.
	 * @return True if the header contents has changed, false otherwise.
	 */
	bool SaveHeaderIfChanged(const TCHAR* HeaderPath, const TCHAR* NewHeaderContents);

	/**
	 * Deletes all .generated.h files which do not correspond to any of the classes.
	 */
	void DeleteUnusedGeneratedHeaders();

	/**
	 * Exports macros that manages UObject constructors.
	 * 
	 * @param	VTableOut								The destination to write vtable helpers to.
	 * @param	StandardUObjectConstructorsMacroCall	The destination to write standard constructor macros to.
	 * @param	EnhancedUObjectConstructorsMacroCall	The destination to write enhanced constructor macros to.
	 * @param	ConstructorsMacroPrefix					Prefix for constructors macro.
	 * @param	Class									Class for which to export macros.
	 */
	static void ExportConstructorsMacros(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& VTableOut, FOutputDevice& StandardUObjectConstructorsMacroCall, FOutputDevice& EnhancedUObjectConstructorsMacroCall, const FString& ConstructorsMacroPrefix, FClass* Class, const TCHAR* APIArg);

public:

	/** 
	 * Properties in source files generated from blueprint assets have a symbol name that differs from the source asset.
	 * This function returns the original name of the field (rather than the native, symbol name).
	 */
	static FString GetOverriddenName(const UField* Item);
	static FName GetOverriddenFName(const UField* Item);
	static FString GetOverriddenPathName(const UField* Item);

	// Constructor
	FNativeClassHeaderGenerator(
		const UPackage* InPackage,
		const TArray<FUnrealSourceFile*>& SourceFiles,
		FClasses& AllClasses,
		bool InAllowSaveExportedHeaders
	);

	/**
	 * Gets string with function return type.
	 * 
	 * @param Function Function to get return type of.
	 * @return FString with function return type.
	 */
	FString GetFunctionReturnString(UFunction* Function);

	/**
	* Gets string with function parameters (with names).
	*
	* @param Function Function to get parameters of.
	* @return FString with function parameters.
	*/
	FString GetFunctionParameterString(UFunction* Function);

	/**
	 * Checks if function is missing "virtual" specifier.
	 * 
	 * @param SourceFile SourceFile where function is declared.
	 * @param FunctionNamePosition Position of name of function in SourceFile.
	 * @return true if function misses "virtual" specifier, false otherwise.
	 */
	static bool IsMissingVirtualSpecifier(const FString& SourceFile, int32 FunctionNamePosition);
};
