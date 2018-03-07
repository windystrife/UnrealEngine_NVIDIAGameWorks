// Copyright (C) 2009 Nine Realms, Inc
//

#pragma once

#include "CoreMinimal.h"

#define SIMP_CACHE	1

#include "MeshSimplifyElements.h"
#include "Quadric.h"
#include "Containers/HashTable.h"
#include "Containers/BinaryHeap.h"
//#include "Cache.h"

template< typename T, uint32 NumAttributes >
class TMeshSimplifier
{
	typedef typename TSimpVert<T>::TriIterator	TriIterator;
	typedef TQuadricAttr< NumAttributes >		QuadricType;

public:
						TMeshSimplifier( const T* Verts, uint32 NumVerts, const uint32* Indexes, uint32 NumIndexes );
						~TMeshSimplifier();

	void				SetAttributeWeights( const float* weights );
	void				SetBoundaryLocked();

	void				InitCosts();

	float				SimplifyMesh( float maxErrorLimit, int minTris );

	int					GetNumVerts() const { return numVerts; }
	int					GetNumTris() const { return numTris; }

	void				OutputMesh( T* Verts, uint32* Indexes );

protected:
	void				LockVertFlags( uint32 flag );
	void				UnlockVertFlags( uint32 flag );

	void				LockTriFlags( uint32 flag );
	void				UnlockTriFlags( uint32 flag );

	void				GatherUpdates( TSimpVert<T>* v );

	void				GroupVerts();
	void				GroupEdges();

	void				InitVert( TSimpVert<T>* v );

	QuadricType			GetQuadric( TSimpVert<T>* v );
	FQuadric			GetEdgeQuadric( TSimpVert<T>* v );

	// TODO move away from pointers and remove these functions
	uint32				GetVertIndex( const TSimpVert<T>* vert ) const;
	uint32				GetTriIndex( const TSimpTri<T>* tri ) const;
	uint32				GetEdgeIndex( const TSimpEdge<T>* edge ) const;

	uint32				HashPoint( const FVector& p ) const;
	uint32				HashEdge( const TSimpVert<T>* u, const TSimpVert<T>* v ) const;
	TSimpEdge<T>*		FindEdge( const TSimpVert<T>* u, const TSimpVert<T>* v );
	
	void				RemoveEdge( TSimpEdge<T>* edge );
	void				ReplaceEdgeVert( const TSimpVert<T>* oldV, const TSimpVert<T>* otherV, TSimpVert<T>* newV );
	void				CollapseEdgeVert( const TSimpVert<T>* oldV, const TSimpVert<T>* otherV, TSimpVert<T>* newV );

	float				ComputeNewVerts( TSimpEdge<T>* edge, TArray< T, TInlineAllocator<16> >& newVerts );
	float				ComputeEdgeCollapseCost( TSimpEdge<T>* edge );
	void				Collapse( TSimpEdge<T>* edge );

	void				UpdateTris();
	void				UpdateVerts();
	void				UpdateEdges();

	uint32				vertFlagLock;
	uint32				triFlagLock;
	
	float				attributeWeights[ NumAttributes ];
	
	TSimpVert<T>*		sVerts;
	TSimpTri<T>*		sTris;

	int					numSVerts;
	int					numSTris;

	int					numVerts;
	int					numTris;

	TArray< TSimpEdge<T> >	edges;
	FHashTable				edgeHash;
	FBinaryHeap<float>		edgeHeap;

	TArray< TSimpVert<T>* >	updateVerts;
	TArray< TSimpTri<T>* >	updateTris;
	TArray< TSimpEdge<T>* >	updateEdges;

#if SIMP_CACHE
	TBitArray<>				VertQuadricsValid;
	TArray< QuadricType >	VertQuadrics;

	TBitArray<>				TriQuadricsValid;
	TArray< QuadricType >	TriQuadrics;

	TBitArray<>				EdgeQuadricsValid;
	TArray< FQuadric >		EdgeQuadrics;
#endif
};


//=============
// TMeshSimplifier
//=============
template< typename T, uint32 NumAttributes >
TMeshSimplifier<T, NumAttributes>::TMeshSimplifier( const T* Verts, uint32 NumVerts, const uint32* Indexes, uint32 NumIndexes )
	: edgeHash( 1 << FMath::Min( 16u, FMath::FloorLog2( NumVerts ) ) )
{
	vertFlagLock = 0;
	triFlagLock = 0;

	for( uint32 i = 0; i < NumAttributes; i++ )
	{
		attributeWeights[i] = 1.0f;
	}

	numSVerts = NumVerts;
	numSTris = NumIndexes / 3;

	this->numVerts = numSVerts;
	this->numTris = numSTris;

	sVerts = new TSimpVert<T>[ numSVerts ];
	sTris = new TSimpTri<T>[ numSTris ];

#if SIMP_CACHE
	VertQuadricsValid.Init( false, numSVerts );
	VertQuadrics.SetNum( numSVerts );

	TriQuadricsValid.Init( false, numSTris );
	TriQuadrics.SetNum( numSTris );

	EdgeQuadricsValid.Init( false, numSVerts );
	EdgeQuadrics.SetNum( numSVerts );
#endif

	for( int i = 0; i < numSVerts; i++ )
	{
		sVerts[i].vert = Verts[i];
	}
	
	for( int i = 0; i < numSTris; i++ )
	{
		for( int j = 0; j < 3; j++ )
		{
			sTris[i].verts[j] = &sVerts[ Indexes[3*i+j] ];
			sTris[i].verts[j]->adjTris.Add( &sTris[i] );
		}
	}

	GroupVerts();

	int maxEdgeSize = FMath::Min( 3 * numSTris, 3 * numSVerts - 6 );
	edges.Empty( maxEdgeSize );
	for( int i = 0; i < numSVerts; i++ )
	{
		InitVert( &sVerts[i] );
	}

	// Guessed wrong on num edges. Array was resized so fix up pointers.
	if( edges.Num() > maxEdgeSize )
	{
		for( int i = 0; i < edges.Num(); i++ )
		{
			TSimpEdge<T>& edge = edges[i];
			edge.next = &edge;
			edge.prev = &edge;
		}
	}

	GroupEdges();

	edgeHash.Resize( edges.Num() );
	for( int i = 0; i < edges.Num(); i++ )
	{
		edgeHash.Add( HashEdge( edges[i].v0, edges[i].v1 ), i );
	}

	edgeHeap.Resize( edges.Num(), edges.Num() );
}

