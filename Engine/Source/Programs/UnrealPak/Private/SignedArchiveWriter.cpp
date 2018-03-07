// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SignedArchiveWriter.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "HAL/FileManager.h"

FSignedArchiveWriter::FSignedArchiveWriter(FArchive& InPak, const FString& InPakFilename, const FEncryptionKey& InPublicKey, const FEncryptionKey& InPrivateKey)
: BufferArchive(Buffer)
	, PakWriter(InPak)
	, PakSignaturesFilename(FPaths::ChangeExtension(InPakFilename, TEXT("sig")))
	, SizeOnDisk(0)
	, PakSize(0)
	, PublicKey(InPublicKey)
	, PrivateKey(InPrivateKey)
{
	Buffer.Reserve(FPakInfo::MaxChunkDataSize);
}

FSignedArchiveWriter::~FSignedArchiveWriter()
{
	if (BufferArchive.Tell() > 0)
	{
		SerializeBufferAndSign();
	}
	delete &PakWriter;
}

void FSignedArchiveWriter::SerializeBufferAndSign()
{
	// Compute a hash for this buffer data
	ChunkHashes.Add(ComputePakChunkHash(&Buffer[0], Buffer.Num()));

	// Flush the buffer
	PakWriter.Serialize(&Buffer[0], Buffer.Num());
	BufferArchive.Seek(0);
	Buffer.Empty(FPakInfo::MaxChunkDataSize);
}

bool FSignedArchiveWriter::Close()
{
	if (BufferArchive.Tell() > 0)
	{
		SerializeBufferAndSign();
	}

	FEncryptedSignature EncryptedMasterHash;
	FDecryptedSignature DecryptedMasterHash;
	DecryptedMasterHash.Data = ComputePakChunkHash((const uint8*)&ChunkHashes[0], ChunkHashes.Num() * sizeof(TPakChunkHash));
	FEncryption::EncryptSignature(DecryptedMasterHash, EncryptedMasterHash, PrivateKey);

	FArchive* SignatureWriter = IFileManager::Get().CreateFileWriter(*PakSignaturesFilename);
	*SignatureWriter << EncryptedMasterHash;
	*SignatureWriter << ChunkHashes;
	delete SignatureWriter;

	return FArchive::Close();
}

void FSignedArchiveWriter::Serialize(void* Data, int64 Length)
{
	// Serialize data to a buffer. When the max buffer size is reached, the buffer is signed and
	// serialized to disk with its signature
	uint8* DataToWrite = (uint8*)Data;
	int64 RemainingSize = Length;
	while (RemainingSize > 0)
	{
		int64 BufferPos = BufferArchive.Tell();
		int64 SizeToWrite = RemainingSize;
		if (BufferPos + SizeToWrite > FPakInfo::MaxChunkDataSize)
		{
			SizeToWrite = FPakInfo::MaxChunkDataSize - BufferPos;
		}

		BufferArchive.Serialize(DataToWrite, SizeToWrite);
		if (BufferArchive.Tell() == FPakInfo::MaxChunkDataSize)
		{
			SerializeBufferAndSign();
		}
			
		SizeOnDisk += SizeToWrite;
		PakSize += SizeToWrite;

		RemainingSize -= SizeToWrite;
		DataToWrite += SizeToWrite;
	}
}

int64 FSignedArchiveWriter::Tell()
{
	return PakSize;
}

int64 FSignedArchiveWriter::TotalSize()
{
	return PakSize;
}

void FSignedArchiveWriter::Seek(int64 InPos)
{
	UE_LOG(LogPakFile, Fatal, TEXT("Seek is not supported in FSignedArchiveWriter."));
}

/**
 * Useful code for testing the encryption methods
 */
void TestEncryption()
{
	FEncryptionKey PublicKey;
	FEncryptionKey PrivateKey;
	TEncryptionInt P(TEXT("0x21443BD2DD63E995403"));
	TEncryptionInt Q(TEXT("0x28CBB6E5749AC65749"));
	FEncryption::GenerateKeyPair(P, Q, PublicKey, PrivateKey);

	// Generate random data
	const int32 DataSize = 1024;
	uint8* Data = (uint8*)FMemory::Malloc(DataSize);
	for (int32 Index = 0; Index < DataSize; ++Index)
	{
		Data[Index] = (uint8)(Index % 255);
	}

	// Generate signature
	FDecryptedSignature OriginalSignature;
	FEncryptedSignature EncryptedSignature;
	FDecryptedSignature DecryptedSignature;
	OriginalSignature.Data = FCrc::MemCrc32(Data, DataSize);

	// Encrypt signature with the private key
	FEncryption::EncryptSignature(OriginalSignature, EncryptedSignature, PrivateKey);
	FEncryption::DecryptSignature(EncryptedSignature, DecryptedSignature, PublicKey);

	// Check
	if (OriginalSignature == DecryptedSignature)
	{
		UE_LOG(LogPakFile, Display, TEXT("Keys match"));
	}
	else
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Keys mismatched!"));
	}

	double OverallTime = 0.0;
	double OverallNumTests = 0.0;
	for (int32 TestIndex = 0; TestIndex < 10; ++TestIndex)
	{
		static const int64 NumTests = 500;
		double Timer = FPlatformTime::Seconds();
		{
			for (int64 i = 0; i < NumTests; ++i)
			{
				FEncryption::DecryptSignature(EncryptedSignature, DecryptedSignature, PublicKey);
			}
		}
		Timer = FPlatformTime::Seconds() - Timer;
		OverallTime += Timer;
		OverallNumTests += (double)NumTests;
		UE_LOG(LogPakFile, Display, TEXT("%i signatures decrypted in %.4fs, Avg = %.4fs, OverallAvg = %.4fs"), NumTests, Timer, Timer / (double)NumTests, OverallTime / OverallNumTests);
	}
	
	FMemory::Free(Data);
}