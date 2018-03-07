// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#ifndef DEPRECATED
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
	 * @param MESSAGE A message text containing additional upgrade notes.
	 */
	#define DEPRECATED(VERSION, MESSAGE)
#endif // DEPRECATED

#ifndef PRAGMA_DISABLE_DEPRECATION_WARNINGS
	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif

#ifndef PRAGMA_ENABLE_DEPRECATION_WARNINGS
	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

#ifndef EMIT_CUSTOM_WARNING_AT_LINE
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning)
#endif // EMIT_CUSTOM_WARNING_AT_LINE

#ifndef EMIT_CUSTOM_WARNING
	#define EMIT_CUSTOM_WARNING(Warning) \
		EMIT_CUSTOM_WARNING_AT_LINE(__LINE__, Warning)
#endif // EMIT_CUSTOM_WARNING

#ifndef DEPRECATED_MACRO
	#define DEPRECATED_MACRO(Version, Message) EMIT_CUSTOM_WARNING(Message " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")
#endif
