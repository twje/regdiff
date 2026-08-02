#pragma once

#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <string>

// Formats a Registry value into a human-readable representation suitable for
// reports. Unknown Registry value types are displayed as hexadecimal.
std::string FormatRegistryValue(const RegistryValue& value);