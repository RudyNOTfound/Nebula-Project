#pragma once
#include <string>
#include "store.h"

void saveSnapshot(Store& db, const std::string& path);
void loadSnapshot(Store& db, const std::string& path);