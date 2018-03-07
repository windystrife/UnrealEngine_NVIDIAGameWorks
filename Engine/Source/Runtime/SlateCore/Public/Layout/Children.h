// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Widgets/SNullWidget.h"
#include "SlotBase.h"
#include "Widgets/SWidget.h"

/**
 * FChildren is an interface that must be implemented by all child containers.
 * It allows iteration over a list of any Widget's children regardless of how
 * the underlying Widget happens to store its children.
 * 
 * FChildren is intended to be returned by the GetChildren() method.
 * 
 */
class SLATECORE_API FChildren
{
public:
	/** @return the number of children */
	virtual int32 Num() const = 0;
	/** @return pointer to the Widget at the specified Index. */
	virtual TSharedRef<SWidget> GetChildAt( int32 Index ) = 0;
	/** @return const pointer to the Widget at the specified Index. */
	virtual TSharedRef<const SWidget> GetChildAt( int32 Index ) const = 0;

protected:
	friend class SWidget;
	/** @return the const reference to the slot at the specified Index */
	virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const = 0;

protected:
	virtual ~FChildren(){}
};


/**
 * Widgets with no Children can return an instance of FNoChildren.
 * For convenience a shared instance SWidget::NoChildrenInstance can be used.
 */
class SLATECORE_API FNoChildren : public FChildren
{
public:
	virtual int32 Num() const override { return 0; }
	
	virtual TSharedRef<SWidget> GetChildAt( int32 ) override
	{
		// Nobody should be getting a child when there aren't any children.
		// We expect this to crash!
		check( false );
		return TSharedPtr<SWidget>(nullptr).ToSharedRef();
	}
	
	virtual TSharedRef<const SWidget> GetChildAt( int32 ) const override
	{
		// Nobody should be getting a child when there aren't any children.
		// We expect this to crash!
		check( false );
		return TSharedPtr<const SWidget>(nullptr).ToSharedRef();
	}

private:
	friend class SWidget;
	virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const override
	{
		check(false);
		static FSlotBase NullSlot;
		return NullSlot;
	}


};

/**
 * Widgets that will only have one child can return an instance of FOneChild.
 */
template <typename MixedIntoType>
class TSupportsOneChildMixin : public FChildren, public TSlotBase<MixedIntoType>
{
public:
	TSupportsOneChildMixin():TSlotBase<MixedIntoType>(){}

	virtual int32 Num() const override { return 1; }
	virtual TSharedRef<SWidget> GetChildAt( int32 ChildIndex ) override { check(ChildIndex == 0); return FSlotBase::GetWidget(); }
	virtual TSharedRef<const SWidget> GetChildAt( int32 ChildIndex ) const override { check(ChildIndex == 0); return FSlotBase::GetWidget(); }
private:
	virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const override { check(ChildIndex == 0); return *this; }
};

/**
 * For widgets that do not own their content, but are responsible for presenting someone else's content.
 * e.g. Tooltips are just presented by the owner window; not actually owned by it. They can go away at any time
 *      and then they'll just stop being shown.
 */
template <typename ChildType>
class TWeakChild : public FChildren
{
public:
	TWeakChild()
	: WidgetPtr()
	{
	}

	virtual int32 Num() const override { return WidgetPtr.IsValid() ? 1 : 0 ; }
	virtual TSharedRef<SWidget> GetChildAt( int32 ChildIndex ) override { check(ChildIndex == 0); return WidgetPtr.Pin().ToSharedRef(); }
	virtual TSharedRef<const SWidget> GetChildAt( int32 ChildIndex ) const override { check(ChildIndex == 0); return WidgetPtr.Pin().ToSharedRef(); }

private:
	virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const override { static FSlotBase NullSlot; check(ChildIndex == 0); return NullSlot; }

public:
	void AttachWidget(const TSharedPtr<SWidget>& InWidget)
	{
		WidgetPtr = InWidget;
	}

	TSharedRef<SWidget> GetWidget() const
	{
		TSharedPtr<SWidget> Widget = WidgetPtr.Pin();
		return (Widget.IsValid()) ? Widget.ToSharedRef() : SNullWidget::NullWidget;
	}

private:
	TWeakPtr<ChildType> WidgetPtr;
};

