// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#ifndef DISABLE_DEPRECATION
	#pragma clang diagnostic warning "-Wdeprecated-declarations"

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
	#define DEPRECATED(VERSION, MESSAGE) __attribute__((deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")))

	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
		_Pragma ("clang diagnostic push") \
		_Pragma ("clang diagnostic ignored \"-Wdeprecated-declarations\"")

	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
		_Pragma ("clang diagnostic pop")
#endif // DISABLE_DEPRECATION

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
	#define PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

#ifndef PRAGMA_POP
	#define PRAGMA_POP \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_POP

// Disable common CA warnings around SDK includes
#ifndef THIRD_PARTY_INCLUDES_START
	#define THIRD_PARTY_INCLUDES_START \
		PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
		PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS \
		PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
#endif

#ifndef THIRD_PARTY_INCLUDES_END
	#define THIRD_PARTY_INCLUDES_END \
		PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
		PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
		PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
#endif

#ifndef EMIT_CUSTOM_WARNING_AT_LINE
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
		_Pragma(PREPROCESSOR_TO_STRING(message(WARNING_LOCATION(Line) Warning)))
#endif // EMIT_CUSTOM_WARNING_AT_LINE

// Make certain warnings always be warnings, even despite -Werror.
// Rationale: we don't want to suppress those as there are plans to address them (e.g. UE-12341), but breaking builds due to these warnings is very expensive
// since they cannot be caught by all compilers that we support. They are deemed to be relatively safe to be ignored, at least until all SDKs/toolchains start supporting them.
#pragma clang diagnostic warning "-Wparentheses-equality"

#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL _Pragma("clang optimize off")
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL  _Pragma("clang optimize on")
