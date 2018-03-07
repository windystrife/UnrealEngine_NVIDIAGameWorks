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

// No ifndef guard because this file is not a normal include file.

#if TBB_USE_DEBUG
#define DEBUG_SUFFIX "_debug"
#else
#define DEBUG_SUFFIX
#endif /* TBB_USE_DEBUG */

// RML_SERVER_NAME is the name of the RML server library.
#if _WIN32||_WIN64
#define RML_SERVER_NAME "irml" DEBUG_SUFFIX ".dll"
#elif __APPLE__
#define RML_SERVER_NAME "libirml" DEBUG_SUFFIX ".dylib"
#elif __linux__
#define RML_SERVER_NAME "libirml" DEBUG_SUFFIX ".so.1"
#elif __FreeBSD__ || __NetBSD__ || __sun || _AIX
#define RML_SERVER_NAME "libirml" DEBUG_SUFFIX ".so"
#else
#error Unknown OS
#endif

#include "library_assert.h"

const ::rml::versioned_object::version_type CLIENT_VERSION = 2;

#if __TBB_WEAK_SYMBOLS
    #pragma weak __RML_open_factory
    #pragma weak __TBB_make_rml_server
    #pragma weak __RML_close_factory
    #pragma weak __TBB_call_with_my_server_info
    extern "C" {
        ::rml::factory::status_type __RML_open_factory ( ::rml::factory&, ::rml::versioned_object::version_type&, ::rml::versioned_object::version_type );
        ::rml::factory::status_type __TBB_make_rml_server( tbb::internal::rml::tbb_factory& f, tbb::internal::rml::tbb_server*& server, tbb::internal::rml::tbb_client& client );
        void __TBB_call_with_my_server_info( ::rml::server_info_callback_t cb, void* arg );
        void __RML_close_factory( ::rml::factory& f );
    }
#endif /* __TBB_WEAK_SYMBOLS */

::rml::factory::status_type FACTORY::open() {
    // Failure of following assertion indicates that factory is already open, or not zero-inited.
    LIBRARY_ASSERT( !library_handle, NULL );
    status_type (*open_factory_routine)( factory&, version_type&, version_type );
    dynamic_link_descriptor server_link_table[4] = {
        DLD(__RML_open_factory,open_factory_routine),
        MAKE_SERVER(my_make_server_routine),
        DLD(__RML_close_factory,my_wait_to_close_routine),
        GET_INFO(my_call_with_server_info_routine),
    };
    status_type result;
    if( dynamic_link( RML_SERVER_NAME, server_link_table, 4, 4, &library_handle ) ) {
        version_type server_version;
        result = (*open_factory_routine)( *this, server_version, CLIENT_VERSION );
        // server_version can be checked here for incompatibility here if necessary.
    } else {
        library_handle = NULL;
        result = st_not_found;
    }
    return result;
}

void FACTORY::close() {
    if( library_handle )
        (*my_wait_to_close_routine)(*this);
    if( (size_t)library_handle>FACTORY::c_dont_unload ) {
        dynamic_unlink(library_handle);
        library_handle = NULL;
    }
}

::rml::factory::status_type FACTORY::make_server( SERVER*& s, CLIENT& c) {
    // Failure of following assertion means that factory was not successfully opened.
    LIBRARY_ASSERT( my_make_server_routine, NULL );
    return (*my_make_server_routine)(*this,s,c);
}

void FACTORY::call_with_server_info( ::rml::server_info_callback_t cb, void* arg ) const {
    // Failure of following assertion means that factory was not successfully opened.
    LIBRARY_ASSERT( my_call_with_server_info_routine, NULL );
    (*my_call_with_server_info_routine)( cb, arg );
}
