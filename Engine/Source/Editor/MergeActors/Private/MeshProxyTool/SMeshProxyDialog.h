// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FMeshProxyTool;
enum class ECheckBoxState : uint8;

/*-----------------------------------------------------------------------------
	SMeshProxyDialog
-----------------------------------------------------------------------------*/
class SMeshProxyDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshProxyDialog)
	{
	}
	SLATE_END_ARGS()

public:
	/** SWidget functions */
	void Construct(const FArguments& InArgs, FMeshProxyTool* InTool);

protected:
	/** ScreenSize accessors */
	TOptional<int32> GetScreenSize() const;
	void ScreenSizeChanged(int32 NewValue);		//used with editable text block (Simplygon)

	/** Recalculate Normals accessors */
	ECheckBoxState GetRecalculateNormals() const;
	void SetRecalculateNormals(ECheckBoxState NewValue);

	/** Hard Angle Threshold accessors */
	TOptional<float> GetHardAngleThreshold() const;
	bool HardAngleThresholdEnabled() const;
	void HardAngleThresholdChanged(float NewValue);

	/** Hole filling accessors */
	TOptional<int32> GetMergeDistance() const;
	void MergeDistanceChanged(int32 NewValue);

	/** TextureResolution accessors */
	void SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void SetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Export material properties acessors **/
	ECheckBoxState GetExportNormalMap() const;
	void SetExportNormalMap(ECheckBoxState NewValue);
	ECheckBoxState GetExportMetallicMap() const;
	void SetExportMetallicMap(ECheckBoxState NewValue);
	ECheckBoxState GetExportRoughnessMap() const;
	void SetExportRoughnessMap(ECheckBoxState NewValue);
	ECheckBoxState GetExportSpecularMap() const;
	void SetExportSpecularMap(ECheckBoxState NewValue);

private:
	/** Creates the geometry mode controls */
	void CreateLayout();

	int32 FindTextureResolutionEntryIndex(int32 InResolution) const;
	FText GetPropertyToolTipText(const FName& PropertyName) const;

private:
	FMeshProxyTool* Tool;

	TArray< TSharedPtr<FString> >	CuttingPlaneOptions;
	TArray< TSharedPtr<FString> >	TextureResolutionOptions;
};

