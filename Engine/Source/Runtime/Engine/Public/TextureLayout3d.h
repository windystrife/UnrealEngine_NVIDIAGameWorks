// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureLayout3d.h: Texture space allocation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/**
 * An incremental texture space allocator.
 * For best results, add the elements ordered descending in size.
 */
class FTextureLayout3d
{
public:

	/**
	 * Minimal initialization constructor.
	 * @param	MinSizeX - The minimum width of the texture.
	 * @param	MinSizeY - The minimum height of the texture.
	 * @param	MaxSizeX - The maximum width of the texture.
	 * @param	MaxSizeY - The maximum height of the texture.
	 * @param	InPowerOfTwoSize - True if the texture size must be a power of two.
	 */
	FTextureLayout3d(uint32 MinSizeX, uint32 MinSizeY, uint32 MinSizeZ, uint32 MaxSizeX, uint32 MaxSizeY, uint32 MaxSizeZ, bool bInPowerOfTwoSize = false, bool bInAlignByFour = true):
		SizeX(MinSizeX),
		SizeY(MinSizeY),
		SizeZ(MinSizeZ),
		bPowerOfTwoSize(bInPowerOfTwoSize),
		bAlignByFour(bInAlignByFour)
	{
		checkSlow(MaxSizeX < USHRT_MAX && MaxSizeY < USHRT_MAX && MaxSizeZ < USHRT_MAX);
		new(Nodes) FTextureLayoutNode3d(0, 0, 0, MaxSizeX, MaxSizeY, MaxSizeZ);
	}

