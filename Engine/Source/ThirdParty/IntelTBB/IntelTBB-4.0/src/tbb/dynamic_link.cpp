/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

#include "dynamic_link.h"

/*
    This file is used by both TBB and OpenMP RTL. Do not use __TBB_ASSERT() macro
    and runtime_warning() function because they are not available in OpenMP. Use
    LIBRARY_ASSERT and DYNAMIC_LINK_WARNING instead.
*/

#ifndef LIBRARY_ASSERT
    #include "tbb/tbb_stddef.h"
    #define LIBRARY_ASSERT(x,y) __TBB_ASSERT(x,y)
#endif /* !LIBRARY_ASSERT */

#include <cstdarg>          // va_list etc.
#if _WIN32||_WIN64
    #include <malloc.h>     /* alloca */
#else
    #include <dlfcn.h>
    #include <string.h>
    #include <unistd.h>
    #include <limits.h>
#if __FreeBSD__ || __NetBSD__
    #include <stdlib.h>     /* alloca */
#else
    #include <alloca.h>
#endif
#endif

#ifdef __TBB_BUILD
#include "tbb/atomic.h"
#endif /* __TBB_BUILD */

OPEN_INTERNAL_NAMESPACE

#if !defined(DYNAMIC_LINK_WARNING) && __TBB_DYNAMIC_LOAD_ENABLED
    // Report runtime errors and continue.
    #define DYNAMIC_LINK_WARNING dynamic_link_warning
    static void dynamic_link_warning( dynamic_link_error_t code, ... ) {
        (void) code;
    } // library_warning
#endif /* DYNAMIC_LINK_WARNING */

