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

#ifndef PRAGMA_POP
	#define PRAGMA_POP \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_POP

#ifndef PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
	#define PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wdelete-non-virtual-dtor\"")
#endif // PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

#ifndef PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
	#define PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

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
