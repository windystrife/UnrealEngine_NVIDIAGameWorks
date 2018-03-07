// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BigInt.h"

/**
 * Encryption keys: public and private
 */
struct FKeyPair
{
	/** Public decryption key */
	FEncryptionKey PublicKey;
	/** Private encryption key */
	FEncryptionKey PrivateKey;

	friend FArchive& operator<<(FArchive& Ar, FKeyPair& Pair)
	{
		Ar << Pair.PublicKey.Exponent;
		Ar << Pair.PublicKey.Modulus;
		Ar << Pair.PrivateKey.Exponent;
		Ar << Pair.PrivateKey.Modulus;
		return Ar;
	}
};

/**
 * Generates a prime number table where the max prime number value is less than MaxValue
 */
void GeneratePrimeNumberTable(int64 MaxValue, const TCHAR* Filename);
/**
 * Generates a file with the encryption keys
 */
bool GenerateKeys(const TCHAR* KeyFilename);

bool SaveKeysToFile(const FKeyPair& Keys, const TCHAR* KeyFilename);

bool ReadKeysFromFile(const TCHAR* KeyFilename, FKeyPair& OutKeys);

bool TestKeys(FKeyPair& Pair);