#if __TBB_DYNAMIC_LOAD_ENABLED
#if _WIN32 || _WIN64
/*
    There is a security issue on Windows: LoadLibrary() may load and execute malicious code.
    See http://www.microsoft.com/technet/security/advisory/2269637.mspx for details.
    To avoid the issue, we have to pass full path (not just library name) to LoadLibrary. This
    function constructs full path to the specified library (it is assumed the library located
    side-by-side with the tbb.dll.

    The function constructs absolute path for given relative path. Important: Base directory is not
    current one, it is the directory tbb.dll loaded from.

    Example:
        Let us assume "tbb.dll" is located in "c:\program files\common\intel\" directory, e. g.
        absolute path of tbb library is "c:\program files\common\intel\tbb.dll". Absolute path for
        "tbbmalloc.dll" would be "c:\program files\common\intel\tbbmalloc.dll". Absolute path for
        "malloc\tbbmalloc.dll" would be "c:\program files\common\intel\malloc\tbbmalloc.dll".

    Arguments:
        in  name -- Name of a file (may be with relative path; it must not be an absolute one).
        out path -- Buffer to save result (absolute path) to.
        in  len  -- Size of buffer.
        ret      -- 0         -- Error occurred.
                    > len     -- Buffer too short, required size returned.
                    otherwise -- Ok, number of characters (not counting terminating null) written to
                                 buffer.
*/
static size_t abs_path( char const * name, char * path, size_t len ) {
    // Get handle of our DLL first.
    HMODULE handle;
    BOOL brc =
        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)( & abs_path ),
            & handle
        );
    if ( ! brc ) {    // Error occurred.
        int err = GetLastError();
        DYNAMIC_LINK_WARNING( dl_sys_fail, "GetModuleHandleEx", err );
        return 0;
    } // if

    // Now get path to our DLL.
    DWORD drc = GetModuleFileName( handle, path, static_cast< DWORD >( len ) );
    if ( drc == 0 ) {    // Error occurred.
        int err = GetLastError();
        DYNAMIC_LINK_WARNING( dl_sys_fail, "GetModuleFileName", err );
        return drc;
    } // if
    if ( drc >= len ) {  // Buffer too short.
        DYNAMIC_LINK_WARNING( dl_buff_too_small );
        return drc;
    } // if

    // Find the position of the last backslash.
    char * backslash = path + drc;            // backslash points behind the path.
    LIBRARY_ASSERT( * backslash == 0, NULL );
    while ( backslash > path && * backslash != '\\' ) {
        -- backslash;
    } // while
    if ( backslash <= path ) {    // Backslash not found.
        return 0;                 // Unbelievable.
    } // if

    // Now append name to construct the full path.
    LIBRARY_ASSERT( * backslash == '\\', NULL );
    size_t rc = ( backslash + 1 - path ) + strlen( name );
    if ( rc >= len ) {
        DYNAMIC_LINK_WARNING( dl_buff_too_small );
        return rc + 1;
    } // if
    strcpy( backslash + 1, name );
    LIBRARY_ASSERT( rc == strlen( path ), NULL );
    return rc;
} // abs_path
#else /* WIN */
    class _abs_path {
        char _path[PATH_MAX+1];
        size_t _len;
        enum {
            ap_invalid = 0,
            ap_only_cwd,
            ap_ready
        } _state;

        bool prepare_full_path() {
            LIBRARY_ASSERT( _state==ap_only_cwd, NULL );

            Dl_info dlinfo;
            int res = dladdr( (void*)&dynamic_unlink, &dlinfo );
            if ( !res ) {
                char const * err = dlerror();
                DYNAMIC_LINK_WARNING( dl_sys_fail, "dladdr", err );
                return false;
            }

            size_t fname_len = strlen( dlinfo.dli_fname );
            // Find the position of the last backslash.
            while ( fname_len>0 && dlinfo.dli_fname[fname_len-1]!='/' ) fname_len-=1;

            size_t rc;

            if ( dlinfo.dli_fname[0]=='/' ) {
                rc = 0;
                _len = fname_len;
            } else {
                rc = _len;
                _len += fname_len;
            }

            if ( fname_len>0 ) {
                if ( _len>PATH_MAX ) {
                    DYNAMIC_LINK_WARNING( dl_buff_too_small );
                    return false;
                }
                memcpy( _path+rc, dlinfo.dli_fname, fname_len*sizeof(char) );
                _path[_len]=0;
            }

            return true;
        }
    public:
        _abs_path() {
            if ( getcwd( _path, PATH_MAX+1 ) ) {
                _state = ap_only_cwd;
                _len = strlen( _path );
                _path[_len++]='/';
            } else {
                DYNAMIC_LINK_WARNING( dl_buff_too_small );
                _state = ap_invalid;
            }
        }

        /*
            The function constructs absolute path for given relative path. Important: Base directory is not
            current one, it is the directory libtbb.so loaded from.

            Arguments:
            in  name -- Name of a file (may be with relative path; it must not be an absolute one).
            out path -- Buffer to save result (absolute path) to.
            in  len  -- Size of buffer.
            ret      -- 0         -- Error occured.
                        > len     -- Buffer too short, required size returned.
                        otherwise -- Ok, number of characters (not counting terminating null) written to
                                     buffer.
        */
        size_t operator ()( char const * name, char * path, size_t len ) {
            switch ( _state ) {
                case ap_only_cwd:
                    if ( !prepare_full_path() ) {
                        _state = ap_invalid;
                        return 0;
                    }
                    _state = ap_ready;
                    // 'break' is missed since we really want to do next case.
                case ap_ready: {
                        size_t name_len = strlen( name );
                        size_t full_len = name_len+_len;
                        if ( full_len+_len < len ) {
                            memcpy( path, _path, _len );
                            memcpy( path+_len, name, name_len );
                            path[full_len] = 0;
                        }
                        return full_len;
                    }
                default:
                    return 0;
            }
        }

    };

_abs_path abs_path;
#endif /* WIN */
#endif /* __TBB_DYNAMIC_LOAD_ENABLED */

#if __TBB_WEAK_SYMBOLS

bool dynamic_link( dynamic_link_handle, const dynamic_link_descriptor descriptors[], size_t n, size_t required )
{
    if ( required == ~(size_t)0 )
        required = n;
    LIBRARY_ASSERT( required<=n, "Number of required entry points exceeds their total number" );
    size_t k = 0;
    // Check if the first required entries are present in what was loaded into our process
    while ( k < required && descriptors[k].ptr )
        ++k;
    if ( k < required )
        return false;
    // Commit all the entry points.
    for ( k = 0; k < n; ++k )
        *descriptors[k].handler = (pointer_to_handler) descriptors[k].ptr;
    return true;
}

