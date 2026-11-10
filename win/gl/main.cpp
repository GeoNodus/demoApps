// GeoNodus demo (OpenGL): shows a GeoNodus graph (.gng, gnSaveGraph) and plays its animation.
// Command line: geonodus_gl [graph.gng] [--play]
#include "gl_renderer.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    auto renderer = demo::createGLRenderer();
    return demo::runDemo(*renderer, L"GeoNodus OpenGL demo");
}
