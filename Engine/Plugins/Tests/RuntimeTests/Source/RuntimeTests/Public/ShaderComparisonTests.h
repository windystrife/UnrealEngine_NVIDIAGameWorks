#pragma once

#include "Tests/AutomationCommon.h"

/**
 * CompareAllShaderVars
 * Comparison automation test the determines which shader variables need extra precision
 */
IMPLEMENT_COMPLEX_AUTOMATION_CLASS(FCompareBasepassShaders, "System.Engine.CompareShaderPrecision", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled)