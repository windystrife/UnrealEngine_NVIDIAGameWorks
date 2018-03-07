// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericOctree.h: Generic octree definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "GenericOctreePublic.h"

/** A concise iteration over the children of an octree node. */
#define FOREACH_OCTREE_CHILD_NODE(ChildRef) \
	for(FOctreeChildNodeRef ChildRef(0);!ChildRef.IsNULL();ChildRef.Advance())

class FOctreeChildNodeRef;

/** An unquantized bounding box. */
class FBoxCenterAndExtent
{
public:
	FVector4 Center;
	FVector4 Extent;

	/** Default constructor. */
	FBoxCenterAndExtent() {}

	/** Initialization constructor. */
	FBoxCenterAndExtent(const FVector& InCenter,const FVector& InExtent)
	:	Center(InCenter,0)
	,	Extent(InExtent,0)
	{}

	/** FBox conversion constructor. */
	FBoxCenterAndExtent(const FBox& Box)
	{
		Box.GetCenterAndExtents((FVector&)Center,(FVector&)Extent);
		Center.W = Extent.W = 0;
	}

	/** FBoxSphereBounds conversion constructor. */
	explicit FBoxCenterAndExtent(const FBoxSphereBounds& BoxSphere)
	{
		Center = BoxSphere.Origin;
		Extent = BoxSphere.BoxExtent;
		Center.W = Extent.W = 0;
	}

	/** Center - radius as four contiguous floats conversion constructor. */
	explicit FBoxCenterAndExtent(const float PositionRadius[4])
	{
		Center = FVector(PositionRadius[0],PositionRadius[1],PositionRadius[2]);
		Extent = FVector(PositionRadius[3]);
		Center.W = Extent.W = 0;
	}

	/** Converts to a FBox. */
	FBox GetBox() const
	{
		return FBox(Center - Extent,Center + Extent);
	}

	/**
	 * Determines whether two boxes intersect.
	 * Warning: this operates on the W of the bounds positions!
	 * @return true if the boxes intersect, or false.
	 */
	friend FORCEINLINE bool Intersect(const FBoxCenterAndExtent& A,const FBoxCenterAndExtent& B)
	{
		// CenterDifference is the vector between the centers of the bounding boxes.
		const VectorRegister CenterDifference = VectorAbs(VectorSubtract(VectorLoadAligned(&A.Center),VectorLoadAligned(&B.Center)));

		// CompositeExtent is the extent of the bounding box which is the convolution of A with B.
		const VectorRegister CompositeExtent = VectorAdd(VectorLoadAligned(&A.Extent),VectorLoadAligned(&B.Extent));

		// For each axis, the boxes intersect on that axis if the projected distance between their centers is less than the sum of their
		// extents.  If the boxes don't intersect on any of the axes, they don't intersect.
		return VectorAnyGreaterThan(CenterDifference,CompositeExtent) == false;
	}
	/**
	 * Determines whether two boxes intersect.
	 * Warning: this operates on the W of the bounds positions!
	 * @return true if the boxes intersect, or false.
	 */
	friend FORCEINLINE bool Intersect(const FBoxSphereBounds& A,const FBoxCenterAndExtent& B)
	{
		// CenterDifference is the vector between the centers of the bounding boxes.
		const VectorRegister CenterDifference = VectorAbs(VectorSubtract(VectorLoadFloat3_W0(&A.Origin),VectorLoadAligned(&B.Center)));

		// CompositeExtent is the extent of the bounding box which is the convolution of A with B.
		const VectorRegister CompositeExtent = VectorAdd(VectorLoadFloat3_W0(&A.BoxExtent),VectorLoadAligned(&B.Extent));

		// For each axis, the boxes intersect on that axis if the projected distance between their centers is less than the sum of their
		// extents.  If the boxes don't intersect on any of the axes, they don't intersect.
		return VectorAnyGreaterThan(CenterDifference,CompositeExtent) == false;
	}
	/**
	 * Determines whether two boxes intersect.
	 * Warning: this operates on the W of the bounds positions!
	 * @param A box given in center - radius form as four contiguous floats
	 * @return true if the boxes intersect, or false.
	 */
	friend FORCEINLINE bool Intersect(const float A[4],const FBoxCenterAndExtent& B)
	{
		// CenterDifference is the vector between the centers of the bounding boxes.
		const VectorRegister CenterDifference = VectorAbs(VectorSubtract(VectorLoadFloat3_W0(A),VectorLoadAligned(&B.Center)));

		// CompositeExtent is the extent of the bounding box which is the convolution of A with B.
		const VectorRegister CompositeExtent = VectorAdd(VectorSet_W0(VectorLoadFloat1(A+3)),VectorLoadAligned(&B.Extent));

		// For each axis, the boxes intersect on that axis if the projected distance between their centers is less than the sum of their
		// extents.  If the boxes don't intersect on any of the axes, they don't intersect.
		return VectorAnyGreaterThan(CenterDifference,CompositeExtent) == false;
	}
};

