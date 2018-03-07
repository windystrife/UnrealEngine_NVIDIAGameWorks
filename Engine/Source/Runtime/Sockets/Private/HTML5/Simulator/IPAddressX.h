// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"

/**
 * Represents an internet ip address, using the relatively standard SOCKADDR_IN structure. All data is in network byte order
 */

class FInternetAddrRaw; 

class FInternetAddrX : public FInternetAddr
{
	
public:
	/**
	 * Constructor. Sets address to default state
	 */
	FInternetAddrX();
	/**
	 * Sets the ip address from a host byte order uint32
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr) override;
	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;

	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const override;

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort) override;

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	virtual void GetPort(int32& OutPort) const override;

	/**
	 * Returns the port number from this address in host byte order
	 */
	virtual int32 GetPort(void) const override;

	/** Sets the address to be any address */
	virtual void SetAnyAddress(void) override;

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override;

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const override;
	/**
	 * Is this a well formed internet address
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const override;

	FInternetAddrRaw* GetPimpl() { return Pimpl; }

private: 

	FInternetAddrRaw* Pimpl;  
};


