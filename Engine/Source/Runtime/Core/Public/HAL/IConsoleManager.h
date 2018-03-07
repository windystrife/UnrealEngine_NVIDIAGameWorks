// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Delegates/IDelegateInstance.h"
#include "Delegates/Delegate.h"

#define TRACK_CONSOLE_FIND_COUNT !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

template <class T> class TConsoleVariableData;

/**
 * Console variable usage guide:
 *
 * The variable should be creates early in the initialization but not before (not in global variable construction).
 * Choose the right variable type, consider using a console command if more functionality is needed (see Exec()).
 * Available types: int, float, int&, float&, string
 * There is no bool type as the int type provides enough functionality (0=false, 1=true) and can be extended easily (e.g. 2=auto, -1=debug)
 * Always provide a good help text, other should be able to understand the function of the console variable by reading this help.
 * The help length should be limited to a reasonable width in order to work well for low res screen resolutions.
 *
 * Usage in the game console:
 *   <COMMAND> ?				print the HELP
 *   <COMMAND>	 				print the current state of the console variable
 *   <COMMAND> x 				set and print the new state of the console variable
 *
 * All variables support auto completion. The single line help that can show up there is currently not connected to the help as the help text
 * is expected to be multi line.
 * The former Exec() system can be used to access the console variables.
 * Use console variables only in main thread.
 * The state of console variables is not network synchronized or serialized (load/save). The plan is to allow to set the state in external files (game/platform/engine/local).
 */

/**
 * Bitmask 0x1, 0x2, 0x4, ..
 */
enum EConsoleVariableFlags
{
	/**
	 * Default, no flags are set, the value is set by the constructor 
	 */
	ECVF_Default = 0x0,
	/**
	 * Console variables marked with this flag behave differently in a final release build.
	 * Then they are are hidden in the console and cannot be changed by the user.
	 */
	ECVF_Cheat = 0x1,
	/**
	 * Console variables cannot be changed by the user (from console).
	 * Changing from C++ or ini is still possible.
	 */
	ECVF_ReadOnly = 0x4,
	/**
	 * UnregisterConsoleObject() was called on this one.
	 * If the variable is registered again with the same type this object is reactivated. This is good for DLL unloading.
	 */
	ECVF_Unregistered = 0x8,
	/**
	 * This flag is set by the ini loading code when the variable wasn't registered yet.
	 * Once the variable is registered later the value is copied over and the variable is destructed.
	 */
	ECVF_CreatedFromIni = 0x10,
	/**
	 * Maintains another shadow copy and updates the copy with render thread commands to maintain proper ordering.
	 * Could be extended for more/other thread.
 	 * Note: On console variable references it assumes the reference is accessed on the render thread only
	 * (Don't use in any other thread or better don't use references to avoid the potential pitfall).
	 */
	ECVF_RenderThreadSafe = 0x20,

	/* ApplyCVarSettingsGroupFromIni will complain if this wasn't set, should not be combined with ECVF_Cheat */
	ECVF_Scalability = 0x40,

	/* those cvars control other cvars with the flag ECVF_Scalability, names should start with "sg." */
	ECVF_ScalabilityGroup = 0x80,

	// ------------------------------------------------

	/* to get some history of where the last value was set by ( useful for track down why a cvar is in a specific state */
	ECVF_SetByMask =				0xff000000,
	
	// the ECVF_SetBy are sorted in override order (weak to strong), the value is not serialized, it only affects it's override behavior when calling Set()

