// GeoNodus demo (OpenGL): shows a GeoNodus graph (.json, gnSaveGraph) and plays its animation.
// Command line: geonodus_gl [graph.json] [--play]
#include "gl_renderer.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    auto renderer = demo::createGLRenderer();
    return demo::runDemo(*renderer, L"GeoNodus OpenGL demo");
}
