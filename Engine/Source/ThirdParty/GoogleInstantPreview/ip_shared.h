#ifndef INSTANT_PREVIEW_IP_SHARED_IP_SHARED_H_

#define INSTANT_PREVIEW_IP_SHARED_IP_SHARED_H_





// The following ifdef block is the standard way of creating macros which make exporting

// from a DLL simpler. All files within this DLL are compiled with the IP_SHARED_EXPORTS

// symbol defined on the command line. This symbol should not be defined on any project

// that uses this DLL. This way any other project whose source files include this file see

// IP_SHARED_API functions as being imported from a DLL, whereas this DLL sees symbols

// defined with this macro as being exported.



#ifdef IP_SHARED_EXPORTS

#ifdef _WIN32

#define IP_SHARED_API __declspec(dllexport)

#else  // _WIN32

#define IP_SHARED_API __attribute__((visibility("default")))

#endif  // _WIN32

#else  // IP_SHARED_EXPORTS

#ifdef _WIN32

#define IP_SHARED_API __declspec(dllimport)

#else  // _WIN32

#define IP_SHARED_API extern

#endif  // _WIN32

#endif  // IP_SHARED_EXPORTS



namespace instant_preview {

  class Server;

  class Session;

}



IP_SHARED_API void ip_create_server(instant_preview::Server** server);

IP_SHARED_API void ip_destroy_server(instant_preview::Server* server);





typedef void* ip_static_server_handle;



// Creates a static server running on the given address or returns the current

// server if one is already active.  If listen address is NULL, listens

// on the default port.  This call has acquire/release semantics.  The server

// will remain active until all ip_static_server_start calls have a matching

// ip_static_server_stop.

IP_SHARED_API ip_static_server_handle ip_static_server_start(const char* listen_address, bool adb_reverse, const char* adb_path);

// Tests whether adb is available at the specified adb_path.

IP_SHARED_API bool ip_static_server_is_adb_available(ip_static_server_handle handle);

// Gets a pointer to the active session, and locks the session until it has been released.

IP_SHARED_API instant_preview::Session* ip_static_server_acquire_active_session(ip_static_server_handle handle);

// Releases the session so that it can be cleaned up if it is no longer active.

IP_SHARED_API void ip_static_server_release_active_session(ip_static_server_handle handle, instant_preview::Session* session);

// Stops the given server handle (acquire/release semantics) that was started

// with a call to ip_static_server_start.

IP_SHARED_API void ip_static_server_stop(ip_static_server_handle handle);

// Gets the current version string for this build.

IP_SHARED_API const char* ip_get_version_string();



#endif  // INSTANT_PREVIEW_IP_SHARED_IP_SHARED_H_

