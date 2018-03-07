// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#ifndef DISABLE_DEPRECATION
	/**
	 * Macro for marking up deprecated code, functions and types.
	 *
	 * Features that are marked as deprecated are scheduled to be removed from the code base
	 * in a future release. If you are using a deprecated feature in your code, you should
	 * replace it before upgrading to the next release. See the Upgrade Notes in the release
	 * notes for the release in which the feature was marked deprecated.
	 *
	 * Sample usage (note the slightly different syntax for classes and structures):
	 *
	 *		DEPRECATED(4.xx, "Message")
	 *		void Function();
	 *
	 *		struct DEPRECATED(4.xx, "Message") MODULE_API MyStruct
	 *		{
	 *			// StructImplementation
	 *		};
	 *		class DEPRECATED(4.xx, "Message") MODULE_API MyClass
	 *		{
	 *			// ClassImplementation
	 *		};
	 *
	 * @param VERSION The release number in which the feature was marked deprecated.
	 * @param MESSAGE A message containing upgrade notes.
	 */
	#if defined(__clang__)
		#define DEPRECATED(VERSION, MESSAGE) __attribute__((deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")))

		#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
				_Pragma ("clang diagnostic push") \
				_Pragma ("clang diagnostic ignored \"-Wdeprecated-declarations\"")

		#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
				_Pragma ("clang diagnostic pop")
	#else
		#define DEPRECATED(VERSION, MESSAGE) __declspec(deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile."))

		#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
			__pragma (warning(push)) \
			__pragma (warning(disable:4995)) \
			__pragma (warning(disable:4996))

		#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
			__pragma (warning(pop))
	#endif

#endif // DISABLE_DEPRECATION

#if defined(__clang__)
	// Clang compiler on Windows
	#ifndef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
				_Pragma ("clang diagnostic push") \
				_Pragma ("clang diagnostic ignored \"-Wshadow\"")
	#endif // PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
			_Pragma("clang diagnostic pop")
	#endif // PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS
		#define PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			_Pragma("clang diagnostic push") \
			_Pragma("clang diagnostic ignored \"-Wundef\"")
	#endif // PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS

	#ifndef PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS
		#define PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			_Pragma("clang diagnostic pop")
	#endif // PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS

	#ifndef PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
		#define PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
			_Pragma("clang diagnostic push") \
			_Pragma("clang diagnostic ignored \"-Wdelete-non-virtual-dtor\"")
	#endif // PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

	#ifndef PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
		#define PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			_Pragma("clang diagnostic pop")
	#endif // PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS

	#ifndef PRAGMA_POP
		#define PRAGMA_POP \
			_Pragma("clang diagnostic pop")
	#endif // PRAGMA_POP

	// Disable common CA warnings around SDK includes
	#ifndef THIRD_PARTY_INCLUDES_START
		#define THIRD_PARTY_INCLUDES_START \
			PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
			PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS
	#endif

	#ifndef THIRD_PARTY_INCLUDES_END
		#define THIRD_PARTY_INCLUDES_END \
			PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
	#endif
#else
	// VC++
	// 4456 - declaration of 'LocalVariable' hides previous local declaration
	// 4457 - declaration of 'LocalVariable' hides function parameter
	// 4458 - declaration of 'LocalVariable' hides class member
	// 4459 - declaration of 'LocalVariable' hides global declaration
	// 6244 - local declaration of <variable> hides previous declaration at <line> of <file>
	#ifndef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
			__pragma (warning(push)) \
			__pragma (warning(disable:4456)) \
			__pragma (warning(disable:4457)) \
			__pragma (warning(disable:4458)) \
			__pragma (warning(disable:4459)) \
			__pragma (warning(disable:6244))
	#endif // PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
			__pragma(warning(pop))
	#endif // PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS
		#define PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			__pragma(warning(push)) \
			__pragma(warning(disable: 4668))  /* 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives' */
	#endif // PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS

	#ifndef PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS
		#define PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			__pragma(warning(pop))
	#endif // PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS

	#ifndef PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
		#define PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
			__pragma(warning(push)) \
			__pragma(warning(disable:4265))
	#endif // PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

	#ifndef PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
		#define PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
			__pragma(warning(pop))
	#endif // PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

	#ifndef PRAGMA_POP
		#define PRAGMA_POP \
			__pragma(warning(pop))
	#endif // PRAGMA_POP

	// Disable common CA warnings around SDK includes
	#ifndef THIRD_PARTY_INCLUDES_START
		#define THIRD_PARTY_INCLUDES_START \
			__pragma(warning(push)) \
			__pragma(warning(disable: 4510))  /* '<class>': default constructor could not be generated. */ \
			__pragma(warning(disable: 4610))  /* object '<class>' can never be instantiated - user-defined constructor required. */ \
			__pragma(warning(disable: 4946))  /* reinterpret_cast used between related classes: '<class1>' and '<class1>' */ \
			__pragma(warning(disable: 4996))  /* '<obj>' was declared deprecated. */ \
			__pragma(warning(disable: 6011))  /* Dereferencing NULL pointer '<ptr>'. */ \
			__pragma(warning(disable: 6101))  /* Returning uninitialized memory '<expr>'.  A successful path through the function does not set the named _Out_ parameter. */ \
			__pragma(warning(disable: 6287))  /* Redundant code:  the left and right sub-expressions are identical. */ \
			__pragma(warning(disable: 6308))  /* 'realloc' might return null pointer: assigning null pointer to 'X', which is passed as an argument to 'realloc', will cause the original memory block to be leaked. */ \
			__pragma(warning(disable: 6326))  /* Potential comparison of a constant with another constant. */ \
			__pragma(warning(disable: 6340))  /* Mismatch on sign: Incorrect type passed as parameter in call to function. */ \
			__pragma(warning(disable: 6385))  /* Reading invalid data from '<ptr>':  the readable size is '<num1>' bytes, but '<num2>' bytes may be read. */ \
			__pragma(warning(disable: 6386))  /* Buffer overrun while writing to '<ptr>':  the writable size is '<num1>' bytes, but '<num2>' bytes might be written. */ \
			__pragma(warning(disable: 28182)) /* Dereferencing NULL pointer. '<ptr1>' contains the same NULL value as '<ptr2>' did. */ \
			__pragma(warning(disable: 28251)) /* Inconsistent annotation for '<func>': this instance has no annotations. */ \
			__pragma(warning(disable: 28252)) /* Inconsistent annotation for '<func>': return/function has '<annotation>' on the prior instance. */ \
			__pragma(warning(disable: 28253)) /* Inconsistent annotation for '<func>': _Param_(<num>) has '<annotation>' on the prior instance. */ \
			PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
			PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
	#endif

	#ifndef THIRD_PARTY_INCLUDES_END
		#define THIRD_PARTY_INCLUDES_END \
			PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
			PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
			PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
			__pragma(warning(pop))
	#endif
#endif

#if defined(__clang__)
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
		_Pragma(PREPROCESSOR_TO_STRING(message(WARNING_LOCATION(Line) Warning)))
#else
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
		__pragma(message(WARNING_LOCATION(Line) ": warning C4996: " Warning))
#endif

#if defined(__clang__)
	// Make certain warnings always be warnings, even despite -Werror.
	// Rationale: we don't want to suppress those as there are plans to address them (e.g. UE-12341), but breaking builds due to these warnings is very expensive
	// since they cannot be caught by all compilers that we support. They are deemed to be relatively safe to be ignored, at least until all SDKs/toolchains start supporting them.
	#pragma clang diagnostic warning "-Wreorder"
#endif

