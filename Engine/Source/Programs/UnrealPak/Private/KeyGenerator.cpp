// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KeyGenerator.h"
#include "UnrealPak.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "BigInt.h"
#include "TaskGraphInterfaces.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Primes.inl"

// Global constants
namespace
{
	const TEncryptionInt Two(2);
	const TEncryptionInt IterationStep(1000);
	TArray<TEncryptionInt> PrimeLookupTable;
}

/**
 * A thread that finds factors in the given range
 */
class FPrimeCheckRunnable : public FRunnable
{
	/** Flag indicating if a factor has been found. Shared across multiple threads. */
	FThreadSafeCounter&	FoundFactor;
	/** Candidate for a prime number */
	TEncryptionInt PotentialPrime;
	/** Start of a range to check for factors */
	TEncryptionInt InitialValue;
	/** End of a range to check for factors */
	TEncryptionInt MaxValue;
	/** This thread */
	FRunnableThread* Thread;

public:

	FPrimeCheckRunnable(FThreadSafeCounter& InFoundFactor, TEncryptionInt Candidate, TEncryptionInt InInitialValue, TEncryptionInt InMaxValue)
		: FoundFactor(InFoundFactor)
		, PotentialPrime(Candidate)
		, InitialValue(InInitialValue)
		, MaxValue(InMaxValue)
	{
		// Must be odd number
		check(!(Candidate & TEncryptionInt::One).IsZero());
		Thread = FRunnableThread::Create(this, TEXT("FPrimeCheckRunnable"));
	}

	virtual ~FPrimeCheckRunnable()
	{
		delete Thread;
		Thread = NULL;
	}

	FRunnableThread* GetThread()
	{
		return Thread;
	}

	// Start FRunnable interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override
	{
		TEncryptionInt Remainder;
		int32 FactorCheckTimer = 0;
		for (TEncryptionInt Factor = InitialValue; InitialValue <= MaxValue; Factor += Two)
		{
			TEncryptionInt Dividend(PotentialPrime);
			Dividend.DivideWithRemainder(Factor, Remainder);
			if (Remainder.IsZero())
			{
				FoundFactor.Increment();
				break;
			}
			// Don't check the FoundFactor too often
			FactorCheckTimer++;
			if (FactorCheckTimer >= 100)
			{
				FactorCheckTimer = 0;
				if (FoundFactor.GetValue() != 0)
				{
					// Another thread found a factor
					break;
				}
			}
		}
		return 0;
	}
	// End FRunnable interface

};

/**
 * Checks if the value is a prime number.
 */
bool IsPrime(const TEncryptionInt& InValue, bool bUseTasks)
{
	// 2 is but we don't care about small numbers here.
	if ((InValue & TEncryptionInt::One) == 0)
	{
		return false;
	}

	TEncryptionInt Remainder;

	// Check against known prime factors
	int32 Index;
	for (Index = 0; Index < PrimeLookupTable.Num() && PrimeLookupTable[Index] < InValue; ++Index)
	{
		TEncryptionInt Dividend(InValue);
		Dividend.DivideWithRemainder(PrimeLookupTable[Index], Remainder);
		if (Remainder.IsZero())
		{
			return false;
		}
	}
	// This means the number is smaller than one of the primes in the prime table and it has no factors
	if (Index < PrimeLookupTable.Num())
	{
		return true;
	}

	// Brute force, check all odd numbers > MaxKnownPrime < sqrt(Number)
	TEncryptionInt MaxFactorValue(InValue);
	MaxFactorValue.Sqrt();
	TEncryptionInt Factor(PrimeLookupTable[PrimeLookupTable.Num() - 1] + Two); 
	if (bUseTasks)
	{
		// Mutithreaded path. Split the range we look for factors over multiple threads. If one thread finds a factor
		// we stop and reject this number.
		// The worst case is when we actually have a prime number.
		UE_LOG(LogPakFile, Display, TEXT("Detected potentially prime number %s. This may take a while..."), *InValue.ToString());

		const int32 TaskCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
		TArray<FPrimeCheckRunnable*> Tasks;
		Tasks.Reserve(TaskCount);
		FThreadSafeCounter FoundFactors(0);

		TEncryptionInt Range(MaxFactorValue - Factor);
		Range /= TaskCount;

		// Spawn threads
		for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
		{
			TEncryptionInt MaxValue(Factor + Range);
			Tasks.Add(new FPrimeCheckRunnable(FoundFactors, InValue, Factor, MaxValue)); 
			Factor = MaxValue;
			if ((Factor & TEncryptionInt::One) == 0)
			{
				++Factor;
			}
		}
		// Wait for all threads to complete
		for (int32 TaskIndex = 0; TaskIndex < Tasks.Num(); ++TaskIndex)
		{
			Tasks[TaskIndex]->GetThread()->WaitForCompletion();
			delete Tasks[TaskIndex];
		}

		if (FoundFactors.GetValue() > 0)
		{
			UE_LOG(LogPakFile, Display, TEXT("%s is not prime."), *InValue.ToString());
			return false;
		}
		else
		{
			UE_LOG(LogPakFile, Display, TEXT("%s is prime!"), *InValue.ToString());
		}
	}
	else
	{		
		// Single threaded path (used for generating prime table)
		while (Factor < MaxFactorValue)
		{
			TEncryptionInt Dividend(InValue);
			Dividend.DivideWithRemainder(Factor, Remainder);
			if (Remainder.IsZero())
			{
				return false;
			}
			Factor += Two;
		}
	}
	return true;
}

