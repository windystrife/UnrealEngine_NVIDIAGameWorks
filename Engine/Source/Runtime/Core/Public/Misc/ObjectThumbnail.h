// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"

/**
 * Thumbnail compression interface for packages.  The engine registers a class that can compress and
 * decompress thumbnails that the package linker uses while loading and saving data.
 */
class FThumbnailCompressionInterface
{
public:

	/**
	 * Compresses an image
	 *
	 * @param	InUncompressedData	The uncompressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutCompressedData	[Out] Compressed image data
	 * @return	true if the image was compressed successfully, otherwise false if an error occurred
	 */
	virtual bool CompressImage( const TArray< uint8 >& InUncompressedData, const int32 InWidth, const int32 InHeight, TArray< uint8 >& OutCompressedData ) = 0;

	/**
	 * Decompresses an image
	 *
	 * @param	InCompressedData	The compressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutUncompressedData	[Out] Uncompressed image data
	 * @return	true if the image was decompressed successfully, otherwise false if an error occurred
	 */
	virtual bool DecompressImage( const TArray< uint8 >& InCompressedData, const int32 InWidth, const int32 InHeight, TArray< uint8 >& OutUncompressedData ) = 0;
};


/**
 * Thumbnail image data for an object.
 */
class CORE_API FObjectThumbnail
{
public:

	/**
	 * Static: Sets the thumbnail compressor to use when loading/saving packages.  The caller is
	 * responsible for the object's lifespan.
	 *
	 * @param	InThumbnailCompressor	A class derived from FThumbnailCompressionInterface.
	 */
	static void SetThumbnailCompressor( FThumbnailCompressionInterface* InThumbnailCompressor )
	{
		ThumbnailCompressor = InThumbnailCompressor;
	}

private:

	/** Static: Thumbnail compressor. */
	static FThumbnailCompressionInterface* ThumbnailCompressor;

public:

	/** Default constructor. */
	FObjectThumbnail();

	/** Returns the width of the thumbnail. */
	int32 GetImageWidth() const
	{
		return ImageWidth;
	}

	/** Returns the height of the thumbnail. */
	int32 GetImageHeight() const
	{
		return ImageHeight;
	}

	/** @return		the number of bytes in this thumbnail's compressed image data. */
	int32 GetCompressedDataSize() const
	{
		return CompressedImageData.Num();
	}

	/** Sets the image dimensions. */
	void SetImageSize( int32 InWidth, int32 InHeight )
	{
		ImageWidth = InWidth;
		ImageHeight = InHeight;
	}

	/** Returns true if the thumbnail was loaded from disk and not dynamically generated. */
	bool IsLoadedFromDisk(void) const { return bLoadedFromDisk; }

	/** Returns true if the thumbnail was saved AFTER custom-thumbnails for shared thumbnail asset types was supported. */
	bool IsCreatedAfterCustomThumbsEnabled (void) const { return bCreatedAfterCustomThumbForSharedTypesEnabled; }
	
	/** For newly generated custom thumbnails, mark it as valid in the future. */
	void SetCreatedAfterCustomThumbsEnabled(void) { bCreatedAfterCustomThumbForSharedTypesEnabled = true; }

	/** Returns true if the thumbnail is dirty and needs to be regenerated at some point. */
	bool IsDirty() const
	{
		return bIsDirty;
	}
	
	/** Marks the thumbnail as dirty. */
	void MarkAsDirty()
	{
		bIsDirty = true;
	}
	
	/** Access the image data in place (does not decompress). */
	TArray< uint8 >& AccessImageData()
	{
		return ImageData;
	}

	/** Access the image data in place (does not decompress) const version. */
	const TArray< uint8 >& AccessImageData() const
	{
		return ImageData;
	}

	/** Access the compressed image data. */
	TArray< uint8 >& AccessCompressedImageData()
	{
		return CompressedImageData;
	}

	/** Returns true if this is an empty thumbnail. */
	bool IsEmpty() const
	{
		return ImageWidth == 0 || ImageHeight == 0;
	}

	/** Returns uncompressed image data, decompressing it on demand if needed. */
	const TArray< uint8 >& GetUncompressedImageData() const;

