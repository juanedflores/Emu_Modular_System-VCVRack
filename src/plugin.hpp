#pragma once
#include <rack.hpp>
#include "RNBO.h"
#include "widgets.hpp"
#include "utils.hpp"
#include "EMU_VCO.inl"

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelEMU_VCO;
