// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SslCertificateManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFile.h"
#include "HAL/FileManager.h"

#if WITH_SSL

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#endif

#include <openssl/ssl.h>

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

#include "UniquePtr.h"

void FSslCertificateManager::AddCertificatesToSslContext(SSL_CTX* SslContextPtr)
{
	X509_STORE* CertStore = SSL_CTX_get_cert_store((SSL_CTX *)SslContextPtr);
	for (int i = 0; i < RootCertificateArray.Num(); ++i)
	{
		if (X509_STORE_add_cert(CertStore, RootCertificateArray[i]) == 0)
		{
			printf("error adding certificate\n");
		}
	}
}

void FSslCertificateManager::BuildRootCertificateArray()
{
	FString CertificateBundlePath;
#if !UE_BUILD_SHIPPING
	FString OverrideCertificateBundlePath;
	if (GConfig->GetString(TEXT("SSL"), TEXT("OverrideCertificateBundlePath"), OverrideCertificateBundlePath, GEngineIni) && OverrideCertificateBundlePath.Len() > 0)
	{
		if (FPaths::FileExists(*(OverrideCertificateBundlePath)))
		{
			CertificateBundlePath = OverrideCertificateBundlePath;
		}
	}
#endif

	if (CertificateBundlePath.IsEmpty())
	{
		if (FPaths::FileExists(*(FPaths::ProjectContentDir() + TEXT("Certificates/cacert.pem"))))
		{
			CertificateBundlePath = FPaths::ProjectContentDir() + TEXT("Certificates/cacert.pem");
		}
		else if (FPaths::FileExists(*(FPaths::EngineContentDir() + TEXT("Certificates/ThirdParty/cacert.pem"))))
		{
			CertificateBundlePath = FPaths::EngineContentDir() + TEXT("Certificates/ThirdParty/cacert.pem");
		}
	}

	{
	int64 CertificateBundleBufferSize = 0;
		TUniquePtr<char[]> CertificateBundleBuffer;

		if (TUniquePtr<FArchive> CertificateBundleArchive = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*CertificateBundlePath, 0)))
	{
		CertificateBundleBufferSize = CertificateBundleArchive->TotalSize();
			CertificateBundleBuffer.Reset(new char[CertificateBundleBufferSize + 1]);
			CertificateBundleArchive->Serialize(CertificateBundleBuffer.Get(), CertificateBundleBufferSize);
			CertificateBundleBuffer[CertificateBundleBufferSize] = '\0';
	}

	if (CertificateBundleBufferSize > 0 && CertificateBundleBuffer != nullptr)
	{
		static const char BeginCertificateString[] = "-----BEGIN CERTIFICATE-----";
		static const char EndCertificateString[] = "-----END CERTIFICATE-----";

			const char* FoundString = CertificateBundleBuffer.Get();
		while (nullptr != (FoundString = FPlatformString::Strstr(FoundString, BeginCertificateString)))
		{
			const char* EndString = FPlatformString::Strstr(FoundString, EndCertificateString);
			if (EndString != nullptr)
			{
				size_t LengthOfCertificateData = EndString - FoundString + sizeof(EndCertificateString);
				BIO* CertificateBio = BIO_new_mem_buf(FoundString, LengthOfCertificateData);
				X509* Certificate = PEM_read_bio_X509(CertificateBio, NULL, 0, NULL);
				RootCertificateArray.Add(Certificate);
				BIO_free(CertificateBio);
			}
			FoundString = EndString;
		}
	}
	}

	FString DebuggingCertificatePath;
	if (GConfig->GetString(TEXT("SSL"), TEXT("DebuggingCertificatePath"), DebuggingCertificatePath, GEngineIni) && DebuggingCertificatePath.Len() > 0)
	{
		if (FPaths::FileExists(DebuggingCertificatePath))
		{
			FArchive* DebuggingCertificateArchive = IFileManager::Get().CreateFileReader(*DebuggingCertificatePath, 0);
			int64 CertificateBufferSize = DebuggingCertificateArchive->TotalSize();
			char* CertificateBuffer = new char[CertificateBufferSize + 1];
			DebuggingCertificateArchive->Serialize(CertificateBuffer, CertificateBufferSize);
			CertificateBuffer[CertificateBufferSize] = '\0';
			BIO* CertificateBio = BIO_new_mem_buf(CertificateBuffer, -1);
			X509* Certificate = PEM_read_bio_X509(CertificateBio, NULL, 0, NULL);
			RootCertificateArray.Add(Certificate);
			BIO_free(CertificateBio);
			delete[] CertificateBuffer;
			CertificateBuffer = nullptr;
		}
	}
}

void FSslCertificateManager::EmptyRootCertificateArray()
{
	for (int i = 0; i < RootCertificateArray.Num(); ++i)
	{
		X509_free(RootCertificateArray[i]);
	}
	RootCertificateArray.Reset();
}

#endif // #if WITH_SSL