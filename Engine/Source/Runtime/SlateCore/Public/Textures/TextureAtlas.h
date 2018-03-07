// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"

/** 
 * Specifies how to handle texture atlas padding (when specified for the atlas). 
 * We only support one pixel of padding because we don't support mips or aniso filtering on atlas textures right now.
 */
enum ESlateTextureAtlasPaddingStyle
{
	/** Don't pad the atlas. */
	NoPadding,
	/** Dilate the texture by one pixel to pad the atlas. */
	DilateBorder,
	/** One pixel uniform padding border filled with zeros. */
	PadWithZero,
};

/** 
 * The type of thread that owns a texture atlas - this is the only thread that can safely update it 
 */
enum class ESlateTextureAtlasThreadId : uint8
{
	/** Owner thread is currently unknown */
	Unknown,
	/** Atlas is owned by the game thread */
	Game,
	/** Atlas is owned by the render thread */
	Render,
};

/** 
 * Get the correct atlas thread ID based on the thread we're currently in 
 */
SLATECORE_API ESlateTextureAtlasThreadId GetCurrentSlateTextureAtlasThreadId();

/**
 * Structure holding information about where a texture is located in the atlas. Inherits a linked-list interface.
 *
 * When a slot is occupied by texture data, the remaining space in the slot (if big enough) is split off into two new (smaller) slots,
 * building a tree of texture rectangles which, instead of being stored as a tree, are flattened into two linked-lists:
 *	- AtlastEmptySlots:	A linked-list of empty slots ready for texture data - iterates in same order as a depth-first-search on a tree
 *	- AtlasUsedSlots:	An unordered linked-list of slots containing texture data
 */
struct FAtlasedTextureSlot : public TIntrusiveLinkedList<FAtlasedTextureSlot>
{
	/** The X position of the character in the texture */
	uint32 X;
	/** The Y position of the character in the texture */
	uint32 Y;
	/** The width of the character */
	uint32 Width;
	/** The height of the character */
	uint32 Height;
	/** Uniform Padding. can only be zero or one. See ESlateTextureAtlasPaddingStyle. */
	uint8 Padding;


	FAtlasedTextureSlot( uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight, uint8 InPadding )
		: TIntrusiveLinkedList<FAtlasedTextureSlot>()
		, X(InX)
		, Y(InY)
		, Width(InWidth)
		, Height(InHeight)
		, Padding(InPadding)
	{
	}
};

/**
 * Base class texture atlases in Slate
 */
class SLATECORE_API FSlateTextureAtlas
{
public:
	FSlateTextureAtlas( uint32 InWidth, uint32 InHeight, uint32 InBytesPerPixel, ESlateTextureAtlasPaddingStyle InPaddingStyle )
		: AtlasData()
		, AtlasUsedSlots(NULL)
		, AtlasEmptySlots(NULL)
		, AtlasWidth( InWidth )
		, AtlasHeight( InHeight )
		, BytesPerPixel( InBytesPerPixel )
		, PaddingStyle( InPaddingStyle )
		, bNeedsUpdate( false )
		, AtlasOwnerThread( ESlateTextureAtlasThreadId::Unknown )
	{
		InitAtlasData();
	}

	virtual ~FSlateTextureAtlas();

	/**
	 * Clears atlas data
	 */
	void Empty();

	/**
	 * Adds a texture to the atlas
	 *
	 * @param TextureWidth	Width of the texture
	 * @param TextureHeight	Height of the texture
	 * @param Data			Raw texture data
	 */
	const FAtlasedTextureSlot* AddTexture( uint32 TextureWidth, uint32 TextureHeight, const TArray<uint8>& Data );

	/** @return the width of the atlas */
	uint32 GetWidth() const { return AtlasWidth; }
	/** @return the height of the atlas */
	uint32 GetHeight() const { return AtlasHeight; }

	/** Marks the texture as dirty and needing its rendering resources updated */
	void MarkTextureDirty();
	