	/**
	 * Finds a free area in the texture large enough to contain a surface with the given size.
	 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
	 * set to the coordinates of the upper left corner of the free area and the function return true.
	 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
	 * @param	OutBaseX - If the function succeeds, contains the X coordinate of the upper left corner of the free area on return.
	 * @param	OutBaseY - If the function succeeds, contains the Y coordinate of the upper left corner of the free area on return.
	 * @param	ElementSizeX - The size of the surface to allocate in horizontal pixels.
	 * @param	ElementSizeY - The size of the surface to allocate in vertical pixels.
	 * @return	True if succeeded, false otherwise.
	 */
	bool AddElement(uint32& OutBaseX, uint32& OutBaseY, uint32& OutBaseZ, uint32 ElementSizeX, uint32 ElementSizeY, uint32 ElementSizeZ)
	{
		if (ElementSizeX == 0 || ElementSizeY == 0 || ElementSizeZ == 0)
		{
			OutBaseX = 0;
			OutBaseY = 0;
			OutBaseZ = 0;
			return true;
		}

		if (bAlignByFour)
		{
			// Pad to 4 to ensure alignment
			ElementSizeX = (ElementSizeX + 3) & ~3;
			ElementSizeY = (ElementSizeY + 3) & ~3;
			ElementSizeZ = (ElementSizeZ + 3) & ~3;
		}
		
		// Try allocating space without enlarging the texture.
		int32	NodeIndex = AddSurfaceInner(0, ElementSizeX, ElementSizeY, ElementSizeZ, false);
		if (NodeIndex == INDEX_NONE)
		{
			// Try allocating space which might enlarge the texture.
			NodeIndex = AddSurfaceInner(0, ElementSizeX, ElementSizeY, ElementSizeZ, true);
		}

		if (NodeIndex != INDEX_NONE)
		{
			FTextureLayoutNode3d&	Node = Nodes[NodeIndex];
			Node.bUsed = true;
			OutBaseX = Node.MinX;
			OutBaseY = Node.MinY;
			OutBaseZ = Node.MinZ;

			if (bPowerOfTwoSize)
			{
				SizeX = FMath::Max<uint32>(SizeX, FMath::RoundUpToPowerOfTwo(Node.MinX + ElementSizeX));
				SizeY = FMath::Max<uint32>(SizeY, FMath::RoundUpToPowerOfTwo(Node.MinY + ElementSizeY));
				SizeZ = FMath::Max<uint32>(SizeZ, FMath::RoundUpToPowerOfTwo(Node.MinZ + ElementSizeZ));
			}
			else
			{
				SizeX = FMath::Max<uint32>(SizeX, Node.MinX + ElementSizeX);
				SizeY = FMath::Max<uint32>(SizeY, Node.MinY + ElementSizeY);
				SizeZ = FMath::Max<uint32>(SizeZ, Node.MinZ + ElementSizeZ);
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	/** 
	 * Removes a previously allocated element from the layout and collapses the tree as much as possible,
	 * In order to create the largest free block possible and return the tree to its state before the element was added.
	 * @return	True if the element specified by the input parameters existed in the layout.
	 */
	bool RemoveElement(uint32 ElementBaseX, uint32 ElementBaseY, uint32 ElementBaseZ, uint32 ElementSizeX, uint32 ElementSizeY, uint32 ElementSizeZ)
	{
		int32 FoundNodeIndex = INDEX_NONE;
		// Search through nodes to find the element to remove
		//@todo - traverse the tree instead of iterating through all nodes
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
		{
			FTextureLayoutNode3d&	Node = Nodes[NodeIndex];

			if (Node.MinX == ElementBaseX
				&& Node.MinY == ElementBaseY
				&& Node.MinZ == ElementBaseZ
				&& Node.SizeX == ElementSizeX
				&& Node.SizeY == ElementSizeY
				&& Node.SizeZ == ElementSizeZ)
			{
				FoundNodeIndex = NodeIndex;
				break;
			}
		}

		if (FoundNodeIndex != INDEX_NONE)
		{
			// Mark the found node as not being used anymore
			Nodes[FoundNodeIndex].bUsed = false;

			// Walk up the tree to find the node closest to the root that doesn't have any used children
			int32 ParentNodeIndex = FindParentNode(FoundNodeIndex);
			ParentNodeIndex = IsNodeUsed(ParentNodeIndex) ? INDEX_NONE : ParentNodeIndex;
			int32 LastParentNodeIndex = ParentNodeIndex;
			while (ParentNodeIndex != INDEX_NONE 
				&& !IsNodeUsed(Nodes[ParentNodeIndex].ChildA) 
				&& !IsNodeUsed(Nodes[ParentNodeIndex].ChildB))
			{
				LastParentNodeIndex = ParentNodeIndex;
				ParentNodeIndex = FindParentNode(ParentNodeIndex);
			} 

			// Remove the children of the node closest to the root with only unused children,
			// Which restores the tree to its state before this element was allocated,
			// And allows allocations as large as LastParentNode in the future.
			if (LastParentNodeIndex != INDEX_NONE)
			{
				RemoveChildren(LastParentNodeIndex);
			}
			return true;
		}

		return false;
	}

	/**
	 * Returns the minimum texture width which will contain the allocated surfaces.
	 */
	uint32 GetSizeX() const { return SizeX; }

	/**
	 * Returns the minimum texture height which will contain the allocated surfaces.
	 */
	uint32 GetSizeY() const { return SizeY; }

	uint32 GetSizeZ() const { return SizeZ; }

	FIntVector GetSize() const { return FIntVector(SizeX, SizeY, SizeZ); }

	uint32 GetMaxSizeX() const 
	{
		return Nodes[0].SizeX;
	}

	uint32 GetMaxSizeY() const 
	{
		return Nodes[0].SizeY;
	}

	uint32 GetMaxSizeZ() const 
	{
		return Nodes[0].SizeZ;
	}

private:

	struct FTextureLayoutNode3d
	{
		int32	ChildA,
				ChildB;
		uint16	MinX,
				MinY,
				MinZ,
				SizeX,
				SizeY,
				SizeZ;
		bool	bUsed;

		FTextureLayoutNode3d() {}

		FTextureLayoutNode3d(uint16 InMinX, uint16 InMinY, uint16 InMinZ, uint16 InSizeX, uint16 InSizeY, uint16 InSizeZ):
			ChildA(INDEX_NONE),
			ChildB(INDEX_NONE),
			MinX(InMinX),
			MinY(InMinY),
			MinZ(InMinZ),
			SizeX(InSizeX),
			SizeY(InSizeY),
			SizeZ(InSizeZ),
			bUsed(false)
		{}
	};

	uint32 SizeX;
	uint32 SizeY;
	uint32 SizeZ;
	bool bPowerOfTwoSize;
	bool bAlignByFour;
	TArray<FTextureLayoutNode3d,TInlineAllocator<5> > Nodes;

	/** Recursively traverses the tree depth first and searches for a large enough leaf node to contain the requested allocation. */
	int32 AddSurfaceInner(int32 NodeIndex, uint32 ElementSizeX, uint32 ElementSizeY, uint32 ElementSizeZ, bool bAllowTextureEnlargement)
	{
		checkSlow(NodeIndex != INDEX_NONE);
		// But do access this node via a pointer until the first recursive call. Prevents a ton of LHS.
		const FTextureLayoutNode3d* CurrentNodePtr = &Nodes[NodeIndex];
		if (CurrentNodePtr->ChildA != INDEX_NONE)
		{
			// Children are always allocated together
			checkSlow(CurrentNodePtr->ChildB != INDEX_NONE);

			// Traverse the children
			const int32 Result = AddSurfaceInner(CurrentNodePtr->ChildA, ElementSizeX, ElementSizeY, ElementSizeZ, bAllowTextureEnlargement);
			
			// The pointer is now invalid, be explicit!
			CurrentNodePtr = 0;

			if (Result != INDEX_NONE)
			{
				return Result;
			}

			return AddSurfaceInner(Nodes[NodeIndex].ChildB, ElementSizeX, ElementSizeY, ElementSizeZ, bAllowTextureEnlargement);
		}
		// Node has no children, it is a leaf
		else
		{
			// Reject this node if it is already used
			if (CurrentNodePtr->bUsed)
			{
				return INDEX_NONE;
			}

			// Reject this node if it is too small for the element being placed
			if (CurrentNodePtr->SizeX < ElementSizeX || CurrentNodePtr->SizeY < ElementSizeY || CurrentNodePtr->SizeZ < ElementSizeZ)
			{
				return INDEX_NONE;
			}

			if (!bAllowTextureEnlargement)
			{
				// Reject this node if this is an attempt to allocate space without enlarging the texture, 
				// And this node cannot hold the element without enlarging the texture.
				if (CurrentNodePtr->MinX + ElementSizeX > SizeX || CurrentNodePtr->MinY + ElementSizeY > SizeY || CurrentNodePtr->MinZ + ElementSizeY > SizeZ)
				{
					return INDEX_NONE;
				}
			}

			// Use this node if the size matches the requested element size
			if (CurrentNodePtr->SizeX == ElementSizeX && CurrentNodePtr->SizeY == ElementSizeY && CurrentNodePtr->SizeZ == ElementSizeZ)
			{
				return NodeIndex;
			}

			const uint32 ExcessWidth = CurrentNodePtr->SizeX - ElementSizeX;
			const uint32 ExcessHeight = CurrentNodePtr->SizeY - ElementSizeY;
			const uint32 ExcessDepth = CurrentNodePtr->SizeZ - ElementSizeZ;

			// The pointer to the current node may be invalidated below, be explicit!
			CurrentNodePtr = 0;

			// Add new nodes, and link them as children of the current node.
			if (ExcessWidth > ExcessHeight)
			{
				// Store a copy of the current node on the stack for easier debugging.
				// Can't store a pointer to the current node since Nodes may be reallocated in this function.
				const FTextureLayoutNode3d CurrentNode = Nodes[NodeIndex];

				if (ExcessWidth > ExcessDepth)
				{
					// Update the child indices
					Nodes[NodeIndex].ChildA = Nodes.Num();

					// Create a child with the same width as the element being placed.
					// The height may not be the same as the element height yet, in that case another subdivision will occur when traversing this child node.
					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY,
						CurrentNode.MinZ,
						ElementSizeX,
						CurrentNode.SizeY,
						CurrentNode.SizeZ
						);

					// Create a second child to contain the leftover area in the X direction
					Nodes[NodeIndex].ChildB = Nodes.Num();
					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX + ElementSizeX,
						CurrentNode.MinY,
						CurrentNode.MinZ,
						CurrentNode.SizeX - ElementSizeX,
						CurrentNode.SizeY,
						CurrentNode.SizeZ
						);
				}
				else
				{
					Nodes[NodeIndex].ChildA = Nodes.Num();

					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY,
						CurrentNode.MinZ,
						CurrentNode.SizeX,
						CurrentNode.SizeY,
						ElementSizeZ
						);

					Nodes[NodeIndex].ChildB = Nodes.Num();
					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY,
						CurrentNode.MinZ + ElementSizeZ,
						CurrentNode.SizeX,
						CurrentNode.SizeY,
						CurrentNode.SizeZ - ElementSizeZ
						);
				}
			}
			else
			{
				// Store a copy of the current node on the stack for easier debugging.
				// Can't store a pointer to the current node since Nodes may be reallocated in this function.
				const FTextureLayoutNode3d CurrentNode = Nodes[NodeIndex];

				if (ExcessHeight > ExcessDepth)
				{
					Nodes[NodeIndex].ChildA = Nodes.Num();
					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY,
						CurrentNode.MinZ,
						CurrentNode.SizeX,
						ElementSizeY,
						CurrentNode.SizeZ
						);

					Nodes[NodeIndex].ChildB = Nodes.Num();
					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY + ElementSizeY,
						CurrentNode.MinZ,
						CurrentNode.SizeX,
						CurrentNode.SizeY - ElementSizeY,
						CurrentNode.SizeZ
						);
				}
				else
				{
					Nodes[NodeIndex].ChildA = Nodes.Num();

					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY,
						CurrentNode.MinZ,
						CurrentNode.SizeX,
						CurrentNode.SizeY,
						ElementSizeZ
						);