template <typename MixedIntoType>
class TSupportsContentAlignmentMixin
{
public:
	TSupportsContentAlignmentMixin(const EHorizontalAlignment InHAlign, const EVerticalAlignment InVAlign)
	: HAlignment( InHAlign )
	, VAlignment( InVAlign )
	{
		
	}

	MixedIntoType& HAlign( EHorizontalAlignment InHAlignment )
	{
		HAlignment = InHAlignment;
		return *(static_cast<MixedIntoType*>(this));
	}

	MixedIntoType& VAlign( EVerticalAlignment InVAlignment )
	{
		VAlignment = InVAlignment;
		return *(static_cast<MixedIntoType*>(this));
	}
	
	EHorizontalAlignment HAlignment;
	EVerticalAlignment VAlignment;
};

template <typename MixedIntoType>
class TSupportsContentPaddingMixin
{
public:
	MixedIntoType& Padding( const TAttribute<FMargin> InPadding )
	{
		SlotPadding = InPadding;
		return *(static_cast<MixedIntoType*>(this));
	}

	MixedIntoType& Padding( float Uniform )
	{
		SlotPadding = FMargin(Uniform);
		return *(static_cast<MixedIntoType*>(this));
	}

	MixedIntoType& Padding( float Horizontal, float Vertical )
	{
		SlotPadding = FMargin(Horizontal, Vertical);
		return *(static_cast<MixedIntoType*>(this));
	}

	MixedIntoType& Padding( float Left, float Top, float Right, float Bottom )
	{
		SlotPadding = FMargin(Left, Top, Right, Bottom);
		return *(static_cast<MixedIntoType*>(this));
	}

	TAttribute< FMargin > SlotPadding;
};

/** A slot that support alignment of content and padding */
class SLATECORE_API FSimpleSlot : public TSupportsOneChildMixin<FSimpleSlot>, public TSupportsContentAlignmentMixin<FSimpleSlot>, public TSupportsContentPaddingMixin<FSimpleSlot>
{
public:
	FSimpleSlot()
	: TSupportsOneChildMixin<FSimpleSlot>()
	, TSupportsContentAlignmentMixin<FSimpleSlot>(HAlign_Fill, VAlign_Fill)
	{
	}
};


/**
 * A generic FChildren that stores children along with layout-related information.
 * The type containing Widget* and layout info is specified by ChildType.
 * ChildType must have a public member SWidget* Widget;
 */
template<typename SlotType>
class TPanelChildren : public FChildren, private TIndirectArray< SlotType >
{
private:
	virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const override
	{
		return (*this)[ChildIndex];
	}

public:
	TPanelChildren()
		: bEmptying(false)
	{
	}
	
	virtual int32 Num() const override
	{
		return TIndirectArray<SlotType>::Num();
	}

	virtual TSharedRef<SWidget> GetChildAt( int32 Index ) override
	{
		return (*this)[Index].GetWidget();
	}

	virtual TSharedRef<const SWidget> GetChildAt( int32 Index ) const override
	{
		return (*this)[Index].GetWidget();
	}

	int32 Add( SlotType* Slot )
	{
		if ( bEmptying )
		{
			return INDEX_NONE;
		}

		return TIndirectArray< SlotType >::Add(Slot);
	}

	void RemoveAt( int32 Index )
	{
		if ( !bEmptying )
		{
			TIndirectArray< SlotType >::RemoveAt(Index);
		}
	}

	void Empty()
	{
		if ( !bEmptying )
		{
			TGuardValue<bool> GuardEmptying(bEmptying, true);

			TIndirectArray< SlotType >::Empty();
		}
	}

	void Insert(SlotType* Item, int32 Index)
	{
		if(!bEmptying)
		{
			TIndirectArray< SlotType >::Insert(Item, Index);
		}
	}

