// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

/*-----------------------------------------------------------------------------
	Coder/decoder base class.
-----------------------------------------------------------------------------*/

DECLARE_LOG_CATEGORY_EXTERN(LogDataCodex, Log, All);

class FCodec
{
public:
	virtual bool Encode( FArchive& In, FArchive& Out )=0;
	virtual bool Decode( FArchive& In, FArchive& Out )=0;
	virtual ~FCodec(){}
};


/*-----------------------------------------------------------------------------
	Burrows-Wheeler inspired data compressor.
-----------------------------------------------------------------------------*/

class FCodecBWT : public FCodec
{
private:
	enum {MAX_BUFFER_SIZE=0x40000}; /* Hand tuning suggests this is an ideal size */
	static uint8* CompressBuffer;
	static int32 CompressLength;
	
	struct FCompareClampedBuffer
	{
		FORCEINLINE bool operator()( const int32& P1, const int32& P2 ) const
		{
			uint8* B1 = CompressBuffer + P1;
			uint8* B2 = CompressBuffer + P2;
			for( int32 Count=CompressLength-FMath::Max(P1,P2); Count>0; Count--,B1++,B2++ )
			{
				if( *B1 < *B2 )
					return true;
				else if( *B1 > *B2 )
					return false;
			}
			return (P1 - P2) < 0 ? true : false;
		}
	};
public:
	bool Encode( FArchive& In, FArchive& Out )
	{
		TArray<uint8> CompressBufferArray;
		TArray<int32> CompressPosition;
		CompressBufferArray.AddUninitialized(MAX_BUFFER_SIZE);
		CompressPosition   .AddUninitialized(MAX_BUFFER_SIZE+1);
		CompressBuffer = CompressBufferArray.GetData();
		int32 i, First=0, Last=0;
		while( !In.AtEnd() )
		{
			CompressLength = FMath::Min<int32>( In.TotalSize()-In.Tell(), MAX_BUFFER_SIZE );
			In.Serialize( CompressBuffer, CompressLength );
			for( i=0; i<CompressLength+1; i++ )
				CompressPosition[i] = i;
			CompressPosition.Sort(FCompareClampedBuffer());
			for( i=0; i<CompressLength+1; i++ )
				if( CompressPosition[i]==1 )
					First = i;
				else if( CompressPosition[i]==0 )
					Last = i;
			Out << CompressLength << First << Last;
			for( i=0; i<CompressLength+1; i++ )
				Out << CompressBuffer[CompressPosition[i]?CompressPosition[i]-1:0];
			//UE_LOG(LogDataCodex, Warning, TEXT("Compression table"));
			//for( i=0; i<CompressLength+1; i++ )
			//	UE_LOG(LogDataCodex, Warning, TEXT("    %03i: %s"),CompressPosition(i)?CompressBuffer[CompressPosition(i)-1]:-1,ANSI_TO_TCHAR((ANSICHAR*)CompressBuffer+CompressPosition(i)));
		}
		return 0;
	}
	bool Decode( FArchive& In, FArchive& Out )
	{
		TArray<uint8> DecompressBuffer;
		TArray<int32> Temp;
		DecompressBuffer.AddUninitialized(MAX_BUFFER_SIZE+1);
		Temp            .AddUninitialized(MAX_BUFFER_SIZE+1);
		int32 DecompressLength, DecompressCount[256+1], RunningTotal[256+1], i, j;
		while( !In.AtEnd() )
		{
			int32 First, Last;
			In << DecompressLength << First << Last;
			check(DecompressLength<=MAX_BUFFER_SIZE+1);
			check(DecompressLength<=In.TotalSize()-In.Tell());
			In.Serialize(DecompressBuffer.GetData(), ++DecompressLength);
			for( i=0; i<257; i++ )
				DecompressCount[ i ]=0;
			for( i=0; i<DecompressLength; i++ )
				DecompressCount[ i!=Last ? DecompressBuffer[i] : 256 ]++;
			int32 Sum = 0;
			for( i=0; i<257; i++ )
			{
				RunningTotal[i] = Sum;
				Sum += DecompressCount[i];
				DecompressCount[i] = 0;
			}
			for( i=0; i<DecompressLength; i++ )
			{
				int32 Index = i!=Last ? DecompressBuffer[i] : 256;
				Temp[RunningTotal[Index] + DecompressCount[Index]++] = i;
			}
			for( i=First,j=0 ; j<DecompressLength-1; i=Temp[i],j++ )
				Out << DecompressBuffer[i];
		}
		return 1;
	}
};

