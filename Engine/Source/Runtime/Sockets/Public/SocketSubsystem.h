// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SocketTypes.h"

class Error;
class FInternetAddr;

SOCKETS_API DECLARE_LOG_CATEGORY_EXTERN(LogSockets, Log, All);

// Need to guarantee the "default" socket subsystem on these platforms
// as other subsystems (ie Steam) might override the default
#ifndef PLATFORM_SOCKETSUBSYSTEM
	#if PLATFORM_WINDOWS
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("WINDOWS"))
	#elif PLATFORM_MAC
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("MAC"))
	#elif PLATFORM_IOS
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("IOS"))
	#elif PLATFORM_LINUX
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("LINUX"))
	#elif PLATFORM_ANDROID
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("ANDROID"))
	#elif PLATFORM_PS4
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("PS4"))
	#elif PLATFORM_XBOXONE
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("XBOXONE"))
	#elif PLATFORM_HTML5
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("HTML5"))
	#elif PLATFORM_SWITCH
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT("SWITCH"))
	#else
		#define PLATFORM_SOCKETSUBSYSTEM FName(TEXT(""))
	#endif
#endif

class FInternetAddr;

/**
 * This is the base interface to abstract platform specific sockets API
 * differences.
 */
class SOCKETS_API ISocketSubsystem
{

public:

	/**
	 * Get the singleton socket subsystem for the given named subsystem
	 */
	static ISocketSubsystem* Get(const FName& SubsystemName=NAME_None);

	/**
	 * Shutdown all registered subsystems
	 */
	static void ShutdownAllSystems();


	virtual ~ISocketSubsystem() { }

	/**
	 * Does per platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return true if initialized ok, false otherwise
	 */
	virtual bool Init(FString& Error) = 0;

	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Shutdown() = 0;

	/**
	 * Creates a socket
	 *
	 * @Param SocketType type of socket to create (DGram, Stream, etc)
	 * @param SocketDescription debug description
	 * @param bForceUDP overrides any platform specific protocol with UDP instead
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false) = 0;

	/**
	* Creates a resolve info cached struct to hold the resolved address
	*
	* @Param Addr address to resolve for the socket subsystem
	*
	* @return the new resolved address or NULL if failed
	*/
	virtual class FResolveInfoCached* CreateResolveInfoCached(TSharedPtr<FInternetAddr> Addr) const;

	/**
	 * Cleans up a socket class
	 *
	 * @param Socket the socket object to destroy
	 */
	virtual void DestroySocket(class FSocket* Socket) = 0;

	/**
	 * Does a DNS look up of a host name
	 *
	 * @param HostName the name of the host to look up
	 * @param Addr the address to copy the IP address to
	 */
	virtual ESocketErrors GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr) = 0;

	/**
	 * Creates a platform specific async hostname resolution object
	 *
	 * @param HostName the name of the host to look up
	 *
	 * @return the resolve info to query for the address
	 */
	virtual class FResolveInfo* GetHostByName(const ANSICHAR* HostName);

	/**
	 * Some platforms require chat data (voice, text, etc.) to be placed into
	 * packets in a special way. This function tells the net connection
	 * whether this is required for this platform
	 */
	virtual bool RequiresChatDataBeSeparate() = 0;

	/**
	 * Some platforms require packets be encrypted. This function tells the
	 * net connection whether this is required for this platform
	 */
	virtual bool RequiresEncryptedPackets() = 0;

	/**
	 * Determines the name of the local machine
	 *
	 * @param HostName the string that receives the data
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool GetHostName(FString& HostName) = 0;

	/**
	 *	Create a proper FInternetAddr representation
	 * @param Address host address
	 * @param Port host port
	 */
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address=0, uint32 Port=0) = 0;

	/**
	 * @return Whether the machine has a properly configured network device or not
	 */
	virtual bool HasNetworkDevice() = 0;

	/**
	 *	Get the name of the socket subsystem
	 * @return a string naming this subsystem
	 */
	virtual const TCHAR* GetSocketAPIName() const = 0;

	/**
	 * Returns the last error that has happened
	 */
	virtual ESocketErrors GetLastErrorCode() = 0;

	/**
	 * Translates the platform error code to a ESocketErrors enum
	 */
	virtual ESocketErrors TranslateErrorCode(int32 Code) = 0;


	// The following functions are not expected to be overridden

	/**
	 * Returns a human readable string from an error code
	 *
	 * @param Code the error code to check
	 */
	const TCHAR* GetSocketError(ESocketErrors Code = SE_GET_LAST_ERROR_CODE);

	/**
	 * Gets the list of addresses associated with the adapters on the local computer.
	 *
	 * @param OutAdresses - Will hold the address list.
	 *
	 * @return true on success, false otherwise.
	 */
	virtual bool GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses ) = 0;

	/**
	 *	Get local IP to bind to
	 */
	virtual TSharedRef<FInternetAddr> GetLocalBindAddr(FOutputDevice& Out);

	/**
	 * Bind to next available port.
	 *
	 * @param Socket The socket that that will bind to the port
	 * @param Addr The local address and port that is being bound to (usually the result of GetLocalBindAddr()). This addresses port number will be modified in place
	 * @param PortCount How many ports to try
	 * @param PortIncrement The amount to increase the port number by on each attempt
	 *
	 * @return The bound port number, or 0 on failure
	 */
	int32 BindNextPort(class FSocket* Socket, FInternetAddr& Addr, int32 PortCount, int32 PortIncrement);

	/**
	 * Uses the platform specific look up to determine the host address
	 *
	 * @param Out the output device to log messages to
	 * @param bCanBindAll true if all can be bound (no primarynet), false otherwise
	 *
	 * @return The local host address
	 */
	virtual TSharedRef<FInternetAddr> GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll);

	/**
	 * Checks the host name cache for an existing entry (faster than resolving again)
	 *
	 * @param HostName the host name to search for
	 * @param Addr the out param that the IP will be copied to
	 *
	 * @return true if the host was found, false otherwise
	 */
	bool GetHostByNameFromCache(const ANSICHAR* HostName, TSharedPtr<class FInternetAddr>& Addr);

	/**
	 * Stores the ip address with the matching host name
	 *
	 * @param HostName the host name to search for
	 * @param Addr the IP that will be copied from
	 */
	void AddHostNameToCache(const ANSICHAR* HostName, TSharedPtr<class FInternetAddr> Addr);

	/**
	 * Removes the host name to ip mapping from the cache
	 *
	 * @param HostName the host name to search for
	 */
	void RemoveHostNameFromCache(const ANSICHAR* HostName);


private:

	/** Used to provide threadsafe access to the host name cache */
	FCriticalSection HostNameCacheSync;

	/** Stores a resolved IP address for a given host name */
	TMap<FString, TSharedPtr<FInternetAddr> > HostNameCache;
};

typedef TSharedPtr<ISocketSubsystem> IOnlineSocketPtr;