	// lowest priority (default after console variable creation)
	ECVF_SetByConstructor =			0x00000000,
	// from Scalability.ini (lower priority than game settings so it's easier to override partially)
	ECVF_SetByScalability =			0x01000000,
	// (in game UI or from file)
	ECVF_SetByGameSetting =			0x02000000,
	// project settings (editor UI or from file, higher priority than game setting to allow to enforce some setting fro this project)
	ECVF_SetByProjectSetting =		0x03000000,
	// per device setting (e.g. specific iOS device, higher priority than per project to do device specific settings)
	ECVF_SetByDeviceProfile =		0x04000000,
	// per project setting (ini file e.g. Engine.ini or Game.ini)
	ECVF_SetBySystemSettingsIni =	0x05000000,
	// consolevariables.ini (for multiple projects)
	ECVF_SetByConsoleVariablesIni = 0x06000000,
	// a minus command e.g. -VSync (very high priority to enforce the setting for the application)
	ECVF_SetByCommandline =			0x07000000,
	// least useful, likely a hack, maybe better to find the correct SetBy...
	ECVF_SetByCode =				0x08000000,
	// editor UI or console in game or editor
	ECVF_SetByConsole =				0x09000000,

	// ------------------------------------------------
};

class IConsoleVariable;

/** Console variable delegate type  This is a void callback function. */
DECLARE_DELEGATE_OneParam( FConsoleVariableDelegate, IConsoleVariable* );

/** Console command delegate type (takes no arguments.)  This is a void callback function. */
DECLARE_DELEGATE( FConsoleCommandDelegate );

/** Console command delegate type (with arguments.)  This is a void callback function that always takes a list of arguments. */
DECLARE_DELEGATE_OneParam( FConsoleCommandWithArgsDelegate, const TArray< FString >& );

/** Console command delegate type with a world argument. This is a void callback function that always takes a world. */
DECLARE_DELEGATE_OneParam( FConsoleCommandWithWorldDelegate, UWorld* );

/** Console command delegate type (with a world and arguments.)  This is a void callback function that always takes a list of arguments and a world. */
DECLARE_DELEGATE_TwoParams(FConsoleCommandWithWorldAndArgsDelegate, const TArray< FString >&, UWorld*);

/** Console command delegate type (with a world arguments and output device.)  This is a void callback function that always takes a list of arguments, a world and output device. */
DECLARE_DELEGATE_ThreeParams(FConsoleCommandWithWorldArgsAndOutputDeviceDelegate, const TArray< FString >&, UWorld*, FOutputDevice&);

/** Console command delegate type with the output device passed through. */
DECLARE_DELEGATE_OneParam( FConsoleCommandWithOutputDeviceDelegate, FOutputDevice& );

template <class T> class TConsoleVariableData;

/**
 * Interface for console objects (variables and commands)
 */
class IConsoleObject
{

public:

	IConsoleObject()
#if TRACK_CONSOLE_FIND_COUNT
		: FindCallCount(0)
#endif
	{}

	virtual ~IConsoleObject() {}

	/**
	 *  @return never 0, can be multi line ('\n')
	 */
	virtual const TCHAR* GetHelp() const = 0;
	/**
	 *  @return never 0, can be multi line ('\n')
	 */
	virtual void SetHelp(const TCHAR* Value) = 0;
	/**
	 * Get the internal state of the flags.
	 */
	virtual EConsoleVariableFlags GetFlags() const = 0;
	/**
	 * Sets the internal flag state to the specified value.
	 */
	virtual void SetFlags(const EConsoleVariableFlags Value) = 0;

	// Convenience methods -------------------------------------

	/**
	 * Removes the specified flags in the internal state.
	 */
	void ClearFlags(const EConsoleVariableFlags Value)
	{
		uint32 New = (uint32)GetFlags() & ~(uint32)Value;
	
		SetFlags((EConsoleVariableFlags)New);
	}
	/**
	 * Test is any of the specified flags is set in the internal state.
	 */
	bool TestFlags(const EConsoleVariableFlags Value) const
	{
		return ((uint32)GetFlags() & (uint32)Value) != 0;
	}

	/**
	 * Casts this object to an IConsoleVariable, returns 0 if it's not
	 */
	virtual class IConsoleVariable* AsVariable()
	{
		return 0; 
	}

	virtual bool IsVariableInt() const { return false; }

