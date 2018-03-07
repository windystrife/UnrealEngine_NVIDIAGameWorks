// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisjointSet
{
public:
			FDisjointSet( const uint32 Size );
	
	void	Union( uint32 x, uint32 y );
	uint32	Find( uint32 i );

	uint32	operator[]( uint32 i ) const	{ return Parents[i]; }

private:
	TArray<	uint32 >	Parents;
};

FDisjointSet::FDisjointSet( const uint32 Size )
{
	Parents.SetNumUninitialized( Size );
	for( uint32 i = 0; i < Size; i++ )
	{
		Parents[i] = i;
	}
}

// Union with splicing
void FDisjointSet::Union( uint32 x, uint32 y )
{
	while( Parents[x] != Parents[y] )
	{
		// Pick larger
		if( Parents[x] < Parents[y] )
		{
			uint32 p = Parents[x];
			Parents[x] = Parents[y];
			if( x == p )
			{
				return;
			}
			x = p;
		}
		else
		{
			uint32 p = Parents[y];
			Parents[y] = Parents[x];
			if( y == p )
			{
				return;
			}
			y = p;
		}
	}
}

// Find with path compression
uint32 FDisjointSet::Find( uint32 i )
{
	// Find root
	uint32 Start = i;
	uint32 Root = Parents[i];
	while( Root != i )
	{
		i = Root;
		Root = Parents[i];
	}

	// Point all nodes on path to root
	i = Start;
	uint32 Parent = Parents[i];
	while( Parent != Root )
	{
		Parents[i] = Root;
		i = Parent;
		Parent = Parents[i];
	}

	return Root;
}
