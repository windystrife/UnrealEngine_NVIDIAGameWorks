// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BPTerminal.h"
#include "BlueprintCompiledStatement.h"

class FCompilerResultsLog;
class FKismetCompilerContext;
class UAnimGraphNode_Base;
class UBlueprint;
class UEdGraph;
class UEdGraphSchema_K2;
class UK2Node;
class UK2Node_CallFunction;
struct FKismetFunctionContext;

//////////////////////////////////////////////////////////////////////////
// FKismetCompilerUtilities

/** This is a loose collection of utilities used when 'compiling' a new UClass from a K2 graph. */
class KISMETCOMPILER_API FKismetCompilerUtilities
{
public:
	// Rename a class and it's CDO into the transient package, and clear RF_Public on both of them
	static void ConsignToOblivion(UClass* OldClass, bool bForceNoResetLoaders);


	static void UpdateBlueprintSkeletonStubClassAfterFailedCompile(UBlueprint* Blueprint, UClass* StubClass);


	/** 
	 * Invalidates the export of a property, and any of its inners
	 *
	 * @param PropertyToInvalidate	The property to invalidate the export for
	 */
	static void InvalidatePropertyExport(UProperty* PropertyToInvalidate);

	/**
	 * Finds any class with the specified class name, and consigns it to oblivion, along with the specified class to consign.
	 * This should ensure that the specified name is free for use
	 *
	 * @param ClassToConsign	Class to consign to oblivion no matter what
	 * @param ClassName			Name that we want to ensure isn't used
	 * @param Blueprint			The blueprint that is in charge of these classes, used for scoping
	 */
	static void EnsureFreeNameForNewClass(UClass* ClassToConsign, FString& ClassName, UBlueprint* Blueprint);

	/**
	 * Tests to see if a pin is schema compatible with a property.
	 *
	 * @param	SourcePin		If non-null, source object.
	 * @param	Property		The property to check.
	 * @param	MessageLog  	The message log.
	 * @param	Schema			Schema.
	 * @param	SelfClass   	Self class (needed for pins marked Self).
	 *
	 * @return	true if the pin type/direction is compatible with the property.
	 */
	static bool IsTypeCompatibleWithProperty(UEdGraphPin* SourcePin, UProperty* Property, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass);

	/** Finds a property by name, starting in the specified scope; Validates property type and returns NULL along with emitting an error if there is a mismatch. */
	static UProperty* FindPropertyInScope(UStruct* Scope, UEdGraphPin* Pin, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass);

	// Finds a property by name, starting in the specified scope, returning NULL if it's not found
	static UProperty* FindNamedPropertyInScope(UStruct* Scope, FName PropertyName);

	/** return function, that overrides BlueprintImplementableEvent with given name in given class (super-classes are not considered) */
	static const UFunction* FindOverriddenImplementableEvent(const FName& EventName, const UClass* Class);

	/** Helper function for creating property for primitive types. Used only to create inner peroperties for UArrayProperty, USetProperty, and UMapProperty: */
	static UProperty* CreatePrimitiveProperty( UObject* PropertyScope, const FName& ValidatedPropertyName, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, UClass* SelfClass, bool bIsWeakPointer, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog);

	/** Creates a property named PropertyName of type PropertyType in the Scope or returns NULL if the type is unknown, but does *not* link that property in */
	static UProperty* CreatePropertyOnScope(UStruct* Scope, const FName& PropertyName, const FEdGraphPinType& Type, UClass* SelfClass, uint64 PropertyFlags, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog);

	/**
	 * Checks that the property name isn't taken in the given scope (used by CreatePropertyOnScope())
	 *
	 * @return	Ptr to an existing object with that name in the given scope or nullptr if none exists
	 */
	static UObject* CheckPropertyNameOnScope(UStruct* Scope, const FName& PropertyName);

	// Find groups of nodes, that can be executed separately.
	static TArray<TSet<UEdGraphNode*>> FindUnsortedSeparateExecutionGroups(const TArray<UEdGraphNode*>& Nodes);

private:
	/** Counter to ensure unique names in the transient package, to avoid GC collection issues with classes and their CDOs */
	static uint32 ConsignToOblivionCounter;
public:
	static void CompileDefaultProperties(UClass* Class);

	static void LinkAddedProperty(UStruct* Structure, UProperty* NewProperty);

	static void RemoveObjectRedirectorIfPresent(UObject* Package, const FString& ClassName, UObject* ObjectBeingMovedIn);

	/* checks if enum variables from given object store proper indexes */
	static void ValidateEnumProperties(UObject* DefaultObject, FCompilerResultsLog& MessageLog);

	/** checks if the specified pin can default to self */
	static bool ValidateSelfCompatibility(const UEdGraphPin* Pin, FKismetFunctionContext& Context);

	/** Create 'set var by name' nodes and hook them up - used to set values when components are added or actor are created at run time. Returns the 'last then' pin of the assignment nodes */
	static UEdGraphPin* GenerateAssignmentNodes( class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* CallBeginSpawnNode, UEdGraphNode* SpawnNode, UEdGraphPin* CallBeginResult, const UClass* ForClass );

	/** Create Kismet assignment statement with proper object <-> interface cast */
	static void CreateObjectAssignmentStatement(FKismetFunctionContext& Context, UEdGraphNode* Node, FBPTerminal* SrcTerm, FBPTerminal* DstTerm);