/*-----------------------------------------------------------------------------
	RLE compressor.
-----------------------------------------------------------------------------*/

class FCodecRLE : public FCodec
{
private:
	enum {RLE_LEAD=5};
	void EncodeEmitRun( FArchive& Out, uint8 Char, uint8 Count )
	{
		for( int32 Down=FMath::Min<int32>(Count,RLE_LEAD); Down>0; Down-- )
			Out << Char;
		if( Count>=RLE_LEAD )
			Out << Count;
	}
public:
	bool Encode( FArchive& In, FArchive& Out )
	{
		uint8 PrevChar=0, PrevCount=0, B;
		while( !In.AtEnd() )
		{
			In << B;
			if( B!=PrevChar || PrevCount==255 )
			{
				EncodeEmitRun( Out, PrevChar, PrevCount );
				PrevChar  = B;
				PrevCount = 0;
			}
			PrevCount++;
		}
		EncodeEmitRun( Out, PrevChar, PrevCount );
		return 0;
	}
	bool Decode( FArchive& In, FArchive& Out )
	{
		int32 Count=0;
		uint8 PrevChar=0, B, C;
		while( !In.AtEnd() )
		{
			In << B;
			Out << B;
			if( B!=PrevChar )
			{
				PrevChar = B;
				Count    = 1;
			}
			else if( ++Count==RLE_LEAD )
			{
				In << C;
				check(C>=2);
				while( C-->RLE_LEAD )
					Out << B;
				Count = 0;
			}
		}
		return 1;
	}
};

/*-----------------------------------------------------------------------------
	Huffman codec.
-----------------------------------------------------------------------------*/

class FCodecHuffman : public FCodec
{
private:
	struct FHuffman
	{
		int32 Ch, Count;
		TArray<FHuffman*> Child;
		TArray<uint8> Bits;
		FHuffman( int32 InCh )
		: Ch(InCh), Count(0)
		{
		}
		~FHuffman()
		{
			for( int32 i=0; i<Child.Num(); i++ )
				delete Child[ i ];
		}
		void PrependBit( uint8 B )
		{
			Bits.InsertUninitialized( 0 );
			Bits[0] = B;
			for( int32 i=0; i<Child.Num(); i++ )
				Child[i]->PrependBit( B );
		}
		void WriteTable( FBitWriter& Writer )
		{
			Writer.WriteBit( Child.Num()!=0 );
			if( Child.Num() )
				for( int32 i=0; i<Child.Num(); i++ )
					Child[i]->WriteTable( Writer );
			else
			{
				uint8 B = Ch;
				Writer << B;
			}
		}
		void ReadTable( FBitReader& Reader )
		{
			if( Reader.ReadBit() )
			{
				Child.AddUninitialized( 2 );
				for( int32 i=0; i<Child.Num(); i++ )
				{
					Child[ i ] = new FHuffman( -1 );
					Child[ i ]->ReadTable( Reader );
				}
			}
			else Ch = Arctor<uint8>( Reader );
		}
	};
	
public:
	bool Encode( FArchive& In, FArchive& Out )
	{
		int64 SavedPos = In.Tell();
		int32 Total=0, i;

		// Compute character frequencies.
		TArray<FHuffman*> Huff;
		Huff.AddUninitialized(256);
		for( i=0; i<256; i++ )
			Huff[i] = new FHuffman(i);
		TArray<FHuffman*> Index = Huff;
		while( !In.AtEnd() )
			Huff[Arctor<uint8>(In)]->Count++, Total++;
		In.Seek( SavedPos );
		Out << Total;

		// Build compression table.
		while( Huff.Num()>1 && Huff.Last()->Count==0 )
			delete Huff.Pop();
		int32 BitCount = Huff.Num()*(8+1);
		while( Huff.Num()>1 )
		{
			FHuffman* Node  = new FHuffman( -1 );
			Node->Child.AddUninitialized( 2 );
			for( i=0; i<Node->Child.Num(); i++ )
			{
				Node->Child[i] = Huff.Pop();
				Node->Child[i]->PrependBit(i);
				Node->Count += Node->Child[i]->Count;
			}
			for( i=0; i<Huff.Num(); i++ )
				if( Huff[i]->Count < Node->Count )
					break;
			Huff.InsertUninitialized( i );
			Huff[ i ] = Node;
			BitCount++;
		}
		FHuffman* Root = Huff.Pop();

		// Calc stats.
		while( !In.AtEnd() )
			BitCount += Index[Arctor<uint8>(In)]->Bits.Num();
		In.Seek( SavedPos );

		// Save table and bitstream.
		FBitWriter Writer( BitCount );
		Root->WriteTable( Writer );
		while( !In.AtEnd() )
		{
			FHuffman* P = Index[Arctor<uint8>(In)];
			for( int32 j=0; j < P->Bits.Num(); j++ )
				Writer.WriteBit( P->Bits[j] );
		}
		check(!Writer.IsError());
		check(Writer.GetNumBits()==BitCount);
		Out.Serialize( Writer.GetData(), Writer.GetNumBytes() );

		// Finish up.
		delete Root;
		return 0;
	}
	bool Decode( FArchive& In, FArchive& Out )
	{
		int32 Total;
		In << Total;
		TArray<uint8> InArray;
		InArray.AddUninitialized( In.TotalSize()-In.Tell() );
		In.Serialize(InArray.GetData(), InArray.Num());
		FBitReader Reader(InArray.GetData(), InArray.Num() * 8);
		FHuffman Root(-1);
		Root.ReadTable( Reader );
		while( Total-- > 0 )
		{
			check(!Reader.AtEnd());
			FHuffman* Node = &Root;
			while( Node->Ch==-1 )
				Node = Node->Child[ Reader.ReadBit() ];
			uint8 B = Node->Ch;
			Out << B;
		}
		return 1;
	}
};

