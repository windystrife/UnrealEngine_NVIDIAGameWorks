// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma  once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "Engine/Blueprint.h" // for FCompilerNativizationOptions

class USCS_Node;
class UUserDefinedEnum;
class UUserDefinedStruct;
struct FNonativeComponentData;
enum class ENativizedTermUsage : uint8;
struct FEmitterLocalContext;

struct FCodeText
{
	FString Indent;
	FString Result;

	void IncreaseIndent()
	{
		Indent += TEXT("\t");
	}

	void DecreaseIndent()
	{
		Indent.RemoveFromEnd(TEXT("\t"));
	}

	void AddLine(const FString& Line)
	{
		Result += FString::Printf(TEXT("%s%s\n"), *Indent, *Line);
	}
};

/** 
 * A helper struct that adds indented {} scope to the specified context. 
 * Additionally, manages locals that were added via FEmitHelper::GenerateGetPropertyByName()
 * and removes them from the cache on destruction, so we don't try to use the
 * variables outside of the scope.
 */
struct FScopeBlock
{
public:
	 FScopeBlock(FEmitterLocalContext& Context);
	~FScopeBlock();

	void TrackLocalAccessorDecl(const UProperty* Property);

private:
	FEmitterLocalContext& Context;
	FScopeBlock*  OuterScopeBlock;
	TArray<const UProperty*> LocalAccessorDecls;
};

struct FEmitterLocalContext
{
	enum class EClassSubobjectList
	{
		ComponentTemplates,
		Timelines,
		DynamicBindingObjects,
		MiscConvertedSubobjects,
	};

	enum class EGeneratedCodeType
	{
		SubobjectsOfClass,
		CommonConstructor,
		Regular,
	};

	EGeneratedCodeType CurrentCodeType;

	// List od assets directly used in class implementation.
	TArray<const UObject*> UsedObjectInCurrentClass; 
	TArray<const UUserDefinedEnum*> EnumsInCurrentClass;

	// Nativized UDS doesn't reference its default value dependencies. When ::GetDefaultValue is used, then we need to reference the dependencies in the class.
	TArray<UUserDefinedStruct*> StructsWithDefaultValuesUsed;

	//ConstructorOnly Local Names
	TMap<UObject*, FString> ClassSubobjectsMap;
	//ConstructorOnly Local Names
	TMap<UObject*, FString> CommonSubobjectsMap;

	// used to track the innermost FScopeBlock on the stack, so GenerateGetPropertyByName() can register added locals
	FScopeBlock* ActiveScopeBlock;

	// See TInlineValue. If the structure is initialized in constructor, then its header must be included.
	TSet<UField*> StructsUsedAsInlineValues;

	// List of wrappers that were actually used in the generated code.
	TSet<UField*> UsedUnconvertedWrapper;

	// Objects like UChildActorComponent::ChildActorTemplate. They will be stored at the beginning of MiscConvertedSubobjects.
	TArray<UObject*> TemplateFromSubobjectsOfClass;

	// Class subobjects
	TArray<UObject*> MiscConvertedSubobjects;
	TArray<UObject*> DynamicBindingObjects;
	TArray<UObject*> ComponentTemplates;
	TArray<UObject*> Timelines;
private:
	int32 LocalNameIndexMax;

public:

	FCodeText Header;
	FCodeText Body;
	FCodeText* DefaultTarget;

	const FGatherConvertedClassDependencies& Dependencies;
	const FCompilerNativizationOptions& NativizationOptions;

public:
	TMap<UFunction*, FString> MCDelegateSignatureToSCDelegateType;

	FEmitterLocalContext(const FGatherConvertedClassDependencies& InDependencies, const FCompilerNativizationOptions& InNativizationOptions)
		: CurrentCodeType(EGeneratedCodeType::Regular)
		, ActiveScopeBlock(nullptr)
		, LocalNameIndexMax(0)
		, DefaultTarget(&Body)
		, Dependencies(InDependencies)
		, NativizationOptions(InNativizationOptions)
	{}

	// Call this functions to make sure the wrapper (necessary for the given field) will be included and generated.
	void MarkUnconvertedClassAsNecessary(UField* InField);

	// PROPERTIES FOR INACCESSIBLE MEMBER VARIABLES
	TMap<const UProperty*, FString> PropertiesForInaccessibleStructs;
	void ResetPropertiesForInaccessibleStructs()
	{
		PropertiesForInaccessibleStructs.Empty();
	}