	/** Checks if each execution path ends with a Return node */
	static void ValidateProperEndExecutionPath(FKismetFunctionContext& Context);

	/** Generate an error for non-const output parameters */
	static void DetectValuesReturnedByRef(const UFunction* Func, const UK2Node * Node, FCompilerResultsLog& MessageLog);

	static bool IsStatementReducible(EKismetCompiledStatementType StatementType);

	/**
	 * Intended to avoid errors that come from checking for external member 
	 * (function, variable, etc.) dependencies. This can happen when a Blueprint
	 * was saved without having new members compiled in (saving w/out compiling),
	 * and a Blueprint that uses those members is compiled-on-load before the 
	 * uncompiled one. Any valid errors should surface later, when the dependant 
	 * Blueprint's bytecode is recompiled.
	 */
	static bool IsMissingMemberPotentiallyLoading(const UBlueprint* SelfBlueprint, const UStruct* MemberOwner);
};

//////////////////////////////////////////////////////////////////////////
// FNodeHandlingFunctor

class KISMETCOMPILER_API FNodeHandlingFunctor
{
public:
	class FKismetCompilerContext& CompilerContext;

protected:
	/** Helper function that verifies the variable name referenced by the net exists in the associated scope (either the class being compiled or via an object reference on the Self pin), and then creates/registers a term for that variable access. */
	void ResolveAndRegisterScopedTerm(FKismetFunctionContext& Context, UEdGraphPin* Net, TIndirectArray<FBPTerminal>& NetArray);

	// Generate a goto on the corresponding exec pin
	FBlueprintCompiledStatement& GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node, UEdGraphPin* ThenExecPin);

	// Generate a goto corresponding to the then pin(s)
	FBlueprintCompiledStatement& GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node);

	// If the net is a literal, it validates the default value and registers it.
	// Returns true if the net is *not* a literal, or if it's a literal that is valid.
	// Returns false only for a bogus literal value.
	bool ValidateAndRegisterNetIfLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net);

	// Helper to register literal term
	virtual FBPTerminal* RegisterLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net);
public:
	FNodeHandlingFunctor(FKismetCompilerContext& InCompilerContext)
		: CompilerContext(InCompilerContext)
	{
	}

	virtual ~FNodeHandlingFunctor() 
	{
	}

	//virtual void Validate(FKismetFunctionContext& Context, UEdGraphNode* Node) {}
	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node)
	{
	}

	virtual void Transform(FKismetFunctionContext& Context, UEdGraphNode* Node)
	{
	}

	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Pin)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node);

	// Returns true if this kind of node needs to be handled in the first pass, prior to execution scheduling (this is only used for function entry and return nodes)
	virtual bool RequiresRegisterNetsBeforeScheduling() const
	{
		return false;
	}

	/**
	 * Creates a sanitized name.
	 *
	 * @param [in,out]	Name	The name to modify and make a legal C++ identifier.
	 */
	static void SanitizeName(FString& Name);
};

class FKCHandler_Passthru : public FNodeHandlingFunctor
{
public:
	FKCHandler_Passthru(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node)
	{
		// Generate the output impulse from this node
		GenerateSimpleThenGoto(Context, *Node);
	}
};


//////////////////////////////////////////////////////////////////////////
// FNetNameMapping

// Map from a name to the number of times it's been 'created' (identical nodes create the same variable names, so they need something appended)
struct FNetNameMapping
{
public:
	TMap<const UObject*, FString> NetToName;
	TMap<UEdGraphPin*, FString> NetPinToName;
	TMap<FString, int32> BaseNameToCount;

public:
	KISMETCOMPILER_API static FString MakeBaseName(const UEdGraphNode* Net);
	KISMETCOMPILER_API static FString MakeBaseName(const UEdGraphPin* Net);;
	KISMETCOMPILER_API static FString MakeBaseName(const UAnimGraphNode_Base* Net);

	// Come up with a valid, unique (within the scope of NetNameMap) name based on an existing Net object.
	// The resulting name is stable across multiple calls if given the same pointer.
	template<typename NetType>
	FString MakeValidName(const NetType* Net)
	{
		// Check to see if this net was already used to generate a name
		if (FString* Result = NetToName.Find(Net))
		{
			return *Result;
		}
		else
		{
			FString NetName = MakeBaseName(Net);
			FNodeHandlingFunctor::SanitizeName(NetName);

			int32& ExistingCount = BaseNameToCount.FindOrAdd(NetName);
			++ExistingCount;
			if (ExistingCount > 1)
			{
				NetName += FString::FromInt(ExistingCount);
			}

			NetToName.Add(Net, NetName);

			return NetName;
		}
	}

	FString MakeValidName(UEdGraphPin* Net)
	{
		if (FString* Result = NetPinToName.Find(Net))
		{
			return *Result;
		}
		else
		{
			FString NetName = MakeBaseName(Net);
			FNodeHandlingFunctor::SanitizeName(NetName);

			int32& ExistingCount = BaseNameToCount.FindOrAdd(NetName);
			++ExistingCount;
			if (ExistingCount > 1)
			{
				NetName += FString::FromInt(ExistingCount);
			}

			NetPinToName.Add(Net, NetName);

			return NetName;
		}
	}
};

