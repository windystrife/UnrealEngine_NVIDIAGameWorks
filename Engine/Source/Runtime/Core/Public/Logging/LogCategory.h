// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"

/** Base class for all log categories. **/
struct CORE_API FLogCategoryBase
{
	/**
	* Constructor, registers with the log suppression system and sets up the default values.
	* @param CategoryName, name of the category
	* @param InDefaultVerbosity, default verbosity for the category, may ignored and overrridden by many other mechanisms
	* @param InCompileTimeVerbosity, mostly used to keep the working verbosity in bounds, macros elsewhere actually do the work of stripping compiled out things.
	**/
	FLogCategoryBase(const TCHAR *CategoryName, ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity);

	/** Destructor, unregisters from the log suppression system **/
	~FLogCategoryBase();
	/** Should not generally be used directly. Tests the runtime verbosity and maybe triggers a debug break, etc. **/
	FORCEINLINE bool IsSuppressed(ELogVerbosity::Type VerbosityLevel) const
	{
		if ((VerbosityLevel & ELogVerbosity::VerbosityMask) <= Verbosity)
		{
			return false;
		}
		return true;
	}
	/** Called just after a logging statement being allow to print. Checks a few things and maybe breaks into the debugger. **/
	void PostTrigger(ELogVerbosity::Type VerbosityLevel);
	FORCEINLINE FName GetCategoryName() const
	{
		return CategoryFName;
	}

	/** Gets the working verbosity **/
	ELogVerbosity::Type GetVerbosity() const
	{
		return (ELogVerbosity::Type)Verbosity;
	}
	
	/** Sets up the working verbosity and clamps to the compile time verbosity. **/
	void SetVerbosity(ELogVerbosity::Type Verbosity);

private:
	friend class FLogSuppressionImplementation;
	friend class FLogScopedVerbosityOverride;
	/** Internal call to set up the working verbosity from the boot time default. **/
	void ResetFromDefault();
protected:

	/** Holds the current suppression state **/
	ELogVerbosity::Type Verbosity;
	/** Holds the break flag **/
	bool DebugBreakOnLog;
	/** Holds default suppression **/
	uint8 DefaultVerbosity;
	/** Holds compile time suppression **/
	ELogVerbosity::Type CompileTimeVerbosity;
	/** FName for this category **/
	FName CategoryFName;
};

/** Template for log categories that transfers the compile-time constant default and compile time verbosity to the FLogCategoryBase constructor. **/
template<uint8 InDefaultVerbosity, uint8 InCompileTimeVerbosity>
struct FLogCategory : public FLogCategoryBase
{
	static_assert((InDefaultVerbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity, "Bogus default verbosity.");
	static_assert(InCompileTimeVerbosity < ELogVerbosity::NumVerbosity, "Bogus compile time verbosity.");
	enum
	{
		CompileTimeVerbosity = InCompileTimeVerbosity
	};

	FORCEINLINE FLogCategory(const TCHAR *CategoryName)
		: FLogCategoryBase(CategoryName, ELogVerbosity::Type(InDefaultVerbosity), ELogVerbosity::Type(CompileTimeVerbosity))
	{
	}
};
