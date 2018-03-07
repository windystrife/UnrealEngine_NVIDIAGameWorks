// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Stack.h: Kismet VM execution stack definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "Misc/CoreMisc.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"

DECLARE_LOG_CATEGORY_EXTERN(LogScriptFrame, Warning, All);

/**
 * Property data type enums.
 * @warning: if values in this enum are modified, you must update:
 * - FPropertyBase::GetSize() hardcodes the sizes for each property type
 */
enum EPropertyType
{
	CPT_None,
	CPT_Byte,
	CPT_UInt16,
	CPT_UInt32,
	CPT_UInt64,
	CPT_Int8,
	CPT_Int16,
	CPT_Int,
	CPT_Int64,
	CPT_Bool,
	CPT_Bool8,
	CPT_Bool16,
	CPT_Bool32,
	CPT_Bool64,
	CPT_Float,
	CPT_ObjectReference,
	CPT_Name,
	CPT_Delegate,
	CPT_Interface,
	CPT_Unused_Index_19,
	CPT_Struct,
	CPT_Unused_Index_21,
	CPT_Unused_Index_22,
	CPT_String,
	CPT_Text,
	CPT_MulticastDelegate,
	CPT_WeakObjectReference,
	CPT_LazyObjectReference,
	CPT_SoftObjectReference,
	CPT_Double,
	CPT_Map,
	CPT_Set,

	CPT_MAX
};



/*-----------------------------------------------------------------------------
	Execution stack helpers.
-----------------------------------------------------------------------------*/

typedef TArray< CodeSkipSizeType, TInlineAllocator<8> > FlowStackType;

//
// Information remembered about an Out parameter.
//
struct FOutParmRec
{
	UProperty* Property;
	uint8*      PropAddr;
	FOutParmRec* NextOutParm;
};

//
// Information about script execution at one stack level.
//

struct FFrame : public FOutputDevice
{	
public:
	// Variables.
	UFunction* Node;
	UObject* Object;
	uint8* Code;
	uint8* Locals;

	UProperty* MostRecentProperty;
	uint8* MostRecentPropertyAddress;

	/** The execution flow stack for compiled Kismet code */
	FlowStackType FlowStack;

	/** Previous frame on the stack */
	FFrame* PreviousFrame;

	/** contains information on any out parameters */
	FOutParmRec* OutParms;

	/** If a class is compiled in then this is set to the property chain for compiled-in functions. In that case, we follow the links to setup the args instead of executing by code. */
	UField* PropertyChainForCompiledIn;

	/** Currently executed native function */
	UFunction* CurrentNativeFunction;

	bool bArrayContextFailed;
public:

	// Constructors.
	FFrame( UObject* InObject, UFunction* InNode, void* InLocals, FFrame* InPreviousFrame = NULL, UField* InPropertyChainForCompiledIn = NULL );

	virtual ~FFrame()
	{
#if DO_BLUEPRINT_GUARD
		FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();
		if (BlueprintExceptionTracker.ScriptStack.Num())
		{
			BlueprintExceptionTracker.ScriptStack.Pop(false);
		}
#endif
	}

	// Functions.
	COREUOBJECT_API void Step( UObject* Context, RESULT_DECL );

	/** Replacement for Step that uses an explicitly specified property to unpack arguments **/
	COREUOBJECT_API void StepExplicitProperty(void*const Result, UProperty* Property);

	/** Replacement for Step that checks the for byte code, and if none exists, then PropertyChainForCompiledIn is used. Also, makes an effort to verify that the params are in the correct order and the types are compatible. **/
	template<class TProperty>
	FORCEINLINE_DEBUGGABLE void StepCompiledIn(void*const Result);

	/** Replacement for Step that checks the for byte code, and if none exists, then PropertyChainForCompiledIn is used. Also, makes an effort to verify that the params are in the correct order and the types are compatible. **/
	template<class TProperty, typename TNativeType>
	FORCEINLINE_DEBUGGABLE TNativeType& StepCompiledInRef(void*const TemporaryBuffer);

	COREUOBJECT_API virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
	
	COREUOBJECT_API static void KismetExecutionMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity, FName WarningId = FName());

	/** Returns the current script op code */
	const uint8 PeekCode() const { return *Code; }

	/** Skips over the number of op codes specified by NumOps */
	void SkipCode(const int32 NumOps) { Code += NumOps; }

	template<typename TNumericType>
	TNumericType ReadInt();
	float ReadFloat();
	FName ReadName();
	UObject* ReadObject();
	int32 ReadWord();
	UProperty* ReadProperty();

	/** May return null */
	UProperty* ReadPropertyUnchecked();

	/**
	 * Reads a value from the bytestream, which represents the number of bytes to advance
	 * the code pointer for certain expressions.
	 *
	 * @param	ExpressionField		receives a pointer to the field representing the expression; used by various execs
	 *								to drive VM logic
	 */
	CodeSkipSizeType ReadCodeSkipCount();

	/**
	 * Reads a value from the bytestream which represents the number of bytes that should be zero'd out if a NULL context
	 * is encountered
	 *
	 * @param	ExpressionField		receives a pointer to the field representing the expression; used by various execs
	 *								to drive VM logic
	 */
	VariableSizeType ReadVariableSize(UProperty** ExpressionField);

	/**
 	 * This will return the StackTrace of the current callstack from the last native entry point
	 **/
	COREUOBJECT_API FString GetStackTrace() const;

	/**
	* This will return the StackTrace of the all script frames currently active
	**/
	COREUOBJECT_API static FString GetScriptCallstack();
};


