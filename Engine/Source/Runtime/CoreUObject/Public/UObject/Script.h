// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Script.h: Blueprint bytecode execution engine.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSingleton.h"
#include "Stats/Stats.h"
#include "Misc/EnumClassFlags.h"

struct FFrame;

// It's best to set only one of these, but strictly speaking you could set both.
// The results will be confusing. Native time would be included only in a coarse 
// 'native time' timer, and all overhead would be broken up per script function
#define TOTAL_OVERHEAD_SCRIPT_STATS (STATS && 0)
#define PER_FUNCTION_SCRIPT_STATS (STATS && 1)

DECLARE_STATS_GROUP(TEXT("Scripting"), STATGROUP_Script, STATCAT_Advanced);

#if TOTAL_OVERHEAD_SCRIPT_STATS
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Blueprint - (All) VM Time (ms)"),     STAT_ScriptVmTime_Total,     STATGROUP_Script, COREUOBJECT_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Blueprint - (All) Native Time (ms)"), STAT_ScriptNativeTime_Total, STATGROUP_Script, COREUOBJECT_API);
#endif // TOTAL_OVERHEAD_SCRIPT_STATS

/*-----------------------------------------------------------------------------
	Constants & types.
-----------------------------------------------------------------------------*/

// Sizes.
enum { MAX_STRING_CONST_SIZE = 1024 }; 

/**
 * this is the size of the buffer used by the VM for unused simple (not constructed) return values.
 */
enum { MAX_SIMPLE_RETURN_VALUE_SIZE = 64 };

/**
 * a typedef for the size (in bytes) of a property; typedef'd because this value must be synchronized between the
 * blueprint compiler and the VM
 */
typedef uint16 VariableSizeType;


/**
 * a typedef for the number of bytes to skip-over when certain expressions are evaluated by the VM
 * (e.g. context expressions that resolve to NULL, etc.)
 * typedef'd because this type must be synchronized between the blueprint compiler and the VM
 */

// If you change this, make sure to bump either VER_MIN_SCRIPTVM_UE4 or VER_MIN_SCRIPTVM_LICENSEEUE4
#define SCRIPT_LIMIT_BYTECODE_TO_64KB 0

#if SCRIPT_LIMIT_BYTECODE_TO_64KB
typedef uint16 CodeSkipSizeType;
#else
typedef uint32 CodeSkipSizeType;
#endif


//
// Blueprint VM intrinsic return value declaration.
//
#define RESULT_PARAM Z_Param__Result
#define RESULT_DECL void*const RESULT_PARAM

/** Space where UFunctions are asking to be called */
namespace FunctionCallspace
{
	enum Type
	{
		/** This function call should be absorbed (ie client side with no authority) */
		Absorbed = 0x0,
		/** This function call should be called remotely via its net driver */
		Remote = 0x1,
		/** This function call should be called locally */
		Local = 0x2
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(FunctionCallspace::Type Callspace)
	{
		switch (Callspace)
		{
		case Absorbed:
			return TEXT("Absorbed");
		case Remote:
			return TEXT("Remote");
		case Local:
			return TEXT("Local");
		}

		return TEXT("");
	}
}

//
// Function flags.
//
// Note: Please keep ParseFunctionFlags in sync when this enum is modified.
enum EFunctionFlags : uint32
{
	// Function flags.
	FUNC_None				= 0x00000000,

