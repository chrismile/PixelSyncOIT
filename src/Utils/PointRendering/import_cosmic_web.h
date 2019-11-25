#pragma once

#include "types.h"

namespace pl {

// Import a single brick of the cosmic web dataset into the model
void import_cosmic_web(const FileName &file_name, ParticleModel &model);

}

