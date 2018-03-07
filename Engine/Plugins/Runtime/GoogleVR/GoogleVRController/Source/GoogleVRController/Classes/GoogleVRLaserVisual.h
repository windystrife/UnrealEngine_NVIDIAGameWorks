/* Copyright 2017 Google Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "Components/SceneComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "GoogleVRLaserVisual.generated.h"

/**
* IGoogleVRLaserVisual is an interface for visual representation used with UGoogleVRLaserVisualComponent.
*/

UCLASS(abstract)
class GOOGLEVRCONTROLLER_API UGoogleVRLaserVisual : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual UMaterialInstanceDynamic* GetLaserMaterial() const { check(0 && "You must override this"); return nullptr; };
	virtual void SetPointerDistance(float Distance, float WorldToMetersScale, FVector CameraLocation) { check(0 && "You must override this"); };

	virtual float GetMaxPointerDistance(float WorldToMetersScale) const { check(0 && "You must override this"); return 0.0f; };

	virtual void SetDefaultLaserDistance(float WorldToMetersScale) { check(0 && "You must override this"); };
	virtual void UpdateLaserDistance(float Distance) { check(0 && "You must override this"); };
	virtual void UpdateLaserCorrection(FVector Correction) { check(0 && "You must override this"); };

	virtual FMaterialSpriteElement* GetReticleSprite() const { check(0 && "You must override this"); return nullptr; };
	virtual float GetReticleSize() { check(0 && "You must override this"); return 0.0f; };
	virtual void SetDefaultReticleDistance(float WorldToMetersScale, FVector CameraLocation) { check(0 && "You must override this"); };
	virtual void UpdateReticleLocation(FVector Location, FVector OriginLocation, float WorldToMetersScale, FVector CameraLocation) { check(0 && "You must override this"); };

	virtual void SetSubComponentsEnabled(bool bNewEnabled) { check(0 && "You must override this"); };

};
