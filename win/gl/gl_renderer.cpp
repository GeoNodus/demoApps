#include "gl_renderer.h"

#include <GL/gl.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace demo {

namespace {

// --- What is missing from glext.h / wglext.h ------------------------------------------------------------------------
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

const GLenum GL_ARRAY_BUFFER = 0x8892;
const GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
const GLenum GL_STATIC_DRAW = 0x88E4;
const GLenum GL_FRAGMENT_SHADER = 0x8B30;
const GLenum GL_VERTEX_SHADER = 0x8B31;
const GLenum GL_COMPILE_STATUS = 0x8B81;
const GLenum GL_LINK_STATUS = 0x8B82;
const GLenum GL_INFO_LOG_LENGTH = 0x8B84;
const GLenum GL_PROGRAM_POINT_SIZE = 0x8642;
const GLenum GL_MULTISAMPLE = 0x809D;
const GLenum GL_SAMPLES = 0x80A9;

const int WGL_DRAW_TO_WINDOW_ARB = 0x2001;
const int WGL_ACCELERATION_ARB = 0x2003;
const int WGL_SUPPORT_OPENGL_ARB = 0x2010;
const int WGL_DOUBLE_BUFFER_ARB = 0x2011;
const int WGL_PIXEL_TYPE_ARB = 0x2013;
const int WGL_COLOR_BITS_ARB = 0x2014;
const int WGL_DEPTH_BITS_ARB = 0x2022;
const int WGL_STENCIL_BITS_ARB = 0x2023;
const int WGL_FULL_ACCELERATION_ARB = 0x2027;
const int WGL_TYPE_RGBA_ARB = 0x202B;
const int WGL_SAMPLE_BUFFERS_ARB = 0x2041;
const int WGL_SAMPLES_ARB = 0x2042;
const int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
const int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
const int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
const int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x0001;

typedef BOOL(WINAPI* PFN_wglChoosePixelFormatARB)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef HGLRC(WINAPI* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFN_wglSwapIntervalEXT)(int);

// GL functions above 1.1 (opengl32.dll exports only 1.1)
#define DEMO_GL_FUNCTIONS(X)                                                                                           \
    X(void, GenBuffers, (GLsizei n, GLuint * buffers))                                                                 \
    X(void, DeleteBuffers, (GLsizei n, const GLuint* buffers))                                                         \
    X(void, BindBuffer, (GLenum target, GLuint buffer))                                                                \
    X(void, BufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                              \
    X(void, GenVertexArrays, (GLsizei n, GLuint * arrays))                                                             \
    X(void, DeleteVertexArrays, (GLsizei n, const GLuint* arrays))                                                     \
    X(void, BindVertexArray, (GLuint array))                                                                           \
    X(void, EnableVertexAttribArray, (GLuint index))                                                                   \
    X(void, VertexAttribPointer,                                                                                       \
        (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer))            \
    X(GLuint, CreateShader, (GLenum type))                                                                             \
    X(void, DeleteShader, (GLuint shader))                                                                             \
    X(void, ShaderSource, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length))            \
    X(void, CompileShader, (GLuint shader))                                                                            \
    X(void, GetShaderiv, (GLuint shader, GLenum pname, GLint * params))                                                \
    X(void, GetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog))                    \
    X(GLuint, CreateProgram, ())                                                                                       \
    X(void, DeleteProgram, (GLuint program))                                                                           \
    X(void, AttachShader, (GLuint program, GLuint shader))                                                             \
    X(void, LinkProgram, (GLuint program))                                                                             \
    X(void, GetProgramiv, (GLuint program, GLenum pname, GLint * params))                                              \
    X(void, GetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog))                  \
    X(void, UseProgram, (GLuint program))                                                                              \
    X(GLint, GetUniformLocation, (GLuint program, const GLchar* name))                                                 \
    X(void, UniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value))              \
    X(void, Uniform4fv, (GLint location, GLsizei count, const GLfloat* value))

struct GLFunctions
{
#define DEMO_GL_DECLARE(ret, name, args) ret(APIENTRY* name) args = nullptr;
    DEMO_GL_FUNCTIONS(DEMO_GL_DECLARE)
#undef DEMO_GL_DECLARE

