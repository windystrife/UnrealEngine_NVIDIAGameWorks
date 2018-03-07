//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSpatializationSourceSettings.h"

UPhononSpatializationSourceSettings::UPhononSpatializationSourceSettings()
	: SpatializationMethod(EIplSpatializationMethod::HRTF)
	, HrtfInterpolationMethod(EIplHrtfInterpolationMethod::NEAREST)
{}

#if WITH_EDITOR
bool UPhononSpatializationSourceSettings::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPhononSpatializationSourceSettings, HrtfInterpolationMethod)))
	{
		return ParentVal && SpatializationMethod == EIplSpatializationMethod::HRTF;
	}

	return ParentVal;
}
#endif