/*-----------------------------------------------------------------------------
	Move-to-front encoder.
-----------------------------------------------------------------------------*/

class FCodecMTF : public FCodec
{
public:
	bool Encode( FArchive& In, FArchive& Out )
	{
		uint8 List[256], B, C;
		int32 i;
		for( i=0; i<256; i++ )
			List[i] = i;
		while( !In.AtEnd() )
		{
			In << B;
			for( i=0; i<256; i++ )
				if( List[i]==B )
					break;
			check(i<256);
			C = i;
			Out << C;
			int32 NewPos=0;
			for( ; i>NewPos; i-- )
				List[i]=List[i-1];
			List[NewPos] = B;
		}
		return 0;
	}
	bool Decode( FArchive& In, FArchive& Out )
	{
		uint8 List[256], B, C;
		int32 i;
		for( i=0; i<256; i++ )
			List[i] = i;
		while( !In.AtEnd() )
		{
			In << B;
			C = List[B];
			Out << C;
			int32 NewPos=0;
			for( i=B; i>NewPos; i-- )
				List[i]=List[i-1];
			List[NewPos] = C;
		}
		return 1;
	}
};

/*-----------------------------------------------------------------------------
	General compressor codec.
-----------------------------------------------------------------------------*/

class FCodecFull : public FCodec
{
private:
	TArray<FCodec*> Codecs;
	void Code( FArchive& In, FArchive& Out, int32 Step, int32 First, bool (FCodec::*Func)(FArchive&,FArchive&) )
	{
		TArray<uint8> InData, OutData;
		for( int32 i=0; i<Codecs.Num(); i++ )
		{
			FMemoryReader Reader(InData);
			FMemoryWriter Writer(OutData);
			(Codecs[First + Step*i]->*Func)( *(i ? &Reader : &In), *(i<Codecs.Num()-1 ? &Writer : &Out) );
			if( i<Codecs.Num()-1 )
			{
				InData = OutData;
				OutData.Empty();
			}
		}
	}
public:
	bool Encode( FArchive& In, FArchive& Out )
	{
		Code( In, Out, 1, 0, &FCodec::Encode );
		return 0;
	}
	bool Decode( FArchive& In, FArchive& Out )
	{
		Code( In, Out, -1, Codecs.Num()-1, &FCodec::Decode );
		return 1;
	}
	void AddCodec( FCodec* InCodec )
	{
		Codecs.Add( InCodec );
	}
	~FCodecFull()
	{
		for( int32 i=0; i<Codecs.Num(); i++ )
			delete Codecs[ i ];
	}
};