	FUNC_Final				= 0x00000001,	// Function is final (prebindable, non-overridable function).
	FUNC_RequiredAPI			= 0x00000002,	// Indicates this function is DLL exported/imported.
	FUNC_BlueprintAuthorityOnly= 0x00000004,   // Function will only run if the object has network authority
	FUNC_BlueprintCosmetic	= 0x00000008,   // Function is cosmetic in nature and should not be invoked on dedicated servers
	// FUNC_				= 0x00000010,   // unused.
	// FUNC_				= 0x00000020,   // unused.
	FUNC_Net				= 0x00000040,   // Function is network-replicated.
	FUNC_NetReliable		= 0x00000080,   // Function should be sent reliably on the network.
	FUNC_NetRequest			= 0x00000100,	// Function is sent to a net service
	FUNC_Exec				= 0x00000200,	// Executable from command line.
	FUNC_Native				= 0x00000400,	// Native function.
	FUNC_Event				= 0x00000800,   // Event function.
	FUNC_NetResponse		= 0x00001000,   // Function response from a net service
	FUNC_Static				= 0x00002000,   // Static function.
	FUNC_NetMulticast		= 0x00004000,	// Function is networked multicast Server -> All Clients
	// FUNC_				= 0x00008000,   // unused.
	FUNC_MulticastDelegate	= 0x00010000,	// Function is a multi-cast delegate signature (also requires FUNC_Delegate to be set!)
	FUNC_Public				= 0x00020000,	// Function is accessible in all classes (if overridden, parameters must remain unchanged).
	FUNC_Private			= 0x00040000,	// Function is accessible only in the class it is defined in (cannot be overridden, but function name may be reused in subclasses.  IOW: if overridden, parameters don't need to match, and Super.Func() cannot be accessed since it's private.)
	FUNC_Protected			= 0x00080000,	// Function is accessible only in the class it is defined in and subclasses (if overridden, parameters much remain unchanged).
	FUNC_Delegate			= 0x00100000,	// Function is delegate signature (either single-cast or multi-cast, depending on whether FUNC_MulticastDelegate is set.)
	FUNC_NetServer			= 0x00200000,	// Function is executed on servers (set by replication code if passes check)
	FUNC_HasOutParms		= 0x00400000,	// function has out (pass by reference) parameters
	FUNC_HasDefaults		= 0x00800000,	// function has structs that contain defaults
	FUNC_NetClient			= 0x01000000,	// function is executed on clients
	FUNC_DLLImport			= 0x02000000,	// function is imported from a DLL
	FUNC_BlueprintCallable	= 0x04000000,	// function can be called from blueprint code
	FUNC_BlueprintEvent		= 0x08000000,	// function can be overridden/implemented from a blueprint
	FUNC_BlueprintPure		= 0x10000000,	// function can be called from blueprint code, and is also pure (produces no side effects). If you set this, you should set FUNC_BlueprintCallable as well.
	FUNC_EditorOnly			= 0x20000000,	// function can only be called from an editor scrippt.
	FUNC_Const				= 0x40000000,	// function can be called from blueprint code, and only reads state (never writes state)
	FUNC_NetValidate		= 0x80000000,	// function must supply a _Validate implementation

