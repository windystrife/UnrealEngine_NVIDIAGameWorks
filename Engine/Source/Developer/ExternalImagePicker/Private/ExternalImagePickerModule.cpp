// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "IExternalImagePickerModule.h"
#include "SExternalImagePicker.h"

/**
 * Public interface for splash screen editor module
 */
class FExternalImagePickerModule : public IExternalImagePickerModule
{
public:
	virtual TSharedRef<SWidget> MakeEditorWidget(const FExternalImagePickerConfiguration& InConfiguration) override
	{
		return SNew(SExternalImagePicker)
			.TargetImagePath(InConfiguration.TargetImagePath)
			.DefaultImagePath(InConfiguration.DefaultImagePath)
			.OnExternalImagePicked(InConfiguration.OnExternalImagePicked)
			.OnGetPickerPath(InConfiguration.OnGetPickerPath)
			.MaxDisplayedImageDimensions(InConfiguration.MaxDisplayedImageDimensions)
			.RequiresSpecificSize(InConfiguration.bRequiresSpecificSize)
			.RequiredImageDimensions(InConfiguration.RequiredImageDimensions)
			.Extensions(InConfiguration.FileExtensions);
	}
};

IMPLEMENT_MODULE(FExternalImagePickerModule, ExternalImagePicker)