	// CONSTRUCTOR FUNCTIONS

	static const TCHAR* ClassSubobjectListName(EClassSubobjectList ListType)
	{
		if (ListType == EClassSubobjectList::ComponentTemplates)
		{
			return GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ComponentTemplates);
		}
		if (ListType == EClassSubobjectList::Timelines)
		{
			return GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, Timelines);
		}
		if (ListType == EClassSubobjectList::DynamicBindingObjects)
		{
			return GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, DynamicBindingObjects);
		}
		return GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, MiscConvertedSubobjects);
	}

	void RegisterClassSubobject(UObject* Object, EClassSubobjectList ListType)
	{
		ensure(CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass);
		if (ListType == EClassSubobjectList::ComponentTemplates)
		{
			ComponentTemplates.Add(Object);
		}
		else if (ListType == EClassSubobjectList::Timelines)
		{
			Timelines.Add(Object);
		}
		else if(ListType == EClassSubobjectList::DynamicBindingObjects)
		{
			DynamicBindingObjects.Add(Object);
		}
		else
		{
			MiscConvertedSubobjects.Add(Object);
		}
	}

	void AddClassSubObject_InConstructor(UObject* Object, const FString& NativeName)
	{
		ensure(CurrentCodeType == EGeneratedCodeType::SubobjectsOfClass);
		ensure(!ClassSubobjectsMap.Contains(Object));
		ClassSubobjectsMap.Add(Object, NativeName);
	}

	void AddCommonSubObject_InConstructor(UObject* Object, const FString& NativeName)
	{
		ensure(CurrentCodeType == EGeneratedCodeType::CommonConstructor);
		ensure(!CommonSubobjectsMap.Contains(Object));
		CommonSubobjectsMap.Add(Object, NativeName);
	}

	// UNIVERSAL FUNCTIONS

	UClass* GetFirstNativeOrConvertedClass(UClass* InClass) const
	{
		return Dependencies.GetFirstNativeOrConvertedClass(InClass);
	}

	FString GenerateUniqueLocalName();

	UClass* GetCurrentlyGeneratedClass() const
	{
		return Cast<UClass>(Dependencies.GetActualStruct());
	}

	/** All objects (that can be referenced from other package) that will have a different path in cooked build
	(due to the native code generation), should be handled by this function */
	FString FindGloballyMappedObject(const UObject* Object, const UClass* ExpectedClass = nullptr, bool bLoadIfNotFound = false, bool bTryUsedAssetsList = true);

	// Functions needed for Unconverted classes
	enum class EPropertyNameInDeclaration
	{
		Regular,
		Skip,
		ForceConverted,
	};

	FString ExportCppDeclaration(const UProperty* Property, EExportedDeclaration::Type DeclarationType, uint32 AdditionalExportCPPFlags, EPropertyNameInDeclaration ParameterName = EPropertyNameInDeclaration::Regular, const FString& NamePostfix = FString(), const FString& TypePrefix = FString()) const;
	FString ExportTextItem(const UProperty* Property, const void* PropertyValue) const;

	// AS FCodeText
	void IncreaseIndent()
	{
		DefaultTarget->IncreaseIndent();
	}

	void DecreaseIndent()
	{
		DefaultTarget->DecreaseIndent();
	}

	void AddLine(const FString& Line)
	{
		DefaultTarget->AddLine(Line);
	}

private:
	FEmitterLocalContext(const FEmitterLocalContext&) = delete;
};

struct FEmitHelper
{
	// bUInterface - use interface with "U" prefix, by default there is "I" prefix
	static FString GetCppName(const UField* Field, bool bUInterface = false, bool bForceParameterNameModification = false);

	// returns an unique number for a structure in structures hierarchy
	static int32 GetInheritenceLevel(const UStruct* Struct);

	static FString FloatToString(float Value);

	static bool PropertyForConstCast(const UProperty* Property);

	static void ArrayToString(const TArray<FString>& Array, FString& OutString, const TCHAR* Separator);

	static bool HasAllFlags(uint64 Flags, uint64 FlagsToCheck);

	static bool IsMetaDataValid(const FName Name, const FString& Value);