					Nodes[NodeIndex].ChildB = Nodes.Num();
					new(Nodes) FTextureLayoutNode3d(
						CurrentNode.MinX,
						CurrentNode.MinY,
						CurrentNode.MinZ + ElementSizeZ,
						CurrentNode.SizeX,
						CurrentNode.SizeY,
						CurrentNode.SizeZ - ElementSizeZ
						);
				}
			}

			// Only traversing ChildA, since ChildA is always the newly created node that matches the element size
			return AddSurfaceInner(Nodes[NodeIndex].ChildA, ElementSizeX, ElementSizeY, ElementSizeZ, bAllowTextureEnlargement);
		}
	}

	/** Returns the index into Nodes of the parent node of SearchNode. */
	int32 FindParentNode(int32 SearchNodeIndex)
	{
		//@todo - could be a constant time search if the nodes stored a parent index
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
		{
			FTextureLayoutNode3d&	Node = Nodes[NodeIndex];
			if (Node.ChildA == SearchNodeIndex || Node.ChildB == SearchNodeIndex)
			{
				return NodeIndex;
			}
		}
		return INDEX_NONE;
	}

	/** Returns true if the node or any of its children are marked used. */
	bool IsNodeUsed(int32 NodeIndex)
	{
		bool bChildrenUsed = false;
		if (Nodes[NodeIndex].ChildA != INDEX_NONE)
		{
			checkSlow(Nodes[NodeIndex].ChildB != INDEX_NONE);
			bChildrenUsed = IsNodeUsed(Nodes[NodeIndex].ChildA) || IsNodeUsed(Nodes[NodeIndex].ChildB);
		}
		return Nodes[NodeIndex].bUsed || bChildrenUsed;
	}

	/** Recursively removes the children of a given node from the Nodes array and adjusts existing indices to compensate. */
	void RemoveChildren(int32 NodeIndex)
	{
		// Traverse the children depth first
		if (Nodes[NodeIndex].ChildA != INDEX_NONE)
		{
			RemoveChildren(Nodes[NodeIndex].ChildA);
		}
		if (Nodes[NodeIndex].ChildB != INDEX_NONE)
		{
			RemoveChildren(Nodes[NodeIndex].ChildB);
		}

		if (Nodes[NodeIndex].ChildA != INDEX_NONE)
		{
			// Store off the index of the child since it may be changed in the code below
			const int32 OldChildA = Nodes[NodeIndex].ChildA;

			// Remove the child from the Nodes array
			Nodes.RemoveAt(OldChildA);

			// Iterate over all the Nodes and fix up their indices now that an element has been removed
			for (int32 OtherNodeIndex = 0; OtherNodeIndex < Nodes.Num(); OtherNodeIndex++)
			{
				if (Nodes[OtherNodeIndex].ChildA >= OldChildA)
				{
					Nodes[OtherNodeIndex].ChildA--;
				}
				if (Nodes[OtherNodeIndex].ChildB >= OldChildA)
				{
					Nodes[OtherNodeIndex].ChildB--;
				}
			}
			// Mark the node as not having a ChildA
			Nodes[NodeIndex].ChildA = INDEX_NONE;
		}

		if (Nodes[NodeIndex].ChildB != INDEX_NONE)
		{
			const int32 OldChildB = Nodes[NodeIndex].ChildB;
			Nodes.RemoveAt(OldChildB);
			for (int32 OtherNodeIndex = 0; OtherNodeIndex < Nodes.Num(); OtherNodeIndex++)
			{
				if (Nodes[OtherNodeIndex].ChildA >= OldChildB)
				{
					Nodes[OtherNodeIndex].ChildA--;
				}
				if (Nodes[OtherNodeIndex].ChildB >= OldChildB)
				{
					Nodes[OtherNodeIndex].ChildB--;
				}
			}
			Nodes[NodeIndex].ChildB = INDEX_NONE;
		}
	}
};