/** A reference to a child of an octree node. */
class FOctreeChildNodeRef
{
public:

	union
	{
		struct
		{
			uint32 X : 1;
			uint32 Y : 1;
			uint32 Z : 1;
			uint32 bNULL : 1;
		};
		uint32 Index : 3;
	};

	/** Initialization constructor. */
	FOctreeChildNodeRef(int32 InX,int32 InY,int32 InZ)
	:	X(InX)
	,	Y(InY)
	,	Z(InZ)
	,	bNULL(false)
	{}

	/** Initialized the reference with a child index. */
	FOctreeChildNodeRef(int32 InIndex = 0)
	:	Index(InIndex)
	{
		// some compilers do not allow multiple members of a union to be specified in the constructor init list
		bNULL = false;
	}

	/** Advances the reference to the next child node.  If this was the last node remain, sets bInvalid=true. */
	FORCEINLINE void Advance()
	{
		if(Index < 7)
		{
			++Index;
		}
		else
		{
			bNULL = true;
		}
	}

	/** @return true if the reference isn't set. */
	FORCEINLINE bool IsNULL() const
	{
		return bNULL;
	}
};

/** A subset of an octree node's children that intersect a bounding box. */
class FOctreeChildNodeSubset
{
public:

	union
	{
		struct 
		{
			uint32 bPositiveX : 1;
			uint32 bPositiveY : 1;
			uint32 bPositiveZ : 1;
			uint32 bNegativeX : 1;
			uint32 bNegativeY : 1;
			uint32 bNegativeZ : 1;
		};

		struct
		{
			/** Only the bits for the children on the positive side of the splits. */
			uint32 PositiveChildBits : 3;

			/** Only the bits for the children on the negative side of the splits. */
			uint32 NegativeChildBits : 3;
		};

		/** All the bits corresponding to the child bits. */
		uint32 ChildBits : 6;

		/** All the bits used to store the subset. */
		uint32 AllBits;
	};

	/** Initializes the subset to be empty. */
	FOctreeChildNodeSubset()
	:	AllBits(0)
	{}

	/** Initializes the subset to contain a single node. */
	FOctreeChildNodeSubset(FOctreeChildNodeRef ChildRef)
	:	AllBits(0)
	{
		// The positive child bits correspond to the child index, and the negative to the NOT of the child index.
		PositiveChildBits = ChildRef.Index;
		NegativeChildBits = ~ChildRef.Index;
	}

	/** Determines whether the subset contains a specific node. */
	bool Contains(FOctreeChildNodeRef ChildRef) const;
};

/** the float table {-1.0f,1.0f} **/
extern ENGINE_API float GNegativeOneOneTable[2];
/** The context of an octree node, derived from the traversal of the tree. */
class FOctreeNodeContext
{
public:

	/** The node bounds are expanded by their extent divided by LoosenessDenominator. */
	enum { LoosenessDenominator = 16 };

	/** The bounds of the node. */
	FBoxCenterAndExtent Bounds;

	/** The extent of the node's children. */
	float ChildExtent;

	/** The offset of the childrens' centers from the center of this node. */
	float ChildCenterOffset;

	/** Bits used for culling, semantics left up to the caller (except that it is always set to zero at the root). This does not consume storage because it is leftover in the padding.*/
	uint32 InCullBits;

	/** Bits used for culling, semantics left up to the caller (except that it is always set to zero at the root). This does not consume storage because it is leftover in the padding.*/
	uint32 OutCullBits;

	/** Default constructor. */
	FOctreeNodeContext()
	{}

	/** Initialization constructor, this one is used when we done care about the box anymore */
	FOctreeNodeContext(uint32 InInCullBits, uint32 InOutCullBits)
		:	InCullBits(InInCullBits)
		,	OutCullBits(InOutCullBits)
	{
	}

