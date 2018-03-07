// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"
#include "Engine/EngineBaseTypes.h"
#include "OnlineSubsystemOculusTypes.h"
#include "OnlineSubsystemOculusPackage.h"

/**
* Fake an internet ip address but in reality using an ovrID
*/
class FInternetAddrOculus : public FInternetAddr
{
PACKAGE_SCOPE:
	FUniqueNetIdOculus OculusId;

	/**
	* Copy Constructor
	*/
	FInternetAddrOculus(const FInternetAddrOculus& Src) :
		OculusId(Src.OculusId)
	{
	}

public:
	/**
	* Constructor. Sets address to default state
	*/
	FInternetAddrOculus() :
		OculusId(0ull)
	{
	}

	/**
	* Constructor
	*/
	explicit FInternetAddrOculus(const FUniqueNetIdOculus& InOculusId) :
		OculusId(InOculusId)
	{
	}

	/**
	* Constructor
	*/
	explicit FInternetAddrOculus(const FURL& ConnectURL)
	{
		auto Host = ConnectURL.Host;

		// Parse the URL: unreal://<oculus_id>.oculus or unreal://<oculus_id>
		int32 DotIndex;
		auto OculusStringID = (Host.FindChar('.', DotIndex)) ? Host.Left(DotIndex) : Host;
		OculusId = strtoull(TCHAR_TO_ANSI(*OculusStringID), nullptr, 10);
	}

	ovrID GetID() const
	{
		return OculusId.GetID();
	}

	/**
	* Sets the ip address from a host byte order uint32
	*
	* @param InAddr the new address to use (must convert to network byte order)
	*/
	void SetIp(uint32 InAddr) override
	{
		/** Not used */
	}

	/**
	* Sets the ip address from a string ("A.B.C.D")
	*
	* @param InAddr the string containing the new ip address to use
	*/
	void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{
		/** Not used */
	}

	/**
	* Copies the network byte order ip address to a host byte order dword
	*
	* @param OutAddr the out param receiving the ip address
	*/
	void GetIp(uint32& OutAddr) const override
	{
		/** Not used */
	}

	/**
	* Sets the port number from a host byte order int
	*
	* @param InPort the new port to use (must convert to network byte order)
	*/
	void SetPort(int32 InPort) override
	{
		/** Not used */
	}

	/**
	* Copies the port number from this address and places it into a host byte order int
	*
	* @param OutPort the host byte order int that receives the port
	*/
	void GetPort(int32& OutPort) const override
	{
		/** Not used */
	}

	/**
	* Returns the port number from this address in host byte order
	*/
	int32 GetPort() const override
	{
		/** Not used */
		return 0;
	}

	/** Sets the address to be any address */
	void SetAnyAddress() override
	{
		/** Not used */
	}

	/** Sets the address to broadcast */
	void SetBroadcastAddress() override
	{
		/** Not used */
	}

	/**
	* Converts this internet ip address to string form
	*
	* @param bAppendPort whether to append the port information or not
	*/
	FString ToString(bool bAppendPort) const override
	{
		return OculusId.ToString();
	}

	/**
	* Compares two internet ip addresses for equality
	*
	* @param Other the address to compare against
	*/
	virtual bool operator==(const FInternetAddr& Other) const override
	{
		FInternetAddrOculus& OculusOther = (FInternetAddrOculus&)Other;
		return OculusId == OculusOther.OculusId;
	}

	bool operator!=(const FInternetAddrOculus& Other) const
	{
		return !(FInternetAddrOculus::operator==(Other));
	}

	/**
	* Is this a well formed internet address
	*
	* @return true if a valid Oculus id, false otherwise
	*/
	virtual bool IsValid() const override
	{
		return OculusId.IsValid();
	}
};