	void Move(int32 IndexToMove, int32 IndexToDestination)
	{
		// Since we're dealing with an Indirect Array, we can't move an item already in the array.
		if (IndexToMove > IndexToDestination) // going up
		{
			TIndirectArray< SlotType >::Insert(new SlotType(), IndexToDestination);
			TIndirectArray< SlotType >::Swap(IndexToDestination, IndexToMove + 1);
			TIndirectArray< SlotType >::RemoveAt(IndexToMove + 1);
		}
		else // going down
		{
			TIndirectArray< SlotType >::Insert(new SlotType(), IndexToDestination + 1);
			TIndirectArray< SlotType >::Swap(IndexToMove, IndexToDestination + 1);
			TIndirectArray< SlotType >::RemoveAt(IndexToMove);
		}
	}

	void Reserve( int32 NumToReserve )
	{
		TIndirectArray< SlotType >::Reserve(NumToReserve);
	}

	bool IsValidIndex( int32 Index ) const
	{
		return TIndirectArray< SlotType >::IsValidIndex( Index );
	}

	const SlotType& operator[](int32 Index) const { return TIndirectArray< SlotType >::operator[](Index); }

	SlotType& operator[](int32 Index) { return TIndirectArray< SlotType >::operator[](Index ); }

	template <class PREDICATE_CLASS>
	void Sort( const PREDICATE_CLASS& Predicate )
	{
		::Sort(TIndirectArray< SlotType >::GetData(), TIndirectArray<SlotType>::Num(), Predicate);
	}

	void Swap( int32 IndexA, int32 IndexB )
	{
		TIndirectArray< SlotType >::Swap(IndexA, IndexB);
	}

private:
	bool bEmptying;
};

/**
 * Some advanced widgets contain no layout information, and do not require slots.
 * Those widgets may wish to store a specialized type of child widget.
 * In those cases, using TSlotlessChildren is convenient.
 *
 * TSlotlessChildren should not be used for general-purpose widgets.
 */
template<typename ChildType>
class TSlotlessChildren : public FChildren, private TArray< TSharedRef<ChildType> >
{
private:
	virtual const FSlotBase& GetSlotAt(int32 ChildIndex) const override
	{
		// @todo slate : slotless children should be removed altogether; for now they return a fake slot.
		static FSlotBase NullSlot;
		return NullSlot;
	}

public:
	TSlotlessChildren()
	{
	}

	virtual int32 Num() const override
	{
		return TArray< TSharedRef<ChildType> >::Num();
	}

	virtual TSharedRef<SWidget> GetChildAt( int32 Index ) override
	{
		return (*this)[Index];
	}

	virtual TSharedRef<const SWidget> GetChildAt( int32 Index ) const override
	{
		return (*this)[Index];
	}

	int32 Add( const TSharedRef<ChildType>& Child )
	{
		return TArray< TSharedRef<ChildType> >::Add(Child);
	}

	void Empty()
	{
		TArray< TSharedRef<ChildType> >::Empty();
	}

	void Insert(const TSharedRef<ChildType>& Child, int32 Index)
	{
		TArray< TSharedRef<ChildType> >::Insert(Child, Index);
	}

	int32 Remove( const TSharedRef<ChildType>& Child )
	{
		const int32 NumFoundAndRemoved = TArray< TSharedRef<ChildType> >::Remove( Child );
		return NumFoundAndRemoved;
	}

	void RemoveAt( int32 Index )
	{
		TArray< TSharedRef<ChildType> >::RemoveAt( Index );
	}

	int32 Find( const TSharedRef<ChildType>& Item ) const
	{
		return TArray< TSharedRef<ChildType> >::Find( Item );
	}

	TArray< TSharedRef< ChildType > > AsArrayCopy() const
	{
		const int32 NumChildren = this->Num();
		TArray< TSharedRef< ChildType > > Copy(*this);
		return Copy;
	}

	const TSharedRef<ChildType>& operator[](int32 Index) const { return TArray< TSharedRef<ChildType> >::operator[](Index); }
	TSharedRef<ChildType>& operator[](int32 Index) { return TArray< TSharedRef<ChildType> >::operator[](Index); }

	template <class PREDICATE_CLASS>
	void Sort( const PREDICATE_CLASS& Predicate )
	{
		TArray< TSharedRef<ChildType> >::Sort( Predicate );
	}

	void Swap( int32 IndexA, int32 IndexB )
	{
		TIndirectArray< ChildType >::Swap(IndexA, IndexB);
	}
};
