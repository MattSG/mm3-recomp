# The DirectX Shader Compiler, which builds the SDL3 GPU renderer's HLSL into
# SPIR-V (Vulkan) and DXIL (Direct3D 12). The Windows SDK's dxc has no SPIR-V
# back end, so the pinned official release is downloaded unless
# XBOXRECOMP_DXC names a dxc built with SPIR-V support. (Its Linux build runs on
# glibc 2.35, Ubuntu 22.04; later releases need 2.38.)
#
# Sets XBOXRECOMP_DXC (cache) to the dxc executable.
if(XBOXRECOMP_DXC)
    return()
endif()
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)   # archive timestamps: extraction time
endif()
set(_dxc_release https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1)
if(CMAKE_HOST_WIN32)
    FetchContent_Declare(dxc
        URL ${_dxc_release}/dxc_2025_07_14.zip
        URL_HASH SHA256=9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4)
    set(_dxc_exe bin/x64/dxc.exe)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    FetchContent_Declare(dxc
        URL ${_dxc_release}/linux_dxc_2025_07_14.x86_64.tar.gz
        URL_HASH SHA256=f2213da1fc99dc8778c8823078e16ba97c7f80f86a1d4520ab1adf4b462bc48c)
    set(_dxc_exe bin/dxc)
else()
    message(FATAL_ERROR "No pinned dxc for this host: set XBOXRECOMP_DXC to a dxc built with SPIR-V support")
endif()
FetchContent_MakeAvailable(dxc)
set(_dxc "${dxc_SOURCE_DIR}/${_dxc_exe}")
execute_process(COMMAND "${_dxc}" --version RESULT_VARIABLE _dxc_status OUTPUT_QUIET ERROR_VARIABLE _dxc_error)
if(NOT _dxc_status EQUAL 0)
    message(FATAL_ERROR "The downloaded dxc does not run here (${_dxc_error}): "
                        "set XBOXRECOMP_DXC to a dxc built with SPIR-V support")
endif()
set(XBOXRECOMP_DXC "${_dxc}" CACHE FILEPATH "dxc with SPIR-V support" FORCE)
message(STATUS "dxc: ${XBOXRECOMP_DXC}")