	/**
	 * Updates the texture used for rendering if needed
	 */
	virtual void ConditionalUpdateTexture() = 0;
	
protected:
	/**
	 * Finds the optimal slot for a texture in the atlas
	 * 
	 * @param Width The width of the texture we are adding
	 * @param Height The height of the texture we are adding
	 */
	const FAtlasedTextureSlot* FindSlotForTexture( uint32 InWidth, uint32 InHeight );

	/**
	 * Creates enough space for a single texture the width and height of the atlas
	 */
	void InitAtlasData();

	struct FCopyRowData
	{
		/** Source data to copy */
		const uint8* SrcData;
		/** Place to copy data to */
		uint8* DestData;
		/** The row number to copy */
		uint32 SrcRow;
		/** The row number to copy to */
		uint32 DestRow;
		/** The width of a source row */
		uint32 RowWidth;
		/** The width of the source texture */
		uint32 SrcTextureWidth;
		/** The width of the dest texture */
		uint32 DestTextureWidth;
	};

	/**
	 * Copies a single row from a source texture to a dest texture,
	 * respecting the padding.
	 *
	 * @param CopyRowData	Information for how to copy a row
	 */
	void CopyRow( const FCopyRowData& CopyRowData );

	/**
	 * Zeros out a row in the dest texture (used with PaddingStyle == PadWithZero).
	 * respecting the padding.
	 *
	 * @param CopyRowData	Information for how to copy a row
	 */
	void ZeroRow( const FCopyRowData& CopyRowData );

	/** 
	 * Copies texture data into the atlas at a given slot
	 *
	 * @param SlotToCopyTo	The occupied slot in the atlas where texture data should be copied to
	 * @param Data			The data to copy into the atlas
	 */
	void CopyDataIntoSlot( const FAtlasedTextureSlot* SlotToCopyTo, const TArray<uint8>& Data );

private:
	/** Returns the amount of padding needed for the current padding style */
	FORCEINLINE int32 GetPaddingAmount() const
	{
		return (PaddingStyle == ESlateTextureAtlasPaddingStyle::NoPadding) ? 0 : 1;
	}
protected:
	/** Actual texture data contained in the atlas */
	TArray<uint8> AtlasData;
	/** The list of atlas slots pointing to used texture data in the atlas */
	FAtlasedTextureSlot* AtlasUsedSlots;
	/** The list of atlas slots pointing to empty texture data in the atlas */
	FAtlasedTextureSlot* AtlasEmptySlots;
	/** Width of the atlas */
	uint32 AtlasWidth;
	/** Height of the atlas */
	uint32 AtlasHeight;
	/** Bytes per pixel in the atlas */
	uint32 BytesPerPixel;
	/** Padding style */
	ESlateTextureAtlasPaddingStyle PaddingStyle;

	/** True if this texture needs to have its rendering resources updated */
	bool bNeedsUpdate;

	/** 
	 * The type of thread that owns this atlas - this is the only thread that can safely update it 
	 * NOTE: We don't use the thread ID here, as the render thread can be recreated if it gets suspended and resumed, giving it a new ID
	 */
	ESlateTextureAtlasThreadId AtlasOwnerThread;
};

/**
 * Interface to allow the Slate atlas visualizer to query atlas page information for an atlas provider
 */
class ISlateAtlasProvider
{
public:
	/** Virtual destructor */
	virtual ~ISlateAtlasProvider() {}

	/** Get the number of atlas pages this atlas provider has available when calling GetAtlasPageResource */
	virtual int32 GetNumAtlasPages() const = 0;

	/** Get the size of each atlas page */
	virtual FIntPoint GetAtlasPageSize() const = 0;

	/** Get the page resource for the given index (verify with GetNumAtlasPages) */ 
	virtual class FSlateShaderResource* GetAtlasPageResource(const int32 InIndex) const = 0;

	/** Do the atlas page resources only contain alpha information? This affects how the atlas visualizer will sample them */
	virtual bool IsAtlasPageResourceAlphaOnly() const = 0;
};