	virtual class TConsoleVariableData<int32>* AsVariableInt()
	{
		ensureMsgf(false, TEXT("Attempted to access variable data of a console variable type that doesn't support it.  For example FindTConsoleVariableData* on a FAutoConsoleVariableRef."));
		return 0; 
	}

	virtual class TConsoleVariableData<float>* AsVariableFloat()
	{
		ensureMsgf(false, TEXT("Attempted to access variable data of a console variable type that doesn't support it.  For example FindTConsoleVariableData* on a FAutoConsoleVariableRef."));
		return 0; 
	}

	virtual class TConsoleVariableData<FString>* AsVariableString()
	{
		ensureMsgf(false, TEXT("Attempted to access variable data of a console variable type that doesn't support it.  For example FindTConsoleVariableData* on a FAutoConsoleVariableRef."));
		return 0;
	}

	/**
	 * Casts this object to an IConsoleCommand, verifying first that it is safe to do so
	 */
	virtual struct IConsoleCommand* AsCommand()
	{
		return 0; 
	}

private: // -----------------------------------------

#if TRACK_CONSOLE_FIND_COUNT
	// no longer pure visual, if that causes problems we can change the interface
	// to track down FindConsoleObject/FindConsoleVariable calls without static
	uint32 FindCallCount;
#endif

	/**
	 *  should only be called by the manager, needs to be implemented for each instance
	 */
	virtual void Release() = 0;

	friend class FConsoleManager;
};

/**
 * Interface for console variables
 */
class IConsoleVariable : public IConsoleObject
{
public:

	/**
	 * Set the internal value from the specified string. 
	 * @param SetBy anything in ECVF_LastSetMask e.g. ECVF_SetByScalability
	 **/
	virtual void Set(const TCHAR* InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode) = 0;
	/**
	 * Get the internal value as int (should not be used on strings).
	 * @return value is not rounded (simple cast)
	 */
	virtual int32 GetInt() const = 0;
	/** Get the internal value as float (works on all types). */
	virtual float GetFloat() const = 0;
	/** Get the internal value as string (works on all types). */
	virtual FString GetString() const = 0;

	/**
	 * Allows to specify a callback function that is called when the console variable value changes.
 	 * Is even called if the value is the same as the value before. Will always be on the game thread.
	 * This can be dangerous (instead try to use RegisterConsoleVariableSink())
	 * - Setting other console variables in the delegate can cause infinite loops
	 * - Setting many console variables could result in wasteful cycles (e.g. if multiple console variables require to reattach all objects it would happen for each one)
	 * - The call can be at any time during initialization.
	 * As this cannot be specified during constructions you are not called on creation.
	 * We also don't call for the SetOnChangedCallback() call as this is up to the caller.
	 **/
	virtual void SetOnChangedCallback(const FConsoleVariableDelegate& Callback) = 0;

	// convenience methods

	/** Set the internal value from the specified int. */
	void Set(int32 InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode)
	{
		// inefficient but no common code path
		Set(*FString::Printf(TEXT("%d"), InValue), SetBy);
	}
	/** Set the internal value from the specified float. */
	void Set(float InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode)
	{
		// inefficient but no common code path
		Set(*FString::Printf(TEXT("%g"), InValue), SetBy);
	}
	void SetWithCurrentPriority(int32 InValue)
	{
		EConsoleVariableFlags CurFlags = (EConsoleVariableFlags)(GetFlags() & ECVF_SetByMask);
		Set(InValue, CurFlags);
	}
	void SetWithCurrentPriority(float InValue)
	{
		EConsoleVariableFlags CurFlags = (EConsoleVariableFlags)(GetFlags() & ECVF_SetByMask);
		Set(InValue, CurFlags);
	}
};

/**
 * Interface for console commands
 */
struct IConsoleCommand : public IConsoleObject
{
	/**
	 * Executes this command (optionally, with arguments)
	 *
	 * @param	Args		Argument list for this command
	 * @param	InWorld		World context for this command
	 * @return	True if the delegate for this command was executed successfully
	 */
	virtual bool Execute( const TArray< FString >& Args, UWorld* InWorld, class FOutputDevice& OutputDevice ) = 0;
};

