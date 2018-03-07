// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/Optional.h"

namespace ScopeExitSupport
{
	/**
	 * Not meant for direct consumption : use ON_SCOPE_EXIT instead.
	 *
	 * RAII class that calls a lambda when it is destroyed.
	 */
	template <typename FuncType>
	class TScopeGuard : public FNoncopyable
	{
	public:
		// Given a lambda, constructs an RAII scope guard.
		explicit TScopeGuard(FuncType&& InFunc)
			: Func(MoveTemp(InFunc))
		{
		}

		// This constructor needs to be available for the code to compile.
		// It will be almost definitely be RVOed out (even in DEBUG).
		TScopeGuard(TScopeGuard&& Other)
			: Func(MoveTemp(Other.Func))
		{
			Other.Func.Reset();
		}

		// Causes
		~TScopeGuard()
		{
			if (Func.IsSet())
			{
				Func.GetValue()();
			}
		}

	private:
		// The lambda to be executed when this guard goes out of scope.
		TOptional<FuncType> Func;
	};

	struct FScopeGuardSyntaxSupport
	{
		template <typename FuncType>
		TScopeGuard<FuncType> operator+(FuncType&& InFunc)
		{
			return TScopeGuard<FuncType>(Forward<FuncType>(InFunc));
		}
	};
}



/**
 * Enables a lambda to be executed on scope exit.
 *
 * Example:
 *    {
 *      FileHandle* Handle = GetFileHandle();
 *      ON_SCOPE_EXIT
 *      {
 *          CloseFile(Handle);
 *      };
 *
 *      DoSomethingWithFile( Handle );
 *
 *      // File will be closed automatically no matter how the scope is exited, e.g.:
 *      // * Any return statement.
 *      // * break or continue (if the scope is a loop body).
 *      // * An exception is thrown outside the block.
 *      // * Execution reaches the end of the block.
 *    }
 */
#define ON_SCOPE_EXIT const auto ANONYMOUS_VARIABLE(ScopeGuard_) = ::ScopeExitSupport::FScopeGuardSyntaxSupport() + [&]()
