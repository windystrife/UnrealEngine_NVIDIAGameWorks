// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if HACK_HEADER_GENERATOR
	#include "Templates/IsValidVariadicFunctionArg.h"
	#include "Templates/AndOrNot.h"

	/** 
	 * FError
	 * Set of functions for error reporting 
	 **/
	struct COREUOBJECT_API FError
	{
		/**
		 * Throws a printf-formatted exception as a const TCHAR*.
		 */
		template <typename... Types>
		FUNCTION_NO_RETURN_START
		static void VARARGS Throwf(const TCHAR* Fmt, Types... Args)
		FUNCTION_NO_RETURN_END
		{
			static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FError::Throwf");

			ThrowfImpl(Fmt, Args...);
		}

	private:
		FUNCTION_NO_RETURN_START
		static void VARARGS ThrowfImpl(const TCHAR* Fmt, ...)
		FUNCTION_NO_RETURN_END;
	};
#endif


