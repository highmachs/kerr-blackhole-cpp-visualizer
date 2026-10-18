// GLSL program loader.  Handles compute and graphics programs; reports
// compile / link errors with file names and line numbers preserved.

#pragma once

#include "glad/glad.h"
#include <string>

namespace kerr {

GLuint loadComputeProgram(const std::string& path,
                          const char* embeddedFallback = nullptr);
GLuint loadGraphicsProgram(const std::string& vertPath,
                           const std::string& fragPath,
                           const char* embeddedVertFallback = nullptr,
                           const char* embeddedFragFallback = nullptr);

// Utility: read a text file into a string.  Returns empty string on error.
std::string readFile(const std::string& path);

} // namespace kerr