template< typename T, uint32 NumAttributes >
TMeshSimplifier<T, NumAttributes>::~TMeshSimplifier()
{
	delete[] sVerts;
	delete[] sTris;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::SetAttributeWeights( const float* weights )
{
	for( uint32 i = 0; i < NumAttributes; i++ )
	{
		attributeWeights[i] = weights[i];
	}
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::SetBoundaryLocked()
{
	TArray< TSimpVert<T>*, TInlineAllocator<64> > adjVerts;

	for( int i = 0; i < numSVerts; i++ )
	{
		TSimpVert<T>* v0 = &sVerts[i];
		check( v0->adjTris.Num() > 0 );

		adjVerts.Reset();
		v0->FindAdjacentVertsGroup( adjVerts );

		for( TSimpVert<T>* v1 : adjVerts )
		{
			if( v0 < v1 )
			{
				LockTriFlags( SIMP_MARK1 );

				// set if this edge is boundary
				// find faces that share v0 and v1
				v0->EnableAdjTriFlagsGroup( SIMP_MARK1 );
				v1->DisableAdjTriFlagsGroup( SIMP_MARK1 );

				int faceCount = 0;
				TSimpVert<T>* vert = v0;
				do
				{
					for( TriIterator j = vert->adjTris.Begin(); j != vert->adjTris.End(); ++j )
					{
						TSimpTri<T>* tri = *j;
						faceCount += tri->TestFlags( SIMP_MARK1 ) ? 0 : 1;
					}
					vert = vert->next;
				} while( vert != v0 );

				v0->DisableAdjTriFlagsGroup( SIMP_MARK1 );

				if( faceCount == 1 )
				{
					// only one face on this edge
					v0->EnableFlagsGroup( SIMP_LOCKED );
					v1->EnableFlagsGroup( SIMP_LOCKED );
				}

				UnlockTriFlags( SIMP_MARK1 );
			}
		}
	}
}

// locking functions for nesting safety
template< typename T, uint32 NumAttributes >
FORCEINLINE void TMeshSimplifier<T, NumAttributes>::LockVertFlags( uint32 f )
{
	checkSlow( ( vertFlagLock & f ) == 0 );
	vertFlagLock |= f;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE void TMeshSimplifier<T, NumAttributes>::UnlockVertFlags( uint32 f )
{
	vertFlagLock &= ~f;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE void TMeshSimplifier<T, NumAttributes>::LockTriFlags( uint32 f )
{
	checkSlow( ( triFlagLock & f ) == 0 );
	triFlagLock |= f;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE void TMeshSimplifier<T, NumAttributes>::UnlockTriFlags( uint32 f )
{
	triFlagLock &= ~f;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::InitVert( TSimpVert<T>* v )
{
	check( v->adjTris.Num() > 0 );

	TArray< TSimpVert<T>*, TInlineAllocator<64> > adjVerts;
	v->FindAdjacentVerts( adjVerts );

	TSimpVert<T>* v0 = v;
	for( TSimpVert<T>* v1 : adjVerts )
	{
		if( v0 < v1 )
		{
			check( v0->GetMaterialIndex() == v1->GetMaterialIndex() );
			
			// add edge
			edges.AddDefaulted();
			TSimpEdge<T>& edge = edges.Last();
			edge.v0 = v0;
			edge.v1 = v1;
		}
	}
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::GroupVerts()
{
	// group verts that share a point
	FHashTable HashTable( 1 << FMath::Min( 16u, FMath::FloorLog2( numSVerts / 2 ) ), numSVerts );

	for( int i = 0; i < numSVerts; i++ )
	{
		HashTable.Add( HashPoint( sVerts[i].GetPos() ), i );
	}
	
	for( int i = 0; i < numSVerts; i++ )
	{
		// already grouped
		if( sVerts[i].next != &sVerts[i] )
		{
			continue;
		}

		// find any matching verts
		uint32 hash = HashPoint( sVerts[i].GetPos() );
		for( int j = HashTable.First( hash ); HashTable.IsValid(j); j = HashTable.Next(j) )
		{
			TSimpVert<T>* v1 = &sVerts[i];
			TSimpVert<T>* v2 = &sVerts[j];

			if( v1 == v2 )
				continue;

			// link
			if( v1->GetPos() == v2->GetPos() )
			{
				checkSlow( v2->next == v2 );
				checkSlow( v2->prev == v2 );

				v2->next = v1->next;
				v2->prev = v1;
				v2->next->prev = v2;
				v2->prev->next = v2;
			}
		}
	}
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::GroupEdges()
{
	FHashTable HashTable( 1 << FMath::Min( 16u, FMath::FloorLog2( edges.Num() / 2 ) ), edges.Num() );
	for( int i = 0; i < edges.Num(); i++ )
	{
		uint32 Hash0 = HashPoint( edges[i].v0->GetPos() );
		uint32 Hash1 = HashPoint( edges[i].v1->GetPos() );
		uint32 Hash = Murmur32( { FMath::Min( Hash0, Hash1 ), FMath::Max( Hash0, Hash1 ) } );
		HashTable.Add( Hash, i );
	}

	for( int i = 0; i < edges.Num(); i++ )
	{
		// already grouped
		if( edges[i].next != &edges[i] )
		{
			continue;
		}

		// find any matching edges
		uint32 Hash0 = HashPoint( edges[i].v0->GetPos() );
		uint32 Hash1 = HashPoint( edges[i].v1->GetPos() );
		uint32 Hash = Murmur32( { FMath::Min( Hash0, Hash1 ), FMath::Max( Hash0, Hash1 ) } );
		for( uint32 j = HashTable.First( Hash ); HashTable.IsValid(j); j = HashTable.Next(j) )
		{
			TSimpEdge<T>* e1 = &edges[i];
			TSimpEdge<T>* e2 = &edges[j];

			if( e1 == e2 )
				continue;

			bool m1 =	e1->v0->GetPos() == e2->v0->GetPos() &&
						e1->v1->GetPos() == e2->v1->GetPos();

			bool m2 =	e1->v0->GetPos() == e2->v1->GetPos() &&
						e1->v1->GetPos() == e2->v0->GetPos();

			// backwards
			if( m2 )
			{
				Swap( e2->v0, e2->v1 );
			}

			// link
			if( m1 || m2 )
			{
				check( e2->next == e2 );
				check( e2->prev == e2 );

				e2->next = e1->next;
				e2->prev = e1;
				e2->next->prev = e2;
				e2->prev->next = e2;
			}
		}
	}
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::InitCosts()
{
	for( int i = 0; i < edges.Num(); i++ )
	{
		float cost = ComputeEdgeCollapseCost( &edges[i] );
		check( FMath::IsFinite( cost ) );
		edgeHeap.Add( cost, i );
	}
}

template< typename T, uint32 NumAttributes >
TQuadricAttr< NumAttributes > TMeshSimplifier<T, NumAttributes>::GetQuadric( TSimpVert<T>* v )
{
#if SIMP_CACHE
	uint32 VertIndex = GetVertIndex( v );
	if( VertQuadricsValid[ VertIndex ] )
	{
		return VertQuadrics[ VertIndex ];
	}
#endif

	QuadricType vertQuadric;
	vertQuadric.Zero();
	
	// sum tri quadrics
	for( TriIterator i = v->adjTris.Begin(); i != v->adjTris.End(); ++i )
	{
		TSimpTri<T>* tri = *i;
#if SIMP_CACHE
		uint32 TriIndex = GetTriIndex( tri );
		if( TriQuadricsValid[ TriIndex ] )
		{
			vertQuadric += TriQuadrics[ TriIndex ];
		}
		else
		{
			QuadricType triQuadric(
				tri->verts[0]->GetPos(),		tri->verts[1]->GetPos(),		tri->verts[2]->GetPos(),
				tri->verts[0]->GetAttributes(),	tri->verts[1]->GetAttributes(),	tri->verts[2]->GetAttributes(),
				attributeWeights );
			vertQuadric += triQuadric;
			
			TriQuadricsValid[ TriIndex ] = true;
			TriQuadrics[ TriIndex ] = triQuadric;
		}
#else
		QuadricType triQuadric(
			tri->verts[0]->GetPos(),		tri->verts[1]->GetPos(),		tri->verts[2]->GetPos(),
			tri->verts[0]->GetAttributes(),	tri->verts[1]->GetAttributes(),	tri->verts[2]->GetAttributes(),
			attributeWeights );
		vertQuadric += triQuadric;
#endif
	}

#if SIMP_CACHE
	VertQuadricsValid[ VertIndex ] = true;
	VertQuadrics[ VertIndex ] = vertQuadric;
#endif

	return vertQuadric;
}

template< typename T, uint32 NumAttributes >
FQuadric TMeshSimplifier<T, NumAttributes>::GetEdgeQuadric( TSimpVert<T>* v )
{
#if SIMP_CACHE
	uint32 VertIndex = GetVertIndex( v );
	if( EdgeQuadricsValid[ VertIndex ] )
	{
		return EdgeQuadrics[ VertIndex ];
	}
#endif

	FQuadric vertQuadric;
	vertQuadric.Zero();

	TArray< TSimpVert<T>*, TInlineAllocator<64> > adjVerts;
	v->FindAdjacentVerts( adjVerts );

	LockTriFlags( SIMP_MARK1 );
	v->EnableAdjTriFlags( SIMP_MARK1 );
	
	for( TSimpVert<T>* vert : adjVerts )
	{
		TSimpTri<T>* face = NULL;
		int faceCount = 0;
		for( TriIterator j = vert->adjTris.Begin(); j != vert->adjTris.End(); ++j )
		{
			TSimpTri<T>* tri = *j;
			if( tri->TestFlags( SIMP_MARK1 ) )
			{
				face = tri;
				faceCount++;
			}
		}

		if( faceCount == 1 )
		{
			// only one face on this edge
			FQuadric edgeQuadric( v->GetPos(), vert->GetPos(), face->GetNormal(), 256.0f );
			vertQuadric += edgeQuadric;
		}
	}

	v->DisableAdjTriFlags( SIMP_MARK1 );
	UnlockTriFlags( SIMP_MARK1 );

#if SIMP_CACHE
	EdgeQuadricsValid[ VertIndex ] = true;
	EdgeQuadrics[ VertIndex ] = vertQuadric;
#endif

	return vertQuadric;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE uint32 TMeshSimplifier<T, NumAttributes>::GetVertIndex( const TSimpVert<T>* vert ) const
{
	ptrdiff_t Index = vert - &sVerts[0];
	return (uint32)Index;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE uint32 TMeshSimplifier<T, NumAttributes>::GetTriIndex( const TSimpTri<T>* tri ) const
{
	ptrdiff_t Index = tri - &sTris[0];
	return (uint32)Index;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE uint32 TMeshSimplifier<T, NumAttributes>::GetEdgeIndex( const TSimpEdge<T>* edge ) const
{
	ptrdiff_t Index = edge - &edges[0];
	return (uint32)Index;
}

template< typename T, uint32 NumAttributes >
FORCEINLINE uint32 TMeshSimplifier<T, NumAttributes>::HashPoint( const FVector& p ) const
{
	union { float f; uint32 i; } x;
	union { float f; uint32 i; } y;
	union { float f; uint32 i; } z;

	x.f = p.X;
	y.f = p.Y;
	z.f = p.Z;

	return Murmur32( { x.i, y.i, z.i } );
}

template< typename T, uint32 NumAttributes >
FORCEINLINE uint32 TMeshSimplifier<T, NumAttributes>::HashEdge( const TSimpVert<T>* u, const TSimpVert<T>* v ) const
{
	uint32 ui = GetVertIndex( u );
	uint32 vi = GetVertIndex( v );
	// must be symmetrical
	return Murmur32( { FMath::Min( ui, vi ), FMath::Max( ui, vi ) } );
}

template< typename T, uint32 NumAttributes >
TSimpEdge<T>* TMeshSimplifier<T, NumAttributes>::FindEdge( const TSimpVert<T>* u, const TSimpVert<T>* v )
{
	uint32 hash = HashEdge( u, v );
	for( uint32 i = edgeHash.First( hash ); edgeHash.IsValid(i); i = edgeHash.Next(i) )
	{
		if( ( edges[i].v0 == u && edges[i].v1 == v ) ||
			( edges[i].v0 == v && edges[i].v1 == u ) )
		{
			return &edges[i];
		}
	}

	return NULL;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::RemoveEdge( TSimpEdge<T>* edge )
{
	if( edge->TestFlags( SIMP_REMOVED ) )
	{
		checkSlow( edge->next == edge );
		checkSlow( edge->prev == edge );
		return;
	}

	uint32 hash = HashEdge( edge->v0, edge->v1 );
	for( uint32 i = edgeHash.First( hash ); edgeHash.IsValid(i); i = edgeHash.Next(i) )
	{
		if( &edges[i] == edge )
		{
			edgeHash.Remove( hash, i );
			edgeHeap.Remove( i );
			break;
		}
	}

	// remove edge
	edge->EnableFlags( SIMP_REMOVED );

	// ungroup edge
	edge->prev->next = edge->next;
	edge->next->prev = edge->prev;
	edge->next = edge;
	edge->prev = edge;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::ReplaceEdgeVert( const TSimpVert<T>* oldV, const TSimpVert<T>* otherV, TSimpVert<T>* newV )
{
	uint32 hash = HashEdge( oldV, otherV );
	uint32 index;
	for( index = edgeHash.First( hash ); edgeHash.IsValid( index ); index = edgeHash.Next( index ) )
	{
		if( ( edges[ index ].v0 == oldV && edges[ index ].v1 == otherV ) ||
			( edges[ index ].v1 == oldV && edges[ index ].v0 == otherV ) )
			break;
	}

	checkSlow( index != -1 );
	TSimpEdge<T>* edge = &edges[ index ];
	
	edgeHash.Remove( hash, index );

	TSimpEdge<T>* ExistingEdge = FindEdge( newV, otherV );
	if( ExistingEdge )
	{
		// Not entirely sure why this happens. I believe these are invalid edges from bridge tris.
		RemoveEdge( ExistingEdge );
	}

	if( newV )
	{
		edgeHash.Add( HashEdge( newV, otherV ), index );

		if( edge->v0 == oldV )
			edge->v0 = newV;
		else
			edge->v1 = newV;
	}
	else
	{
		// remove edge
		edge->EnableFlags( SIMP_REMOVED );

		// ungroup old edge
		edge->prev->next = edge->next;
		edge->next->prev = edge->prev;
		edge->next = edge;
		edge->prev = edge;

		edgeHeap.Remove( index );
	}
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::CollapseEdgeVert( const TSimpVert<T>* oldV, const TSimpVert<T>* otherV, TSimpVert<T>* newV )
{
	uint32 hash = HashEdge( oldV, otherV );
	uint32 index;
	for( index = edgeHash.First( hash ); edgeHash.IsValid( index ); index = edgeHash.Next( index ) )
	{
		if( ( edges[ index ].v0 == oldV && edges[ index ].v1 == otherV ) ||
			( edges[ index ].v1 == oldV && edges[ index ].v0 == otherV ) )
			break;
	}

	checkSlow( index != -1 );
	TSimpEdge<T>* edge = &edges[ index ];
	
	edgeHash.Remove( hash, index );
	edgeHeap.Remove( index );

	// remove edge
	edge->EnableFlags( SIMP_REMOVED );

	// ungroup old edge
	edge->prev->next = edge->next;
	edge->next->prev = edge->prev;
	edge->next = edge;
	edge->prev = edge;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::GatherUpdates( TSimpVert<T>* v )
{
	// Update all tris touching collapse edge.
	for( TriIterator i = v->adjTris.Begin(); i != v->adjTris.End(); ++i )
	{
		updateTris.AddUnique( *i );
	}

	TArray< TSimpVert<T>*, TInlineAllocator<64> > adjVerts;
	v->FindAdjacentVerts( adjVerts );

	LockVertFlags( SIMP_MARK1 | SIMP_MARK2 );

	// Update verts from tris adjacent to collapsed edge
	for( int i = 0, Num = adjVerts.Num(); i < Num; i++ )
	{
		updateVerts.AddUnique( adjVerts[i] );

		adjVerts[i]->EnableFlags( SIMP_MARK2 );
	}

	// update the costs of all edges connected to any face adjacent to v
	for( int i = 0, Num = adjVerts.Num(); i < Num; i++ )
	{
		adjVerts[i]->EnableAdjVertFlags( SIMP_MARK1 );

		for( TriIterator j = adjVerts[i]->adjTris.Begin(); j != adjVerts[i]->adjTris.End(); ++j )
		{
			TSimpTri<T>* tri = *j;
			for( int k = 0; k < 3; k++ )
			{
				TSimpVert<T>* vert = tri->verts[k];
				if( vert->TestFlags( SIMP_MARK1 ) && !vert->TestFlags( SIMP_MARK2 ) )
				{
					TSimpEdge<T>* edge = FindEdge( adjVerts[i], vert );
					updateEdges.AddUnique( edge );
				}
				vert->DisableFlags( SIMP_MARK1 );
			}
		}
		adjVerts[i]->DisableFlags( SIMP_MARK2 );
	}

	UnlockVertFlags( SIMP_MARK1 | SIMP_MARK2 );
}

template< typename T, uint32 NumAttributes >
float TMeshSimplifier<T, NumAttributes>::ComputeNewVerts( TSimpEdge<T>* edge, TArray< T, TInlineAllocator<16> >& newVerts )
{
	TSimpEdge<T>* e;
	TSimpVert<T>* v;

	TArray< QuadricType, TInlineAllocator<16> > quadrics;

	TQuadricAttrOptimizer< NumAttributes > optimizer;

	LockVertFlags( SIMP_MARK1 );
	
	edge->v0->EnableFlagsGroup( SIMP_MARK1 );
	edge->v1->EnableFlagsGroup( SIMP_MARK1 );

	// add edges
	e = edge;
	do {
		checkSlow( e == FindEdge( e->v0, e->v1 ) );
		checkSlow( e->v0->adjTris.Num() > 0 );
		checkSlow( e->v1->adjTris.Num() > 0 );
		checkSlow( e->v0->GetMaterialIndex() == e->v1->GetMaterialIndex() );

		newVerts.Add( e->v0->vert );

		QuadricType quadric;
		quadric  = GetQuadric( e->v0 );
		quadric += GetQuadric( e->v1 );
		quadrics.Add( quadric );
		optimizer.AddQuadric( quadric );

		e->v0->DisableFlags( SIMP_MARK1 );
		e->v1->DisableFlags( SIMP_MARK1 );

		e = e->next;
	} while( e != edge );

	// add remainder verts
	v = edge->v0;
	do {
		if( v->TestFlags( SIMP_MARK1 ) )
		{
			newVerts.Add( v->vert );

			QuadricType quadric;
			quadric = GetQuadric( v );
			quadrics.Add( quadric );
			optimizer.AddQuadric( quadric );

			v->DisableFlags( SIMP_MARK1 );
		}
		v = v->next;
	} while( v != edge->v0 );

	v = edge->v1;
	do {
		if( v->TestFlags( SIMP_MARK1 ) )
		{
			newVerts.Add( v->vert );

			QuadricType quadric;
			quadric = GetQuadric( v );
			quadrics.Add( quadric );
			optimizer.AddQuadric( quadric );

			v->DisableFlags( SIMP_MARK1 );
		}
		v = v->next;
	} while( v != edge->v1 );

	UnlockVertFlags( SIMP_MARK1 );

	check( quadrics.Num() <= 256 );

	FQuadric edgeQuadric;
	edgeQuadric.Zero();

	v = edge->v0;
	do {
		edgeQuadric += GetEdgeQuadric( v );
		v = v->next;
	} while( v != edge->v0 );

	v = edge->v1;
	do {
		edgeQuadric += GetEdgeQuadric( v );
		v = v->next;
	} while( v != edge->v1 );

	optimizer.AddQuadric( edgeQuadric );
	
	FVector newPos;
	{
		bool bLocked0 = edge->v0->TestFlags( SIMP_LOCKED );
		bool bLocked1 = edge->v1->TestFlags( SIMP_LOCKED );
		//checkSlow( !bLocked0 || !bLocked1 );

		// find position
		if( bLocked0 )
		{
			// v0 position
			newPos = edge->v0->GetPos();
		}
		else if( bLocked1 )
		{
			// v1 position
			newPos = edge->v1->GetPos();
		}
		else
		{
			// optimal position
			bool valid = optimizer.Optimize( newPos );
			if( !valid )
			{
				// Couldn't find optimal so choose middle
				newPos = ( edge->v0->GetPos() + edge->v1->GetPos() ) * 0.5f;
			}
		}
	}
	
	float cost = 0.0f;

	for( int i = 0; i < quadrics.Num(); i++ )
	{
		newVerts[i].GetPos() = newPos;

		if( quadrics[i].a > 1e-8 )
		{
			// calculate vert attributes from the new position
			quadrics[i].CalcAttributes( newVerts[i].GetPos(), newVerts[i].GetAttributes(), attributeWeights );
			newVerts[i].Correct();
		}

		// sum cost of new verts
		cost += quadrics[i].Evaluate( newVerts[i].GetPos(), newVerts[i].GetAttributes(), attributeWeights );
	}

	cost += edgeQuadric.Evaluate( newPos );

	return cost;
}

template< typename T, uint32 NumAttributes >
float TMeshSimplifier<T, NumAttributes>::ComputeEdgeCollapseCost( TSimpEdge<T>* edge )
{
	TArray< T, TInlineAllocator<16> > newVerts;
	float cost = ComputeNewVerts( edge, newVerts );

	const FVector& newPos = newVerts[0].GetPos();

	// add penalties
	// the below penalty code works with groups so no need to worry about remainder verts
	TSimpVert<T>* u = edge->v0;
	TSimpVert<T>* v = edge->v1;
	TSimpVert<T>* vert;

	float penalty = 0.0f;

	{
		const int degreeLimit = 24;
		const float degreePenalty = 100.0f;

		int degree = 0;

		// u
		vert = u;
		do {
			degree += vert->adjTris.Num();
			vert = vert->next;
		} while( vert != u );

		// v
		vert = v;
		do {
			degree += vert->adjTris.Num();
			vert = vert->next;
		} while( vert != v );

		if( degree > degreeLimit )
			penalty += degreePenalty * ( degree - degreeLimit );
	}

	{
		// Penalty to prevent edge folding 
		const float invalidPenalty = 1000000.0f;

		LockTriFlags( SIMP_MARK1 );
	
		v->EnableAdjTriFlagsGroup( SIMP_MARK1 );

		// u
		vert = u;
		do {
			for( TriIterator i = vert->adjTris.Begin(); i != vert->adjTris.End(); ++i )
			{
				TSimpTri<T>* tri = *i;
				if( !tri->TestFlags( SIMP_MARK1 ) )
					penalty += tri->ReplaceVertexIsValid( vert, newPos ) ? 0.0f : invalidPenalty;
				tri->DisableFlags( SIMP_MARK1 );
			}
			vert = vert->next;
		} while( vert != u );

		// v
		vert = v;
		do {
			for( TriIterator i = vert->adjTris.Begin(); i != vert->adjTris.End(); ++i )
			{
				TSimpTri<T>* tri = *i;
				if( tri->TestFlags( SIMP_MARK1 ) )
					penalty += tri->ReplaceVertexIsValid( vert, newPos ) ? 0.0f : invalidPenalty;
				tri->DisableFlags( SIMP_MARK1 );
			}
			vert = vert->next;
		} while( vert != v );

		UnlockTriFlags( SIMP_MARK1 );
	}

	return cost + penalty;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::Collapse( TSimpEdge<T>* edge )
{
	TSimpVert<T>* u = edge->v0;
	TSimpVert<T>* v = edge->v1;
	
	// Collapse the edge uv by moving vertex u onto v
	checkSlow( u && v );
	checkSlow( edge == FindEdge( u, v ) );
	checkSlow( u->adjTris.Num() > 0 );
	checkSlow( v->adjTris.Num() > 0 );
	checkSlow( u->GetMaterialIndex() == v->GetMaterialIndex() );

	if( u->TestFlags( SIMP_LOCKED ) )
		v->EnableFlags( SIMP_LOCKED );

	LockVertFlags( SIMP_MARK1 );

	// update edges from u to v
	u->EnableAdjVertFlags( SIMP_MARK1 );
	v->DisableAdjVertFlags( SIMP_MARK1 );

	if( u->TestFlags( SIMP_MARK1 ) )
	{
		// Invalid edge, results from collapsing a bridge tri
		// There are no actual triangles connecting these verts
		u->DisableAdjVertFlags( SIMP_MARK1 );
		UnlockVertFlags( SIMP_MARK1 );
		return;
	}

	for( TriIterator i = u->adjTris.Begin(); i != u->adjTris.End(); ++i )
	{
		TSimpTri<T>* tri = *i;
		for( int j = 0; j < 3; j++ )
		{
			TSimpVert<T>* vert = tri->verts[j];
			if( vert->TestFlags( SIMP_MARK1 ) )
			{
				ReplaceEdgeVert( u, vert, v );
				vert->DisableFlags( SIMP_MARK1 );
			}
		}
	}

	// remove dead edges
	u->EnableAdjVertFlags( SIMP_MARK1 );
	u->DisableFlags( SIMP_MARK1 );
	v->DisableFlags( SIMP_MARK1 );

	for( TriIterator i = v->adjTris.Begin(); i != v->adjTris.End(); ++i )
	{
		TSimpTri<T>* tri = *i;
		for( int j = 0; j < 3; j++ )
		{
			TSimpVert<T>* vert = tri->verts[j];
			if( vert->TestFlags( SIMP_MARK1 ) )
			{
				ReplaceEdgeVert( u, vert, NULL );
				vert->DisableFlags( SIMP_MARK1 );
			}
		}
	}

	u->DisableAdjVertFlags( SIMP_MARK1 );

	// fixup triangles
	for( TriIterator i = u->adjTris.Begin(); i != u->adjTris.End(); ++i )
	{
		TSimpTri<T>* tri = *i;
		checkSlow( !tri->TestFlags( SIMP_REMOVED ) );
		checkSlow( tri->HasVertex(u) );
		
		if( tri->HasVertex(v) )
		{
			// delete triangles on edge uv
			numTris--;
			tri->EnableFlags( SIMP_REMOVED );
#if SIMP_CACHE
			TriQuadricsValid[ GetTriIndex( tri ) ] = false;
#endif

			// remove references to tri
			for( int j = 0; j < 3; j++ )
			{
				TSimpVert<T>* vert = tri->verts[j];
				checkSlow( !vert->TestFlags( SIMP_REMOVED ) );
				if( vert != u )
				{
					vert->adjTris.Remove( tri );
#if 0
					if( vert != v && vert->adjTris.Num() == 0 ) {
						if( !vert->TestFlags( SIMP_REMOVED ) ) {	// not sure why this happens
							numVerts--;
							vert->EnableFlags( SIMP_REMOVED );
						}
						ReplaceEdgeVert( v, vert, NULL );
					}
#endif
				}
			}
		}
		else
		{
			// update triangles to have v instead of u
			tri->ReplaceVertex( u, v );
			v->adjTris.Add( tri );
		}
	}

#if SIMP_CACHE
	// remove modified verts and tris from cache
	v->EnableAdjVertFlags( SIMP_MARK1 );
	for( TriIterator i = v->adjTris.Begin(); i != v->adjTris.End(); ++i )
	{
		TSimpTri<T>* tri = *i;
		TriQuadricsValid[ GetTriIndex( tri ) ] = false;

		for( int j = 0; j < 3; j++ )
		{
			TSimpVert<T>* vert = tri->verts[j];
			if( vert->TestFlags( SIMP_MARK1 ) )
			{
				VertQuadricsValid[ GetVertIndex( vert ) ] = false;
				vert->DisableFlags( SIMP_MARK1 );
			}
		}
	}
#endif
	
	UnlockVertFlags( SIMP_MARK1 );
	
	u->adjTris.Clear();	// u has been removed
	u->EnableFlags( SIMP_REMOVED );
	numVerts--;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::UpdateTris()
{
	// remove degenerate triangles
	// not sure why this happens
	for( TSimpTri<T>* tri : updateTris )
	{
		if( tri->TestFlags( SIMP_REMOVED ) )
			continue;

#if SIMP_CACHE
		TriQuadricsValid[ GetTriIndex( tri ) ] = false;
#endif

		const FVector& p0 = tri->verts[0]->GetPos();
		const FVector& p1 = tri->verts[1]->GetPos();
		const FVector& p2 = tri->verts[2]->GetPos();
		const FVector n = ( p2 - p0 ) ^ ( p1 - p0 );

		if( n.SizeSquared() == 0.0f )
		{
			numTris--;
			tri->EnableFlags( SIMP_REMOVED );

			// remove references to tri
			for( int j = 0; j < 3; j++ )
			{
				TSimpVert<T>* vert = tri->verts[j];
				vert->adjTris.Remove( tri );
				// orphaned verts are removed below
			}
		}
	}
	updateTris.Reset();
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::UpdateVerts()
{
	// remove orphaned verts
	for( TSimpVert<T>* vert : updateVerts )
	{
		if( vert->TestFlags( SIMP_REMOVED ) )
			continue;

#if SIMP_CACHE
		VertQuadricsValid[ GetVertIndex( vert ) ] = false;
		EdgeQuadricsValid[ GetVertIndex( vert ) ] = false;
#endif

		if( vert->adjTris.Num() == 0 )
		{
			numVerts--;
			vert->EnableFlags( SIMP_REMOVED );

			// ungroup
			vert->prev->next = vert->next;
			vert->next->prev = vert->prev;
			vert->next = vert;
			vert->prev = vert;
		}
	}
	updateVerts.Reset();
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::UpdateEdges()
{
	uint32 NumEdges = updateEdges.Num();

	// add all grouped edges
	for( uint32 i = 0; i < NumEdges; i++ )
	{
		TSimpEdge<T>* edge = updateEdges[i];

		if( edge->TestFlags( SIMP_REMOVED ) )
			continue;

		TSimpEdge<T>* e = edge;
		do {
			updateEdges.AddUnique(e);
			e = e->next;
		} while( e != edge );
	}
		
	// remove dead edges
	for( uint32 i = 0, Num = updateEdges.Num(); i < Num; i++ )
	{
		TSimpEdge<T>* edge = updateEdges[i];

		if( edge->TestFlags( SIMP_REMOVED ) )
			continue;

		if( edge->v0->TestFlags( SIMP_REMOVED ) ||
			edge->v1->TestFlags( SIMP_REMOVED ) )
		{
			RemoveEdge( edge );
			continue;
		}
	}

	// Fix edge groups
	{
		FHashTable HashTable( 128, NumEdges );

		// ungroup edges
		for( uint32 i = 0, Num = updateEdges.Num(); i < Num; i++ )
		{
			TSimpEdge<T>* edge = updateEdges[i];

			if( edge->TestFlags( SIMP_REMOVED ) )
				continue;
				
			edge->next = edge;
			edge->prev = edge;
				
			HashTable.Add( HashPoint( edge->v0->GetPos() ) ^ HashPoint( edge->v1->GetPos() ), i );
		}

		// regroup edges
		for( uint32 i = 0, Num = updateEdges.Num(); i < Num; i++ )
		{
			TSimpEdge<T>* edge = updateEdges[i];

			if( edge->TestFlags( SIMP_REMOVED ) )
				continue;

			// already grouped
			if( edge->next != edge )
				continue;

			// find any matching edges
			uint32 hash = HashPoint( edge->v0->GetPos() ) ^ HashPoint( edge->v1->GetPos() );
			for( uint32 j = HashTable.First( hash ); HashTable.IsValid(j); j = HashTable.Next( j ) )
			{
				TSimpEdge<T>* e1 = updateEdges[i];
				TSimpEdge<T>* e2 = updateEdges[j];

				if( e1 == e2 )
					continue;

				bool m1 =	e1->v0->GetPos() == e2->v0->GetPos() &&
							e1->v1->GetPos() == e2->v1->GetPos();

				bool m2 =	e1->v0->GetPos() == e2->v1->GetPos() &&
							e1->v1->GetPos() == e2->v0->GetPos();

				// backwards
				if( m2 )
					Swap( e2->v0, e2->v1 );

				// link
				if( m1 || m2 )
				{
					checkSlow( e2->next == e2 );
					checkSlow( e2->prev == e2 );

					e2->next = e1->next;
					e2->prev = e1;
					e2->next->prev = e2;
					e2->prev->next = e2;
				}
			}
		}
	}

	// update edges
	for( uint32 i = 0; i < NumEdges; i++ )
	{
		TSimpEdge<T>* edge = updateEdges[i];

		if( edge->TestFlags( SIMP_REMOVED ) )
			continue;

		float cost = ComputeEdgeCollapseCost( edge );

		TSimpEdge<T>* e = edge;
		do {
			uint32 EdgeIndex = GetEdgeIndex(e);
			if( edgeHeap.IsPresent( EdgeIndex ) )
			{
				edgeHeap.Update( cost, EdgeIndex );
			}
			e = e->next;
		} while( e != edge );
	}
	updateEdges.Reset();
}

template< typename T, uint32 NumAttributes >
float TMeshSimplifier<T, NumAttributes>::SimplifyMesh( float maxErrorLimit, int minTris )
{
	TSimpVert<T>* v;
	TSimpEdge<T>* e;

	float maxError = 0.0f;

	while( edgeHeap.Num() > 0 )
	{
		if( numTris <= minTris )
			break;

		// get the next vertex to collapse
		uint32 TopIndex = edgeHeap.Top();

		float error = edgeHeap.GetKey( TopIndex );
		if( error > maxErrorLimit )
		{
			break;
		}
		maxError = FMath::Max( maxError, error );
		
		edgeHeap.Pop();

		TSimpEdge<T>* top = &edges[ TopIndex ];

		int numEdges = 0;
		TSimpEdge<T>* edgeList[32];

		TSimpEdge<T>* edge = top;
		do {
			edgeList[ numEdges++ ] = edge;
			edge = edge->next;
		} while( edge != top );

		check(top);

		// skip locked edges
		bool locked = false;
		for( int i = 0; i < numEdges; i++ )
		{
			edge = edgeList[i];
			if( edge->v0->TestFlags( SIMP_LOCKED ) && edge->v1->TestFlags( SIMP_LOCKED ) )
			{
				locked = true;
			}
		}
		if( locked )
			continue;

		v = top->v0;
		do {
			GatherUpdates( v );
			v = v->next;
		} while( v != top->v0 );

		v = top->v1;
		do {
			GatherUpdates( v );
			v = v->next;
		} while( v != top->v1 );

#if 1
		// remove edges with already removed verts
		// not sure why this happens
		for( int i = 0; i < numEdges; i++ )
		{
			if( edgeList[i]->v0->adjTris.Num() == 0 ||
				edgeList[i]->v1->adjTris.Num() == 0 )
			{
				RemoveEdge( edgeList[i] );
				edgeList[i] = NULL;
			}
			else
			{
				checkSlow( !edgeList[i]->TestFlags( SIMP_REMOVED ) );
			}
		}
		if( top->v0->adjTris.Num() == 0 || top->v1->adjTris.Num() == 0 )
			continue;
#endif

		// move verts to new verts
		{	
			edge = top;
			
			TArray< T, TInlineAllocator<16> > newVerts;
			ComputeNewVerts( edge, newVerts );

			uint32 i = 0;

			LockVertFlags( SIMP_MARK1 );

			edge->v0->EnableFlagsGroup( SIMP_MARK1 );
			edge->v1->EnableFlagsGroup( SIMP_MARK1 );

			// edges
			e = edge;
			do {
				checkSlow( e == FindEdge( e->v0, e->v1 ) );
				checkSlow( e->v0->adjTris.Num() > 0 );
				checkSlow( e->v1->adjTris.Num() > 0 );
				checkSlow( e->v0->GetMaterialIndex() == e->v1->GetMaterialIndex() );

				e->v1->vert = newVerts[ i++ ];
				e->v0->DisableFlags( SIMP_MARK1 );
				e->v1->DisableFlags( SIMP_MARK1 );

				e = e->next;
			} while( e != edge );

			// remainder verts
			v = edge->v0;
			do {
				if( v->TestFlags( SIMP_MARK1 ) )
				{
					v->vert = newVerts[ i++ ];
					v->DisableFlags( SIMP_MARK1 );
				}
				v = v->next;
			} while( v != edge->v0 );

			v = edge->v1;
			do {
				if( v->TestFlags( SIMP_MARK1 ) ) {
					v->vert = newVerts[ i++ ];
					v->DisableFlags( SIMP_MARK1 );
				}
				v = v->next;
			} while( v != edge->v1 );

			UnlockVertFlags( SIMP_MARK1 );
		}

		// collapse all edges
		for( int i = 0; i < numEdges; i++ )
		{
			edge = edgeList[i];
			if( !edge )
				continue;
			if( edge->TestFlags( SIMP_REMOVED ) )	// wtf?
				continue;
			if( edge->v0->adjTris.Num() == 0 )
				continue;
			if( edge->v1->adjTris.Num() == 0 )
				continue;

			Collapse( edge );
			RemoveEdge( edge );
		}

		// add v0 remainder verts to v1
		{
			// combine v0 and v1 groups
			top->v0->next->prev = top->v1->prev;
			top->v1->prev->next = top->v0->next;
			top->v0->next = top->v1;
			top->v1->prev = top->v0;

			// ungroup removed verts
			uint32	vertListNum = 0;
			TSimpVert<T>*	vertList[256];
			
			v = top->v1;
			do {
				vertList[ vertListNum++ ] = v;
				v = v->next;
			} while( v != top->v1 );

			check( vertListNum <= 256 );
			
			for( uint32 i = 0; i < vertListNum; i++ )
			{
				v = vertList[i];
				if( v->TestFlags( SIMP_REMOVED ) )
				{
					// ungroup
					v->prev->next = v->next;
					v->next->prev = v->prev;
					v->next = v;
					v->prev = v;
				}
			}
		}

		{
			// spread locked flag to vert group
			uint32 flags = 0;

			v = top->v1;
			do {
				flags |= v->flags & SIMP_LOCKED;
				v = v->next;
			} while( v != top->v1 );

			v = top->v1;
			do {
				v->flags |= flags;
				v = v->next;
			} while( v != top->v1 );
		}

		UpdateTris();
		UpdateVerts();
		UpdateEdges();
	}

	// remove degenerate triangles
	// not sure why this happens
	for( int i = 0; i < numSTris; i++ )
	{
		TSimpTri<T>* tri = &sTris[i];

		if( tri->TestFlags( SIMP_REMOVED ) )
			continue;

		const FVector& p0 = tri->verts[0]->GetPos();
		const FVector& p1 = tri->verts[1]->GetPos();
		const FVector& p2 = tri->verts[2]->GetPos();
		const FVector n = ( p2 - p0 ) ^ ( p1 - p0 );

		if( n.SizeSquared() == 0.0f )
		{
			numTris--;
			tri->EnableFlags( SIMP_REMOVED );

			// remove references to tri
			for( int j = 0; j < 3; j++ )
			{
				TSimpVert<T>* vert = tri->verts[j];
				vert->adjTris.Remove( tri );
				// orphaned verts are removed below
			}
		}
	}

	// remove orphaned verts
	for( int i = 0; i < numSVerts; i++ )
	{
		TSimpVert<T>* vert = &sVerts[i];

		if( vert->TestFlags( SIMP_REMOVED ) )
			continue;

		if( vert->adjTris.Num() == 0 )
		{
			numVerts--;
			vert->EnableFlags( SIMP_REMOVED );
		}
	}

	return maxError;
}

template< typename T, uint32 NumAttributes >
void TMeshSimplifier<T, NumAttributes>::OutputMesh( T* verts, uint32* indexes )
{
	FHashTable HashTable( 4096, GetNumVerts() );

#if 1
	int count = 0;
	for( int i = 0; i < numSVerts; i++ )
		count += sVerts[i].TestFlags( SIMP_REMOVED ) ? 0 : 1;

	check( numVerts == count );
#endif

	int numV = 0;
	int numI = 0;

	for( int i = 0; i < numSTris; i++ )
	{
		if( sTris[i].TestFlags( SIMP_REMOVED ) )
			continue;

		// TODO this is sloppy. There should be no duped verts. Alias by index.
		for( int j = 0; j < 3; j++ )
		{
			TSimpVert<T>* vert = sTris[i].verts[j];
			checkSlow( !vert->TestFlags( SIMP_REMOVED ) );
			checkSlow( vert->adjTris.Num() != 0 );

			const FVector& p = vert->GetPos();
			uint32 hash = HashPoint( p );
			uint32 f;
			for( f = HashTable.First( hash ); HashTable.IsValid(f); f = HashTable.Next( f ) )
			{
				if( vert->vert == verts[f] )
					break;
			}
			if( !HashTable.IsValid(f) )
			{
				HashTable.Add( hash, numV );
				verts[ numV ] = vert->vert;
				indexes[ numI++ ] = numV;
				numV++;
			}
			else
			{
				indexes[ numI++ ] = f;
			}
		}
	}

	check( numV <= numVerts );
	check( numI <= numTris * 3 );
	
	numVerts = numV;
	numTris = numI / 3;
}