#else /* !__TBB_WEAK_SYMBOLS */

bool dynamic_link( dynamic_link_handle module, const dynamic_link_descriptor descriptors[], size_t n, size_t required )
{
    LIBRARY_ASSERT( module != NULL, "Module handle is NULL" );
    if ( module == NULL ) {
        return false;
    } // if
    pointer_to_handler *h = (pointer_to_handler*)alloca(n * sizeof(pointer_to_handler));
    if ( required == ~(size_t)0 )
        required = n;
    LIBRARY_ASSERT( required<=n, "Number of required entry points exceeds their total number" );
    size_t k = 0;
    for ( ; k < n; ++k ) {
        dynamic_link_descriptor const & desc = descriptors[k];
#if _WIN32||_WIN64
        FARPROC addr = GetProcAddress( module, desc.name );
        if ( addr == NULL ) {
            int err = GetLastError();
            DYNAMIC_LINK_WARNING( dl_sym_not_found, desc.name, err );
        } // if
        h[k] = pointer_to_handler( addr );
#else /* !WIN */
        void * addr = dlsym( module, desc.name );
        if ( addr == NULL ) {
            char const * err = dlerror();
            DYNAMIC_LINK_WARNING( dl_sym_not_found, desc.name, err );
        } // if
        // Lvalue casting is used; this way icc -strict-ansi does not warn about nonstandard pointer conversion
        (void *&)h[k] = addr;
#endif /* !WIN */
        if ( !h[k] && k < required )
            return false;
    }
    LIBRARY_ASSERT( k == n, "if required entries are initialized, all entries are expected to be walked");
    // Commit the entry points.
    // Cannot use memset here, because the writes must be atomic.
    for( k = 0; k < n; ++k )
        *descriptors[k].handler = h[k];
    return true;
}

#endif /* !__TBB_WEAK_SYMBOLS */

void dynamic_unlink( dynamic_link_handle handle ) {
    if ( handle ) {
#if __TBB_DYNAMIC_LOAD_ENABLED
#if _WIN32||_WIN64
        FreeLibrary( handle );
#else
        dlclose( handle );
#endif
        (void)handle;
#endif /* __TBB_DYNAMIC_LOAD_ENABLED */
    }
}

#if __TBB_BUILD

// Class handle_storage is used by dynamic_link routine to store handles of
// all loaded or pinned dynamic libraries. When TBB is shut down, it calls
// dynamic_unlink_all() that unloads modules referenced by handle_storage.
// This functionality is only used by TBB. It should not be used by other
// libraries reusing this source file to avoid dependency on tbb::atomic<>.

#define MAX_LOADED_MODULES 8 // The number of maximum possible modules which can be loaded

class handle_storage {
    tbb::atomic<size_t> my_size;
    dynamic_link_handle my_handles[MAX_LOADED_MODULES];
public:

    handle_storage() {
        my_size = 0;
    }

    void add_handle(dynamic_link_handle &handle) {
        const size_t ind = my_size++;
        LIBRARY_ASSERT( ind < MAX_LOADED_MODULES, "Too many modules are loaded" );
        my_handles[ind] = handle;
    }

    void free_handles() {
        const size_t size = my_size.fetch_and_store( 0 );
        for (size_t i=0; i<size; ++i)
            dynamic_unlink( my_handles[i] );
    }

} handles;
#endif /* __TBB_BUILD */

