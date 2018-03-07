// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArrayView.h"
#include "PlatformCryptoTypes.h"

/**
 * Interface to certain cryptographic algorithms, using OpenSSL to implement them.
 */
class PLATFORMCRYPTOOPENSSL_API FEncryptionContextOpenSSL
{

public:

	TArray<uint8> Encrypt_AES_256_ECB(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);

	TArray<uint8> Decrypt_AES_256_ECB(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);
};

typedef FEncryptionContextOpenSSL FEncryptionContext;