/**
 * Interface to propagate changes of console variables to another thread
 */
struct IConsoleThreadPropagation
{
	virtual void OnCVarChange(int32& Dest, int32 NewValue) = 0;
	virtual void OnCVarChange(float& Dest, float NewValue) = 0;
	virtual void OnCVarChange(bool& Dest, bool NewValue) = 0;
	virtual void OnCVarChange(FString& Dest, const FString& NewValue) = 0;
};

/**
 * Declares a delegate type that's used by the console manager to call back into a user function for each
 * known console object.
 *
 * First parameter is the Name string for the current console object
 * Second parameter is the current console object
 */
DECLARE_DELEGATE_TwoParams( FConsoleObjectVisitor, const TCHAR*, IConsoleObject* );


/**
 * Class representing an handle to an online delegate.
 */
class FConsoleVariableSinkHandle
{
public:
	FConsoleVariableSinkHandle()
	{
	}

	explicit FConsoleVariableSinkHandle(FDelegateHandle InHandle)
		: Handle(InHandle)
	{
	}

	template <typename MulticastDelegateType>
	void RemoveFromDelegate(MulticastDelegateType& MulticastDelegate)
	{
		MulticastDelegate.Remove(Handle);
	}

	template <typename DelegateType>
	bool HasSameHandle(const DelegateType& Delegate) const
	{
		return Delegate.GetHandle() == Handle;
	}

private:
	FDelegateHandle Handle;
};


/**
 * handles console commands and variables, registered console variables are released on destruction
 */
struct CORE_API IConsoleManager
{
	/**
	 * Create a int console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, int32 DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	/**
	 * Create a float console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, float DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	/**
	 * Create a string console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, const FString& DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	/**
	 * Create a reference to a int console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	/**
	 * Create a reference to a float console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	/**
	* Create a reference to a bool console variable
	* @param Name must not be 0
	* @param Help must not be 0
	* @param Flags bitmask combined from EConsoleVariableFlags
	*/
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, bool& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	/**
	 * Create a reference to a show flag variable
	 * @param CVarName must not be 0, e.g. "Show.PostProcessing"
	 * @param FlagName must not be 0, e.g. "PostProcessing"
	 * @param BitNumber in the memory defined by Force0MaskPtr and Force1MaskPtr
	 * @param Force0MaskPtr memory that contains the bits that should be forced to 0
	 * @param Force1MaskPtr memory that contains the bits that should be forced to 1
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	virtual IConsoleVariable* RegisterConsoleVariableBitRef(const TCHAR* CVarName, const TCHAR* FlagName, uint32 BitNumber, uint8* Force0MaskPtr, uint8* Force1MaskPtr, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;

	// ----------

	/**
	 * The sinks are only called if a change has been done since the last time
	 * Should be called in very few points:
	 *  - after ini file loading
	 *  - after user console input
	 *  - user initiated a console variable change (it needs to be clear to user that a cvar can change e.g. game options menu)
	 *  - beginning of Tick (to catch stray Set() calls, which are usually bad)
	 */
	virtual void CallAllConsoleVariableSinks() = 0;

	/**
	 * The registered command is executed at few defined points (see CallAllConsoleVariableSinks)
	 * @param Command
	 */
	virtual FConsoleVariableSinkHandle RegisterConsoleVariableSink_Handle(const FConsoleCommandDelegate& Command) = 0;

	/**
	 * The registered command is executed at few defined points (see CallAllConsoleVariableSinks)
	 * @param Command
	 */
	virtual void UnregisterConsoleVariableSink_Handle(FConsoleVariableSinkHandle Handle) = 0;

	// ----------