/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

inline FFrame::FFrame( UObject* InObject, UFunction* InNode, void* InLocals, FFrame* InPreviousFrame, UField* InPropertyChainForCompiledIn )
	: Node(InNode)
	, Object(InObject)
	, Code(InNode->Script.GetData())
	, Locals((uint8*)InLocals)
	, MostRecentProperty(NULL)
	, MostRecentPropertyAddress(NULL)
	, PreviousFrame(InPreviousFrame)
	, OutParms(NULL)
	, PropertyChainForCompiledIn(InPropertyChainForCompiledIn)
	, CurrentNativeFunction(NULL)
	, bArrayContextFailed(false)
{
#if DO_BLUEPRINT_GUARD
	FScriptTraceStackNode StackNode(InNode->GetOuter()->GetFName(), InNode->GetFName());
	FBlueprintExceptionTracker::Get().ScriptStack.Push(StackNode);
#endif
}

template<typename TNumericType>
inline TNumericType FFrame::ReadInt()
{
	TNumericType Result;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(TNumericType));
#else
	Result = *(TNumericType*)Code;
#endif
	Code += sizeof(TNumericType);
	return Result;
}

inline UObject* FFrame::ReadObject()
{
	// we always pull 64-bits of data out, which is really a UObject* in some representation (depending on platform)
	ScriptPointerType TempCode;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
	FMemory::Memcpy(&TempCode, Code, sizeof(ScriptPointerType));
#else
	TempCode = *(ScriptPointerType*)Code;
#endif

	// turn that uint32 into a UObject pointer
	UObject* Result = (UObject*)(TempCode);
	Code += sizeof(ScriptPointerType);
	return Result;
}

inline UProperty* FFrame::ReadProperty()
{
	UProperty* Result = (UProperty*)ReadObject();
	MostRecentProperty = Result;

	// Callers don't check for NULL; this method is expected to succeed.
	check(Result);

	return Result;
}

inline UProperty* FFrame::ReadPropertyUnchecked()
{
	UProperty* Result = (UProperty*)ReadObject();
	MostRecentProperty = Result;
	return Result;
}

inline float FFrame::ReadFloat()
{
	float Result;
#ifdef REQUIRES_ALIGNED_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(float));
#else
	Result = *(float*)Code;
#endif
	Code += sizeof(float);
	return Result;
}

inline int32 FFrame::ReadWord()
{
	int32 Result;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	uint16 Temporary;
	FMemory::Memcpy(&Temporary, Code, sizeof(uint16));
	Result = Temporary;
#else
	Result = *(uint16*)Code;
#endif
	Code += sizeof(uint16);
	return Result;
}

/**
 * Reads a value from the bytestream, which represents the number of bytes to advance
 * the code pointer for certain expressions.
 */
inline CodeSkipSizeType FFrame::ReadCodeSkipCount()
{
	CodeSkipSizeType Result;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(CodeSkipSizeType));
#else
	Result = *(CodeSkipSizeType*)Code;
#endif

	Code += sizeof(CodeSkipSizeType);
	return Result;
}

inline VariableSizeType FFrame::ReadVariableSize( UProperty** ExpressionField )
{
	VariableSizeType Result=0;

	UObject* Field = ReadObject();
	UProperty* Property = dynamic_cast<UProperty*>(Field);
	if (Property)
	{
		Result = Property->GetSize();
	}

	if ( ExpressionField != nullptr )
	{
		*ExpressionField = Property;
	}

	return Result;
}

inline FName FFrame::ReadName()
{
	FScriptName Result;
#ifdef REQUIRES_ALIGNED_ACCESS
	FMemory::Memcpy(&Result, Code, sizeof(FScriptName));
#else
	Result = *(FScriptName*)Code;
#endif
	Code += sizeof(FScriptName);
	return ScriptNameToName(Result);
}

COREUOBJECT_API void GInitRunaway();

/**
 * Replacement for Step that checks the for byte code, and if none exists, then PropertyChainForCompiledIn is used.
 * Also makes an effort to verify that the params are in the correct order and the types are compatible.
 **/
template<class TProperty>
FORCEINLINE_DEBUGGABLE void FFrame::StepCompiledIn(void*const Result)
{
	if (Code)
	{
		Step(Object, Result);
	}
	else
	{
		checkSlow(dynamic_cast<TProperty*>(PropertyChainForCompiledIn) && dynamic_cast<UProperty*>(PropertyChainForCompiledIn));
		TProperty* Property = (TProperty*)PropertyChainForCompiledIn;
		PropertyChainForCompiledIn = Property->Next;
		StepExplicitProperty(Result, Property);
	}
}

template<class TProperty, typename TNativeType>
FORCEINLINE_DEBUGGABLE TNativeType& FFrame::StepCompiledInRef(void*const TemporaryBuffer)
{
	MostRecentPropertyAddress = NULL;

	if (Code)
	{
		Step(Object, TemporaryBuffer);
	}
	else
	{
		checkSlow(dynamic_cast<TProperty*>(PropertyChainForCompiledIn) && dynamic_cast<UProperty*>(PropertyChainForCompiledIn));
		TProperty* Property = (TProperty*)PropertyChainForCompiledIn;
		PropertyChainForCompiledIn = Property->Next;
		StepExplicitProperty(TemporaryBuffer, Property);
	}

	return (MostRecentPropertyAddress != NULL) ? *(TNativeType*)(MostRecentPropertyAddress) : *(TNativeType*)TemporaryBuffer;
}
