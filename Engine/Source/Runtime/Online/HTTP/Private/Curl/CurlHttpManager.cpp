// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Curl/CurlHttpManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Curl/CurlHttpThread.h"
#include "Curl/CurlHttp.h"
#include "Modules/ModuleManager.h"

#if WITH_SSL
#include "Modules/ModuleManager.h"
#endif

#if WITH_LIBCURL

#if PLATFORM_WINDOWS
#include "SslModule.h"
#endif

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"

// recreating parts of winhttp.h in here because winhttp.h and wininet.h do not play well with each other.
#if defined(_WIN64)
#include <pshpack8.h>
#else
#include <pshpack4.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(_WINHTTP_INTERNAL_)
#define WINHTTPAPI DECLSPEC_IMPORT
#else
#define WINHTTPAPI

#endif

// WinHttpOpen dwAccessType values (also for WINHTTP_PROXY_INFO::dwAccessType)
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY               0
#define WINHTTP_ACCESS_TYPE_NO_PROXY                    1
#define WINHTTP_ACCESS_TYPE_NAMED_PROXY                 3
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY             4

typedef struct
{
	DWORD  dwAccessType;      // see WINHTTP_ACCESS_* types below
	LPWSTR lpszProxy;         // proxy server list
	LPWSTR lpszProxyBypass;   // proxy bypass list
}
WINHTTP_PROXY_INFO, *LPWINHTTP_PROXY_INFO;
WINHTTPAPI BOOL WINAPI WinHttpGetDefaultProxyConfiguration(WINHTTP_PROXY_INFO* pProxyInfo);

typedef struct
{
	BOOL    fAutoDetect;
	LPWSTR  lpszAutoConfigUrl;
	LPWSTR  lpszProxy;
	LPWSTR  lpszProxyBypass;
} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;
WINHTTPAPI BOOL WINAPI WinHttpGetIEProxyConfigForCurrentUser(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pProxyConfig);

#if defined(__cplusplus)
}
#endif

#include "HideWindowsPlatformTypes.h"
#endif

CURLM* FCurlHttpManager::GMultiHandle = NULL;
CURLSH* FCurlHttpManager::GShareHandle = NULL;

FCurlHttpManager::FCurlRequestOptions FCurlHttpManager::CurlRequestOptions;

#if PLATFORM_LINUX	// known to be available for Linux libcurl+libcrypto bundle at least
extern "C"
{
void CRYPTO_get_mem_functions(
		void *(**m)(size_t, const char *, int),
		void *(**r)(void *, size_t, const char *, int),
		void (**f)(void *, const char *, int));
int CRYPTO_set_mem_functions(
		void *(*m)(size_t, const char *, int),
		void *(*r)(void *, size_t, const char *, int),
		void (*f)(void *, const char *, int));
}
#endif // PLATFORM_LINUX

// set functions that will init the memory
namespace LibCryptoMemHooks
{
	void* (*ChainedMalloc)(size_t Size, const char* Src, int Line) = nullptr;
	void* (*ChainedRealloc)(void* Ptr, const size_t Size, const char* Src, int Line) = nullptr;
	void (*ChainedFree)(void* Ptr, const char* Src, int Line) = nullptr;
	bool bMemoryHooksSet = false;

