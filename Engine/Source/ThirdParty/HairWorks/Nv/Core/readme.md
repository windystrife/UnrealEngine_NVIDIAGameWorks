NvCore
======

This is a very small C/C++ header based library designed to provide the most basic abstraction of compiler, platform, and compiler/platform detection. 

There are 4 files

* NvDefines.h
	* Contains compiler and platform abstraction.
* NvTypes.h  
	* Contains definitions for basic types
* NvAssert.h
	* Provides simple assert support
* NvResult.h
	* Provides support for 'Result' style return value - compatible with and broadly similar to HRESULT

It is designed to be backwardly compatible and a superset of NvFoundation/1.1s NvPreprocessor.h include file. 
	
This document is written in 'markdown' format and can be read in a normal text editor, but may be easier to read in with a nice 'reader'. Some examples

* Markdown plugin for firefox https://addons.mozilla.org/en-US/firefox/addon/markdown-viewer/
	
Releases
--------

* 1.0 - Original Release
	
Integration
-----------

It is recommended that you branch the Nv/Core path into your project. Within your project NvCore headers should be included via their full include path. Ie

```
#include <Nv/Core/1.0/NvTypes.h>
```

Note that path also includes the version number. Doing so makes it clear within the source exactly which version of the files is needed. It is intended that these files change rarely, and if they do change they do not break compatibility. There will only be a version change if there are substantial changes, or the files break backwards compatibility.

NvDefines.h
-----------

Include into your project using 

```
#include <Nv/Core/1.0/NvDefines.h>
```

Note that the majority of the macros defined are switches that can be true of false. All such macros are defined to be 0 as false and != 0 as true. 

```

// This is right
#if NV_VC
// Do something visual studio related
#endif 

// This is ALWAYS WRONG!
#if defined(NV_VC)
#endif
// Equally this is WRONG!
#ifdef NV_VC
#enid
```

As NV_VC is always defined for all platforms - it's it's _value_ that is important and that should be tested. There are several benefits of this behaviour - firstly it generally allows an IDE name completion to be able to help you. Secondly it means you can use normal boolean logic

```
#if NV_VC && NV_PTR_IS_64
// It's Visual Studio and we have 64 bit pointers
#endif
```

### Commonly used macros

The following section is an overview of some of the more commonly used macros

* NV_OVERRIDE

