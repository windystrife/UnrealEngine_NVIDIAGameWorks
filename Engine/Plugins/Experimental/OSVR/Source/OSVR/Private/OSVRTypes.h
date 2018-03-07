//
// Copyright 2014, 2015 Razer Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

FORCEINLINE FVector OSVR2FVector(const OSVR_Vec3& Vec3, float WorldToMetersScale)
{
	// OSVR: The coordinate system is right-handed, with X to the right, Y up, and Z near.
    // @todo automatically use meters-to-world-units scale here.
	return FVector(-float(Vec3.data[2]), float(Vec3.data[0]), float(Vec3.data[1])) * WorldToMetersScale;
}

FORCEINLINE FQuat OSVR2FQuat(const OSVR_Quaternion& Quat)
{
	//UE_LOG(LogActor, Warning, TEXT("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f"), osvrQuatGetX(&Quat), osvrQuatGetY(&Quat), osvrQuatGetZ(&Quat), osvrQuatGetW(&Quat));
	FQuat q = FQuat(-osvrQuatGetZ(&Quat), osvrQuatGetX(&Quat), osvrQuatGetY(&Quat), -osvrQuatGetW(&Quat));
	// Hydra is 15 degrees twisted.
	//q = q * FQuat(FVector(0, 1, 0), -15 * PI / 180);
	return q;
}

// Assumes row-major, left-handed matrix
FORCEINLINE FMatrix OSVR2FMatrix(const float in[16])
{
    return FMatrix(
        FPlane(in[0], in[1], in[2], in[3]),
        FPlane(in[4], in[5], in[6], in[7]),
        FPlane(in[8], in[9], in[10], in[11]),
        FPlane(in[12], in[13], in[14], in[15]));
}