	/** This malloc will init the memory, keeping valgrind happy */
	void* MallocWithInit(size_t Size, const char* Src, int Line)
	{
		void* Result = FMemory::Malloc(Size);
		if (LIKELY(Result))
		{
			FMemory::Memzero(Result, Size);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void* ReallocWithInit(void* Ptr, const size_t Size, const char* Src, int Line)
	{
		size_t CurrentUsableSize = FMemory::GetAllocSize(Ptr);
		void* Result = FMemory::Realloc(Ptr, Size);
		if (LIKELY(Result) && CurrentUsableSize < Size)
		{
			FMemory::Memzero(reinterpret_cast<uint8 *>(Result) + CurrentUsableSize, Size - CurrentUsableSize);
		}

		return Result;
	}

	/** This realloc will init the memory, keeping valgrind happy */
	void Free(void* Ptr, const char* Src, int Line)
	{
		return FMemory::Free(Ptr);
	}

	void SetMemoryHooks()
	{
		// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_LINUX && !UE_BUILD_SHIPPING
		CRYPTO_get_mem_functions(&ChainedMalloc, &ChainedRealloc, &ChainedFree);
		CRYPTO_set_mem_functions(MallocWithInit, ReallocWithInit, Free);
#endif // PLATFORM_LINUX && !UE_BUILD_SHIPPING

		bMemoryHooksSet = true;
	}

	void UnsetMemoryHooks()
	{
		// remove our overrides
		if (LibCryptoMemHooks::bMemoryHooksSet)
		{
			// do not set this in Shipping until we prove that the change in OpenSSL behavior is safe
#if PLATFORM_LINUX && !UE_BUILD_SHIPPING
			CRYPTO_set_mem_functions(LibCryptoMemHooks::ChainedMalloc, LibCryptoMemHooks::ChainedRealloc, LibCryptoMemHooks::ChainedFree);
#endif // PLATFORM_LINUX && !UE_BUILD_SHIPPING

			bMemoryHooksSet = false;
			ChainedMalloc = nullptr;
			ChainedRealloc = nullptr;
			ChainedFree = nullptr;
		}
	}
}

bool IsUnsignedInteger(const FString& InString)
{
	bool bResult = true;
	for(auto CharacterIter: InString)
	{
		if (!FChar::IsDigit(CharacterIter))
		{
			bResult = false;
			break;
		}
	}
	return bResult;
}

bool IsValidIPv4Address(const FString& InString)
{
	bool bResult = false;

	FString Temp = InString;
	FString AStr, BStr, CStr, DStr, PortStr;

	bool bWasPatternMatched = false;
	if (Temp.Split(TEXT("."), &AStr, &Temp))
	{
		if (Temp.Split(TEXT("."), &BStr, &Temp))
		{
			if (Temp.Split(TEXT("."), &CStr, &Temp))
			{
				if (Temp.Split(TEXT(":"), &DStr, &PortStr))
				{
					bWasPatternMatched = true;
				}
			}
		}
	}

	if (bWasPatternMatched)
	{
		if (IsUnsignedInteger(AStr) && IsUnsignedInteger(BStr) && IsUnsignedInteger(CStr) && IsUnsignedInteger(DStr) && IsUnsignedInteger(PortStr))
		{
			uint32 A, B, C, D, Port;
			Lex::FromString(A, *AStr);
			Lex::FromString(B, *BStr);
			Lex::FromString(C, *CStr);
			Lex::FromString(D, *DStr);
			Lex::FromString(Port, *PortStr);

			if (A < 256 && B < 256 && C < 256 && D < 256 && Port < 65536)
			{
				bResult = true;
			}
		}
	}

	return bResult;
}

void FCurlHttpManager::InitCurl()
{
	if (GMultiHandle != NULL)
	{
		UE_LOG(LogInit, Warning, TEXT("Already initialized multi handle"));
		return;
	}

#if WITH_SSL
	// Make sure ssl is loaded so that we can use the shared cert pool
	FModuleManager::LoadModuleChecked<class FSslModule>("SSL");
#endif // #if WITH_SSL

	// Override libcrypt functions to initialize memory since OpenSSL triggers multiple valgrind warnings due to this.
	// Do this before libcurl/libopenssl/libcrypto has been inited.
	LibCryptoMemHooks::SetMemoryHooks();

	CURLcode InitResult = curl_global_init_mem(CURL_GLOBAL_ALL, CurlMalloc, CurlFree, CurlRealloc, CurlStrdup, CurlCalloc);
	if (InitResult == 0)
	{
		curl_version_info_data * VersionInfo = curl_version_info(CURLVERSION_NOW);
		if (VersionInfo)
		{
			UE_LOG(LogInit, Log, TEXT("Using libcurl %s"), ANSI_TO_TCHAR(VersionInfo->version));
			UE_LOG(LogInit, Log, TEXT(" - built for %s"), ANSI_TO_TCHAR(VersionInfo->host));

			if (VersionInfo->features & CURL_VERSION_SSL)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports SSL with %s"), ANSI_TO_TCHAR(VersionInfo->ssl_version));
			}
			else
			{
				// No SSL
				UE_LOG(LogInit, Log, TEXT(" - NO SSL SUPPORT!"));
			}

			if (VersionInfo->features & CURL_VERSION_LIBZ)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports HTTP deflate (compression) using libz %s"), ANSI_TO_TCHAR(VersionInfo->libz_version));
			}

			UE_LOG(LogInit, Log, TEXT(" - other features:"));

#define PrintCurlFeature(Feature)	\
			if (VersionInfo->features & Feature) \
			{ \
			UE_LOG(LogInit, Log, TEXT("     %s"), TEXT(#Feature));	\
			}

			PrintCurlFeature(CURL_VERSION_SSL);
			PrintCurlFeature(CURL_VERSION_LIBZ);

			PrintCurlFeature(CURL_VERSION_DEBUG);
			PrintCurlFeature(CURL_VERSION_IPV6);
			PrintCurlFeature(CURL_VERSION_ASYNCHDNS);
			PrintCurlFeature(CURL_VERSION_LARGEFILE);
			PrintCurlFeature(CURL_VERSION_IDN);
			PrintCurlFeature(CURL_VERSION_CONV);
			PrintCurlFeature(CURL_VERSION_TLSAUTH_SRP);
#undef PrintCurlFeature
		}

		GMultiHandle = curl_multi_init();
		if (NULL == GMultiHandle)
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize create libcurl multi handle! HTTP transfers will not function properly."));
		}

		GShareHandle = curl_share_init();
		if (NULL != GShareHandle)
		{
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
			curl_share_setopt(GShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		}
		else
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl share handle!"));
		}
	}
	else
	{
		UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl (result=%d), HTTP transfers will not function properly."), (int32)InitResult);
	}

	// Init curl request options

	FString ProxyAddress;
	if (FParse::Value(FCommandLine::Get(), TEXT("httpproxy="), ProxyAddress))
	{
		if (!ProxyAddress.IsEmpty())
		{
			CurlRequestOptions.bUseHttpProxy = true;
			CurlRequestOptions.HttpProxyAddress = ProxyAddress;
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT(" Libcurl: -httpproxy has been passed as a parameter, but the address doesn't seem to be valid"));
		}
	}

#if PLATFORM_WINDOWS
	// Look for the default machine wide proxy setting
	if (ProxyAddress.Len() == 0)
	{
		// Retrieve the default proxy configuration.
		WINHTTP_PROXY_INFO DefaultProxyInfo;
		memset(&DefaultProxyInfo, 0, sizeof(DefaultProxyInfo));
		WinHttpGetDefaultProxyConfiguration(&DefaultProxyInfo);

		if (DefaultProxyInfo.lpszProxy != nullptr)
		{
			FString TempProxy(DefaultProxyInfo.lpszProxy);
			if (IsValidIPv4Address(TempProxy))
			{
				ProxyAddress = TempProxy;
			}
			else
			{
				if (TempProxy.Split(TEXT("https="), nullptr, &TempProxy))
				{
					TempProxy.Split(TEXT(";"), &TempProxy, nullptr);
					if (IsValidIPv4Address(TempProxy))
					{
						ProxyAddress = TempProxy;
					}
				}
			}
		}
	}