	FUNC_AllFlags		= 0xFFFFFFFF,
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, EFunctionFlags& Flags)
{
	Ar << (uint32&)Flags;
	return Ar;
}

ENUM_CLASS_FLAGS(EFunctionFlags)

// Combinations of flags.
#define FUNC_FuncInherit       ((EFunctionFlags)(FUNC_Exec | FUNC_Event | FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_BlueprintAuthorityOnly | FUNC_BlueprintCosmetic))
#define FUNC_FuncOverrideMatch ((EFunctionFlags)(FUNC_Exec | FUNC_Final | FUNC_Static | FUNC_Public | FUNC_Protected | FUNC_Private))
#define FUNC_NetFuncFlags      ((EFunctionFlags)(FUNC_Net | FUNC_NetReliable | FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast))
#define FUNC_AccessSpecifiers  ((EFunctionFlags)(FUNC_Public | FUNC_Private | FUNC_Protected))

//
// Evaluatable expression item types.
//
enum EExprToken
{
	// Variable references.
	EX_LocalVariable		= 0x00,	// A local variable.
	EX_InstanceVariable		= 0x01,	// An object variable.
	EX_DefaultVariable		= 0x02, // Default variable for a class context.
	//						= 0x03,
	EX_Return				= 0x04,	// Return from function.
	//						= 0x05,
	EX_Jump					= 0x06,	// Goto a local address in code.
	EX_JumpIfNot			= 0x07,	// Goto if not expression.
	//						= 0x08,
	EX_Assert				= 0x09,	// Assertion.
	//						= 0x0A,
	EX_Nothing				= 0x0B,	// No operation.
	//						= 0x0C,
	//						= 0x0D,
	//						= 0x0E,
	EX_Let					= 0x0F,	// Assign an arbitrary size value to a variable.
	//						= 0x10,
	//						= 0x11,
	EX_ClassContext			= 0x12,	// Class default object context.
	EX_MetaCast             = 0x13, // Metaclass cast.
	EX_LetBool				= 0x14, // Let boolean variable.
	EX_EndParmValue			= 0x15,	// end of default value for optional function parameter
	EX_EndFunctionParms		= 0x16,	// End of function call parameters.
	EX_Self					= 0x17,	// Self object.
	EX_Skip					= 0x18,	// Skippable expression.
	EX_Context				= 0x19,	// Call a function through an object context.
	EX_Context_FailSilent	= 0x1A, // Call a function through an object context (can fail silently if the context is NULL; only generated for functions that don't have output or return values).
	EX_VirtualFunction		= 0x1B,	// A function call with parameters.
	EX_FinalFunction		= 0x1C,	// A prebound function call with parameters.
	EX_IntConst				= 0x1D,	// Int constant.
	EX_FloatConst			= 0x1E,	// Floating point constant.
	EX_StringConst			= 0x1F,	// String constant.
	EX_ObjectConst		    = 0x20,	// An object constant.
	EX_NameConst			= 0x21,	// A name constant.
	EX_RotationConst		= 0x22,	// A rotation constant.
	EX_VectorConst			= 0x23,	// A vector constant.
	EX_ByteConst			= 0x24,	// A byte constant.
	EX_IntZero				= 0x25,	// Zero.
	EX_IntOne				= 0x26,	// One.
	EX_True					= 0x27,	// Bool True.
	EX_False				= 0x28,	// Bool False.
	EX_TextConst			= 0x29, // FText constant
	EX_NoObject				= 0x2A,	// NoObject.
	EX_TransformConst		= 0x2B, // A transform constant
	EX_IntConstByte			= 0x2C,	// Int constant that requires 1 byte.
	EX_NoInterface			= 0x2D, // A null interface (similar to EX_NoObject, but for interfaces)
	EX_DynamicCast			= 0x2E,	// Safe dynamic class casting.
	EX_StructConst			= 0x2F, // An arbitrary UStruct constant
	EX_EndStructConst		= 0x30, // End of UStruct constant
	EX_SetArray				= 0x31, // Set the value of arbitrary array
	EX_EndArray				= 0x32,
	//						= 0x33,
	EX_UnicodeStringConst   = 0x34, // Unicode string constant.
	EX_Int64Const			= 0x35,	// 64-bit integer constant.
	EX_UInt64Const			= 0x36,	// 64-bit unsigned integer constant.
	//						= 0x37,
	EX_PrimitiveCast		= 0x38,	// A casting operator for primitives which reads the type as the subsequent byte
	EX_SetSet				= 0x39,
	EX_EndSet				= 0x3A,
	EX_SetMap				= 0x3B,
	EX_EndMap				= 0x3C,
	EX_SetConst				= 0x3D,
	EX_EndSetConst			= 0x3E,
	EX_MapConst				= 0x3F,
	EX_EndMapConst			= 0x40,
	//						= 0x41,
	EX_StructMemberContext	= 0x42, // Context expression to address a property within a struct
	EX_LetMulticastDelegate	= 0x43, // Assignment to a multi-cast delegate
	EX_LetDelegate			= 0x44, // Assignment to a delegate
	//						= 0x45, 
	//						= 0x46, // CST_ObjectToInterface
	//						= 0x47, // CST_ObjectToBool
	EX_LocalOutVariable		= 0x48, // local out (pass by reference) function parameter
	//						= 0x49, // CST_InterfaceToBool
	EX_DeprecatedOp4A		= 0x4A,
	EX_InstanceDelegate		= 0x4B,	// const reference to a delegate or normal function object
	EX_PushExecutionFlow	= 0x4C, // push an address on to the execution flow stack for future execution when a EX_PopExecutionFlow is executed.   Execution continues on normally and doesn't change to the pushed address.
	EX_PopExecutionFlow		= 0x4D, // continue execution at the last address previously pushed onto the execution flow stack.
	EX_ComputedJump			= 0x4E,	// Goto a local address in code, specified by an integer value.
	EX_PopExecutionFlowIfNot = 0x4F, // continue execution at the last address previously pushed onto the execution flow stack, if the condition is not true.
	EX_Breakpoint			= 0x50, // Breakpoint.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_InterfaceContext		= 0x51,	// Call a function through a native interface variable
	EX_ObjToInterfaceCast   = 0x52,	// Converting an object reference to native interface variable
	EX_EndOfScript			= 0x53, // Last byte in script code
	EX_CrossInterfaceCast	= 0x54, // Converting an interface variable reference to native interface variable
	EX_InterfaceToObjCast   = 0x55, // Converting an interface variable reference to an object
	//						= 0x56,
	//						= 0x57,
	//						= 0x58,
	//						= 0x59,
	EX_WireTracepoint		= 0x5A, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_SkipOffsetConst		= 0x5B, // A CodeSizeSkipOffset constant
	EX_AddMulticastDelegate = 0x5C, // Adds a delegate to a multicast delegate's targets
	EX_ClearMulticastDelegate = 0x5D, // Clears all delegates in a multicast target
	EX_Tracepoint			= 0x5E, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_LetObj				= 0x5F,	// assign to any object ref pointer
	EX_LetWeakObjPtr		= 0x60, // assign to a weak object pointer
	EX_BindDelegate			= 0x61, // bind object and name to delegate
	EX_RemoveMulticastDelegate = 0x62, // Remove a delegate from a multicast delegate's targets
	EX_CallMulticastDelegate = 0x63, // Call multicast delegate
	EX_LetValueOnPersistentFrame = 0x64,
	EX_ArrayConst			= 0x65,
	EX_EndArrayConst		= 0x66,
	EX_SoftObjectConst		= 0x67,
	EX_CallMath				= 0x68, // static pure function from on local call space
	EX_SwitchValue			= 0x69,
	EX_InstrumentationEvent	= 0x6A, // Instrumentation event
	EX_ArrayGetByRef		= 0x6B,
	EX_Max					= 0x100,
};


enum ECastToken
{
	CST_ObjectToInterface	= 0x46,
	CST_ObjectToBool		= 0x47,
	CST_InterfaceToBool		= 0x49,
	CST_Max					= 0xFF,
};

// Kinds of text literals
enum class EBlueprintTextLiteralType : uint8
{
	/* Text is an empty string. The bytecode contains no strings, and you should use FText::GetEmpty() to initialize the FText instance. */
	Empty,
	/** Text is localized. The bytecode will contain three strings - source, key, and namespace - and should be loaded via FInternationalization */
	LocalizedText,
	/** Text is culture invariant. The bytecode will contain one string, and you should use FText::AsCultureInvariant to initialize the FText instance. */
	InvariantText,
	/** Text is a literal FString. The bytecode will contain one string, and you should use FText::FromString to initialize the FText instance. */
	LiteralString,
	/** Text is from a string table. The bytecode will contain an object pointer (not used) and two strings - the table ID, and key - and should be found via FText::FromStringTable */
	StringTableEntry,
};

// Kinds of Blueprint exceptions
namespace EBlueprintExceptionType
{
	enum Type
	{
		Breakpoint,
		Tracepoint,
		WireTracepoint,
		AccessViolation,
		InfiniteLoop,
		NonFatalError,
		FatalError,
	};
}

// Script instrumentation event types
namespace EScriptInstrumentation
{
	enum Type
	{
		Class = 0,
		ClassScope,
		Instance,
		Event,
		InlineEvent,
		ResumeEvent,
		PureNodeEntry,
		NodeDebugSite,
		NodeEntry,
		NodeExit,
		PushState,
		RestoreState,
		ResetState,
		SuspendState,
		PopState,
		TunnelEndOfThread,
		Stop
	};
}

// Information about a blueprint exception
struct FBlueprintExceptionInfo
{
public:
	FBlueprintExceptionInfo(EBlueprintExceptionType::Type InEventType)
		: EventType(InEventType)
	{
	}