	/** Initialization constructor. */
	FOctreeNodeContext(const FBoxCenterAndExtent& InBounds)
	:	Bounds(InBounds)
	{
		// A child node's tight extents are half its parent's extents, and its loose extents are expanded by 1/LoosenessDenominator.
		const float TightChildExtent = Bounds.Extent.X * 0.5f;
		const float LooseChildExtent = TightChildExtent * (1.0f + 1.0f / (float)LoosenessDenominator);

		ChildExtent = LooseChildExtent;
		ChildCenterOffset = Bounds.Extent.X - LooseChildExtent;
	}

	/** Initialization constructor. */
	FOctreeNodeContext(const FBoxCenterAndExtent& InBounds, uint32 InInCullBits, uint32 InOutCullBits)
		:	Bounds(InBounds)
		,	InCullBits(InInCullBits)
		,	OutCullBits(InOutCullBits)
	{
		// A child node's tight extents are half its parent's extents, and its loose extents are expanded by 1/LoosenessDenominator.
		const float TightChildExtent = Bounds.Extent.X * 0.5f;
		const float LooseChildExtent = TightChildExtent * (1.0f + 1.0f / (float)LoosenessDenominator);

		ChildExtent = LooseChildExtent;
		ChildCenterOffset = Bounds.Extent.X - LooseChildExtent;
	}

	/** Child node initialization constructor. */
	FORCEINLINE FOctreeNodeContext GetChildContext(FOctreeChildNodeRef ChildRef) const
	{
		return FOctreeNodeContext(FBoxCenterAndExtent(
			FVector(
			Bounds.Center.X + ChildCenterOffset * GNegativeOneOneTable[ChildRef.X],
			Bounds.Center.Y + ChildCenterOffset * GNegativeOneOneTable[ChildRef.Y],
			Bounds.Center.Z + ChildCenterOffset * GNegativeOneOneTable[ChildRef.Z]
			),
			FVector(
			ChildExtent,
			ChildExtent,
			ChildExtent
			)
			));
	}

	/** Construct a child context given the child ref. Optimized to remove all LHS. */
	FORCEINLINE void GetChildContext(FOctreeChildNodeRef ChildRef, FOctreeNodeContext * RESTRICT ChildContext) const
	{
		const FOctreeNodeContext* RESTRICT ParentContext = this;
		ChildContext->Bounds.Center.X = ParentContext->Bounds.Center.X + ParentContext->ChildCenterOffset * GNegativeOneOneTable[ChildRef.X];
		ChildContext->Bounds.Center.Y = ParentContext->Bounds.Center.Y + ParentContext->ChildCenterOffset * GNegativeOneOneTable[ChildRef.Y];
		ChildContext->Bounds.Center.Z = ParentContext->Bounds.Center.Z + ParentContext->ChildCenterOffset * GNegativeOneOneTable[ChildRef.Z];
		ChildContext->Bounds.Center.W = 0.0f;
		ChildContext->Bounds.Extent.X = ParentContext->ChildExtent;
		ChildContext->Bounds.Extent.Y = ParentContext->ChildExtent;
		ChildContext->Bounds.Extent.Z = ParentContext->ChildExtent;
		ChildContext->Bounds.Extent.W = 0.0f;

		const float TightChildExtent = ParentContext->ChildExtent * 0.5f;
		const float LooseChildExtent = TightChildExtent * (1.0f + 1.0f / (float)LoosenessDenominator);
		ChildContext->ChildExtent = LooseChildExtent;
		ChildContext->ChildCenterOffset = ParentContext->ChildExtent - LooseChildExtent;
	}
	
	/** Child node initialization constructor. */
	FORCEINLINE FOctreeNodeContext GetChildContext(FOctreeChildNodeRef ChildRef, uint32 InInCullBits, uint32 InOutCullBits) const
	{
		return FOctreeNodeContext(FBoxCenterAndExtent(
			FVector(
			Bounds.Center.X + ChildCenterOffset * GNegativeOneOneTable[ChildRef.X],
			Bounds.Center.Y + ChildCenterOffset * GNegativeOneOneTable[ChildRef.Y],
			Bounds.Center.Z + ChildCenterOffset * GNegativeOneOneTable[ChildRef.Z]
			),
			FVector(
			ChildExtent,
			ChildExtent,
			ChildExtent
			)
			),InInCullBits,InOutCullBits);
	}
	/**
	 * Determines which of the octree node's children intersect with a bounding box.
	 * @param BoundingBox - The bounding box to check for intersection with.
	 * @return A subset of the children's nodes that intersect the bounding box.
	 */
	FOctreeChildNodeSubset GetIntersectingChildren(const FBoxCenterAndExtent& BoundingBox) const;