	// Look for the proxy setting for the current user. Charles proxies count in here.
	if (ProxyAddress.Len() == 0)
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG IeProxyInfo;
		memset(&IeProxyInfo, 0, sizeof(IeProxyInfo));
		WinHttpGetIEProxyConfigForCurrentUser(&IeProxyInfo);

		if (IeProxyInfo.lpszProxy != nullptr)
		{
			FString TempProxy(IeProxyInfo.lpszProxy);
			if (IsValidIPv4Address(TempProxy))
			{
				ProxyAddress = TempProxy;
			}
			else
			{
				if (TempProxy.Split(TEXT("https="), nullptr, &TempProxy))
				{
					TempProxy.Split(TEXT(";"), &TempProxy, nullptr);
					if (IsValidIPv4Address(TempProxy))
					{
						ProxyAddress = TempProxy;
					}
				}
			}
		}
	}
#endif

	if (ProxyAddress.Len() > 0)
	{
		CurlRequestOptions.bUseHttpProxy = true;
		CurlRequestOptions.HttpProxyAddress = ProxyAddress;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("noreuseconn")))
	{
		CurlRequestOptions.bDontReuseConnections = true;
	}

	// discover cert location
	if (PLATFORM_LINUX)	// only relevant to Linux (for now?), not #ifdef'ed to keep the code checked by the compiler when compiling for other platforms
	{
		static const char * KnownBundlePaths[] =
		{
			"/etc/pki/tls/certs/ca-bundle.crt",
			"/etc/ssl/certs/ca-certificates.crt",
			"/etc/ssl/ca-bundle.pem",
			nullptr
		};

		for (const char ** CurrentBundle = KnownBundlePaths; *CurrentBundle; ++CurrentBundle)
		{
			FString FileName(*CurrentBundle);
			UE_LOG(LogInit, Log, TEXT(" Libcurl: checking if '%s' exists"), *FileName);

			if (FPaths::FileExists(FileName))
			{
				CurlRequestOptions.CertBundlePath = *CurrentBundle;
				break;
			}
		}
		if (CurlRequestOptions.CertBundlePath == nullptr)
		{
			UE_LOG(LogInit, Log, TEXT(" Libcurl: did not find a cert bundle in any of known locations, TLS may not work"));
		}
	}
