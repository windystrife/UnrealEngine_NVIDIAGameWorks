// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EdMode.h"
#include "IPersonaEditMode.h"
#include "IMeshPaintMode.h"

class IPersonaToolkit;
class FClothPainter;

class FClothingPaintEditMode : public IMeshPaintEdMode
{
public:
	FClothingPaintEditMode();
	virtual ~FClothingPaintEditMode();

	virtual void Initialize() override;
	virtual bool UsesToolkits() const override { return false; }
	virtual TSharedPtr<class FModeToolkit> GetToolkit() override;

	void SetPersonaToolKit(class TSharedPtr<IPersonaToolkit> InToolkit);

	virtual void Enter() override;
	virtual void Exit() override;

protected:

	TSharedPtr<FClothPainter> ClothPainter;

	class IPersonaPreviewScene* GetAnimPreviewScene() const;
	class TWeakPtr<IPersonaToolkit> PersonaToolkit;
};