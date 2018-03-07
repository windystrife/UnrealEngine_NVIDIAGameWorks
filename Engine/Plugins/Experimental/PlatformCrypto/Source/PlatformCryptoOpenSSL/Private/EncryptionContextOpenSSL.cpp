// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EncryptionContextOpenSSL.h"

#include <openssl/evp.h>

DEFINE_LOG_CATEGORY_STATIC(LogPlatformCryptoOpenSSL, Warning, All);

static const int32 AES256_KeySizeInBytes = 32;
static const int32 AES256_BlockSizeInBytes = 16;

class FScopedEVPContext
{
public:
	FScopedEVPContext() :
		Context(EVP_CIPHER_CTX_new())
	{
	}

	~FScopedEVPContext()
	{
		EVP_CIPHER_CTX_free(Context);
	}

	EVP_CIPHER_CTX* Get() const { return Context; }

private:
	EVP_CIPHER_CTX* Context;
};

TArray<uint8> FEncryptionContextOpenSSL::Encrypt_AES_256_ECB(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("OpenSSL AES256 Encrypt"), STAT_OpenSSL_AES_Encrypt, STATGROUP_PlatformCrypto);

	OutResult = EPlatformCryptoResult::Failure;

	if (Key.Num() != AES256_KeySizeInBytes)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Encrypt_AES_256_ECB: Key size %d is not the expected size %d."), Key.Num(), AES256_KeySizeInBytes);
		return TArray<uint8>();
	}

	FScopedEVPContext Context;
	if (Context.Get() == nullptr)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Encrypt_AES_256_ECB: failed to create EVP context."));
		return TArray<uint8>();
	}

	const int InitResult = EVP_EncryptInit_ex(Context.Get(), EVP_aes_256_ecb(), nullptr, Key.GetData(), nullptr);
	if (InitResult != 1)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Encrypt_AES_256_ECB: EVP_EncryptInit_ex failed."));
		return TArray<uint8>();
	}

	TArray<uint8> Ciphertext;
	Ciphertext.SetNumUninitialized(Plaintext.Num() + AES256_BlockSizeInBytes); // Allow for up to a block of padding.

	int OutLength = 0;
	const int UpdateResult = EVP_EncryptUpdate(Context.Get(), Ciphertext.GetData(), &OutLength, Plaintext.GetData(), Plaintext.Num());
	if (UpdateResult != 1)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Encrypt_AES_256_ECB: EVP_EncryptUpdate failed."));
		return TArray<uint8>();
	}

	int FinalizeLength = 0;
	const int FinalizeResult = EVP_EncryptFinal_ex(Context.Get(), Ciphertext.GetData() + OutLength, &FinalizeLength);
	if (FinalizeResult != 1)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Encrypt_AES_256_ECB: EVP_EncryptFinal_ex failed."));
		return TArray<uint8>();
	}

	Ciphertext.SetNum(OutLength + FinalizeLength);

	OutResult = EPlatformCryptoResult::Success;
	return Ciphertext;
}

TArray<uint8> FEncryptionContextOpenSSL::Decrypt_AES_256_ECB(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("OpenSSL AES256 Decrypt"), STAT_OpenSSL_AES_Decrypt, STATGROUP_PlatformCrypto);

	OutResult = EPlatformCryptoResult::Failure;

	if (Key.Num() != AES256_KeySizeInBytes)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Decrypt_AES_256_ECB: Key size %d is not the expected size %d."), Key.Num(), AES256_KeySizeInBytes);
		return TArray<uint8>();
	}

	FScopedEVPContext Context;
	if (Context.Get() == nullptr)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Decrypt_AES_256_ECB: failed to create EVP context."));
		return TArray<uint8>();
	}

	const int InitResult = EVP_DecryptInit_ex(Context.Get(), EVP_aes_256_ecb(), nullptr, Key.GetData(), nullptr);
	if (InitResult != 1)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Decrypt_AES_256_ECB: EVP_DecryptInit_ex failed."));
		return TArray<uint8>();
	}

	TArray<uint8> Plaintext;
	Plaintext.SetNumUninitialized(Ciphertext.Num());

	int OutLength = 0;
	const int UpdateResult = EVP_DecryptUpdate(Context.Get(), Plaintext.GetData(), &OutLength, Ciphertext.GetData(), Ciphertext.Num());
	if (UpdateResult != 1)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Decrypt_AES_256_ECB: EVP_DecryptUpdate failed."));
		return TArray<uint8>();
	}

	int FinalizeLength = 0;
	const int FinalizeResult = EVP_DecryptFinal_ex(Context.Get(), Plaintext.GetData() + OutLength, &FinalizeLength);
	if (FinalizeResult != 1)
	{
		UE_LOG(LogPlatformCryptoOpenSSL, Warning, TEXT("FEncryptionContextOpenSSL::Decrypt_AES_256_ECB: EVP_DecryptFinal_ex failed."));
		return TArray<uint8>();
	}

	Plaintext.SetNum(OutLength + FinalizeLength);

	OutResult = EPlatformCryptoResult::Success;
	return Plaintext;
}