    bool load()
    {
        bool ok = true;
#define DEMO_GL_LOAD(ret, name, args)                                                                                  \
    name = reinterpret_cast<decltype(name)>(getProc("gl" #name));                                                      \
    ok = ok && name != nullptr;
        DEMO_GL_FUNCTIONS(DEMO_GL_LOAD)
#undef DEMO_GL_LOAD
        return ok;
    }

    static PROC getProc(const char* name)
    {
        // wglGetProcAddress reports a failure not only as nullptr but also as 1, 2, 3, −1
        PROC p = wglGetProcAddress(name);
        const intptr_t v = reinterpret_cast<intptr_t>(p);
        return v == 0 || v == 1 || v == 2 || v == 3 || v == -1 ? nullptr : p;
    }
};

// --- Shaders -------------------------------------------------------------------------------------------------------
// The same as in the Vulkan version (vulkan/shaders), in GLSL 3.30
const char* MESH_VS = R"(#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
uniform mat4 u_mvp;
uniform vec4 u_modelViewRows[3];
uniform vec4 u_params;
out vec3 v_viewPos;
out vec3 v_normal;
out vec4 v_color;
void main()
{
    vec4 p = vec4(a_position, 1.0);
    v_viewPos = vec3(dot(u_modelViewRows[0], p), dot(u_modelViewRows[1], p), dot(u_modelViewRows[2], p));
    // Normal — by the inverse transpose of model-view: a mat3 built from the rows is the transpose, its inverse is
    // what we need
    v_normal = inverse(mat3(u_modelViewRows[0].xyz, u_modelViewRows[1].xyz, u_modelViewRows[2].xyz)) * a_normal;
    v_color = a_color;
    gl_Position = u_mvp * p;
    gl_PointSize = u_params.x;
}
)";

const char* MESH_FS = R"(#version 330 core
in vec3 v_viewPos;
in vec3 v_normal;
in vec4 v_color;
uniform vec4 u_params;
out vec4 o_color;
void main()
{
    if (u_params.y < 0.5) {
        o_color = vec4(v_color.rgb, 1.0);
        return;
    }
    // Face normal from the position derivatives, turned toward the camera: depends neither on the polygon winding nor
    // on the frame axis directions. Back faces are lit with the flipped normal (two-sided lighting, as in the Houdini
    // viewport)
    vec3 toEye = normalize(-v_viewPos);
    vec3 faceN = normalize(cross(dFdx(v_viewPos), dFdy(v_viewPos)));
    if (dot(faceN, toEye) < 0.0)
        faceN = -faceN;
    vec3 n = dot(v_normal, v_normal) > 1e-30 ? normalize(v_normal) : faceN;
    if (dot(n, faceN) < 0.0)
        n = -n;
    // Light from the camera, slightly from above and the left, plus a highlight
    vec3 L = normalize(vec3(-0.3, 0.45, 1.0));
    float diffuse = max(dot(n, L), 0.0);
    float spec = pow(max(dot(n, normalize(L + toEye)), 0.0), 48.0) * 0.2;
    o_color = vec4(v_color.rgb * (0.18 + 0.82 * diffuse) + vec3(spec), 1.0);
}
)";

// Background — a vertical gradient drawn with one full-screen triangle
const char* BACKGROUND_VS = R"(#version 330 core
out float v_t;
void main()
{
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    v_t = p.y * 0.5 + 0.5; // 1 — top of the frame
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

const char* BACKGROUND_FS = R"(#version 330 core
in float v_t;
out vec4 o_color;
void main()
{
    o_color = vec4(mix(vec3(0.13, 0.13, 0.14), vec3(0.36, 0.38, 0.42), v_t), 1.0);
}
)";

const float POINT_SIZE = 4.0f;

class GLRenderer : public IRenderer
{
public:
    bool init(HWND hwnd, std::string& error) override;
    void shutdown() override;
    void resize(int width, int height) override;
    void setGeometry(const SceneGeometry& geometry) override;
    void render(const ViewParams& view) override;
    std::wstring description() const override { return m_description; }

private:
    bool createContext(std::string& error);
    GLuint compile(GLenum type, const char* source, std::string& error);
    GLuint link(const char* vs, const char* fs, std::string& error);
    void setConstants(const ShaderConstants& c);

    HWND m_hwnd = nullptr;
    HDC m_dc = nullptr;
    HGLRC m_rc = nullptr;
    GLFunctions gl;
    int m_width = 1, m_height = 1;
    std::wstring m_description = L"OpenGL";

    GLuint m_mesh_program = 0;
    GLuint m_background_program = 0;
    GLint m_u_mvp = -1, m_u_modelViewRows = -1, m_u_params = -1;
    GLuint m_vao = 0, m_empty_vao = 0, m_vbo = 0, m_ibo = 0;
    size_t m_tri_count = 0, m_line_count = 0; // indices
    std::vector<DrawBatch> m_batches;
};

