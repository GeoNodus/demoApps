#pragma once

// Shell of the demo apps in plain WinAPI: the main window with a toolbar (Open, Play, frame slider, status line), a
// child 3D view window with an orbit camera and a message loop with asynchronous GeoNodus cooks. Rendering is done by
// an IRenderer — OpenGL or Vulkan
#include "demo_math.h"
#include "gn_scene.h"

#include <windows.h>

#include <string>

namespace demo {

struct ViewParams
{
    Mat4 view; // world → camera space
    float fovy; // radians
    float zNear;
    float zFar;
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // hwnd — the 3D view window (its own, with CS_OWNDC). On failure — a message for the user
    virtual bool init(HWND hwnd, std::string& error) = 0;
    virtual void shutdown() = 0;
    virtual void resize(int width, int height) = 0;
    // New frame geometry: copied into GPU buffers
    virtual void setGeometry(const SceneGeometry& geometry) = 0;
    virtual void render(const ViewParams& view) = 0;
    // Text for the window title: API and device
    virtual std::wstring description() const = 0;
};

// Runs the app: title — the name in the window title, command line — [graph.json] [--play]
int runDemo(IRenderer& renderer, const wchar_t* title);

} // namespace demo