	static FString HandleRepNotifyFunc(const UProperty* Property);

	static bool MetaDataCanBeNative(const FName MetaDataName, const UField* Field);

	static FString HandleMetaData(const UField* Field, bool AddCategory = true, const TArray<FString>* AdditinalMetaData = nullptr);

	static TArray<FString> ProperyFlagsToTags(uint64 Flags, bool bIsClassProperty);

	static TArray<FString> FunctionFlagsToTags(uint64 Flags);

	static bool IsBlueprintNativeEvent(uint64 FunctionFlags);

	static bool IsBlueprintImplementableEvent(uint64 FunctionFlags);

	static FString EmitUFuntion(UFunction* Function, const TArray<FString>& AdditionalTags, const TArray<FString>& AdditionalMetaData);

	static int32 ParseDelegateDetails(FEmitterLocalContext& EmitterContext, UFunction* Signature, FString& OutParametersMacro, FString& OutParamNumberStr);

	static void EmitSinglecastDelegateDeclarations(FEmitterLocalContext& EmitterContext, const TArray<UDelegateProperty*>& Delegates);

	static void EmitSinglecastDelegateDeclarations_Inner(FEmitterLocalContext& EmitterContext, UFunction* Signature, const FString& TypeName);

	static void EmitMulticastDelegateDeclarations(FEmitterLocalContext& EmitterContext);

	static void EmitLifetimeReplicatedPropsImpl(FEmitterLocalContext& EmitterContext);

	static FString LiteralTerm(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& Type, const FString& CustomValue, UObject* LiteralObject, const FText* OptionalTextLiteral = nullptr);

	static FString PinTypeToNativeType(const FEdGraphPinType& InType);

	static UFunction* GetOriginalFunction(UFunction* Function);

	static bool ShouldHandleAsNativeEvent(UFunction* Function, bool bOnlyIfOverridden = true);

	static bool ShouldHandleAsImplementableEvent(UFunction* Function);

	static bool GenerateAutomaticCast(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& LType, const FEdGraphPinType& RType, const UProperty* LProp, const UProperty* RProp, FString& OutCastBegin, FString& OutCastEnd, bool bForceReference = false);

	static FString GenerateReplaceConvertedMD(UObject* Obj);

	static FString GetBaseFilename(const UObject* AssetObj, const FCompilerNativizationOptions& NativizationOptions);

	static FString ReplaceConvertedMetaData(UObject* Obj);

	static FString GetPCHFilename();

	static FString GetGameMainHeaderFilename();

	static FString GenerateGetPropertyByName(FEmitterLocalContext& EmitterContext, const UProperty* Property);

	static FString AccessInaccessibleProperty(FEmitterLocalContext& EmitterContext, const UProperty* Property, FString OverrideTypeDeclaration
		, const FString& ContextStr, const FString& ContextAdressOp, int32 StaticArrayIdx
		, ENativizedTermUsage TermUsage, FString* CustomSetExpressionEnding);

	static const TCHAR* EmptyDefaultConstructor(UScriptStruct* Struct);
};

struct FNonativeComponentData;

struct FEmitDefaultValueHelper
{
	static void GenerateGetDefaultValue(const UUserDefinedStruct* Struct, FEmitterLocalContext& EmitterContext);

	static void GenerateConstructor(FEmitterLocalContext& Context);

	static void GenerateCustomDynamicClassInitialization(FEmitterLocalContext& Context, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies);

	enum class EPropertyAccessOperator
	{
		None, // for self scope, this
		Pointer,
		Dot,
	};

	// OuterPath ends context/outer name (or empty, if the scope is "this")
	static void OuterGenerate(FEmitterLocalContext& Context, const UProperty* Property, const FString& OuterPath, const uint8* DataContainer, const uint8* OptionalDefaultDataContainer, EPropertyAccessOperator AccessOperator, bool bAllowProtected = false);


	// PathToMember ends with variable name
	static void InnerGenerate(FEmitterLocalContext& Context, const UProperty* Property, const FString& PathToMember, const uint8* ValuePtr, const uint8* DefaultValuePtr, bool bWithoutFirstConstructionLine = false);

	// Creates the subobject (of class) returns it's native local name, 
	// returns empty string if cannot handle
	static FString HandleClassSubobject(FEmitterLocalContext& Context, UObject* Object, FEmitterLocalContext::EClassSubobjectList ListOfSubobjectsTyp, bool bCreate, bool bInitialize, bool bForceSubobjectOfClass = false);

