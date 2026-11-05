@echo off
rem Compiles the GLSL shaders into SPIR-V C arrays (shaders_spv.h, committed to the repository, so building the demo
rem needs no shader compiler). Needs glslangValidator: from the Vulkan SDK, or pass its path as the first argument.
setlocal
set "GLSLANG=%~1"
if "%GLSLANG%"=="" set "GLSLANG=glslangValidator"
cd /d "%~dp0"
"%GLSLANG%" -V --target-env vulkan1.0 --vn mesh_vert -o mesh_vert.h mesh.vert || exit /b 1
"%GLSLANG%" -V --target-env vulkan1.0 --vn mesh_frag -o mesh_frag.h mesh.frag || exit /b 1
"%GLSLANG%" -V --target-env vulkan1.0 --vn background_vert -o background_vert.h background.vert || exit /b 1
"%GLSLANG%" -V --target-env vulkan1.0 --vn background_frag -o background_frag.h background.frag || exit /b 1
copy /b mesh_vert.h + mesh_frag.h + background_vert.h + background_frag.h shaders_spv.h >nul
del mesh_vert.h mesh_frag.h background_vert.h background_frag.h
echo shaders_spv.h updated