	FBlueprintExceptionInfo(EBlueprintExceptionType::Type InEventType, const FText& InDescription)
		: EventType(InEventType)
		, Description(InDescription)
	{
	}

	EBlueprintExceptionType::Type GetType() const
	{
		return EventType;
	}

	const FText& GetDescription() const
	{
		return Description;
	}
protected:
	EBlueprintExceptionType::Type EventType;
	FText Description;
};

// Information about a blueprint instrumentation signal
struct COREUOBJECT_API FScriptInstrumentationSignal
{
public:

	FScriptInstrumentationSignal(EScriptInstrumentation::Type InEventType, const UObject* InContextObject, const struct FFrame& InStackFrame, const FName EventNameIn = NAME_None);

	FScriptInstrumentationSignal(EScriptInstrumentation::Type InEventType, const UObject* InContextObject, UFunction* InFunction, const int32 LinkId = INDEX_NONE)
		: EventType(InEventType)
		, ContextObject(InContextObject)
		, Function(InFunction)
		, EventName(NAME_None)
		, StackFramePtr(nullptr)
		, LatentLinkId(LinkId)
	{
	}

	/** Access to the event type */
	EScriptInstrumentation::Type GetType() const { return EventType; }

	/** Designates the event type */
	void SetType(EScriptInstrumentation::Type InType) { EventType = InType;	}