	/**
	 * Register a console command that takes no arguments
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command, uint32 Flags = ECVF_Default) = 0;

	/**
	 * Register a console command that takes arguments
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command, uint32 Flags = ECVF_Default) = 0;

	/**
	 * Register a console command that takes arguments
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldDelegate& Command, uint32 Flags = ECVF_Default) = 0;

	/**
	* Register a console command that takes arguments
	*
	* @param	Name		The name of this command (must not be nullptr)
	* @param	Help		Help text for this command
	* @param	Command		The user function to call when this command is executed
	* @param	Flags		Optional flags bitmask
	*/
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldAndArgsDelegate& Command, uint32 Flags = ECVF_Default) = 0;

	/**
	* Register a console command that takes arguments
	*
	* @param	Name		The name of this command (must not be nullptr)
	* @param	Help		Help text for this command
	* @param	Command		The user function to call when this command is executed
	* @param	Flags		Optional flags bitmask
	*/
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default) = 0;

	/**
	 * Register a console command that takes arguments
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default) = 0;

	/**
	 * Register a console command that is handles by an Exec functions (for auto completion)
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Flags		Optional flags bitmask
	 */
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, uint32 Flags = (uint32)ECVF_Default) = 0;

	/**
	 * Unregisters a console object, if that object was registered. O(n), n is the console object count
	 *
	 * @param ConsoleObject - object to remove
	 * @param bool bKeepState if the current state is kept in memory until a cvar with the same name is registered
	 */
	virtual void UnregisterConsoleObject( IConsoleObject* ConsoleObject, bool bKeepState = true) = 0;

	/**
	 * Find a console variable
	 * @param Name must not be 0
	 * @return 0 if the object wasn't found
	 */
	virtual IConsoleVariable* FindConsoleVariable(const TCHAR* Name) const = 0;

	/**
	* Find a console variable or command
	* @param Name must not be 0
	* @return 0 if the object wasn't found
	*/
	virtual IConsoleObject* FindConsoleObject(const TCHAR* Name) const = 0;

	/**
	 * Find a typed console variable (faster access to the value, no virtual function call)
	 * @param Name must not be 0
	 * @return 0 if the object wasn't found
	 */
	TConsoleVariableData<int32>* FindTConsoleVariableDataInt(const TCHAR* Name) const 
	{ 
		IConsoleVariable* P = FindConsoleVariable(Name); 
		
		return P ? P->AsVariableInt() : 0; 
	}

	/**
	 * Find a typed console variable (faster access to the value, no virtual function call)
	 * @param Name must not be 0
	 * @return 0 if the object wasn't found
	 */
	TConsoleVariableData<float>* FindTConsoleVariableDataFloat(const TCHAR* Name) const 
	{ 
		IConsoleVariable* P = FindConsoleVariable(Name); 

		return P ? P->AsVariableFloat() : 0; 
	}


	/**
	 *  Iterate in O(n), not case sensitive, does not guarantee that UnregisterConsoleObject() will work in the loop
	 *  @param Visitor must not be 0
	 *  @param ThatStartsWith must not be 0 
	 */
	virtual void ForEachConsoleObjectThatStartsWith( const FConsoleObjectVisitor& Visitor, const TCHAR* ThatStartsWith = TEXT("")) const = 0;

	/**
	 *  Not case sensitive, does not guarantee that UnregisterConsoleObject() will work in the loop
	 *  @param Visitor must not be 0
	 *  @param ThatContains must not be 0 
	 */
	virtual void ForEachConsoleObjectThatContains(const FConsoleObjectVisitor& Visitor, const TCHAR* ThatContains) const = 0;

	/**
	 * Process user input
	 *  e.g.
	 *  "MyCVar" to get the current value of the console variable
	 *  "MyCVar -5.2" to set the value to -5.2
	 *  "MyCVar ?" to get the help text
	 *  @param	Input		must not be 0
	 *  @param	Ar			archive
	 *  @param	InWorld		world context
	 *  @return true if the command was recognized
	 */
	virtual bool ProcessUserConsoleInput(const TCHAR* Input, FOutputDevice& Ar, UWorld* InWorld) = 0;

	/**
	 * @param Input - must not be 0
	 */
	virtual void AddConsoleHistoryEntry(const TCHAR* Input) = 0;
	
	/**
	 */
	virtual void GetConsoleHistory(TArray<FString>& Out) = 0; 

	/**
	 * Check if a name (command or variable) has been registered with the console manager
	 * @param Name - Name to check. Must not be 0
	 */
	virtual bool IsNameRegistered(const TCHAR* Name) const = 0;

	// currently only for render thread
	// @param InCallback 0 to disable the callbacks
	virtual void RegisterThreadPropagation(uint32 ThreadId = 0, IConsoleThreadPropagation* InCallback = 0) = 0;

	/** Returns the singleton for the console manager **/
	FORCEINLINE static IConsoleManager& Get()
	{
		if (!Singleton)
		{
			SetupSingleton();
			check(Singleton != nullptr);
		}
		return *Singleton;
	}