bool dynamic_link( const char* library, const dynamic_link_descriptor descriptors[], size_t n, size_t required, dynamic_link_handle *handle ) {
    // Get library handle in case it is already loaded into the current process
#if ! __TBB_DYNAMIC_LOAD_ENABLED
    dynamic_link_handle library_handle = NULL;
    __TBB_ASSERT_EX( library, "library name must be provided");
#elif _WIN32||_WIN64
    dynamic_link_handle library_handle = GetModuleHandle( library );
#else
    dynamic_link_handle library_handle = dlopen( NULL, RTLD_LAZY );
#endif /* _WIN32||_WIN64 */

    // Get descriptors from the library
    if ( library_handle && dynamic_link( library_handle, descriptors, n, required ) ) {
#if !__TBB_DYNAMIC_LOAD_ENABLED
        return true;
#else
        // The library has been loaded by another module and contains requested symbols.
        // But after we obtained the library's handle it can be unloaded by another thread
        // invalidating our handle copy. Therefore we need to pin the library in memory.
#if _WIN32||_WIN64
        char library_full_name[ MAX_PATH+1 ];
        // Get library's name from earlier found handle
        if ( GetModuleFileName( library_handle, library_full_name, MAX_PATH+1 ) ) {
            // Pin the library
            library_handle = LoadLibrary( library_full_name );
            if ( library_handle == NULL ) {
                int err = GetLastError();
                DYNAMIC_LINK_WARNING( dl_lib_not_found, library_full_name, err );
            } // if
        } // if
#else /* !WIN */
        Dl_info info;
        // Get library's name from earlier found symbol
        if ( dladdr( (void*)*descriptors[0].handler, &info ) ) {
            // Pin the library
            library_handle = dlopen( info.dli_fname, RTLD_LAZY );
            if ( library_handle == NULL ) {
                char const * err = dlerror();
                DYNAMIC_LINK_WARNING( dl_lib_not_found, info.dli_fname, err );
            } // if
        } // if
#endif /* !WIN */
        else {
            // The library have been unloaded by another thread
            library_handle = 0;
        }
        if ( library_handle ) {
            // If original library was unloaded before we pinned it
            // and then another module loaded in its place, the earlier
            // found symbols would become invalid. So revalidate them.
            if ( !dynamic_link( library_handle, descriptors, n, required ) ) {
                // Wrong library.
                dynamic_unlink(library_handle);
                library_handle = 0;
            }
        }
        if ( !library_handle ) {
            // Failed to pin the library, so clear the descriptors too.
            for( size_t i=0; i<n; ++i )
                *descriptors[i].handler = 0;
        }
#endif /* __TBB_DYNAMIC_LOAD_ENABLED */
    } else {
        library_handle = 0;
    }

#if __TBB_DYNAMIC_LOAD_ENABLED
    if ( !library_handle ) {
#if _WIN32||_WIN64
#if _XBOX
        library_handle = LoadLibrary (library);
#else
        library_handle = NULL;
        // Construct absolute path to the library to avoid security issue.
        size_t const len = MAX_PATH + 1;
        char path[ len ];
        size_t rc = abs_path( library, path, len );
        if ( 0 < rc && rc < len ) {
            // Prevent Windows from displaying silly message boxes if it fails to load library
            // (e.g. because of MS runtime problems - one of those crazy manifest related ones)
            UINT prev_mode = SetErrorMode (SEM_FAILCRITICALERRORS);
            library_handle = LoadLibrary (path);
            SetErrorMode (prev_mode);
            if ( library_handle == NULL ) {
                int err = GetLastError();
                DYNAMIC_LINK_WARNING( dl_lib_not_found, path, err );
            } // if
        } // if
#endif /* !_XBOX */
#else /* !WIN */
        library_handle = NULL;
        // Construct absolute path to the library.
        size_t const len = PATH_MAX + 1;
        char path[ len ];
        size_t rc = abs_path( library, path, len );
        if ( 0 < rc && rc < len ) {
            library_handle = dlopen( path, RTLD_LAZY );
            if ( library_handle == NULL ) {
                char const * err = dlerror();
                DYNAMIC_LINK_WARNING( dl_lib_not_found, library, err );
            } // if
        } // if
#endif /* !WIN */
        if( library_handle ) {
            if( !dynamic_link( library_handle, descriptors, n, required ) ) {
                // The loaded library does not contain all the expected entry points
                dynamic_unlink( library_handle );
                library_handle = NULL;
            }
        }
    }
#endif /* __TBB_DYNAMIC_LOAD_ENABLED */

    if ( library_handle ) {
        if ( handle )
            *handle = library_handle;
#if __TBB_BUILD
        else
            handles.add_handle( library_handle );
#endif /* __TBB_BUILD */
        return true;
    }
    return false;
}

#if __TBB_BUILD
void dynamic_unlink_all() {
    handles.free_handles();
}
#endif /* __TBB_BUILD */

CLOSE_INTERNAL_NAMESPACE
