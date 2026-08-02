#pragma once

// Standard
#include <iosfwd>

// Parses the command line and runs the command it names.
//
// The report goes to out; progress, warnings, and errors go to err, so that
// redirecting the report to a file does not capture them.
//
// Returns the process exit code: 0 success or no differences, 1 differences
// found, 2 error.
int RunCommandLine(int argc, const char* const* argv, std::ostream& out, std::ostream& err);