protected:
	virtual ~IConsoleManager() { }

private:
	/** Singleton for the console manager **/
	static IConsoleManager* Singleton;

	/** Function to create the singleton **/
	static void SetupSingleton();
};


/**
 * auto registering console variable sinks (register a callback function that is called when ever a cvar is changes by the user, changes are grouped and happen in specific engine spots during the frame/main loop)
 */
class CORE_API FAutoConsoleVariableSink
{
public:
	/** Constructor, saves the argument for future removal from the console variable system **/
	FAutoConsoleVariableSink(const FConsoleCommandDelegate& InCommand)
		: Command(InCommand)
	{
		Handle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(Command);
	}
	/** Destructor, removes the console variable sink **/
	virtual ~FAutoConsoleVariableSink()
	{
//disabled for now, destruction order makes this not always working		IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(Handle);
	}

	const FConsoleCommandDelegate& Command;
	FConsoleVariableSinkHandle Handle;
};


/**
 * Base class for autoregistering console commands.
 */
class CORE_API FAutoConsoleObject
{
protected:
	/** Constructor, saves the argument for future removal from the console variable system **/
	FAutoConsoleObject(IConsoleObject* InTarget)
		: Target(InTarget)
	{
		check(Target);
	}
	/** Destructor, removes the console object **/
	virtual ~FAutoConsoleObject()
	{
		IConsoleManager::Get().UnregisterConsoleObject(Target);
	}

public:
	/** returns the contained console object as an IConsoleVariable **/
	FORCEINLINE IConsoleVariable* AsVariable()
	{
		checkSlow(Target->AsVariable());
		return static_cast<IConsoleVariable*>(Target);
	}
	/** returns the contained console object as an IConsoleVariable **/
	FORCEINLINE const IConsoleVariable* AsVariable() const
	{
		checkSlow(Target->AsVariable());
		return static_cast<const IConsoleVariable*>(Target);
	}

private:
	/** Contained console object, cannot be 0 **/
	IConsoleObject* Target;
};

/**
 * Autoregistering float, int or string console variable
 */
class CORE_API FAutoConsoleVariable : private FAutoConsoleObject
{
public:
	/**
	 * Create a int console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	FAutoConsoleVariable(const TCHAR* Name, int32 DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariable(Name, DefaultValue, Help, Flags))
	{
	}
	/**
	 * Create a float console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	FAutoConsoleVariable(const TCHAR* Name, float DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariable(Name, DefaultValue, Help, Flags))
	{
	}
	/**
	 * Create a string console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	FAutoConsoleVariable(const TCHAR* Name, const TCHAR* DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariable(Name, DefaultValue, Help, Flags))
	{
	}
	/** Dereference back to a console variable**/
	FORCEINLINE IConsoleVariable& operator*()
	{
		return *AsVariable();
	}
	FORCEINLINE const IConsoleVariable& operator*() const
	{
		return *AsVariable();
	}
	/** Dereference back to a console variable**/
	FORCEINLINE IConsoleVariable* operator->()
	{
		return AsVariable();
	}
	FORCEINLINE const IConsoleVariable* operator->() const
	{
		return AsVariable();
	}
};

