/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using MobileDeviceInterface;

namespace Manzana
{
    /// <summary>
    /// Structures with unknown innards.  They should only ever be used as the target of a TypedPtr, never inspected.
    /// </summary>
    public class CFString { }
    public class CFURL {   }
    public class CFDictionary {  }
    public class CFNumber {  }
    public class CFBoolean { }

	public enum CFStringBuiltInEncodings
	{
		kCFStringEncodingMacRoman = 0x00000000,
		kCFStringEncodingWindowsLatin1 = 0x00000500,
		kCFStringEncodingISOLatin1 = 0x00000201,
		kCFStringEncodingNextStepLatin = 0x00000B01,
		kCFStringEncodingASCII = 0x00000600,
		kCFStringEncodingUnicode = 0x00000100,
		kCFStringEncodingUTF8 = 0x08000100,
		kCFStringEncodingNonLossyASCII = 0x00000BFF,
		kCFStringEncodingUTF16 = 0x00000100,
		kCFStringEncodingUTF16BE = 0x10000100,
		kCFStringEncodingUTF16LE = 0x14000100,
		kCFStringEncodingUTF32 = 0x0c000100,
		kCFStringEncodingUTF32BE = 0x18000100,
		kCFStringEncodingUTF32LE = 0x1c000100
	};

	public enum CFURLPathStyle
	{
		kCFURLPOSIXPathStyle = 0,
		kCFURLHFSPathStyle = 1,
		kCFURLWindowsPathStyle = 2
	};

	public enum CFNumberType
	{
		kCFNumberSInt8Type = 1,
		kCFNumberSInt16Type = 2,
		kCFNumberSInt32Type = 3,
		kCFNumberSInt64Type = 4,
		kCFNumberFloat32Type = 5,
		kCFNumberFloat64Type = 6,
		kCFNumberCharType = 7,
		kCFNumberShortType = 8,
		kCFNumberIntType = 9,
		kCFNumberLongType = 10,
		kCFNumberLongLongType = 11,
		kCFNumberFloatType = 12,
		kCFNumberDoubleType = 13,
		kCFNumberCFIndexType = 14,
		kCFNumberNSIntegerType = 15,
		kCFNumberCGFloatType = 16,
		kCFNumberMaxType = 16
	};

    internal class RequiredWinAPI
    {
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern IntPtr LoadLibrary(string lpFileName);

        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern IntPtr GetModuleHandle(string lpModuleName);
    }