	/**
	 * Determines which of the octree node's children contain the whole bounding box, if any.
	 * @param BoundingBox - The bounding box to check for intersection with.
	 * @return The octree's node that the bounding box is farthest from the outside edge of, or an invalid node ref if it's not contained
	 *			by any of the children.
	 */
	FOctreeChildNodeRef GetContainingChild(const FBoxCenterAndExtent& BoundingBox) const;
};

/** An octree. */
template<typename ElementType,typename OctreeSemantics>
class TOctree
{
public:

	typedef TArray<ElementType, typename OctreeSemantics::ElementAllocator> ElementArrayType;
	typedef typename ElementArrayType::TConstIterator ElementConstIt;

	/** A node in the octree. */
	class FNode
	{
	public:

		friend class TOctree;

		/** Initialization constructor. */
		explicit FNode(const FNode* InParent)
		:	Parent(InParent)
		,	InclusiveNumElements(0)
		,	bIsLeaf(true)
		{
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				Children[ChildRef.Index] = NULL;
			}
		}

		/** Destructor. */
		~FNode()
		{
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				delete Children[ChildRef.Index];
			}
		}

		// Accessors.
		FORCEINLINE ElementConstIt GetElementIt() const { return ElementConstIt(Elements); }
		FORCEINLINE bool IsLeaf() const { return bIsLeaf; }
		FORCEINLINE bool HasChild(FOctreeChildNodeRef ChildRef) const
		{
			return Children[ChildRef.Index] != NULL && Children[ChildRef.Index]->InclusiveNumElements > 0;
		}
		FORCEINLINE FNode* GetChild(FOctreeChildNodeRef ChildRef) const
		{
			return Children[ChildRef.Index];
		}
		FORCEINLINE int32 GetElementCount() const
		{
			return Elements.Num();
		}
		FORCEINLINE int32 GetInclusiveElementCount() const
		{
			return InclusiveNumElements;
		}
		FORCEINLINE const ElementArrayType& GetElements() const
		{
			return Elements;
		}
		void ShrinkElements()
		{
			Elements.Shrink();
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if (Children[ChildRef.Index])
				{
					Children[ChildRef.Index]->ShrinkElements();
				}
			}
		}

		void ApplyOffset(const FVector& InOffset)
		{
			for (auto& Element : Elements)
			{
				OctreeSemantics::ApplyOffset(Element, InOffset);
			}
			
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if (Children[ChildRef.Index])
				{
					Children[ChildRef.Index]->ApplyOffset(InOffset);
				}
			}
		}
		
	private:

		/** The elements in this node. */
		mutable ElementArrayType Elements;

		/** The parent of this node. */
		const FNode* Parent;

		/** The children of the node. */
		mutable FNode* Children[8];

		/** The number of elements contained by the node and its child nodes. */
		mutable uint32 InclusiveNumElements : 31;

		/** true if the meshes should be added directly to the node, rather than subdividing when possible. */
		mutable uint32 bIsLeaf : 1;
	};



	/** A reference to an octree node, its context, and a read lock. */
	class FNodeReference
	{
	public:

		const FNode* Node;
		FOctreeNodeContext Context;

		/** Default constructor. */
		FNodeReference():
			Node(NULL),
			Context()
		{}

		/** Initialization constructor. */
		FNodeReference(const FNode* InNode,const FOctreeNodeContext& InContext):
			Node(InNode),
			Context(InContext)
		{}
	};	

	/** The default iterator allocator gives the stack enough inline space to contain a path and its siblings from root to leaf. */
	typedef TInlineAllocator<7 * (14 - 1) + 8> DefaultStackAllocator;

	/** An octree node iterator. */
	template<typename StackAllocator = DefaultStackAllocator>
	class TConstIterator
	{
	public:

		/** Pushes a child of the current node onto the stack of nodes to visit. */
		void PushChild(FOctreeChildNodeRef ChildRef)
		{
			FNodeReference* NewNode = new (NodeStack) FNodeReference;
			NewNode->Node = CurrentNode.Node->GetChild(ChildRef);
			CurrentNode.Context.GetChildContext(ChildRef, &NewNode->Context);
		}
		/** Pushes a child of the current node onto the stack of nodes to visit. */
		void PushChild(FOctreeChildNodeRef ChildRef,uint32 FullyInsideView,uint32 FullyOutsideView )
		{
			FNodeReference* NewNode = new (NodeStack) FNodeReference;
			NewNode->Node = CurrentNode.Node->GetChild(ChildRef);
			CurrentNode.Context.GetChildContext(ChildRef, &NewNode->Context);
			NewNode->Context.InCullBits = FullyInsideView;
			NewNode->Context.OutCullBits = FullyOutsideView;
		}
		/** Pushes a child of the current node onto the stack of nodes to visit. */
		void PushChild(FOctreeChildNodeRef ChildRef,const FOctreeNodeContext& Context )
		{
			new (NodeStack) FNodeReference(CurrentNode.Node->GetChild(ChildRef),Context);
		}


		/** Iterates to the next node. */
		void Advance()
		{
			if(NodeStack.Num())
			{
				CurrentNode = NodeStack[NodeStack.Num() - 1];
				NodeStack.RemoveAt(NodeStack.Num() - 1);
			}
			else
			{
				CurrentNode = FNodeReference();
			}
		}

		/** Checks if there are any nodes left to iterate over. */
		bool HasPendingNodes() const
		{
			return CurrentNode.Node != NULL;
		}

		/** Starts iterating at the root of an octree. */
		TConstIterator(const TOctree& Tree)
		:	CurrentNode(FNodeReference(&Tree.RootNode,Tree.RootNodeContext))
		{}

		/** Starts iterating at a particular node of an octree. */
		TConstIterator(const FNode& Node,const FOctreeNodeContext& Context):
			CurrentNode(FNodeReference(&Node,Context))
		{}

		// Accessors.
		const FNode& GetCurrentNode() const
		{
			return *CurrentNode.Node;
		}
		const FOctreeNodeContext& GetCurrentContext() const
		{
			return CurrentNode.Context;
		}

	private:

		/** The node that is currently being visited. */
		FNodeReference CurrentNode;
	
		/** The nodes which are pending iteration. */
		TArray<FNodeReference,StackAllocator> NodeStack;
	};

	/** Iterates over the elements in the octree that intersect a bounding box. */
	template<typename StackAllocator = DefaultStackAllocator>
	class TConstElementBoxIterator
	{
	public:

		/** Iterates to the next element. */
		void Advance()
		{
			++ElementIt;
			AdvanceToNextIntersectingElement();
		}

		/** Checks if there are any elements left to iterate over. */
		bool HasPendingElements() const
		{
			return NodeIt.HasPendingNodes();
		}

		/** Initialization constructor. */
		TConstElementBoxIterator(const TOctree& Tree,const FBoxCenterAndExtent& InBoundingBox)
		:	IteratorBounds(InBoundingBox)
		,	NodeIt(Tree)
		,	ElementIt(Tree.RootNode.GetElementIt())
		{
			ProcessChildren();
			AdvanceToNextIntersectingElement();
		}

		// Accessors.
		const ElementType& GetCurrentElement() const
		{
			return *ElementIt;
		}

	private:

		/** The bounding box to check for intersection with. */
		FBoxCenterAndExtent IteratorBounds;

		/** The octree node iterator. */
		TConstIterator<StackAllocator> NodeIt;

		/** The element iterator for the current node. */
		ElementConstIt ElementIt;

		/** Processes the children of the current node. */
		void ProcessChildren()
		{
			// Add the child nodes that intersect the bounding box to the node iterator's stack.
			const FNode& CurrentNode = NodeIt.GetCurrentNode();
			const FOctreeNodeContext& Context = NodeIt.GetCurrentContext();
			const FOctreeChildNodeSubset IntersectingChildSubset = Context.GetIntersectingChildren(IteratorBounds);
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if(IntersectingChildSubset.Contains(ChildRef) && CurrentNode.HasChild(ChildRef))
				{
					NodeIt.PushChild(ChildRef);
				}
			}
		}

		/** Advances the iterator to the next intersecting primitive, starting at a primitive in the current node. */
		void AdvanceToNextIntersectingElement()
		{
			check(NodeIt.HasPendingNodes()); // please don't call this after iteration has ended

			while (1)
			{
				ElementConstIt LocalElementIt(ElementIt);
				if (LocalElementIt)
				{
					FPlatformMisc::Prefetch( &(*LocalElementIt) );
					FPlatformMisc::Prefetch( &(*LocalElementIt), PLATFORM_CACHE_LINE_SIZE );

					// this is redundantly pull out of the while loop to prevent LHS on the iterator
					// Check if the current element intersects the bounding box.
					if(Intersect(OctreeSemantics::GetBoundingBox(*LocalElementIt),IteratorBounds))
					{
						// If it intersects, break out of the advancement loop.
						Move(ElementIt, LocalElementIt);
						return;
					}

					// Check if we've advanced past the elements in the current node.
					while(++LocalElementIt)
					{
						FPlatformMisc::Prefetch( &(*LocalElementIt), PLATFORM_CACHE_LINE_SIZE );

						// Check if the current element intersects the bounding box.
						if(Intersect(OctreeSemantics::GetBoundingBox(*LocalElementIt),IteratorBounds))

						{
							// If it intersects, break out of the advancement loop.
							Move(ElementIt, LocalElementIt);
							return;
						}
					}
				}
				// Advance to the next node.
				NodeIt.Advance();
				if(!NodeIt.HasPendingNodes())
				{
					Move(ElementIt, LocalElementIt);
					return;
				}
				ProcessChildren();
				// The element iterator can't be assigned to, but it can be replaced by Move.
				Move(ElementIt,NodeIt.GetCurrentNode().GetElementIt());
			}
		}
	};

	/**
	 * Adds an element to the octree.
	 * @param Element - The element to add.
	 * @return An identifier for the element in the octree.
	 */
	void AddElement(typename TTypeTraits<ElementType>::ConstInitType Element);

	/**
	 * Removes an element from the octree.
	 * @param ElementId - The element to remove from the octree.
	 */
	void RemoveElement(FOctreeElementId ElementId);

	void Destroy()
	{
		RootNode.~FNode();
		new (&RootNode) FNode(NULL);

		// this looks a bit @hacky, but FNode's destructor doesn't 
		// update TotalSizeBytes so better to set it to 0 than
		// not do nothing with it (would contain obviously false value)
		SetOctreeMemoryUsage(this, 0);
	}

	/** Accesses an octree element by ID. */
	ElementType& GetElementById(FOctreeElementId ElementId);

	/** Accesses an octree element by ID. */
	const ElementType& GetElementById(FOctreeElementId ElementId) const;

	/** Checks if given ElementId represents a valid Octree element */
	bool IsValidElementId(FOctreeElementId ElementId) const;

	/** Writes stats for the octree to the log. */
	void DumpStats();

	SIZE_T GetSizeBytes() const
	{
		return TotalSizeBytes;
	}

	float GetNodeLevelExtent(int32 Level) const
	{
		const int32 ClampedLevel = FMath::Clamp<uint32>(Level, 0, OctreeSemantics::MaxNodeDepth);
		return RootNodeContext.Bounds.Extent.X * FMath::Pow((1.0f + 1.0f / (float)FOctreeNodeContext::LoosenessDenominator) / 2.0f, ClampedLevel);
	}

	FBoxCenterAndExtent GetRootBounds() const
	{
		return RootNodeContext.Bounds;
	}

	void ShrinkElements()
	{
		RootNode.ShrinkElements();
	}

	/** 
	 * Apply an arbitrary offset to all elements in the tree 
	 * InOffset - offset to apply
	 * bGlobalOctree - hint that this octree is used as a boundless global volume, 
	 *  so only content will be shifted but not origin of the octree
	 */
	void ApplyOffset(const FVector& InOffset, bool bGlobalOctree = false);

	/** Initialization constructor. */
	TOctree(const FVector& InOrigin,float InExtent);

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	TOctree();

private:

	/** The octree's root node. */
	FNode RootNode;

	/** The octree's root node's context. */
	FOctreeNodeContext RootNodeContext;

	/** The extent of a leaf at the maximum allowed depth of the tree. */
	float MinLeafExtent;

	/** this function basically set TotalSizeBytes, but gives opportunity to 
	 *	include this Octree in memory stats */
	template<typename TElement,typename TSemantics>
	friend void SetOctreeMemoryUsage(class TOctree<TElement,TSemantics>* Octree, int32 NewSize);

	SIZE_T TotalSizeBytes;

	/** Adds an element to a node or its children. */
	void AddElementToNode(
		typename TTypeTraits<ElementType>::ConstInitType Element,
		const FNode& InNode,
		const FOctreeNodeContext& InContext
		);
};

#include "GenericOctree.inl"