/**
 * Generate two random prime numbers
 */
void GeneratePrimeNumbers(TEncryptionInt& P, TEncryptionInt& Q)
{
	// Generate a random odd number
	FRandomStream Rand((int32)(FDateTime::Now().GetTicks() % (int64)MAX_int32));
	uint32 RandBits[256/32] = 
	{ 
		0xffffffff, 
		(uint32)Rand.RandRange(0, MAX_int32 - 1), 
		(uint32)Rand.RandRange(0, MAX_int32 - 1), 
		0, //(uint32)Rand.RandRange(0, MAX_int32 - 1) | 0xa0ff0000, 
		0, 0, 0, 0 
	};
	TEncryptionInt InitialValue(RandBits);

	// We need two primes
	TArray<TEncryptionInt> DiscoveredPrimes;

	int64 IterationCounter = 0;
	double TimeAccumulator = 0.0;
	const double StartTime = FPlatformTime::Seconds();
	do
	{		
		if (IterationCounter == 10)
		{
			IterationCounter = 0;
			TimeAccumulator = 0.0;
		}
		IterationCounter++;

		const double IterationStartTime = FPlatformTime::Seconds();

		TEncryptionInt MinValue(InitialValue - IterationStep);
		while (InitialValue >= MinValue && DiscoveredPrimes.Num() < 2)
		{
			if (IsPrime(InitialValue, true))
			{
				DiscoveredPrimes.Add(InitialValue);
			}
			InitialValue -= Two;
		}
	}
	while (DiscoveredPrimes.Num() < 2);

	UE_LOG(LogPakFile, Display, TEXT("Generated prime numbers in %.2lfs."), FPlatformTime::Seconds() - StartTime);
	P = DiscoveredPrimes[0];
	Q = DiscoveredPrimes[1];
	UE_LOG(LogPakFile, Display, TEXT("P=%s"), *P.ToString());
	UE_LOG(LogPakFile, Display, TEXT("Q=%s"), *Q.ToString());
}

/**
 * Lookup table generation - fill it with precompile primes
 */
void FillPrimeLookupTableWithPrecompiledNumbers()
{
	const int32 PrimeTableLength = ARRAY_COUNT(PrimeTable);
	PrimeLookupTable.Reserve(PrimeTableLength * PrimeTableLength);
	for (int32 Index = 0; Index < PrimeTableLength; ++Index)
	{
		PrimeLookupTable.Add(PrimeTable[Index]);
	}
}

void GeneratePrimeNumberTable(int64 MaxValue, const TCHAR* Filename)
{
	FillPrimeLookupTableWithPrecompiledNumbers();

	UE_LOG(LogPakFile, Display, TEXT("Generating prime number table <= %lld: %s."), MaxValue, Filename);

	FString PrimeTableString(TEXT("// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.\nTEncryptionInt PrimeTable[] = \n{\n\t2, "));
	int64 PrimeCount = 1;
	const double StartTime = FPlatformTime::Seconds();
	for (int64 SmallNumber = 3; SmallNumber <= MaxValue; SmallNumber += 2)
	{
		if (IsPrime(SmallNumber, false))
		{
			PrimeTableString += FString::Printf(TEXT("%lld, "), SmallNumber);
			PrimeCount++;
			if ((PrimeCount % 10) == 0)
			{
				PrimeTableString += TEXT("\n\t");
			}
		}
	}
	PrimeTableString += TEXT("\n};\n");
	UE_LOG(LogPakFile, Display, TEXT("Generated %lld primes in %.4lfs."), PrimeCount, FPlatformTime::Seconds() - StartTime);
	FFileHelper::SaveStringToFile(PrimeTableString, Filename);
}


