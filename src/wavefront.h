#pragma once

#include "scene.h"

Scene loadSceneAsObj(const char* filename);
void dumpSceneAsObj(Scene const& s, const char* filename);