	// returns true, and fill OutResult, when the structure is handled in a custom way.
	static bool SpecialStructureConstructor(const UStruct* Struct, const uint8* ValuePtr, FString* OutResult);

	// Add static initialization functions. Must be called after Context.UsedObjectInCurrentClass is fully filled
	static void AddStaticFunctionsForDependencies(FEmitterLocalContext& Context, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies, FCompilerNativizationOptions NativizationOptions);

	static void AddRegisterHelper(FEmitterLocalContext& Context);
private:
	// Returns native term, 
	// returns empty string if cannot handle
	static FString HandleSpecialTypes(FEmitterLocalContext& Context, const UProperty* Property, const uint8* ValuePtr);

	static FString HandleNonNativeComponent(FEmitterLocalContext& Context, const USCS_Node* Node, TSet<const UProperty*>& OutHandledProperties
		, TArray<FString>& NativeCreatedComponentProperties, const USCS_Node* ParentNode, TArray<FNonativeComponentData>& ComponentsToInit
		, bool bBlockRecursion);

	static FString HandleInstancedSubobject(FEmitterLocalContext& Context, UObject* Object, bool bCreateInstance = true, bool bSkipEditorOnlyCheck = false);
};

struct FBackendHelperUMG
{
	static void WidgetFunctionsInHeader(FEmitterLocalContext& Context);

	static void AdditionalHeaderIncludeForWidget(FEmitterLocalContext& EmitterContext);

	// these function should use the same context as Constructor
	static void CreateClassSubobjects(FEmitterLocalContext& Context, bool bCreate, bool bInitialize);
	static void EmitWidgetInitializationFunctions(FEmitterLocalContext& Context);

	static bool SpecialStructureConstructorUMG(const UStruct* Struct, const uint8* ValuePtr, /*out*/ FString* OutResult);

	static UScriptStruct* InlineValueStruct(UScriptStruct* OuterStruct, const uint8* ValuePtr);
	static const uint8* InlineValueData(UScriptStruct* OuterStruct, const uint8* ValuePtr);
	static bool IsTInlineStruct(UScriptStruct* OuterStruct);
};

struct FBackendHelperAnim
{
	static void AddHeaders(FEmitterLocalContext& EmitterContext);

	static void CreateAnimClassData(FEmitterLocalContext& Context);
};

/** this struct helps generate a static function that initializes Static Searchable Values. */
struct FBackendHelperStaticSearchableValues
{
	static bool HasSearchableValues(UClass* Class);
	static FString GetFunctionName();
	static FString GenerateClassMetaData(UClass* Class);
	static void EmitFunctionDeclaration(FEmitterLocalContext& Context);
	static void EmitFunctionDefinition(FEmitterLocalContext& Context);
};
struct FNativizationSummaryHelper
{
	static void InaccessibleProperty(const UProperty* Property);
	// Notify, that the class used a (unrelated) property
	static void PropertyUsed(const UClass* Class, const UProperty* Property);
	// Notify, that the class used a (unrelated) function
	static void FunctionUsed(const UClass* Class, const UFunction* Function);
	static void RegisterClass(const UClass* OriginalClass);

	static void ReducibleFunciton(const UClass* OriginalClass);

	static void RegisterRequiredModules(const FName PlatformName, const TSet<TSoftObjectPtr<UPackage>>& Modules);
};
struct FDependenciesGlobalMapHelper
{
	static FString EmitHeaderCode();
	static FString EmitBodyCode(const FString& PCHFilename);

	static FNativizationSummary::FDependencyRecord& FindDependencyRecord(const FSoftObjectPath& Key);

private:
	static TMap<FSoftObjectPath, FNativizationSummary::FDependencyRecord>& GetDependenciesGlobalMap();
};

struct FDisableUnwantedWarningOnScope
{
private:
	FCodeText& CodeText;

public:
	FDisableUnwantedWarningOnScope(FCodeText& InCodeText);
	~FDisableUnwantedWarningOnScope();
};

struct FStructAccessHelper
{
	static FString EmitStructAccessCode(const UStruct* InStruct);
	static bool CanEmitDirectFieldAccess(const UScriptStruct* InStruct);
};