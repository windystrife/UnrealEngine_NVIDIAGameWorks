// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SBoxPanel.h"
#include "IDetailCustomization.h"

/** Customization for importing vertex colors from a texture see SImportVertexColorOptions */
class FVertexColorImportOptionsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FVertexColorImportOptionsCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
private:
	TSharedRef<SHorizontalBox> CreateColorChannelWidget(TSharedRef<class IPropertyHandle> ChannelProperty);
};