	internal interface CoreFoundationImpl
	{
		TypedPtr<CFString> __CFStringMakeConstantString(byte[] s);
		TypedPtr<CFURL> CFURLCreateWithFileSystemPath(IntPtr Allocator, TypedPtr<CFString> FilePath, CFURLPathStyle PathStyle, int isDirectory);
		Boolean CFStringGetCString(TypedPtr<CFString> theString, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding);
		TypedPtr<CFString> CFStringCreateWithBytes(IntPtr allocator, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding, Boolean isExternalRep);
		TypedPtr<CFString> CFStringCreateWithCString(IntPtr allocator, byte[] buffer);
		TypedPtr<CFString> CFURLGetString(IntPtr anURL);
		uint CFGetTypeID(IntPtr FromInstance);
		uint CFStringGetTypeID();
		uint CFNumberGetTypeID();
		uint CFBooleanGetTypeID();
		uint CFDictionaryGetTypeID();
		int CFDictionaryGetCount (TypedPtr<CFDictionary> Dict);
		IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, ref Int32 Value);
		Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out float Value);
		Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out double Value);
		Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out int Value);
		Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out uint Value);
		Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out Int64 Value);
		CFNumberType CFNumberGetType(TypedPtr<CFNumber> Number);
		void CFPreferencesSetAppValue(IntPtr Key, IntPtr Value, IntPtr ApplicationID);
		void CFDictionarySetValue(TypedPtr<CFDictionary> Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value);
		void CFDictionaryGetKeysAndValues(TypedPtr<CFDictionary> Dict, /*const*/ IntPtr[] Keys, /*const*/ IntPtr[] Values);
		TypedPtr<CFDictionary> CFDictionaryCreateMutable(
			/* CFAllocatorRef */ IntPtr allocator,
			/* CFIndex */ int capacity,
			/* const CFDictionaryKeyCallBacks* */ IntPtr keyCallBacks,
			/* const CFDictionaryValueCallBacks* */ IntPtr valueCallBacks
		);
		Boolean CFStringGetFileSystemRepresentation(TypedPtr<CFString> theString, byte[] buffer, int bufferSize);
	}

	internal class CoreFoundationWin : CoreFoundationImpl
	{

		public TypedPtr<CFString> __CFStringMakeConstantString(byte[] s)
		{
			return CoreFoundation.__CFStringMakeConstantString(s);
		}

		public TypedPtr<CFString> CFStringCreateWithCString(IntPtr allocator, byte[] buffer)
		{
			return CoreFoundation.CFStringCreateWithCString(allocator, buffer, CFStringBuiltInEncodings.kCFStringEncodingUTF8);
		}

		public TypedPtr<CFString> CFStringCreateWithBytes(IntPtr allocator, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding, Boolean isExternalRep)
		{
			return CoreFoundation.CFStringCreateWithBytes(allocator, buffer, bufferSize, encoding, isExternalRep);
		}

		public TypedPtr<CFURL> CFURLCreateWithFileSystemPath(IntPtr Allocator, TypedPtr<CFString> FilePath, CFURLPathStyle PathStyle, int isDirectory)
		{
			return CoreFoundation.CFURLCreateWithFileSystemPath(Allocator, (IntPtr)FilePath, PathStyle, isDirectory);
		}

		public Boolean CFStringGetCString(TypedPtr<CFString> theString, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding)
		{
			return CoreFoundation.CFStringGetCString((IntPtr)theString, buffer, bufferSize, encoding);
		}

		public TypedPtr<CFString> CFURLGetString(IntPtr anURL)
		{
			return CoreFoundation.CFURLGetString(anURL);
		}

		public uint CFGetTypeID(IntPtr FromInstance)
		{
			return CoreFoundation.CFGetTypeID(FromInstance);
		}

		public uint CFStringGetTypeID()
		{
			return CoreFoundation.CFStringGetTypeID();
		}

		public uint CFNumberGetTypeID()
		{
			return CoreFoundation.CFNumberGetTypeID();
		}

		public uint CFBooleanGetTypeID()
		{
			return CoreFoundation.CFBooleanGetTypeID();
		}

		public uint CFDictionaryGetTypeID()
		{
			return CoreFoundation.CFDictionaryGetTypeID();
		}

		public int CFDictionaryGetCount(TypedPtr<CFDictionary> Dict)
		{
			return CoreFoundation.CFDictionaryGetCount ((IntPtr)Dict);
		}

		public IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, ref Int32 Value)
		{
			return CoreFoundation.CFNumberCreate(allocator, theType, ref Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out float Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out double Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out int Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out uint Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out Int64 Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public CFNumberType CFNumberGetType(TypedPtr<CFNumber> Number)
		{
			return CoreFoundation.CFNumberGetType((IntPtr)Number);
		}

		public void CFPreferencesSetAppValue(IntPtr Key, IntPtr Value, IntPtr ApplicationID)
		{
			CoreFoundation.CFPreferencesSetAppValue(Key, Value, ApplicationID);
		}

		public void CFDictionarySetValue(TypedPtr<CFDictionary> Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value)
		{
			CoreFoundation.CFDictionarySetValue((IntPtr)Dict, Key, Value);
		}

		public void CFDictionaryGetKeysAndValues(TypedPtr<CFDictionary> Dict, /*const*/ IntPtr[] Keys, /*const*/ IntPtr[] Values)
		{
			CoreFoundation.CFDictionaryGetKeysAndValues((IntPtr)Dict, Keys, Values);
		}

		public TypedPtr<CFDictionary> CFDictionaryCreateMutable(
			/* CFAllocatorRef */ IntPtr allocator,
			/* CFIndex */ int capacity,
			/* const CFDictionaryKeyCallBacks* */ IntPtr keyCallBacks,
			/* const CFDictionaryValueCallBacks* */ IntPtr valueCallBacks
		)
		{
			return CoreFoundation.CFDictionaryCreateMutable(allocator, capacity, keyCallBacks, valueCallBacks);
		}

		public Boolean CFStringGetFileSystemRepresentation(TypedPtr<CFString> theString, byte[] buffer, int bufferSize)
		{
			return CoreFoundation.CFStringGetFileSystemRepresentation((IntPtr)theString, buffer, bufferSize);
		}

		private class CoreFoundation
		{
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr __CFStringMakeConstantString(byte[] s);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr CFStringCreateWithCString(IntPtr allocator, byte[] buffer, CFStringBuiltInEncodings encoding);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr CFStringCreateWithBytes(IntPtr allocator, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding, Boolean isExtRep);
			
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr/*CFURL*/ CFURLCreateWithFileSystemPath(IntPtr Allocator, IntPtr/*CFString*/ FilePath, CFURLPathStyle PathStyle, int isDirectory);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr/*CFString*/ CFURLGetString(IntPtr anURL);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFGetTypeID(IntPtr FromInstance);
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFStringGetTypeID();
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFNumberGetTypeID();
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFBooleanGetTypeID();
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFDictionaryGetTypeID();

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFDictionaryAddValue(IntPtr/*CFDictionary*/ Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFDictionarySetValue(IntPtr/*CFDictionary*/ Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFDictionaryGetKeysAndValues(IntPtr/*CFDictionary*/ Dict, /*const*/ IntPtr[] Keys, /*const*/ IntPtr[] Values);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr/*CFDictionary*/ CFDictionaryCreateMutable(
				/* CFAllocatorRef */ IntPtr allocator,
				/* CFIndex */ int capacity,
				/* const CFDictionaryKeyCallBacks* */ IntPtr keyCallBacks,
				/* const CFDictionaryValueCallBacks* */ IntPtr valueCallBacks
			);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern int CFDictionaryGetCount(IntPtr/*CFDictionary*/ Dict);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFStringGetCString(IntPtr/*CFString*/ theString, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding);

			//[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			//public extern static IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, IntPtr valuePtr);

			//@TODO: make this more general
			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public extern static IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, ref Int32 Value);

			[DllImport("CoreFoundation.dll", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out float Value);

			[DllImport("CoreFoundation.dll", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out double Value);

			[DllImport("CoreFoundation.dll", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out int Value);

			[DllImport("CoreFoundation.dll", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out uint Value);

			[DllImport("CoreFoundation.dll", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out Int64 Value);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public extern static CFNumberType CFNumberGetType(IntPtr/*CFNumber*/ Number);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFPreferencesSetAppValue(IntPtr Key, IntPtr Value, IntPtr ApplicationID);

			[DllImport("CoreFoundation.dll", CallingConvention = CallingConvention.Cdecl)]
			public static extern Boolean CFStringGetFileSystemRepresentation(IntPtr theString, byte[] buffer, int bufferSize);
		}
	}

	internal class CoreFoundationOSX : CoreFoundationImpl
	{

		public TypedPtr<CFString> __CFStringMakeConstantString(byte[] s)
		{
			return CoreFoundation.__CFStringMakeConstantString(s);
		}

		public TypedPtr<CFString> CFStringCreateWithCString(IntPtr allocator, byte[] buffer)
		{
			return CoreFoundation.CFStringCreateWithCString(allocator, buffer, CFStringBuiltInEncodings.kCFStringEncodingUTF8);
		}

		public TypedPtr<CFString> CFStringCreateWithBytes(IntPtr allocator, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding, Boolean isExternalRep)
		{
			return CoreFoundation.CFStringCreateWithBytes(allocator, buffer, bufferSize, encoding, isExternalRep);
		}

		public TypedPtr<CFURL> CFURLCreateWithFileSystemPath(IntPtr Allocator, TypedPtr<CFString> FilePath, CFURLPathStyle PathStyle, int isDirectory)
		{
			return CoreFoundation.CFURLCreateWithFileSystemPath(Allocator, (IntPtr)FilePath, PathStyle, isDirectory);
		}

		public Boolean CFStringGetCString(TypedPtr<CFString> theString, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding)
		{
			return CoreFoundation.CFStringGetCString((IntPtr)theString, buffer, bufferSize, encoding);
		}

		public TypedPtr<CFString> CFURLGetString(IntPtr anURL)
		{
			return CoreFoundation.CFURLGetString(anURL);
		}

		public uint CFGetTypeID(IntPtr FromInstance)
		{
			return CoreFoundation.CFGetTypeID(FromInstance);
		}

		public uint CFStringGetTypeID()
		{
			return CoreFoundation.CFStringGetTypeID();
		}

		public uint CFNumberGetTypeID()
		{
			return CoreFoundation.CFNumberGetTypeID();
		}

		public uint CFBooleanGetTypeID()
		{
			return CoreFoundation.CFBooleanGetTypeID();
		}

		public uint CFDictionaryGetTypeID()
		{
			return CoreFoundation.CFDictionaryGetTypeID();
		}

		public int CFDictionaryGetCount(TypedPtr<CFDictionary> Dict)
		{
			return CoreFoundation.CFDictionaryGetCount ((IntPtr)Dict);
		}

		public IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, ref Int32 Value)
		{
			return CoreFoundation.CFNumberCreate(allocator, theType, ref Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out float Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out double Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out int Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out uint Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public Boolean CFNumberGetValue(TypedPtr<CFNumber> Number, CFNumberType TheType, out Int64 Value)
		{
			return CoreFoundation.CFNumberGetValue((IntPtr)Number, TheType, out Value);
		}

		public CFNumberType CFNumberGetType(TypedPtr<CFNumber> Number)
		{
			return CoreFoundation.CFNumberGetType((IntPtr)Number);
		}

		public void CFPreferencesSetAppValue(IntPtr Key, IntPtr Value, IntPtr ApplicationID)
		{
			CoreFoundation.CFPreferencesSetAppValue(Key, Value, ApplicationID);
		}

		public void CFDictionarySetValue(TypedPtr<CFDictionary> Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value)
		{
			CoreFoundation.CFDictionarySetValue((IntPtr)Dict, Key, Value);
		}

		public void CFDictionaryGetKeysAndValues(TypedPtr<CFDictionary> Dict, /*const*/ IntPtr[] Keys, /*const*/ IntPtr[] Values)
		{
			CoreFoundation.CFDictionaryGetKeysAndValues((IntPtr)Dict, Keys, Values);
		}

		public TypedPtr<CFDictionary> CFDictionaryCreateMutable(
			/* CFAllocatorRef */ IntPtr allocator,
			/* CFIndex */ int capacity,
			/* const CFDictionaryKeyCallBacks* */ IntPtr keyCallBacks,
			/* const CFDictionaryValueCallBacks* */ IntPtr valueCallBacks
		)
		{
			return CoreFoundation.CFDictionaryCreateMutable(allocator, capacity, keyCallBacks, valueCallBacks);
		}

		public Boolean CFStringGetFileSystemRepresentation(TypedPtr<CFString> theString, byte[] buffer, int bufferSize)
		{
			return CoreFoundation.CFStringGetFileSystemRepresentation((IntPtr)theString, buffer, bufferSize);
		}

		private class CoreFoundation
		{
			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr __CFStringMakeConstantString(byte[] s);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr CFStringCreateWithCString(IntPtr allocator, byte[] buffer, CFStringBuiltInEncodings encoding);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr CFStringCreateWithBytes(IntPtr allocator, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding, Boolean isExtRep);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr/*CFURL*/ CFURLCreateWithFileSystemPath(IntPtr Allocator, IntPtr/*CFString*/ FilePath, CFURLPathStyle PathStyle, int isDirectory);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr/*CFString*/ CFURLGetString(IntPtr anURL);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFGetTypeID(IntPtr FromInstance);
			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFStringGetTypeID();
			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFNumberGetTypeID();
			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFBooleanGetTypeID();
			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern uint CFDictionaryGetTypeID();

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFDictionaryAddValue(IntPtr/*CFDictionary*/ Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFDictionarySetValue(IntPtr/*CFDictionary*/ Dict, /*const*/ IntPtr Key, /*const*/ IntPtr Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFDictionaryGetKeysAndValues(IntPtr/*CFDictionary*/ Dict, /*const*/ IntPtr[] Keys, /*const*/ IntPtr[] Values);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern IntPtr/*CFDictionary*/ CFDictionaryCreateMutable(
				/* CFAllocatorRef */ IntPtr allocator,
				/* CFIndex */ int capacity,
				/* const CFDictionaryKeyCallBacks* */ IntPtr keyCallBacks,
				/* const CFDictionaryValueCallBacks* */ IntPtr valueCallBacks
			);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern int CFDictionaryGetCount(IntPtr/*CFDictionary*/ Dict);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFStringGetCString(IntPtr/*CFString*/ theString, byte[] buffer, int bufferSize, CFStringBuiltInEncodings encoding);

			//[DllImport("CoreFoundation.dylib", CallingConvention = CallingConvention.Cdecl)]
			//public extern static IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, IntPtr valuePtr);

			//@TODO: make this more general
			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public extern static IntPtr CFNumberCreate(IntPtr allocator, CFNumberType theType, ref Int32 Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out float Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out double Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out int Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out uint Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", EntryPoint = "CFNumberGetValue", CallingConvention = CallingConvention.Cdecl)]
			public extern static Boolean CFNumberGetValue(IntPtr/*CFNumber*/ Number, CFNumberType TheType, out Int64 Value);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public extern static CFNumberType CFNumberGetType(IntPtr/*CFNumber*/ Number);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern void CFPreferencesSetAppValue(IntPtr Key, IntPtr Value, IntPtr ApplicationID);

			[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", CallingConvention = CallingConvention.Cdecl)]
			public static extern Boolean CFStringGetFileSystemRepresentation(IntPtr theString, byte[] buffer, int bufferSize);
		}
	}

    /// <summary>
    /// Declarations for various methods in Apple's CoreFoundation.dll, needed for interop with the MobileDevice DLL
    /// </summary>
    internal partial class MobileDevice
    {
        public static IntPtr kCFAllocatorDefault;
        public static IntPtr kCFTypeDictionaryKeyCallBacks;
        public static IntPtr kCFTypeDictionaryValueCallBacks;

        public static TypedPtr<CFBoolean> kCFBooleanTrue;
        public static TypedPtr<CFBoolean> kCFBooleanFalse;

		public static CoreFoundationImpl CoreImpl = null;

        static void InitializeCoreFoundation()
        {
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				CoreImpl = new CoreFoundationOSX();
			}
			else
			{
				CoreImpl = new CoreFoundationWin();

				IntPtr CoreFoundation = RequiredWinAPI.GetModuleHandle("CoreFoundation.dll");

				kCFAllocatorDefault = RequiredWinAPI.GetProcAddress(CoreFoundation, "kCFAllocatorDefault");
				kCFTypeDictionaryKeyCallBacks = RequiredWinAPI.GetProcAddress(CoreFoundation, "kCFTypeDictionaryKeyCallBacks");
				kCFTypeDictionaryValueCallBacks = RequiredWinAPI.GetProcAddress(CoreFoundation, "kCFTypeDictionaryValueCallBacks");

				kCFBooleanTrue = RequiredWinAPI.GetProcAddress(CoreFoundation, "kCFBooleanTrue");
				kCFBooleanFalse = RequiredWinAPI.GetProcAddress(CoreFoundation, "kCFBooleanFalse");
			}
        }

        private static byte[] StringToCString(string value)
        {
            byte[] bytes = new byte[value.Length + 1];
            Encoding.ASCII.GetBytes(value, 0, value.Length, bytes, 0);
            return bytes;
        }

		private static byte[] StringToUTF16String(string value)
		{
			return Encoding.Unicode.GetBytes(value);
		}

		private static byte[] StringToUTF32LEString(string value)
		{
			return Encoding.UTF32.GetBytes(value);
		}

        public static TypedPtr<CFString> CFStringMakeConstantString(string s)
        {
            return CoreImpl.__CFStringMakeConstantString(StringToCString(s));
        }

        public static TypedPtr<CFURL> CFURLCreateWithFileSystemPath(IntPtr Allocator, TypedPtr<CFString> FilePath, CFURLPathStyle PathStyle, int isDirectory)
        {
            return (TypedPtr<CFURL>)(CoreImpl.CFURLCreateWithFileSystemPath(Allocator, FilePath.Handle, PathStyle, isDirectory));
        }

        public static string CFStringGetCString(TypedPtr<CFString> InString)
        {
            byte[] bytes = new byte[2048];
            CoreImpl.CFStringGetCString(InString.Handle, bytes, 2048, CFStringBuiltInEncodings.kCFStringEncodingUTF8);

            int ValidLength = 0;
            foreach (byte b in bytes)
            {
                if (b == 0)
                {
                    break;
                }
                else
                {
                    ValidLength++;
                }
            }

            return Encoding.UTF8.GetString(bytes, 0, ValidLength);
        }

		public static byte[] StringToFileSystemRepresentation(string InString)
		{
			byte[] BString =  StringToUTF32LEString(InString);
			TypedPtr<CFString> cfString = CoreImpl.CFStringCreateWithBytes(kCFAllocatorDefault, BString, BString.Length, CFStringBuiltInEncodings.kCFStringEncodingUTF32LE, false);
			return CFStringGetFileSystemRepresentation(cfString);
		}

		public static byte[] CFStringGetFileSystemRepresentation(TypedPtr<CFString> InString)
		{
			byte[] bytes = new byte[2048];
			CoreImpl.CFStringGetFileSystemRepresentation(InString.Handle, bytes, 2048);

			int ValidLength = 0;
			foreach (byte b in bytes)
			{
				if (b == 0)
				{
					break;
				}
				else
				{
					ValidLength++;
				}
			}

//			return Encoding.UTF8.GetString(bytes, 0, ValidLength);
			return bytes;
		}

		public static string GetStringForUrl(TypedPtr<CFURL> Url)
        {
			TypedPtr<CFString> cfString = CoreImpl.CFURLGetString((IntPtr)Url);
            return CFStringGetCString((IntPtr)cfString);
        }

        public static TypedPtr<CFURL> CreateFileUrlHelper(string InPath, bool bIsDirectory)
        {
            return CFURLCreateWithFileSystemPath(MobileDevice.kCFAllocatorDefault, CFStringMakeConstantString(InPath), CFURLPathStyle.kCFURLWindowsPathStyle, bIsDirectory ? 1 : 0);
        }

        public static TypedPtr<CFNumber> CFNumberCreate(Int32 Value)
        {
			return CoreImpl.CFNumberCreate(kCFAllocatorDefault, CFNumberType.kCFNumberSInt32Type, ref Value);
        }

        public static TypedPtr<CFDictionary> CreateCFDictionaryHelper()
        {
			return CoreImpl.CFDictionaryCreateMutable(kCFAllocatorDefault, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
        }

        /// <summary>
        /// Converts a CF type to a managed type (e.g., CFString to String)
        /// </summary>
        /// <param name="Source"></param>
        /// <returns></returns>
        public static object ConvertArbitraryCFType(IntPtr Source)
        {
            if (Source == IntPtr.Zero)
            {
                return null;
            }

            // Convert based on the type of the source object
			uint TypeID = CoreImpl.CFGetTypeID(Source);

			if (TypeID == CoreImpl.CFNumberGetTypeID())
            {
                return ConvertCFNumber(Source);
            }
			else if (TypeID == CoreImpl.CFStringGetTypeID())
            {
                return CFStringGetCString(Source);
            }
			else if (TypeID == CoreImpl.CFDictionaryGetTypeID())
            {
                return ConvertCFDictionaryToDictionary(Source);
            }
			else if (TypeID == CoreImpl.CFBooleanGetTypeID())
            {
                return (Source == kCFBooleanTrue.Handle);
            }

            return null;
        }

        /// <summary>
        /// Converts an arbitrary CFNumber to a double
        /// </summary>
        public static double ConvertCFNumber(TypedPtr<CFNumber> Number)
        {
			CFNumberType TypeID = CoreImpl.CFNumberGetType((IntPtr)Number);

            switch (TypeID)
            {
                case CFNumberType.kCFNumberFloat32Type:
                case CFNumberType.kCFNumberFloatType:
                    {
                        float Result;
						CoreImpl.CFNumberGetValue((IntPtr)Number, CFNumberType.kCFNumberFloat32Type, out Result);
                        return Result;
                    }

                case CFNumberType.kCFNumberFloat64Type:
                case CFNumberType.kCFNumberDoubleType:
                case CFNumberType.kCFNumberCGFloatType:
                    {
                        double Result;
						CoreImpl.CFNumberGetValue((IntPtr)Number, CFNumberType.kCFNumberFloat64Type, out Result);
                        return Result;
                    }

                case CFNumberType.kCFNumberSInt8Type:
                case CFNumberType.kCFNumberSInt16Type:
                case CFNumberType.kCFNumberSInt32Type:
                case CFNumberType.kCFNumberCharType:
                case CFNumberType.kCFNumberShortType:
                case CFNumberType.kCFNumberIntType:
                case CFNumberType.kCFNumberLongType:
                case CFNumberType.kCFNumberCFIndexType:
                    {
                        int Result;
						CoreImpl.CFNumberGetValue((IntPtr)Number, CFNumberType.kCFNumberIntType, out Result);
                        return Result;
                    }

                case CFNumberType.kCFNumberSInt64Type:
                case CFNumberType.kCFNumberLongLongType:
                case CFNumberType.kCFNumberNSIntegerType:
                    {
                        Int64 Result;
						CoreImpl.CFNumberGetValue((IntPtr)Number, CFNumberType.kCFNumberSInt64Type, out Result);
                        return Result;
                    }

                default:
                    return 0.0;
            }
        }

        /// <summary>
        /// Converts a CFDictionary into a Dictionary (keys and values are object)
        /// </summary>
        public static Dictionary<object, object> ConvertCFDictionaryToDictionary(TypedPtr<CFDictionary> Dict)
        {
            // Get the raw key-value pairs
            int NumPairs = CFDictionaryGetCount(Dict);

            IntPtr [] Keys = new IntPtr[NumPairs];
            IntPtr [] Values = new IntPtr[NumPairs];

			CoreImpl.CFDictionaryGetKeysAndValues((IntPtr)Dict, Keys, Values);

            // Convert each key and value to a managed type
            Dictionary<object, object> Result = new Dictionary<object, object>();
            for (int i = 0; i < NumPairs; ++i)
            {
                try
                {
                    Result.Add(ConvertArbitraryCFType(Keys[i]), ConvertArbitraryCFType(Values[i]));
                }
                catch (Exception)
                {
                    Console.WriteLine("Unable to properly convert dictionary");
                }
            }

            return Result;
        }

        public static Dictionary<string, object> ConvertCFDictionaryToDictionaryString(TypedPtr<CFDictionary> Dict)
        {
            Dictionary<string, object> Result = new Dictionary<string,object>();
            Dictionary<object, object> Temp = ConvertCFDictionaryToDictionary(Dict);

            foreach (KeyValuePair<object, object> KVP in Temp)
            {
                string Key = (string)KVP.Key;
                Result.Add(Key, KVP.Value);
            }

            return Result;
        }

        public static int CFDictionaryGetCount(TypedPtr<CFDictionary> Dict)
        {
			return CoreImpl.CFDictionaryGetCount(Dict);
        }

        public static void CFDictionaryAddHelper(TypedPtr<CFDictionary> InDict, string Key, string Value)
        {
			CoreImpl.CFDictionarySetValue((IntPtr)InDict, (IntPtr)CFStringMakeConstantString(Key), (IntPtr)CFStringMakeConstantString(Value));
        }
    }
}