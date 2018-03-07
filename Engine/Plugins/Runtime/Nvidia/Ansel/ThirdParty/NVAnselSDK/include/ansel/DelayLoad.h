#pragma once

namespace ansel
{
#if defined(ANSEL_SDK_DELAYLOAD)
    enum DelayLoadStatus
    {
        kDelayLoadOk = 0,
        kDelayLoadNoDllFound = -1,
        kDelayLoadIncompatibleVersion = -2,
    };
    // In order to use delay loading instead of regular linking to a dynamic library
    // define ANSEL_SDK_DELAYLOAD and use 'loadLibrary' function with an optional path to
    // the library (otherwise it's going to use the usual LoadLibrary semantic)
    DelayLoadStatus loadLibraryW(const wchar_t* path = nullptr);
    DelayLoadStatus loadLibraryA(const char* path = nullptr);
#endif
}