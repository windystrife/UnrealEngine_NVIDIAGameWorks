// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Array.h>

struct x509_st;
typedef struct x509_st X509;

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

class ISslCertificateManager
{
public:
	//virtual const TArray<X509*>& GetCertificateArray() = 0;
	virtual void AddCertificatesToSslContext(SSL_CTX* SslContextPtr) = 0;
	virtual ~ISslCertificateManager()
	{}
};