#if PLATFORM_ANDROID
	// used #if here to protect against GExternalFilePath only available on Android
	else
	if (PLATFORM_ANDROID)
	{
		const int32 PathLength = 200;
		static ANSICHAR capath[PathLength] = { 0 };

		// if file does not already exist, create local PEM file with system trusted certificates
		extern FString GExternalFilePath;
		FString PEMFilename = GExternalFilePath / TEXT("ca-bundle.pem");
		if (!FPaths::FileExists(PEMFilename))
		{
			FString Contents;

			IFileManager* FileManager = &IFileManager::Get();
			auto Ar = TUniquePtr<FArchive>(FileManager->CreateFileWriter(*PEMFilename, 0));
			if (Ar)
			{
				// check for override ca-bundle.pem embedded in game content
				FString OverridePEMFilename = FPaths::ProjectContentDir() + TEXT("CurlCertificates/ca-bundle.pem");
				if (FFileHelper::LoadFileToString(Contents, *OverridePEMFilename))
				{
					const TCHAR* StrPtr = *Contents;
					auto Src = StringCast<ANSICHAR>(StrPtr, Contents.Len());
					Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
				}
				else
				{
					// gather all the files in system certificates directory
					TArray<FString> directoriesToIgnoreAndNotRecurse;
					FLocalTimestampDirectoryVisitor Visitor(FPlatformFileManager::Get().GetPlatformFile(), directoriesToIgnoreAndNotRecurse, directoriesToIgnoreAndNotRecurse, false);
					FileManager->IterateDirectory(TEXT("/system/etc/security/cacerts"), Visitor);

					for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
					{
						// read and append the certificate file contents
						const FString CertFilename = TimestampIt.Key();
						if (FFileHelper::LoadFileToString(Contents, *CertFilename))
						{
							const TCHAR* StrPtr = *Contents;
							auto Src = StringCast<ANSICHAR>(StrPtr, Contents.Len());
							Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
						}
					}

					// add optional additional certificates
					FString OptionalPEMFilename = FPaths::ProjectContentDir() + TEXT("CurlCertificates/ca-additions.pem");
					if (FFileHelper::LoadFileToString(Contents, *OptionalPEMFilename))
					{
						const TCHAR* StrPtr = *Contents;
						auto Src = StringCast<ANSICHAR>(StrPtr, Contents.Len());
						Ar->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
					}
				}

				FPlatformString::Strncpy(capath, TCHAR_TO_ANSI(*PEMFilename), PathLength);
				CurlRequestOptions.CertBundlePath = capath;
				UE_LOG(LogInit, Log, TEXT(" Libcurl: using generated PEM file: '%s'"), *PEMFilename);
			}
		}
		else
		{
			FPlatformString::Strncpy(capath, TCHAR_TO_ANSI(*PEMFilename), PathLength);
			CurlRequestOptions.CertBundlePath = capath;
			UE_LOG(LogInit, Log, TEXT(" Libcurl: using existing PEM file: '%s'"), *PEMFilename);
		}

		if (CurlRequestOptions.CertBundlePath == nullptr)
		{
			UE_LOG(LogInit, Log, TEXT(" Libcurl: failed to generate a PEM cert bundle, TLS may not work"));
		}
	}