For telling the compiler when a virtual method is overloaded. Unfortunately unlike java, in C++ you have to put it _after_ the method signiture. Also in visual studio, intellisense can be rather dumb with NV_OVERRIDE, and can confuse its processing. This can be fixed by placing a suitable hints.cpp in the root of the project (https://msdn.microsoft.com/en-us/library/dd997977.aspx) which we should probably add to our projects. Example..

```
class Base
{
	public:
	virtual void doSomething() = 0; 
	virtual void doSomethingElse(Int a) = 0;
};

class Derived: public Base
{
	public:
	void doSomething() NV_OVERRIDE;
	void doSomethingElse() NV_OVERRIDE;		//! Will produce an error as it doesn't correctly override the base class
};
```

On compilers that support it, it will produce an error if it isn't appropriately overriding. On compilers that don't it has noeffect. 

NOTE on visual studio NV_OVERRIDE can confuse intellisense. To fix set a cpp.hint file that defines the macro 

```
// https://msdn.microsoft.com/en-us/library/dd997977.aspx
#define NV_OVERRIDE override
```

* NV_HAS_MOVE_SEMANTICS has C++11 move semantics are supported on classes.
* NV_HAS_ENUM_CLASS has C++11 enum classes 
* NV_ALIGN_OF(x) gets the alignment of type (use like sizeof) 

* NV_INT_IS_32/NV_INT_IS_64 - Identifies the size of (strictly NvInt/Nv::Int and Nv::UInt/NvUInt) 
* NV_FLOAT_IS_32/NV_FLOAT_IS_64 - Says size of NvFloat type)
* NV_PTR_IS_32/NV_PTR_IS_64 - Says size of a pointer and NvSizeT/PtrDiffT Nv::SizeT/NvSizeT Nv::PtrDiffT/PtrDiffT
* NV_HAS_UNALIGNED_ACCESS - Is set if processor has _FAST_ unaligned access (it won't fault, and cost is less than say 1/3 perf)
* NV_LITTLE_ENDIAN/NV_BIG_ENDIAN - Set to identify processor endianess

* NV_INLINE is just for completelness with NV_ALWAYS_INLINE and NV_NO_INLINE
* NV_COUNT_OF - returns the size of fixed array/buffer

* NV_NULL - will use the null type that gives most features - typically nullptr in modern C++ compiler, but can be used across C/C++

* NV_BREAKPOINT(x) - when it will cause the program to break/crash into the debugger. The parameter is not generally used and is just an easy way to remember the purpose of the breakpoint.

* NV_FUNCTION_NAME - returns a platform specific version of the current function/method name
* NV_FUNCTION_SIG  - returns a platform specific version of function name AND signiture (if that is supported on the platform)

* NV_INT64(x) - NvInt64 literal
* NV_UINT64(x) - NvUInt64 literal


* NV_NO_INLINE - Never inline the code of this method (although it is compiled as if inline - ie can be used in header)
* NV_NO_COPY - Macro which disables copy constructor, and assignment
* NV_NO_ALIAS - Use to specify that a function has no 'aliasing' of the parameters
* NV_EXTERN_C - When before a function gives it C style naming (ie no C++ name mangling). Equivalent to extern "C"

### Switches

* NV_ENABLE_DEPRECIATION_WARNINGS enable depreciation warnings with NV_ENABLE_DEPRECIATION_WARNINGS

* NV_DISABLE_EXCEPTIONS - Set if code does not have exceptions, doing so will remove the exception signitures on Visual Studio that can cause spurious warnings. 


NvTypes.h
---------

* You can choose int8_t, NvInt8, nvidia::Int8, Nv::Int8 they all alias to the same thing 
* To support C where bool is not available you can use NvBool8 which is an 8 bit 'bool' type'   
* For consistency supports 'platform sized' types NvInt, NvUInt, NvFloat and Nv::Int, Nv::UInt, Nv::Float

NvAssert.h
----------

Provides simple support for adding asserts to your source. The assert mechanism is designed to use the compiler supported assert system is available. 

```
NV_CORE_ASSERT(x > 10) 		/// Will break into the debugger on debug build if x <= 10
NV_CORE_ALWAYS_ASSERT("Unreachable")	/// Always breaks into debugger. The parameter can be used to explain the problem
```

NvResult.h
----------

Provides support for a HRESULT-like return type. The problem this tries to address is that often with a function you want to return a value that determines if something was successful or not. If it is not successful it would be good to be able to determine why there was a failure. You could achieve this with an enum, but if a method calls the method that fails and it uses a different enum - now the inner enum is incompatible with the outer enum. 

Moreover sometimes you want to return that the action was successful, but perhaps not entirely or with some caveat, or even some small amount of data. 

A Result addresses all of these issues providing a single type that can be passed between methods and handled uniformly. A Result is just an Int32 underneath. If it is < 0 it is an error, if not it was successful. In order to encode many different types of error codes it has the concept of a 'facility' which is unique number to identify a large module of code. A simple example might be


```
namespace MyModule { 
using namespace Nv;

// Make sure the facility number of your module is unique
#define NV_FACILITY_MY_MODULE		(NV_FACILITY_EXTERNAL_BASE + 123)

enum FacilityResult
{
	RESULT_OK = NV_OK,
	RESULT_OPEN_FAILED = NV_MAKE_RESULT(NV_SEVERITY_ERROR, NV_FACILITY_MY_MODULE, 1),
	RESULT_INVALID_PARAMETERS = NV_MAKE_RESULT(NV_SEVERITY_ERROR, NV_FACILITY_MY_MODULE, 2),
};

// You can use like this...
Result res = RESULT_OK;

} // namespace MyModule

```

A more complex scenarion might be

```
// Make sure the facility number of your module is unique
#define NV_FACILITY_MY_MODULE		(NV_FACILITY_EXTERNAL_BASE + 123)

// An enum defining all the return types for the module
enum SubResult 
{ 
	SUB_RESULT_OK,						//!< succeeded
	SUB_RESULT_FAIL,					//!< unknown failure
	SUB_RESULT_OPEN_FAILED,				//!< file or stream open(or write) failed
	SUB_RESULT_INVALID_PARAMETERS,		//!< invalid options to the method
	SUB_RESULT_INVALID_FORMAT,			//!< invalid file or memory format
	SUB_RESULT_VERSION_MISMATCH,		//!< not supported version
}; 
// Define the results for your facility

#define NV_MY_MODULE_RESULT_OK(x) x(OK)
#define NV_MY_MODULE_FAIL(x) x(FAIL) x(OPEN_FAILED) x(INVALID_PARAMETERS) x(INVALID_FORMAT) x(VERSION_MISMATCH)

#define NV_MY_MODULE_EXPAND_OK(x) RESULT_##x = NV_MAKE_RESULT(NV_SEVERITY_SUCCESS, NV_FACILITY_MY_MODULE, SUB_RESULT_##x),
#define NV_MY_MODULE_EXPAND_FAIL(x) RESULT_##x = NV_MAKE_RESULT(NV_SEVERITY_ERROR, NV_FACILITY_MY_MODULE, SUB_RESULT_##x),

/*! All of the results for this facility. Can cast a result to NvHair::FacilityResult to quickly see the result */
enum FacilityResult
{
	NV_MY_MODULE_EXPAND_OK(NV_MY_MODULE_RESULT_OK)
	NV_MY_MODULE_EXPAND_FAIL(NV_MY_MODULE_FAIL)
};

// Now FaciltyResult holds all of the results for this facility, and they can be used by name

Result doSomething()
{
	return RESULT_VERSION_MISMATCH;
}
```

An advantage of this method is you can have a function which will return the result as a SubResult, such that it might be easier to handle in your code, or view whats going on in the debugger. Another advantage is it's simpler to add a new Result type. For example

```
NV_FORCE_INLINE SubResult getSubResult(Result res)
{
	if (NV_SUCCEEDED(res)) 
	{
		return SUB_RESULT_OK;
	}
	return (NV_GET_RESULT_FACILITY(res) == NV_FACILITY_MY_MODULE) ? SubResult(NV_GET_RESULT_CODE(res)) : SUB_RESULT_FAIL;
}
```


In C++ you can use Result as part of the nvidia (or Nv) namespace. In C you can use NvResult.
 
Integration
-----------

First it is envisaged that a project for it's own headers it will include a project. For example if the project Physx, with the short name Px, you might have a header

PxDefs.h

This will then include NvDefines.h. These might alias over NvDefs.h defines if a separate prefix space is wanted. The rest of physx then includes PxDefs.h not NvDefines.h.

As another example for foundation - NvPreprocessor.h could be replaced with...
```
#ifndef NV_NVFOUNDATION_NVPREPROCESSOR_H
#define NV_NVFOUNDATION_NVPREPROCESSOR_H

#include <Nv/Core/1.0/NvDefines.h>
#include <Nv/Core/1.0/NvTypes.h>

/**
Define API function declaration

NV_FOUNDATION_DLL=1 - used by the DLL library (PhysXCommon) to export the API
NV_FOUNDATION_DLL=0 - for windows configurations where the NV_FOUNDATION_API is linked through standard static linking
no definition       - this will allow DLLs and libraries to use the exported API from PhysXCommon

*/

#if NV_WINDOWS_FAMILY && !NV_ARM_FAMILY || NV_WINRT
#	ifndef NV_FOUNDATION_DLL
#		define NV_FOUNDATION_API NV_DLL_IMPORT
#	elif NV_FOUNDATION_DLL
#		define NV_FOUNDATION_API NV_DLL_EXPORT
#	endif
#elif NV_UNIX_FAMILY
#	ifdef NV_FOUNDATION_DLL
#		define NV_FOUNDATION_API NV_UNIX_EXPORT
#	endif
#endif

#ifndef NV_FOUNDATION_API
#	define NV_FOUNDATION_API
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVPREPROCESSOR_H
```

Guards
------

Certain sections of the NvDefines.h file can be disable using the following 

* NV_PROCESSOR_FEATURE - If defined, these processor 'features' will not be set
	* NV_INT_IS_32, NV_FLOAT_IS_32, NV_PTR_IS_32, NV_LITTLE_ENDIAN, NV_BIG_ENDIAN, NV_INT_IS_64, NV_FLOAT_IS_64, NV_PTR_IS_64, NV_HAS_UNALIGNED_ACCESS
  
* NV_PLATFORM - If defined stop generation of 'platform' defines
	* NV_WINRT, NV_XBOXONE, NV_WIN64, NV_X360, NV_WIN32, NV_ANDROID, NV_LINUX, NV_IOS, NV_OSX, NV_PS3, NV_PS4, NV_PSP2, NV_WIIU
  
* NV_COMPILER - If defined disables compiler symbols
	* NV_CLANG, NV_SNC, NV_GHS, NV_GCC, NV_VC
  
* NV_ALIGN - Defining this also means NV_ALIGN_PREFIX and NV_ALIGN_SUFFIX will not be defined

Depreciations
-------------

* NV_NO_INLINE - Depreciates NV_NOINLINE 
* NV_NO_COPY - Depreciates NV_NOCOPY
* NV_NO_ALIAS - Depreciates NV_NOALIAS
* NV_NO_ALIAS - Depreciates NV_NOALIAS
* NV_EXTERN_C - Depreciates NV_C_EXPORT (NV_C_EXPORT does not 'export' - just uses C name linkage for symbol name)


