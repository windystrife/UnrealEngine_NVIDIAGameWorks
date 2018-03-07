/** @file
    @brief OSVR display configuration

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com>

*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// 	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_osvr_display_configuration_h_GUID_BF32FB3A_205A_40D0_B774_F38F6419EC54
#define INCLUDED_osvr_display_configuration_h_GUID_BF32FB3A_205A_40D0_B774_F38F6419EC54

// Required for DLL linkage on Windows
#include <osvr/RenderKit/Export.h>

// Internal Includes
#include "osvr_compiler_tests.h"
#include "MonoPointMeshTypes.h"
#include "RGBPointMeshTypes.h"

// Library includes
#include <osvr/Util/Angles.h>

// Standard includes
#include <string>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <vector>

struct DisplayConfigurationParseException : public std::runtime_error {
    DisplayConfigurationParseException(const std::string& message)
        : std::runtime_error("Display descriptor parse error: " + message) {}
};

class OSVRDisplayConfiguration {
  public:
    enum DisplayMode {
        HORIZONTAL_SIDE_BY_SIDE,
        VERTICAL_SIDE_BY_SIDE,
        FULL_SCREEN
    };
    enum DistortionType {
        RGB_SYMMETRIC_POLYNOMIALS,
        MONO_POINT_SAMPLES,
        RGB_POINT_SAMPLES
    };

    OSVR_RENDERMANAGER_EXPORT OSVRDisplayConfiguration();
    OSVR_RENDERMANAGER_EXPORT
    OSVRDisplayConfiguration(const std::string& display_description);

    void OSVR_RENDERMANAGER_EXPORT
    parse(const std::string& display_description);

    void OSVR_RENDERMANAGER_EXPORT print() const;

    /// Read the property information.
    std::string OSVR_RENDERMANAGER_EXPORT getVendor() const;
    std::string OSVR_RENDERMANAGER_EXPORT getModel() const;
    std::string OSVR_RENDERMANAGER_EXPORT getVersion() const;
    std::string OSVR_RENDERMANAGER_EXPORT getNote() const;
    int OSVR_RENDERMANAGER_EXPORT getNumDisplays() const;

    int OSVR_RENDERMANAGER_EXPORT getDisplayTop() const;
    int OSVR_RENDERMANAGER_EXPORT getDisplayLeft() const;
    int OSVR_RENDERMANAGER_EXPORT getDisplayWidth() const;
    int OSVR_RENDERMANAGER_EXPORT getDisplayHeight() const;
    DisplayMode OSVR_RENDERMANAGER_EXPORT getDisplayMode() const;

    osvr::util::Angle getVerticalFOV() const;
    osvr::util::Angle getHorizontalFOV() const;
#if 0
    double OSVR_RENDERMANAGER_EXPORT getVerticalFOV() const;
    double OSVR_RENDERMANAGER_EXPORT getVerticalFOVRadians() const;
    double OSVR_RENDERMANAGER_EXPORT getHorizontalFOV() const;
    double OSVR_RENDERMANAGER_EXPORT getHorizontalFOVRadians() const;
#endif
    double OSVR_RENDERMANAGER_EXPORT getOverlapPercent() const;

#if 0
    double OSVR_RENDERMANAGER_EXPORT getPitchTilt() const;
#endif
    osvr::util::Angle getPitchTilt() const;

    double OSVR_RENDERMANAGER_EXPORT getIPDMeters() const;
    bool OSVR_RENDERMANAGER_EXPORT getSwapEyes() const;

    DistortionType OSVR_RENDERMANAGER_EXPORT getDistortionType() const {
        return m_distortionType;
    }
    /// deprecated
    std::string OSVR_RENDERMANAGER_EXPORT getDistortionTypeString() const;
    /// Only valid if getDistortionType() == MONO_POINT_SAMPLES
    osvr::renderkit::MonoPointDistortionMeshDescriptions
    getDistortionMonoPointMeshes() const;
    /// Only valid if getDistortionType() == RGB_POINT_SAMPLES
    osvr::renderkit::RGBPointDistortionMeshDescriptions
    getDistortionRGBPointMeshes() const;
    /// @name Polynomial distortion
    /// @brief Only valid if getDistortionType() == RGB_SYMMETRIC_POLYNOMIALS
    /// @{
    float OSVR_RENDERMANAGER_EXPORT getDistortionDistanceScaleX() const;
    float OSVR_RENDERMANAGER_EXPORT getDistortionDistanceScaleY() const;
    std::vector<float> const& getDistortionPolynomalRed() const;
    std::vector<float> const& getDistortionPolynomalGreen() const;
    std::vector<float> const& getDistortionPolynomalBlue() const;
    ///@}

    /// Structure holding the information for one eye.
    class EyeInfo {
      public:
        double m_CenterProjX = 0.5;
        double m_CenterProjY = 0.5;
        bool m_rotate180 = false;

        void OSVR_RENDERMANAGER_EXPORT print() const;
    };

    std::vector<EyeInfo> OSVR_RENDERMANAGER_EXPORT const& getEyes() const;

    struct Resolution {
        int width;
        int height;
        int video_inputs;
        DisplayMode display_mode;
    };
    Resolution const& activeResolution() const {
        return m_resolutions.at(m_activeResolution);
    }

  private:
    Resolution& activeResolution() {
        return m_resolutions.at(m_activeResolution);
    }

    std::string m_vendor;
    std::string m_model;
    std::string m_version;
    std::string m_note;

    osvr::util::Angle m_monocularHorizontalFOV;
    osvr::util::Angle m_monocularVerticalFOV;
    double m_overlapPercent;
    osvr::util::Angle m_pitchTilt;

    std::vector<Resolution> m_resolutions;

    double m_IPDMeters = 0.065; // 65 mm;
    bool m_swapEyes = false;

    // Distortion
    DistortionType m_distortionType;
    std::string m_distortionTypeString;
    osvr::renderkit::MonoPointDistortionMeshDescriptions
        m_distortionMonoPointMesh;
    osvr::renderkit::RGBPointDistortionMeshDescriptions
        m_distortionRGBPointMesh;
    float m_distortionDistanceScaleX;
    float m_distortionDistanceScaleY;
    std::vector<float> m_distortionPolynomialRed;
    std::vector<float> m_distortionPolynomialGreen;
    std::vector<float> m_distortionPolynomialBlue;

    // Rendering
    double m_rightRoll = 0.;
    double m_leftRoll = 0.;

    // Eyes
    std::vector<EyeInfo> m_eyes;

    // Active resolution
    size_t m_activeResolution = 0;
};

#endif // INCLUDED_osvr_display_configuration_h_GUID_BF32FB3A_205A_40D0_B774_F38F6419EC54