bool GLRenderer::createContext(std::string& error)
{
    // WGL extensions (an MSAA pixel format, a 3.3 core context) are available only through an existing context, and a
    // window's pixel format can be set only once — hence a temporary window with a temporary context first
    PFN_wglChoosePixelFormatARB choosePixelFormat = nullptr;
    PFN_wglCreateContextAttribsARB createContextAttribs = nullptr;
    PFN_wglSwapIntervalEXT swapInterval = nullptr;

    PIXELFORMATDESCRIPTOR pfd{sizeof(pfd), 1};
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    {
        HWND dummy = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        HDC dc = GetDC(dummy);
        SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
        HGLRC rc = wglCreateContext(dc);
        if (rc && wglMakeCurrent(dc, rc)) {
            choosePixelFormat = reinterpret_cast<PFN_wglChoosePixelFormatARB>(
                GLFunctions::getProc("wglChoosePixelFormatARB"));
            createContextAttribs = reinterpret_cast<PFN_wglCreateContextAttribsARB>(
                GLFunctions::getProc("wglCreateContextAttribsARB"));
            swapInterval = reinterpret_cast<PFN_wglSwapIntervalEXT>(GLFunctions::getProc("wglSwapIntervalEXT"));
            wglMakeCurrent(nullptr, nullptr);
        }
        if (rc)
            wglDeleteContext(rc);
        ReleaseDC(dummy, dc);
        DestroyWindow(dummy);
    }

    m_dc = GetDC(m_hwnd);
    int format = 0;
    if (choosePixelFormat) {
        for (int samples : {4, 0}) {
            const int attribs[] = {WGL_DRAW_TO_WINDOW_ARB, 1, WGL_SUPPORT_OPENGL_ARB, 1, WGL_DOUBLE_BUFFER_ARB, 1,
                WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB, WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB, 32, WGL_DEPTH_BITS_ARB, 24, WGL_STENCIL_BITS_ARB, 8, WGL_SAMPLE_BUFFERS_ARB,
                samples ? 1 : 0, WGL_SAMPLES_ARB, samples, 0};
            UINT count = 0;
            if (choosePixelFormat(m_dc, attribs, nullptr, 1, &format, &count) && count > 0)
                break;
            format = 0;
        }
    }
    if (!format)
        format = ChoosePixelFormat(m_dc, &pfd);
    PIXELFORMATDESCRIPTOR chosen{};
    DescribePixelFormat(m_dc, format, sizeof(chosen), &chosen);
    if (!format || !SetPixelFormat(m_dc, format, &chosen)) {
        error = "No suitable OpenGL pixel format";
        return false;
    }

    if (createContextAttribs) {
        const int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
        m_rc = createContextAttribs(m_dc, nullptr, attribs);
    }
    if (!m_rc)
        m_rc = wglCreateContext(m_dc); // without WGL_ARB_create_context: fine if the driver gives 3.3 there too
    if (!m_rc || !wglMakeCurrent(m_dc, m_rc)) {
        error = "Cannot create an OpenGL context";
        return false;
    }
    if (swapInterval)
        swapInterval(1);

    int major = 0, minor = 0;
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (version)
        sscanf_s(version, "%d.%d", &major, &minor);
    if (major * 10 + minor < 33 || !gl.load()) {
        error = std::string("OpenGL 3.3 is required, the driver provides ") + (version ? version : "nothing");
        return false;
    }

    GLint samples = 0;
    glGetIntegerv(GL_SAMPLES, &samples);
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    char text[256];
    snprintf(text, sizeof(text), "OpenGL %s, %s, MSAA %dx", version, renderer ? renderer : "?", samples);
    m_description.assign(text, text + std::strlen(text));
    return true;
}

GLuint GLRenderer::compile(GLenum type, const char* source, std::string& error)
{
    const GLuint shader = gl.CreateShader(type);
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);
    GLint ok = 0;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 0 ? len : 1), '\0');
        gl.GetShaderInfoLog(shader, len, nullptr, log.data());
        error = "Shader compilation failed:\n" + log;
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint GLRenderer::link(const char* vs, const char* fs, std::string& error)
{
    const GLuint v = compile(GL_VERTEX_SHADER, vs, error);
    const GLuint f = v ? compile(GL_FRAGMENT_SHADER, fs, error) : 0;
    if (!f) {
        if (v)
            gl.DeleteShader(v);
        return 0;
    }
    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, v);
    gl.AttachShader(program, f);
    gl.LinkProgram(program);
    gl.DeleteShader(v);
    gl.DeleteShader(f);
    GLint ok = 0;
    gl.GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 0 ? len : 1), '\0');
        gl.GetProgramInfoLog(program, len, nullptr, log.data());
        error = "Shader link failed:\n" + log;
        gl.DeleteProgram(program);
        return 0;
    }
    return program;
}