	/** Returns true if the context object is valid */
	bool IsContextObjectValid() const { return ContextObject != nullptr; }

	/** Returns the context object */
	const UObject* GetContextObject() const { return ContextObject; }

	/** Returns true if the stackframe is valid */
	bool IsStackFrameValid() const { return StackFramePtr != nullptr; }

	/** Returns the stackframe */
	const FFrame& GetStackFrame() const { return *StackFramePtr; }

	/** Returns the owner class name of the active instance */
	const UClass* GetClass() const;

	/** Returns the function scope class */
	const UClass* GetFunctionClassScope() const;

	/** Returns the name of the active function */
	FName GetFunctionName() const;

	/** Returns the script code offset */
	int32 GetScriptCodeOffset() const;

	/** Returns the latent link id for latent events */
	int32 GetLatentLinkId() const { return LatentLinkId; }

protected:

	/** The event signal type */
	EScriptInstrumentation::Type EventType;
	/** The context object the event is from */
	const UObject* ContextObject;
	/** The function that emitted this event */
	const UFunction* Function;
	/** The event override name */
	const FName EventName;
	/** The stack frame for the  */
	const struct FFrame* StackFramePtr;
	const int32 LatentLinkId;
};

// Blueprint core runtime delegates
class COREUOBJECT_API FBlueprintCoreDelegates
{
public:
	// Callback for debugging events such as a breakpoint (Object that triggered event, active stack frame, Info)
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnScriptDebuggingEvent, const UObject*, const struct FFrame&, const FBlueprintExceptionInfo&);
	// Callback for when script execution terminates.
	DECLARE_MULTICAST_DELEGATE(FOnScriptExecutionEnd);
	// Callback for blueprint profiling signals
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnScriptInstrumentEvent, const FScriptInstrumentationSignal& );
	// Callback for blueprint instrumentation enable/disable events
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnToggleScriptProfiler, bool );

public:
	// Called when a script exception occurs
	static FOnScriptDebuggingEvent OnScriptException;
	// Called when a script execution terminates.
	static FOnScriptExecutionEnd OnScriptExecutionEnd;
	// Called when a script profiling event is fired
	static FOnScriptInstrumentEvent OnScriptProfilingEvent;
	// Called when a script profiler is enabled/disabled
	static FOnToggleScriptProfiler OnToggleScriptProfiler;

public:
	static void ThrowScriptException(const UObject* ActiveObject, const struct FFrame& StackFrame, const FBlueprintExceptionInfo& Info);
	static void InstrumentScriptEvent(const FScriptInstrumentationSignal& Info);
	static void SetScriptMaximumLoopIterations( const int32 MaximumLoopIterations );
};

// Scoped struct to allow execution of script in editor, while resetting the runaway loop counts
struct COREUOBJECT_API FEditorScriptExecutionGuard
{
public:
	FEditorScriptExecutionGuard();
	~FEditorScriptExecutionGuard();

private:
	bool bOldGAllowScriptExecutionInEditor;
};

