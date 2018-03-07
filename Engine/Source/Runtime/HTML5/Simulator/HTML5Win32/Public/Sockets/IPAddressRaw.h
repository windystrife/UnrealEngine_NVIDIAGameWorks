// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Represents an internet ip address, using the relatively standard SOCKADDR_IN structure. All data is in network byte order
 */

struct FInternetAddrRawData; 

class FInternetAddrRaw 
{
	/** The internet ip address structure */
	FInternetAddrRawData* Data; 

public:

	FInternetAddrRaw(); 

	/**
	 * Sets the ip address from a host byte order uint32
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	void SetIp(unsigned int InAddr); 

	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(unsigned int& OutAddr);

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	void SetPort(int InPort);

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	void GetPort(int& OutPort);


	/** Sets the address to be any address */
	void SetAnyAddress();

	/** Sets the address to broadcast */
	void SetBroadcastAddress();

	/**
	 * Is this a well formed internet address
	 *
	 * @return true if a valid IP, false otherwise
	 */
	bool IsValid();

	const FInternetAddrRawData* GetInternalData() const; 
	FInternetAddrRawData* GetInternalData(); 

};
