#pragma once
// autopilot.hpp — pilota automatico deterministico.
//
// Serve a due cose: dimostrazioni riproducibili in --capture e test di
// gameplay (un verme fermo non esercita collisioni ne' raccolta).

#include "core/world.hpp"

namespace w3 {

WorldInput autopilotInput(const World& world);

}  // namespace w3
