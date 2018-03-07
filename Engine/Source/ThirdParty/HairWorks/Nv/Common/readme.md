NvCommon
========

NvCommon is a small stand alone C++ library that provides the following facilities

* Logging (Logger)
* Abstraction of memory management (MemoryAllocator)
* A very simple object model
* Smart pointers (UniquePtr, and ComPtr)
* String handling (String, SubString)
* Simple containers (Array, PodBuffer)
* Platform specific helper facilities 
* Simple rendering API abstractions and tools (Dx, Dx11 and Dx12)
  * RenderContext abstracts typically the rendering window - supports present, sync, clearing, and accessing underlying rendering API	
  * Classes to manage and simplify common rendering problems - especially around Dx12	
	
This document is written in 'markdown' format and can be read in a normal text editor, but may be easier to read in with a nice 'reader'. Some examples

* Markdown plugin for firefox https://addons.mozilla.org/en-US/firefox/addon/markdown-viewer/
	
Releases
--------

* 1.0 - Original Release
	
Integration
-----------

To integrate the appropriate include files need to be included. The main include is 

```
#include <Nv/Common/NvCoCommon.h>
```

All of the files in the NvCommon library are in the Nv/Common directory. All of the files are prefixed with NvCo (short for NvCommon), and are in the namespace nvidia::Common. Typing nvidia::Common can get kind of dull, so there is an alias NvCo. You will see this often used in headers to access NvCommon types explicitly. In cpp files it is not uncommon to just use 'using namespace NvCo' if this doesn't cause significant clashes.

Files are always included by the full path. 

The files in Nv/Common/Platform hold platform specific headers and code. Your project should compiler/include those that are applicable to your platform. 

There are other directories which contain platform specific code. Typically this is identified by 'platform' and/or 'api'. For example DirectX12 implementations are found in Dx12 directories. Directories which hold platform specific code are

* Win - Windows (Win32/Win64)
* StdC - Requires functions from the standard C libraries (such as sprintf etc)
* Dx - Requires DirectX headers (can be for Dx9 - Dx12)
* Dx11 - Requires DirectX11
* Dx12 - Requires DirectX12

It may be the case that some platform independent files require for their implementation the compilation of platform specific files. For example NvCoPathUtil will require NvCoWinPathUtil.cpp to be compiled. 

The NvCommon code base depends on the NvCore/1.0 code base. This header only library provides compiler/machine abstraction as well as support for some simple types and error checking. The Nv core library has to be includable along the path Nv/Core/1.0 to work correctly with NvCommon/1.0.

Platforms
---------

Whilst much of the code base is platform independent, platform specific code generally revolves around windows. The codebase is mainly tested and used with Visual Studio 2015 on Windows 7 and higher. 
