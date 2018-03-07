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
	 *		Unfortunately, VC++ will complain about using member functions and fields from deprecated
	 *		class/structs even for class/struct implementation e.g.:
	 *		class DEPRECATED(4.xx, "") DeprecatedClass
	 *		{
	 *		public:
	 *			DeprecatedClass() {}
	 *
	 *			float GetMyFloat()
	 *			{
	 *				return MyFloat; // This line will cause warning that deprecated field is used.
	 *			}
	 *		private:
	 *			float MyFloat;
	 *		};
	 *
	 *		To get rid of this warning, place all code not called in class implementation in non-deprecated
	 *		base class and deprecate only derived one. This may force you to change some access specifiers
	 *		from private to protected, e.g.:
	 *
	 *		class DeprecatedClass_Base_DEPRECATED
	 *		{
	 *		protected: // MyFloat is protected now, so DeprecatedClass has access to it.
	 *			float MyFloat;
	 *		};
	 *
	 *		class DEPRECATED(4.xx, "") DeprecatedClass : DeprecatedClass_Base_DEPRECATED
	 *		{
	 *		public:
	 *			DeprecatedClass() {}
	 *
	 *			float GetMyFloat()
	 *			{
	 *				return MyFloat;
	 *			}
	 *		};
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
		PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS
#endif

#ifndef THIRD_PARTY_INCLUDES_END
	#define THIRD_PARTY_INCLUDES_END \
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
#pragma clang diagnostic warning "-Wreorder"
#pragma clang diagnostic warning "-Wparentheses-equality"
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#pragma clang diagnostic ignored "-Wundefined-bool-conversion"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#pragma clang diagnostic ignored "-Wconstant-logical-operand"


// Apple LLVM 9.0 (Xcode 9.0) introduced ARKit in IOS 11 only,
#if (__clang_major__ > 8)
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#endif

// Apple LLVM 8.1.0 (Xcode 8.3) introduced -Wundefined-var-template
#if (__clang_major__ > 8) || (__clang_major__ == 8 && __clang_minor__ >= 1)
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wnullability-inferred-on-nested-type"
#pragma clang diagnostic ignored "-Wobjc-protocol-property-synthesis"
#pragma clang diagnostic ignored "-Wnullability-completeness-on-arrays"
#pragma clang diagnostic ignored "-Wnull-dereference"
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#endif

// We can pragma optimisation's on and off as of Apple LLVM 7.3.0 but not before.
#if (__clang_major__ > 7) || (__clang_major__ == 7 && __clang_minor__ >= 3)
#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL _Pragma("clang optimize off")
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL  _Pragma("clang optimize on")
#endif