/**
 * Multithreaded prime number generation (for the prime table)
 * Finds prime numbers in the given range.
 */
class FPrimeFinderRunnable : public FRunnable
{
	/** Range start */
	TEncryptionInt MinValue;
	/** Range end */
	TEncryptionInt MaxValue;
	/** This thread */
	FRunnableThread* Thread;
	/** All primes found in the given range */
	TArray<TEncryptionInt> FoundPrimes;

public:

	FPrimeFinderRunnable(TEncryptionInt InMinValue, TEncryptionInt InMaxValue)
		: MinValue(InMinValue)
		, MaxValue(InMaxValue)
	{
		// Must be an odd number
		check(!(MinValue & TEncryptionInt::One).IsZero());
		Thread = FRunnableThread::Create(this, TEXT("FPrimeFinderRunnable"));
	}

	virtual ~FPrimeFinderRunnable()
	{
		delete Thread;
		Thread = NULL;
	}

	FRunnableThread* GetThread()
	{
		return Thread;
	}

	const TArray<TEncryptionInt>& GetFoundPrimes() const
	{
		return FoundPrimes;
	}

	// Start FRunnable interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override
	{
		TEncryptionInt Remainder;
		int32 FactorCheckTimer = 0;
		for (TEncryptionInt Candidate = MinValue; Candidate <= MaxValue; Candidate += Two)
		{
			if (IsPrime(Candidate, false))
			{
				FoundPrimes.Add(Candidate);
			}
		}
		return 0;
	}
	// End FRunnable interface

};

/**
 * Generates a lookup table in runtime.
 * This is a superset of precompiled prime table and primes generated on startup.
 * The reason for this is that the precompiled table can't be too big because it affects
 * the compile times and we don't usually use UnrealPak for prime number generation anyway.
 */
void GeneratePrimeNumberLookupTable()
{	
	const int32 PrimeTableLength = ARRAY_COUNT(PrimeTable);
	UE_LOG(LogPakFile, Display, TEXT("Generating prime number lookup table (max size: %d)."), PrimeTableLength * PrimeTableLength);
	const double StartTime = FPlatformTime::Seconds();

	FillPrimeLookupTableWithPrecompiledNumbers();

	TEncryptionInt MinPrimeValue(PrimeLookupTable[PrimeLookupTable.Num() - 1] + Two);
	TEncryptionInt MaxPrimeValue(MinPrimeValue);
	MaxPrimeValue *= 100;

	const int32 TaskCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	TArray<FPrimeFinderRunnable*> Tasks;
	Tasks.Reserve(TaskCount);

	TEncryptionInt Range(MaxPrimeValue - MinPrimeValue);
	Range /= TaskCount;

	for (int32 TaskIndex = 0; TaskIndex < TaskCount; ++TaskIndex)
	{
		TEncryptionInt MaxValue(MinPrimeValue + Range);
		Tasks.Add(new FPrimeFinderRunnable(MinPrimeValue, MaxValue)); 
		MinPrimeValue = MaxValue;
		if ((MinPrimeValue & TEncryptionInt::One) == 0)
		{
			++MinPrimeValue;
		}
	}

	TArray<TEncryptionInt> NewPrimes;
	for (int32 TaskIndex = 0; TaskIndex < Tasks.Num(); ++TaskIndex)
	{
		Tasks[TaskIndex]->GetThread()->WaitForCompletion();
		NewPrimes.Append(Tasks[TaskIndex]->GetFoundPrimes());
		delete Tasks[TaskIndex];
	}
	PrimeLookupTable.Append(NewPrimes);

	UE_LOG(LogPakFile, Display, TEXT("Generated %d primes in %.4lfs."), PrimeLookupTable.Num(), FPlatformTime::Seconds() - StartTime);
}

