// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatsPropertyDetails.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorDirectories.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "IDetailPropertyRow.h"
#include "RHI.h"

#define LOCTEXT_NAMESPACE "ShaderFormatsPropertyDetails"

static FText GetFriendlyNameFromRHIName(const FString& InRHIName)
{
	FText FriendlyRHIName = FText::FromString(InRHIName);
	
	FName RHIName(*InRHIName, FNAME_Find);
	EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(RHIName);
	switch(Platform)
	{
		case SP_PCD3D_SM5:
			FriendlyRHIName = LOCTEXT("D3DSM5", "Direct3D 11+ (SM5)");
			break;
		case SP_PCD3D_SM4:
			FriendlyRHIName = LOCTEXT("D3DSM4", "Direct3D 10 (SM4)");
			break;
		case SP_PCD3D_ES3_1:
			FriendlyRHIName = LOCTEXT("D3DES31", "Direct3D (ES3.1, Mobile Preview)");
			break;
		case SP_PCD3D_ES2:
			FriendlyRHIName = LOCTEXT("D3DES2", "Direct3D (ES2, Mobile Preview)");
			break;
		case SP_OPENGL_SM4:
			FriendlyRHIName = LOCTEXT("OpenGL3", "OpenGL 3 (SM4)");
			break;
		case SP_OPENGL_SM5:
			FriendlyRHIName = LOCTEXT("OpenGL4", "OpenGL 4.3+ (SM5)");
			break;
		case SP_OPENGL_PCES2:
			FriendlyRHIName = LOCTEXT("OpenGLES2PC", "OpenGL (ES2, Mobile Preview)");
			break;
		case SP_OPENGL_PCES3_1:
			FriendlyRHIName = LOCTEXT("OpenGLES31PC", "OpenGL (ES3.1, Mobile Preview)");
			break;
		case SP_OPENGL_ES2_ANDROID:
		case SP_OPENGL_ES2_WEBGL:
		case SP_OPENGL_ES2_IOS:
			FriendlyRHIName = LOCTEXT("OpenGLES2", "OpenGLES 2 (Mobile)");
			break;
		case SP_OPENGL_ES31_EXT:
		case SP_OPENGL_ES3_1_ANDROID:
			FriendlyRHIName = LOCTEXT("OpenGLES31", "OpenGLES 3.1 (Mobile)");
			break;
		case SP_METAL:
			FriendlyRHIName = LOCTEXT("Metal", "iOS/tvOS Metal 1.0 (ES 3.1)");
			break;
		case SP_METAL_MRT:
			FriendlyRHIName = LOCTEXT("MetalMRT", "iOS/tvOS Metal 1.1+ (SM5, iOS/tvOS 9.0 or later)");
			break;
		case SP_METAL_SM4:
			FriendlyRHIName = LOCTEXT("MetalSM4", "Mac Metal 1.0 (SM4, OS X El Capitan 10.11.4 or later)");
			break;
		case SP_METAL_SM5:
			FriendlyRHIName = LOCTEXT("MetalSM5", "Mac Metal 1.1+ (SM5, OS X El Capitan 10.11.5 or later)");
			break;
		case SP_METAL_MACES3_1:
			FriendlyRHIName = LOCTEXT("MetalES3.1", "Mac Metal (ES3.1, Mobile Preview)");
			break;
		case SP_METAL_MACES2:
			FriendlyRHIName = LOCTEXT("MetalES2", "Mac Metal (ES2, Mobile Preview)");
			break;
		case SP_METAL_MRT_MAC:
			FriendlyRHIName = LOCTEXT("MetalMRTMac", "Mac Metal 1.1+ (SM5 MRT Preview, OS X El Capitan 10.11.5 or later)");
			break;
		case SP_VULKAN_SM4:
			FriendlyRHIName = LOCTEXT("VulkanSM4", "Vulkan (SM4)");
			break;
		case SP_VULKAN_SM5:
			FriendlyRHIName = LOCTEXT("VulkanSM5", "Vulkan (SM5)");
			break;
		case SP_VULKAN_PCES3_1:
		case SP_VULKAN_ES3_1_ANDROID:
			FriendlyRHIName = LOCTEXT("VulkanES31", "Vulkan (ES 3.1)");
			break;	
		default:
			break;
	}
	
	return FriendlyRHIName;
}

FShaderFormatsPropertyDetails::FShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder, FString InProperty, FString InTitle)
: DetailBuilder(InDetailBuilder)
, Property(InProperty)
, Title(InTitle)
{
	ShaderFormatsPropertyHandle = DetailBuilder->GetProperty(*Property);
	ensure(ShaderFormatsPropertyHandle.IsValid());
}

void FShaderFormatsPropertyDetails::SetOnUpdateShaderWarning(FSimpleDelegate const& Delegate)
{
	ShaderFormatsPropertyHandle->SetOnPropertyValueChanged(Delegate);
}

void FShaderFormatsPropertyDetails::CreateTargetShaderFormatsPropertyView(ITargetPlatform* TargetPlatform)
{
	check(TargetPlatform);
	DetailBuilder->HideProperty(ShaderFormatsPropertyHandle);
	
	// List of supported RHI's and selected targets
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllPossibleShaderFormats(ShaderFormats);
	
	IDetailCategoryBuilder& TargetedRHICategoryBuilder = DetailBuilder->EditCategory(*Title);
	
	for (const FName& ShaderFormat : ShaderFormats)
	{
		FText FriendlyShaderFormatName = GetFriendlyNameFromRHIName(ShaderFormat.ToString());
		
		FDetailWidgetRow& TargetedRHIWidgetRow = TargetedRHICategoryBuilder.AddCustomRow(FriendlyShaderFormatName);
		
		TargetedRHIWidgetRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FriendlyShaderFormatName)
				.Font(DetailBuilder->GetDetailFont())
			 ]
		 ]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FShaderFormatsPropertyDetails::OnTargetedRHIChanged, ShaderFormat)
			.IsChecked(this, &FShaderFormatsPropertyDetails::IsTargetedRHIChecked, ShaderFormat)
		 ];
	}
}


void FShaderFormatsPropertyDetails::OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName)
{
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	// Update the CVars with the selection
	{
		ShaderFormatsPropertyHandle->NotifyPreChange();
		for (void* RawPtr : RawPtrs)
		{
			TArray<FString>& Array = *(TArray<FString>*)RawPtr;
			if(InNewValue == ECheckBoxState::Checked)
			{
				Array.Add(InRHIName.ToString());
			}
			else
			{
				Array.Remove(InRHIName.ToString());
			}
		}
		ShaderFormatsPropertyHandle->NotifyPostChange();
	}
}


ECheckBoxState FShaderFormatsPropertyDetails::IsTargetedRHIChecked(FName InRHIName) const
{
	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	
	TArray<void*> RawPtrs;
	ShaderFormatsPropertyHandle->AccessRawData(RawPtrs);
	
	for(void* RawPtr : RawPtrs)
	{
		TArray<FString>& Array = *(TArray<FString>*)RawPtr;
		if(Array.Contains(InRHIName.ToString()))
		{
			CheckState = ECheckBoxState::Checked;
		}
	}
	return CheckState;
}

#undef LOCTEXT_NAMESPACE