	/** Serializer */
	void Serialize( FArchive& Ar );
	
	/** Compress image data. */
	void CompressImageData();

	/** Decompress image data. */
	void DecompressImageData();

	/**
	 * Calculates the memory usage of this FObjectThumbnail.
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountBytes( FArchive& Ar ) const;

	/**
	 * Calculates the amount of memory used by the compressed bytes array.
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountImageBytes_Compressed( FArchive& Ar ) const;

	/**
	 * Calculates the amount of memory used by the uncompressed bytes array.
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountImageBytes_Uncompressed( FArchive& Ar ) const;

	/** I/O operator */
	friend FArchive& operator<<( FArchive& Ar, FObjectThumbnail& Thumb )
	{
		if ( Ar.IsCountingMemory() )
		{
			Thumb.CountBytes(Ar);
		}
		else
		{
			Thumb.Serialize(Ar);
		}
		return Ar;
	}

	friend FArchive& operator<<( FArchive& Ar, const FObjectThumbnail& Thumb )
	{
		Thumb.CountBytes(Ar);
		return Ar;
	}

	/** Comparison operator */
	bool operator ==( const FObjectThumbnail& Other ) const
	{
		return	ImageWidth			== Other.ImageWidth
			&&	ImageHeight			== Other.ImageHeight
			&&	bIsDirty			== Other.bIsDirty
			&&	CompressedImageData	== Other.CompressedImageData;
	}

	bool operator !=( const FObjectThumbnail& Other ) const
	{
		return	ImageWidth			!= Other.ImageWidth
			||	ImageHeight			!= Other.ImageHeight
			||	bIsDirty			!= Other.bIsDirty
			||	CompressedImageData	!= Other.CompressedImageData;
	}

private:

	/** Thumbnail width (serialized) */
	int32 ImageWidth;

	/** Thumbnail height (serialized) */
	int32 ImageHeight;

	/** Compressed image data (serialized) */
	TArray< uint8 > CompressedImageData;

	/** Image data bytes */
	TArray< uint8 > ImageData;

	/** True if the thumbnail is dirty and should be regenerated at some point */
	bool bIsDirty;

	/** Whether the thumbnail has a backup on disk*/
	bool bLoadedFromDisk;

	/** Whether this was saved AFTER custom-thumbnails for shared thumbnail asset types was supported */
	bool bCreatedAfterCustomThumbForSharedTypesEnabled;
};


/** Maps an object's full name to a thumbnail */
typedef TMap< FName, FObjectThumbnail > FThumbnailMap;


/** Wraps an object's full name and thumbnail */
struct CORE_API FObjectFullNameAndThumbnail
{
	/** Full name of the object */
	FName ObjectFullName;
	
	/** Thumbnail data */
	const FObjectThumbnail* ObjectThumbnail;

	/** Offset in the file where the data is stored */
	int32 FileOffset;

	/** Constructor */
	FObjectFullNameAndThumbnail()
		: ObjectFullName(),
		  ObjectThumbnail( nullptr ),
		  FileOffset( 0 )
	{ }

	/** Constructor */
	FObjectFullNameAndThumbnail( const FName InFullName, const FObjectThumbnail* InThumbnail )
		: ObjectFullName( InFullName ),
		  ObjectThumbnail( InThumbnail ),
		  FileOffset( 0 )
	{ }

	/**
	 * Calculates the memory usage of this FObjectFullNameAndThumbnail.
	 *
	 * @param	Ar	the FArchiveCountMem (or similar) archive that will store the results of the memory usage calculation.
	 */
	void CountBytes( FArchive& Ar ) const;

	/** I/O operator */
	friend FArchive& operator<<( FArchive& Ar, FObjectFullNameAndThumbnail& NameThumbPair )
	{
		if ( Ar.IsCountingMemory() )
		{
			NameThumbPair.CountBytes(Ar);
		}
		else
		{
			Ar << NameThumbPair.ObjectFullName << NameThumbPair.FileOffset;
		}
		return Ar;
	}

	friend FArchive& operator<<( FArchive& Ar, const FObjectFullNameAndThumbnail& NameThumbPair )
	{
		NameThumbPair.CountBytes(Ar);
		return Ar;
	}
};