bool GLRenderer::init(HWND hwnd, std::string& error)
{
    m_hwnd = hwnd;
    if (!createContext(error))
        return false;
    m_mesh_program = link(MESH_VS, MESH_FS, error);
    m_background_program = m_mesh_program ? link(BACKGROUND_VS, BACKGROUND_FS, error) : 0;
    if (!m_background_program)
        return false;
    m_u_mvp = gl.GetUniformLocation(m_mesh_program, "u_mvp");
    m_u_modelViewRows = gl.GetUniformLocation(m_mesh_program, "u_modelViewRows");
    m_u_params = gl.GetUniformLocation(m_mesh_program, "u_params");

    gl.GenVertexArrays(1, &m_empty_vao); // the core profile draws only with a bound VAO, even without attributes
    gl.GenVertexArrays(1, &m_vao);
    gl.GenBuffers(1, &m_vbo);
    gl.GenBuffers(1, &m_ibo);
    gl.BindVertexArray(m_vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo); // stored in the VAO
    gl.EnableVertexAttribArray(0);
    gl.EnableVertexAttribArray(1);
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, p)));
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, n)));
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, c)));
    gl.BindVertexArray(0);
    return true;
}

void GLRenderer::shutdown()
{
    if (!m_rc)
        return;
    if (gl.DeleteBuffers) {
        gl.DeleteBuffers(1, &m_vbo);
        gl.DeleteBuffers(1, &m_ibo);
        gl.DeleteVertexArrays(1, &m_vao);
        gl.DeleteVertexArrays(1, &m_empty_vao);
        gl.DeleteProgram(m_mesh_program);
        gl.DeleteProgram(m_background_program);
    }
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(m_rc);
    m_rc = nullptr;
    if (m_dc)
        ReleaseDC(m_hwnd, m_dc);
    m_dc = nullptr;
}

void GLRenderer::resize(int width, int height)
{
    m_width = width > 0 ? width : 1;
    m_height = height > 0 ? height : 1;
}

void GLRenderer::setGeometry(const SceneGeometry& g)
{
    // Indices one after another: triangles, lines, points
    std::vector<uint32_t> indices;
    indices.reserve(g.triangles.size() + g.lines.size() + g.points.size());
    indices.insert(indices.end(), g.triangles.begin(), g.triangles.end());
    indices.insert(indices.end(), g.lines.begin(), g.lines.end());
    indices.insert(indices.end(), g.points.begin(), g.points.end());
    m_tri_count = g.triangles.size();
    m_line_count = g.lines.size();
    m_batches = g.batches;

    gl.BindVertexArray(0);
    gl.BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(g.vertices.size() * sizeof(Vertex)), g.vertices.data(),
        GL_STATIC_DRAW);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)), indices.data(),
        GL_STATIC_DRAW);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gl.BindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLRenderer::setConstants(const ShaderConstants& c)
{
    gl.UniformMatrix4fv(m_u_mvp, 1, GL_FALSE, c.mvp);
    gl.Uniform4fv(m_u_modelViewRows, 3, &c.modelViewRows[0][0]);
    gl.Uniform4fv(m_u_params, 1, c.params);
}

void GLRenderer::render(const ViewParams& view)
{
    if (!m_rc)
        return;
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_MULTISAMPLE);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Background without depth test and depth writes
    glDisable(GL_DEPTH_TEST);
    gl.UseProgram(m_background_program);
    gl.BindVertexArray(m_empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    const Mat4 proj = perspective(view.fovy, float(m_width) / float(m_height), view.zNear, view.zFar, false);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_PROGRAM_POINT_SIZE);
    // The fill is pushed back so that lines and points on the surface do not flicker
    glPolygonOffset(1.0f, 1.0f);
    gl.UseProgram(m_mesh_program);
    gl.BindVertexArray(m_vao);

    // Per object: its own matrix; indices in the buffer — triangles, lines, points
    auto offset = [](size_t first) { return reinterpret_cast<void*>(first * sizeof(uint32_t)); };
    for (const DrawBatch& b : m_batches) {
        ShaderConstants c = shaderConstants(proj, view.view, b.model, POINT_SIZE);
        if (b.triangleCount) {
            c.params[1] = 1.0f;
            setConstants(c);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(b.triangleCount), GL_UNSIGNED_INT,
                offset(b.firstTriangle));
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
        if (!b.lineCount && !b.pointCount)
            continue;
        c.params[1] = 0.0f;
        setConstants(c);
        if (b.lineCount)
            glDrawElements(GL_LINES, static_cast<GLsizei>(b.lineCount), GL_UNSIGNED_INT,
                offset(m_tri_count + b.firstLine));
        if (b.pointCount)
            glDrawElements(GL_POINTS, static_cast<GLsizei>(b.pointCount), GL_UNSIGNED_INT,
                offset(m_tri_count + m_line_count + b.firstPoint));
    }

    gl.BindVertexArray(0);
    SwapBuffers(m_dc);
}

} // namespace

std::unique_ptr<IRenderer> createGLRenderer()
{
    return std::make_unique<GLRenderer>();
}

} // namespace demo