/**
 * Autoregistering float, int REF variable class...this changes that value when the console variable is changed. 
 */
class CORE_API FAutoConsoleVariableRef : private FAutoConsoleObject
{
public:
	/**
	 * Create a reference to a int console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	FAutoConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariableRef(Name, RefValue, Help, Flags))
	{
	}
	/**
	 * Create a reference to a float console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	FAutoConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariableRef(Name, RefValue, Help, Flags))
	{
	}

	virtual ~FAutoConsoleVariableRef()
	{
	}
	/** Dereference back to a variable**/
	FORCEINLINE IConsoleVariable& operator*()
	{
		return *AsVariable();
	}
	FORCEINLINE const IConsoleVariable& operator*() const
	{
		return *AsVariable();
	}
	/** Dereference back to a variable**/
	FORCEINLINE IConsoleVariable* operator->()
	{
		return AsVariable();
	}
	FORCEINLINE const IConsoleVariable* operator->() const
	{
		return AsVariable();
	}
};



// currently only supports main and render thread
// optimized for read access speed (no virtual function call and no thread handling if using the right functions)
// T: int32, float
template <class T>
class TConsoleVariableData
{
public:
	// constructor
	TConsoleVariableData(const T DefaultValue)
	{
		for(uint32 i = 0; i < ARRAY_COUNT(ShadowedValue); ++i)
		{
			ShadowedValue[i] = DefaultValue;
		}
	}

	// faster than GetValueOnAnyThread()
	T GetValueOnGameThread() const
	{
		// compiled out in shipping for performance (we can change in development later), if this get triggered you need to call GetValueOnRenderThread() or GetValueOnAnyThread(), the last one is a bit slower
		checkCode(ensure(GetShadowIndex() == 0));	// ensure to not block content creators, #if to optimize in shipping
		return ShadowedValue[0];
	}

	// faster than GetValueOnAnyThread()
	T GetValueOnRenderThread() const
	{
#if !defined(__clang__) // @todo Mac: figure out how to make this compile
		// compiled out in shipping for performance (we can change in development later), if this get triggered you need to call GetValueOnGameThread() or GetValueOnAnyThread(), the last one is a bit slower
		checkCode(ensure(IsInParallelRenderingThread()));	// ensure to not block content creators, #if to optimize in shipping
#endif
		return ShadowedValue[1];
	}

	// convenient, for better performance consider using GetValueOnGameThread() or GetValueOnRenderThread()
	T GetValueOnAnyThread(bool bForceGameThread = false) const
	{
		return ShadowedValue[GetShadowIndex(bForceGameThread)];
	}

private: // ----------------------------------------------------

	// [0]:main thread, [1]: render thread, having them both in the same cache line should only hurt on write which happens rarely for cvars
	T ShadowedValue[2];

	// @return 0:main thread, 1: render thread, later more
	static uint32 GetShadowIndex(bool bForceGameThread = false)
	{	
		if (bForceGameThread)
		{
			checkCode(ensure(!IsInActualRenderingThread()));
			return 0;
		}
		return IsInGameThread() ? 0 : 1;
	}

	// needed for FConsoleVariable and FConsoleVariableRef2, intentionally not public
	T& GetReferenceOnAnyThread(bool bForceGameThread = false)
	{
		return ShadowedValue[GetShadowIndex(bForceGameThread)];
	}

	template<class T2> friend class FConsoleVariable;
};


/**
 * Autoregistering float, int variable class...this changes that value when the console variable is changed. 
 */
template <class T>
class TAutoConsoleVariable : public FAutoConsoleObject
{
public:
	/**
	 * Create a float, int or string console variable
	 * @param Name must not be 0
	 * @param Help must not be 0
	 * @param Flags bitmask combined from EConsoleVariableFlags
	 */
	TAutoConsoleVariable(const TCHAR* Name, const T& DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default);

