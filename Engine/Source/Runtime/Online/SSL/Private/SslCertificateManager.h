// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_SSL

#include "Interfaces/ISslCertificateManager.h"

class FSslCertificateManager : public ISslCertificateManager
{
public:
	//virtual const TArray<X509*>& GetCertificateArray() override; 
	virtual void AddCertificatesToSslContext(SSL_CTX* SslContextPtr) override;

	void BuildRootCertificateArray();
	void EmptyRootCertificateArray();

protected:
	TArray<X509*> RootCertificateArray;
};

#endif