#if TOTAL_OVERHEAD_SCRIPT_STATS
	// Low overhead timer used to instrument the VM (ProcessEvent and ProcessInternal):
	struct FBlueprintEventTimer
	{
		struct FPausableScopeTimer;
		struct FScopedVMTimer;

		class FThreadedTimerManager : public TThreadSingleton<FThreadedTimerManager>
		{
		public:
			FThreadedTimerManager()
				: ActiveTimer(nullptr)
				, ActiveVMScope(nullptr)
			{}

			FPausableScopeTimer* ActiveTimer;

			// We need to keep track of the current VM timer because we only want to
			// track time while 'in' the VM. We use this to detect whether we're running
			// script or just doing RPC:
			FScopedVMTimer* ActiveVMScope;
		};

		struct FPausableScopeTimer
		{
			FPausableScopeTimer() 
			{ 
				FPausableScopeTimer*& ActiveTimer = FThreadedTimerManager::Get().ActiveTimer;

				double CurrentTime = FPlatformTime::Seconds();
				if (ActiveTimer)
				{
					ActiveTimer->Pause(CurrentTime);
				}

				PreviouslyActiveTimer = ActiveTimer;
				StartTime = CurrentTime;
				TotalTime = 0.0;

				ActiveTimer = this;
			}

			~FPausableScopeTimer()
			{
				if (PreviouslyActiveTimer)
				{
					PreviouslyActiveTimer->Resume();
				}
				FThreadedTimerManager::Get().ActiveTimer = PreviouslyActiveTimer;
			}

			void Pause(double CurrentTime) { TotalTime += CurrentTime - StartTime; }
			void Resume() { StartTime = FPlatformTime::Seconds();  }
			double Stop() { return TotalTime + (FPlatformTime::Seconds() - StartTime); }

		private:
			FPausableScopeTimer* PreviouslyActiveTimer;
			double TotalTime;
			double StartTime;
		};

		struct FScopedVMTimer
		{
			FScopedVMTimer()
				: Timer()
				, VMParent(nullptr)
			{
				FScopedVMTimer*& ActiveVMTimer = FThreadedTimerManager::Get().ActiveVMScope;
				VMParent = ActiveVMTimer;

				ActiveVMTimer = this;
			}

			~FScopedVMTimer()
			{
				INC_FLOAT_STAT_BY(STAT_ScriptVmTime_Total, Timer.Stop() * 1000.0);
				FThreadedTimerManager::Get().ActiveVMScope = VMParent;
			}
			FPausableScopeTimer Timer;
			FScopedVMTimer* VMParent;
		};

		struct FScopedNativeTimer
		{
			FScopedNativeTimer()
				: Timer()
			{}

			~FScopedNativeTimer()
			{
				// only track native time when in a VM scope, RPC time
				// can be tracked by the online system or whatever is making RPCs:
				if (FThreadedTimerManager::Get().ActiveVMScope)
				{
					INC_FLOAT_STAT_BY(STAT_ScriptNativeTime_Total, Timer.Stop()* 1000.0);
				}
			}

			FPausableScopeTimer Timer;
		};
	};

#define SCOPED_SCRIPT_NATIVE_TIMER(VarName) \
	FBlueprintEventTimer::FScopedNativeTimer VarName

#else  // TOTAL_OVERHEAD_SCRIPT_STATS
	#define SCOPED_SCRIPT_NATIVE_TIMER(VarName) 
#endif // TOTAL_OVERHEAD_SCRIPT_STATS
/** @return True if the char can be used in an identifier in c++ */
COREUOBJECT_API bool IsValidCPPIdentifierChar(TCHAR Char);

/** @return A string that contains only Char if Char IsValidCPPIdentifierChar, otherwise returns a corresponding sequence of valid c++ chars */
COREUOBJECT_API FString ToValidCPPIdentifierChars(TCHAR Char);

/** 
	@param InName The string to transform
	@param bDeprecated whether the name has been deprecated
	@Param Prefix The prefix to be prepended to the return value, accepts nullptr or empty string
	@return A corresponding string that contains only valid c++ characters and is prefixed with Prefix
*/
COREUOBJECT_API FString UnicodeToCPPIdentifier(const FString& InName, bool bDeprecated, const TCHAR* Prefix);