bool GenerateKeys(const TCHAR* KeyFilename)
{
	UE_LOG(LogPakFile, Display, TEXT("Generating keys %s."), KeyFilename);

	GeneratePrimeNumberLookupTable();

	TEncryptionInt P;
	TEncryptionInt Q;

	FString CmdLineP;
	FString CmdLineQ;
	FParse::Value(FCommandLine::Get(), TEXT("P="), CmdLineP);
	FParse::Value(FCommandLine::Get(), TEXT("Q="), CmdLineQ);

	const bool bNoVerifyPrimes = FParse::Param(FCommandLine::Get(), TEXT("NoVerifyPrimes"));

	P.Parse(CmdLineP);
	Q.Parse(CmdLineQ);

	// Check if we have valid primes in the command line.
	// @todo: IsPrime check should probably go when we start to use big primes
	bool bGeneratePrimes = !(P > Two && Q > Two);
	if (!bGeneratePrimes && !bNoVerifyPrimes)
	{
		if (!IsPrime(P, false))
		{
			UE_LOG(LogPakFile, Warning, TEXT("P=%s is not prime!"), *CmdLineP);
			bGeneratePrimes = true;
		}
		if (!IsPrime(Q, false))
		{
			UE_LOG(LogPakFile, Warning, TEXT("Q=%s is not prime!"), *CmdLineQ);
			bGeneratePrimes = true;
		}
	}

	if (bGeneratePrimes)
	{
		// Generate random prime numbers
		UE_LOG(LogPakFile, Display, TEXT("Generating prime numbers..."));
		GeneratePrimeNumbers(P, Q);
	}
	else
	{
		// Use predefined primes
		UE_LOG(LogPakFile, Display, TEXT("Using predefined values to generate keys."));
	}

	// Generate key pair
	UE_LOG(LogPakFile, Display, TEXT("Generating key pair..."));
	FKeyPair Keys;
	FEncryption::GenerateKeyPair(P, Q, Keys.PublicKey, Keys.PrivateKey);

	if (TestKeys(Keys))
	{
		return SaveKeysToFile(Keys, KeyFilename);
	}
	else
	{
		return false;
	}
}

bool SaveKeysToFile(const FKeyPair& Keys, const TCHAR* KeyFilename)
{
	UE_LOG(LogPakFile, Display, TEXT("Saving key pair in %s"), KeyFilename);	
	FString KeyFileContents = FString::Printf(TEXT("%s\n%s\n%s"), *Keys.PrivateKey.Exponent.ToString(), *Keys.PrivateKey.Modulus.ToString(), *Keys.PublicKey.Exponent.ToString());
	return FFileHelper::SaveStringToFile(KeyFileContents, KeyFilename);
}

bool ReadKeysFromFile(const TCHAR* KeyFilename, FKeyPair& OutKeys)
{
	bool bResult = false;
	UE_LOG(LogPakFile, Display, TEXT("Loading key pair from %s"), KeyFilename);
	FString KeyFileContents;
	if (FFileHelper::LoadFileToString(KeyFileContents, KeyFilename))
	{
		TArray<FString> KeyValues;
		KeyFileContents.ParseIntoArrayWS(KeyValues);
		if (KeyValues.Num() != 3)
		{
			UE_LOG(LogPakFile, Error, TEXT("Expecting 3 values in %s, got %d."), KeyFilename, KeyValues.Num());
		}
		else
		{
			OutKeys.PrivateKey.Exponent.Parse(KeyValues[0]);
			OutKeys.PrivateKey.Modulus.Parse(KeyValues[1]);
			OutKeys.PublicKey.Exponent.Parse(KeyValues[2]);
			OutKeys.PublicKey.Modulus = OutKeys.PrivateKey.Modulus;
			bResult = true;
		}
	}
	else
	{
		UE_LOG(LogPakFile, Error, TEXT("Failed to load key pair from %s"), KeyFilename);
	}

	return bResult;
}


bool TestKeys(FKeyPair& Pair)
{
	UE_LOG(LogPakFile, Display, TEXT("Testing signature keys."));

	// Just some random values
	static TEncryptionInt TestData[] =
	{
		11,
		253,
		128,
		234,
		56,
		89,
		34,
		179,
		29,
		1024,
		(int64)(MAX_int32),
		(int64)(MAX_uint32) - 1
	};

	for (int32 TestIndex = 0; TestIndex < ARRAY_COUNT(TestData); ++TestIndex)
	{
		TEncryptionInt EncryptedData = FEncryption::ModularPow(TestData[TestIndex], Pair.PrivateKey.Exponent, Pair.PrivateKey.Modulus);
		TEncryptionInt DecryptedData = FEncryption::ModularPow(EncryptedData, Pair.PublicKey.Exponent, Pair.PublicKey.Modulus);
		if (TestData[TestIndex] != DecryptedData)
		{
			UE_LOG(LogPakFile, Error, TEXT("Keys do not properly encrypt/decrypt data (failed test with %lld)"), TestData[TestIndex].ToInt());
			return false;
		}
	}

	UE_LOG(LogPakFile, Display, TEXT("Signature keys check completed successfuly."));

	return true;
}