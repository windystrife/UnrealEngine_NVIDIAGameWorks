// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Map.h"
#include "NameTypes.h"

/*
	A helper object for counting instances of struct or classes for debugging
	purposes, e.g. to get an absolute count of the number of "Foo"'s in existence

	Suggested use

	class UFoo
	{
	public:
		// etc etc

	protected:
		COUNT_INSTANCES(FTypeIWantToCount)
	};

*/
class CORE_API FInstanceCountingObject
{
public:

	/**
	 *	Constructor, though geneerally these objects should be created using the COUNT_INSTANCES_ 
	 *	macro.
	 */
	FInstanceCountingObject(const TCHAR* InName, bool InLogConstruction=false);

	/**
	 *	Copy-constructor for assigment
	 */
	FInstanceCountingObject(const FInstanceCountingObject& RHS);

	/**	Destructor */
	virtual ~FInstanceCountingObject();

	/**
	 * Returns the count of instances with "Name". 
	 *
	 * @param: 	Name		
	 * @return: int32		
	*/static int32 GetInstanceCount(const TCHAR* Name);
	
	/**
	 * Dumps stats for all counted instances to the provided output device. This
	 * is bound to the "LogCountedInstances" console command
	 *
	 * @param: 	OutputDevice		
	 * @return: void		
	*/static void LogCounts(FOutputDevice& OutputDevice);
	
protected:

	/**	Increments stats for objects of this type */
	void IncrementStats();

	/**	Decrements stats for objects of this type */
	void DecrementStats();

protected:
	
	/**	Name we are tracking */
	FName		Name;

	// Log increment/decrement?
	bool		 DoLog;

	/**
	 *	Vars used by our singleton
	 */
	struct CORE_API FGlobalVars
	{
		TMap<FName, int32>	InstanceCounts;
		FCriticalSection	Mutex;
	};

	/**
	 *	Vars are stored as a pointer and initialized on demand due to avoid dependencies
	 *	on global crot order
	 */
	static FGlobalVars* Globals;
	static FGlobalVars& GetGlobals();
};


#if !UE_BUILD_SHIPPING

	/**
	 *	Add to class body of TypeName to track counts of "TypeName". Counts can be accessed via code or console
	 */
	#define COUNT_INSTANCES(TypeName) \
		struct FCount##TypeName : FInstanceCountingObject { FCount##TypeName() : FInstanceCountingObject(TEXT(PREPROCESSOR_TO_STRING(TypeName))) {} }; FCount##TypeName TypeName##Count; 

	 /**
	 *	Identical to COUNT_INSTANCES, but construction/destruction will be written to the log along with the address of this object (us, not the instance that contains us)
	 */
	#define COUNT_INSTANCES_AND_LOG(TypeName) \
		struct FCount##TypeName : FInstanceCountingObject { FCount##TypeName() : FInstanceCountingObject(TEXT(PREPROCESSOR_TO_STRING(TypeName)), true) {} }; FCount##TypeName TypeName##Count;


#else

	#define COUNT_INSTANCES(TypeName)
	#define COUNT_INSTANCES_AND_LOG(TypeName)

#endif // SHIPPING
