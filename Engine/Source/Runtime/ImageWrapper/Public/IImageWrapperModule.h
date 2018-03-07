// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class IImageWrapper;
enum class EImageFormat : int8;


/**
 * Interface for image wrapper modules.
 */
class IImageWrapperModule
	: public IModuleInterface
{
public:

	/**  
	 * Create a helper of a specific type 
	 *
	 * @param InFormat - The type of image we want to deal with
	 * @return The helper base class to manage the data
	 */
	virtual TSharedPtr<IImageWrapper> CreateImageWrapper(const EImageFormat InFormat) = 0;

	/**
	 * Detect image format by looking at the first few bytes of the compressed image data.
	 * You can call this method as soon as you have 8-16 bytes of compressed file content available.
	 *
	 * @param InCompressedData The raw image header.
	 * @param InCompressedSize The size of InCompressedData.
	 * @return the detected format or EImageFormat::Invalid if the method could not detect the image format.
	 */
	virtual EImageFormat DetectImageFormat(const void* InCompressedData, int32 InCompressedSize) = 0;

public:

	/** Virtual destructor. */
	virtual ~IImageWrapperModule() { }
};