#endif

	// set certificate verification (disable to allow self-signed certificates)
	if (CurlRequestOptions.CertBundlePath == nullptr)
	{
		CurlRequestOptions.bVerifyPeer = false;
	}
	else
	{
		bool bVerifyPeer = true;
		if (GConfig->GetBool(TEXT("/Script/Engine.NetworkSettings"), TEXT("n.VerifyPeer"), bVerifyPeer, GEngineIni))
		{
			CurlRequestOptions.bVerifyPeer = bVerifyPeer;
		}
	}

	// print for visibility
	CurlRequestOptions.Log();
}

void FCurlHttpManager::FCurlRequestOptions::Log()
{
	UE_LOG(LogInit, Log, TEXT(" CurlRequestOptions (configurable via config and command line):"));
		UE_LOG(LogInit, Log, TEXT(" - bVerifyPeer = %s  - Libcurl will %sverify peer certificate"),
		bVerifyPeer ? TEXT("true") : TEXT("false"),
		bVerifyPeer ? TEXT("") : TEXT("NOT ")
		);

	UE_LOG(LogInit, Log, TEXT(" - bUseHttpProxy = %s  - Libcurl will %suse HTTP proxy"),
		bUseHttpProxy ? TEXT("true") : TEXT("false"),
		bUseHttpProxy ? TEXT("") : TEXT("NOT ")
		);	
	if (bUseHttpProxy)
	{
		UE_LOG(LogInit, Log, TEXT(" - HttpProxyAddress = '%s'"), *CurlRequestOptions.HttpProxyAddress);
	}

	UE_LOG(LogInit, Log, TEXT(" - bDontReuseConnections = %s  - Libcurl will %sreuse connections"),
		bDontReuseConnections ? TEXT("true") : TEXT("false"),
		bDontReuseConnections ? TEXT("NOT ") : TEXT("")
		);

	UE_LOG(LogInit, Log, TEXT(" - CertBundlePath = %s  - Libcurl will %s"),
		(CertBundlePath != nullptr) ? *FString(CertBundlePath) : TEXT("nullptr"),
		(CertBundlePath != nullptr) ? TEXT("set CURLOPT_CAINFO to it") : TEXT("use whatever was configured at build time.")
		);
}


void FCurlHttpManager::ShutdownCurl()
{
	if (NULL != GMultiHandle)
	{
		curl_multi_cleanup(GMultiHandle);
		GMultiHandle = NULL;
	}

	curl_global_cleanup();

	LibCryptoMemHooks::UnsetMemoryHooks();
}

FHttpThread* FCurlHttpManager::CreateHttpThread()
{
	return new FCurlHttpThread();
}



#endif //WITH_LIBCURL
