// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class IPropertyHandle;

/** Base details customization for curve data interfaces. */
class FNiagaraDataInterfaceCurveDetailsBase : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	virtual void GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const = 0;
	virtual bool GetIsColorCurve() const { return false; }
	virtual bool GetDefaultAreCurvesVisible() const { return true; }
	virtual float GetDefaultHeight() const { return 120; }
};

/** Details customization for curve data interfaces. */
class FNiagaraDataInterfaceCurveDetails : public FNiagaraDataInterfaceCurveDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	virtual void GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const override;
};

/** Details customization for vector 2d curve data interfaces. */
class FNiagaraDataInterfaceVector2DCurveDetails : public FNiagaraDataInterfaceCurveDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	virtual void GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const override;
};

/** Details customization for vector curve data interfaces. */
class FNiagaraDataInterfaceVectorCurveDetails : public FNiagaraDataInterfaceCurveDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	virtual void GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const override;
};

/** Details customization for vector 4 curve data interfaces. */
class FNiagaraDataInterfaceVector4CurveDetails : public FNiagaraDataInterfaceCurveDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	virtual void GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const override;
};

/** Details customization for color curve data interfaces. */
class FNiagaraDataInterfaceColorCurveDetails : public FNiagaraDataInterfaceCurveDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	virtual void GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const override;
	virtual bool GetIsColorCurve() const override { return true; }
	virtual bool GetDefaultAreCurvesVisible() const override { return false; }
	virtual float GetDefaultHeight() const override { return 100; }
};