#pragma once

// OpenGL 3.3 core renderer: a WGL context with MSAA, GL functions are loaded with wglGetProcAddress (no GLEW / GLAD)
#include "demo_app.h"

#include <memory>

namespace demo {

std::unique_ptr<IRenderer> createGLRenderer();

} // namespace demo
