// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/NetworkFile/Private/ITransport.h"

class FTCPTransport : public ITransport
{

public:

	FTCPTransport();
	~FTCPTransport();

	// ITransport Interface. 
	virtual bool Initialize(const TCHAR* HostIp) override;
	virtual bool SendPayloadAndReceiveResponse(TArray<uint8>& In, TArray<uint8>& Out) override; 
	virtual bool ReceiveResponse(TArray<uint8> &Out) override;

private: 

	class FSocket*		FileSocket;
	class FMultichannelTcpSocket* MCSocket;

};