	T GetValueOnGameThread() const
	{
		return Ref->GetValueOnGameThread();
	}

	T GetValueOnRenderThread() const
	{
		return Ref->GetValueOnRenderThread();
	}

	T GetValueOnAnyThread(bool bForceGameThread = false) const
	{
		return Ref->GetValueOnAnyThread(bForceGameThread);
	}
	
	/** Dereference back to a variable**/
	FORCEINLINE IConsoleVariable& operator*()
	{
		return *AsVariable();
	}
	FORCEINLINE const IConsoleVariable& operator*() const
	{
		return *AsVariable();
	}
	/** Dereference back to a variable**/
	FORCEINLINE IConsoleVariable* operator->()
	{
		return AsVariable();
	}
	FORCEINLINE const IConsoleVariable* operator->() const
	{
		return AsVariable();
	}
private:
	TConsoleVariableData<T>* Ref;
};

template <>
inline TAutoConsoleVariable<int32>::TAutoConsoleVariable(const TCHAR* Name, const int32& DefaultValue, const TCHAR* Help, uint32 Flags)
	: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariable(Name, DefaultValue, Help, Flags))
{
	Ref = AsVariable()->AsVariableInt();
}

template <>
inline TAutoConsoleVariable<float>::TAutoConsoleVariable(const TCHAR* Name, const float& DefaultValue, const TCHAR* Help, uint32 Flags)
	: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariable(Name, DefaultValue, Help, Flags))
{
	Ref = AsVariable()->AsVariableFloat();
}

template <>
inline TAutoConsoleVariable<FString>::TAutoConsoleVariable(const TCHAR* Name, const FString& DefaultValue, const TCHAR* Help, uint32 Flags)
	: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleVariable(Name, DefaultValue, Help, Flags))
{
	Ref = AsVariable()->AsVariableString();
}

/**
 * Autoregistering console command
 */
class CORE_API FAutoConsoleCommand : private FAutoConsoleObject
{
public:
	/**
	 * Register a console command that takes no arguments
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command, Flags))
	{
	}

	/**
	 * Register a console command that takes arguments
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command, Flags))
	{
	}

	/**
	* Register a console command that takes arguments, a world argument and an output device
	*
	* @param	Name		The name of this command (must not be nullptr)
	* @param	Help		Help text for this command
	* @param	Command		The user function to call when this command is executed
	* @param	Flags		Optional flags bitmask
	*/
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command, Flags))
	{
	}
};


/**
 * Autoregistering console command with a world
 */
class CORE_API FAutoConsoleCommandWithWorld : private FAutoConsoleObject
{
public:
	/**
	 * Register a console command that takes a world argument
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	FAutoConsoleCommandWithWorld(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldDelegate& Command, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command, Flags))
	{
	}

	
};

/**
 * Autoregistering console command with a world and arguments
 */
class CORE_API FAutoConsoleCommandWithWorldAndArgs : private FAutoConsoleObject
{
public:	
	/**
	 * Register a console command that takes arguments and a world argument
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	FAutoConsoleCommandWithWorldAndArgs(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldAndArgsDelegate& Command, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command, Flags))
	{
	}
};

/**
 * Autoregistering console command with an output device
 */
class CORE_API FAutoConsoleCommandWithOutputDevice : private FAutoConsoleObject
{
public:
	/**
	 * Register a console command that takes an output device
	 *
	 * @param	Name		The name of this command (must not be nullptr)
	 * @param	Help		Help text for this command
	 * @param	Command		The user function to call when this command is executed
	 * @param	Flags		Optional flags bitmask
	 */
	FAutoConsoleCommandWithOutputDevice(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default)
		: FAutoConsoleObject(IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command, Flags))
	{
	}

	
};

CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogConsoleResponse, Log, All);

