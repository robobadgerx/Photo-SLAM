#ifndef OPENXR_APP_H
#define OPENXR_APP_H

#pragma once

#include <vector>       // For std::vector
#include <string>       // For std::string
#include <iostream>     // For std::cout, std::cerr, std::endl
#include <memory>       // For std::shared_ptr, std::unique_ptr
#include <algorithm>    // For std::sort, std::find, etc.
#include <cmath>        // For mathematical functions like std::sin, std::cos, M_PI
#include <map>          // For std::map
#include <set>          // For std::set
#include <unordered_map> // For std::unordered_map
#include <optional>     // Potentially useful for return types

#include <cstdio>       // For C-style I/O like snprintf, vprintf
#include <cstdarg>      // For va_list, va_start, va_end
#include <cstdbool>     // For bool (though built-in in C++)

// Required headers for OpenGL rendering, as well as for including openxr_platform
#include <GL/gl.h>
#include <GL/glext.h>

// Required headers for windowing, as well as the XrGraphicsBindingOpenGLXlibKHR struct.
#include <X11/Xlib.h>
#include <GL/glx.h>

// Assuming soil2 is a C library, include its header directly
#include <soil2/SOIL2.h>

#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
// Assuming openxr_headers are C headers
extern "C" {
    #include "openxr_headers/openxr.h"
    #include "openxr_headers/openxr_platform.h"
}

// SDL is a C library
extern "C" {
    #include "SDL.h"
    #include "SDL_events.h"
}

// --- OpenGL Function Loading ---
// Using macros for this pattern is still common, even in C++ codebases interacting heavily with C APIs like OpenGL
#define GL_DECL(TYPE, FUNC) static TYPE FUNC = nullptr;
#define LOAD_GL_FUNC(TYPE, FUNC) FUNC = reinterpret_cast<TYPE>(SDL_GL_GetProcAddress(#FUNC));

#define FOR_EACH_GL_FUNC(_)                                                                        \
    _(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                                             \
    _(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback)                                         \
    _(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                                                   \
    _(PFNGLCREATESHADERPROC, glCreateShader)                                                         \
    _(PFNGLSHADERSOURCEPROC, glShaderSource)                                                         \
    _(PFNGLCOMPILESHADERPROC, glCompileShader)                                                       \
    _(PFNGLGETSHADERIVPROC, glGetShaderiv)                                                           \
    _(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                                                 \
    _(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                                       \
    _(PFNGLATTACHSHADERPROC, glAttachShader)                                                         \
    _(PFNGLLINKPROGRAMPROC, glLinkProgram)                                                           \
    _(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                                         \
    _(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                                               \
    _(PFNGLDELETESHADERPROC, glDeleteShader)                                                         \
    _(PFNGLGENBUFFERSPROC, glGenBuffers)                                                             \
    _(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                                                   \
    _(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                                                   \
    _(PFNGLBINDBUFFERPROC, glBindBuffer)                                                             \
    _(PFNGLBUFFERDATAPROC, glBufferData)                                                             \
    _(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)                                           \
    _(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)                                   \
    _(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                                             \
    _(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                                                   \
    _(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                                         \
    _(PFNGLUSEPROGRAMPROC, glUseProgram)                                                             \
    _(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                                                 \
    _(PFNGLBLITNAMEDFRAMEBUFFERPROC, glBlitNamedFramebuffer)                                         \
    _(PFNGLUNIFORM3FPROC, glUniform3f)                                                               \
    _(PFNGLUNIFORM4FPROC, glUniform4f)                                                               \
    _(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                                                     \
    _(PFNGLUNIFORM1IPROC, glUniform1i)                                                               \
    _(PFNGLBLITFRAMEBUFFERPROC,     glBlitFramebuffer)                             \

// generates a global declaration for each gl func listed in FOR_EACH_GL_FUNC
FOR_EACH_GL_FUNC(GL_DECL)

static void init_gl_funcs() {
    // initializes each global declaration using SDL_GL_GetProcAddress
    FOR_EACH_GL_FUNC(LOAD_GL_FUNC)
}

// --- Constants and Utilities ---
inline constexpr double degrees_to_radians(double angle_degrees) {
    return (angle_degrees * M_PI / 180.0);
}
inline constexpr double radians_to_degrees(double angle_radians) {
    return (angle_radians * 180.0 / M_PI);
}

// we need an identity pose for creating spaces without offsets
static XrPosef identity_pose = {{0, 0, 0, 1.0f}, {0, 0, 0}};

constexpr int HAND_LEFT_INDEX = 0;
constexpr int HAND_RIGHT_INDEX = 1;
constexpr int HAND_COUNT = 2;


// =============================================================================
// math code adapted from
// https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/common/xr_linear.h
// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2016 Oculus VR, LLC.
// SPDX-License-Identifier: Apache-2.0
// =============================================================================

// Use enum class for stronger type safety
typedef enum
{
	GRAPHICS_VULKAN,
	GRAPHICS_OPENGL,
	GRAPHICS_OPENGL_ES
} GraphicsAPI;

// Keep struct as is, it's a POD type often used with C APIs
typedef struct XrMatrix4x4f {
    float m[16];
} XrMatrix4x4f;

// Use const references for input structs where appropriate
inline static void XrMatrix4x4f_CreateProjectionFov(XrMatrix4x4f* result,
                                                  GraphicsAPI graphicsApi,
                                                  const XrFovf& fov,
                                                  const float nearZ,
                                                  const float farZ) {
    const float tanAngleLeft = tanf(fov.angleLeft);
    const float tanAngleRight = tanf(fov.angleRight);
    const float tanAngleDown = tanf(fov.angleDown);
    const float tanAngleUp = tanf(fov.angleUp);
    const float tanAngleWidth = tanAngleRight - tanAngleLeft;
    const float tanAngleHeight =
        graphicsApi == GraphicsAPI::GRAPHICS_VULKAN ? (tanAngleDown - tanAngleUp) : (tanAngleUp - tanAngleDown);
    const float offsetZ =
        (graphicsApi == GraphicsAPI::GRAPHICS_OPENGL || graphicsApi == GraphicsAPI::GRAPHICS_OPENGL_ES) ? nearZ : 0;

    if (farZ <= nearZ) {
        result->m[0] = 2 / tanAngleWidth;
        result->m[4] = 0;
        result->m[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result->m[12] = 0;

        result->m[1] = 0;
        result->m[5] = 2 / tanAngleHeight;
        result->m[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result->m[13] = 0;

        result->m[2] = 0;
        result->m[6] = 0;
        result->m[10] = -1;
        result->m[14] = -(nearZ + offsetZ);

        result->m[3] = 0;
        result->m[7] = 0;
        result->m[11] = -1;
        result->m[15] = 0;
    } else {
        result->m[0] = 2 / tanAngleWidth;
        result->m[4] = 0;
        result->m[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result->m[12] = 0;

        result->m[1] = 0;
        result->m[5] = 2 / tanAngleHeight;
        result->m[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result->m[13] = 0;

        result->m[2] = 0;
        result->m[6] = 0;
        result->m[10] = -(farZ + offsetZ) / (farZ - nearZ);
        result->m[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

        result->m[3] = 0;
        result->m[7] = 0;
        result->m[11] = -1;
        result->m[15] = 0;
    }
}

inline static void XrMatrix4x4f_CreateFromQuaternion(XrMatrix4x4f* result, const XrQuaternionf& quat) {
    const float x2 = quat.x + quat.x;
    const float y2 = quat.y + quat.y;
    const float z2 = quat.z + quat.z;
    const float xx2 = quat.x * x2;
    const float yy2 = quat.y * y2;
    const float zz2 = quat.z * z2;
    const float yz2 = quat.y * z2;
    const float wx2 = quat.w * x2;
    const float xy2 = quat.x * y2;
    const float wz2 = quat.w * z2;
    const float xz2 = quat.x * z2;
    const float wy2 = quat.w * y2;

    result->m[0] = 1.0f - yy2 - zz2;
    result->m[1] = xy2 + wz2;
    result->m[2] = xz2 - wy2;
    result->m[3] = 0.0f;
    result->m[4] = xy2 - wz2;
    result->m[5] = 1.0f - xx2 - zz2;
    result->m[6] = yz2 + wx2;
    result->m[7] = 0.0f;
    result->m[8] = xz2 + wy2;
    result->m[9] = yz2 - wx2;
    result->m[10] = 1.0f - xx2 - yy2;
    result->m[11] = 0.0f;
    result->m[12] = 0.0f;
    result->m[13] = 0.0f;
    result->m[14] = 0.0f;
    result->m[15] = 1.0f;
}

inline static void XrMatrix4x4f_CreateTranslation(XrMatrix4x4f* result, float x, float y, float z) {
    result->m[0] = 1.0f; result->m[1] = 0.0f; result->m[2] = 0.0f; result->m[3] = 0.0f;
    result->m[4] = 0.0f; result->m[5] = 1.0f; result->m[6] = 0.0f; result->m[7] = 0.0f;
    result->m[8] = 0.0f; result->m[9] = 0.0f; result->m[10] = 1.0f; result->m[11] = 0.0f;
    result->m[12] = x;    result->m[13] = y;    result->m[14] = z;    result->m[15] = 1.0f;
}

inline static void XrMatrix4x4f_Multiply(XrMatrix4x4f* result, const XrMatrix4x4f& a, const XrMatrix4x4f& b) {
    result->m[0] = a.m[0] * b.m[0] + a.m[4] * b.m[1] + a.m[8] * b.m[2] + a.m[12] * b.m[3];
    result->m[1] = a.m[1] * b.m[0] + a.m[5] * b.m[1] + a.m[9] * b.m[2] + a.m[13] * b.m[3];
    result->m[2] = a.m[2] * b.m[0] + a.m[6] * b.m[1] + a.m[10] * b.m[2] + a.m[14] * b.m[3];
    result->m[3] = a.m[3] * b.m[0] + a.m[7] * b.m[1] + a.m[11] * b.m[2] + a.m[15] * b.m[3];
    result->m[4] = a.m[0] * b.m[4] + a.m[4] * b.m[5] + a.m[8] * b.m[6] + a.m[12] * b.m[7];
    result->m[5] = a.m[1] * b.m[4] + a.m[5] * b.m[5] + a.m[9] * b.m[6] + a.m[13] * b.m[7];
    result->m[6] = a.m[2] * b.m[4] + a.m[6] * b.m[5] + a.m[10] * b.m[6] + a.m[14] * b.m[7];
    result->m[7] = a.m[3] * b.m[4] + a.m[7] * b.m[5] + a.m[11] * b.m[6] + a.m[15] * b.m[7];
    result->m[8] = a.m[0] * b.m[8] + a.m[4] * b.m[9] + a.m[8] * b.m[10] + a.m[12] * b.m[11];
    result->m[9] = a.m[1] * b.m[8] + a.m[5] * b.m[9] + a.m[9] * b.m[10] + a.m[13] * b.m[11];
    result->m[10] = a.m[2] * b.m[8] + a.m[6] * b.m[9] + a.m[10] * b.m[10] + a.m[14] * b.m[11];
    result->m[11] = a.m[3] * b.m[8] + a.m[7] * b.m[9] + a.m[11] * b.m[10] + a.m[15] * b.m[11];
    result->m[12] = a.m[0] * b.m[12] + a.m[4] * b.m[13] + a.m[8] * b.m[14] + a.m[12] * b.m[15];
    result->m[13] = a.m[1] * b.m[12] + a.m[5] * b.m[13] + a.m[9] * b.m[14] + a.m[13] * b.m[15];
    result->m[14] = a.m[2] * b.m[12] + a.m[6] * b.m[13] + a.m[10] * b.m[14] + a.m[14] * b.m[15];
    result->m[15] = a.m[3] * b.m[12] + a.m[7] * b.m[13] + a.m[11] * b.m[14] + a.m[15] * b.m[15];
}

// Invert typically takes the source matrix as input
inline static void XrMatrix4x4f_Invert(XrMatrix4x4f* result, const XrMatrix4x4f& src) {
    result->m[0] = src.m[0]; result->m[1] = src.m[4]; result->m[2] = src.m[8]; result->m[3] = 0.0f;
    result->m[4] = src.m[1]; result->m[5] = src.m[5]; result->m[6] = src.m[9]; result->m[7] = 0.0f;
    result->m[8] = src.m[2]; result->m[9] = src.m[6]; result->m[10] = src.m[10]; result->m[11] = 0.0f;
    result->m[12] = -(src.m[0] * src.m[12] + src.m[1] * src.m[13] + src.m[2] * src.m[14]);
    result->m[13] = -(src.m[4] * src.m[12] + src.m[5] * src.m[13] + src.m[6] * src.m[14]);
    result->m[14] = -(src.m[8] * src.m[12] + src.m[9] * src.m[13] + src.m[10] * src.m[14]);
    result->m[15] = 1.0f;
}

// Use const references for vector and quaternion inputs
inline static void XrMatrix4x4f_CreateViewMatrix(XrMatrix4x4f* result,
                                               const XrVector3f& translation,
                                               const XrQuaternionf& rotation) {
    XrMatrix4x4f rotationMatrix;
    XrMatrix4x4f_CreateFromQuaternion(&rotationMatrix, rotation);

    XrMatrix4x4f translationMatrix;
    XrMatrix4x4f_CreateTranslation(&translationMatrix, translation.x, translation.y, translation.z);

    XrMatrix4x4f viewMatrix;
    XrMatrix4x4f_Multiply(&viewMatrix, translationMatrix, rotationMatrix); // Order matters! T * R

    XrMatrix4x4f_Invert(result, viewMatrix);
}

inline static void XrMatrix4x4f_CreateScale(XrMatrix4x4f* result, float x, float y, float z) {
    result->m[0] = x;    result->m[1] = 0.0f; result->m[2] = 0.0f; result->m[3] = 0.0f;
    result->m[4] = 0.0f; result->m[5] = y;    result->m[6] = 0.0f; result->m[7] = 0.0f;
    result->m[8] = 0.0f; result->m[9] = 0.0f; result->m[10] = z;    result->m[11] = 0.0f;
    result->m[12] = 0.0f; result->m[13] = 0.0f; result->m[14] = 0.0f; result->m[15] = 1.0f;
}

inline static void XrMatrix4x4f_CreateModelMatrix(XrMatrix4x4f* result,
                                                const XrVector3f& translation,
                                                const XrQuaternionf& rotation,
                                                const XrVector3f& scale) {
    XrMatrix4x4f scaleMatrix;
    XrMatrix4x4f_CreateScale(&scaleMatrix, scale.x, scale.y, scale.z);

    XrMatrix4x4f rotationMatrix;
    XrMatrix4x4f_CreateFromQuaternion(&rotationMatrix, rotation);

    XrMatrix4x4f translationMatrix;
    XrMatrix4x4f_CreateTranslation(&translationMatrix, translation.x, translation.y, translation.z);

    // Model = Translation * Rotation * Scale
    XrMatrix4x4f rotScaleMatrix;
    XrMatrix4x4f_Multiply(&rotScaleMatrix, rotationMatrix, scaleMatrix);
    XrMatrix4x4f_Multiply(result, translationMatrix, rotScaleMatrix);
}
// =============================================================================


// =============================================================================
// Forward declarations for OpenGL rendering code
// =============================================================================

// Assuming math_3d.h is compatible or provides necessary types/functions for C++
#define MATH_3D_IMPLEMENTATION
#include "math_3d.h" // Needs careful review if not C++ compatible

bool init_sdl_window(Display** xDisplay,
                     uint32_t* visualid,
                     GLXFBConfig* glxFBConfig,
                     GLXDrawable* glxDrawable,
                     GLXContext* glxContext,
                     int w,
                     int h);

int init_gl(uint32_t view_count,
            const std::vector<uint32_t>& swapchain_lengths, // Use const& for vector input
            std::vector<std::vector<GLuint>>& framebuffers, // Use reference for output vector
            GLuint& shader_program_id, // Use reference for output GLuint
            GLuint& VAO); // Use reference for output GLuint

void render_frame(int w,
                  int h,
                  GLuint shader_program_id,
                  GLuint VAO,
                  XrTime predictedDisplayTime,
                  int view_index,
                  const XrMatrix4x4f& projectionmatrix, // Pass matrix by const reference
                  const XrMatrix4x4f& viewmatrix, // Pass matrix by const reference
                  GLuint framebuffer,
                  GLuint image);

// =============================================================================

// --- OpenXR Helper Functions ---

// true if XrResult is a success code, else print error message and return false
bool xr_check(XrInstance instance, XrResult result, const char* format, ...) {
    if (XR_SUCCEEDED(result))
        return true;

    char resultString[XR_MAX_RESULT_STRING_SIZE];
    xrResultToString(instance, result, resultString);

    // Combine format string and result string before printing
    char formatRes[XR_MAX_RESULT_STRING_SIZE + 1024];
    snprintf(formatRes, sizeof(formatRes) - 1, "%s [%s] (%d)\n", format, resultString, result);
    formatRes[sizeof(formatRes) - 1] = '\0'; // Ensure null termination

    va_list args;
    va_start(args, format);
    // Using vprintf for simplicity with varargs, output to stderr
    vfprintf(stderr, formatRes, args);
    va_end(args);

    return false;
}


static void print_instance_properties(XrInstance instance) {
    XrResult result;
    XrInstanceProperties instance_props = {}; // Use C++ aggregate initialization
    instance_props.type = XR_TYPE_INSTANCE_PROPERTIES;
    // instance_props.next = NULL; // Already handled by zero-initialization

    result = xrGetInstanceProperties(instance, &instance_props);
    if (!xr_check(instance, result, "Failed to get instance info"))
        return;

    std::cout << "Runtime Name: " << instance_props.runtimeName << std::endl;
    std::cout << "Runtime Version: " << XR_VERSION_MAJOR(instance_props.runtimeVersion) << "."
              << XR_VERSION_MINOR(instance_props.runtimeVersion) << "."
              << XR_VERSION_PATCH(instance_props.runtimeVersion) << std::endl;
}

static void print_system_properties(const XrSystemProperties& system_properties) { // Pass by const reference
    std::cout << "System properties for system " << system_properties.systemId << ": \""
              << system_properties.systemName << "\", vendor ID " << system_properties.vendorId << std::endl;
    std::cout << "\tMax layers          : " << system_properties.graphicsProperties.maxLayerCount << std::endl;
    std::cout << "\tMax swapchain height: " << system_properties.graphicsProperties.maxSwapchainImageHeight << std::endl;
    std::cout << "\tMax swapchain width : " << system_properties.graphicsProperties.maxSwapchainImageWidth << std::endl;
    std::cout << "\tOrientation Tracking: " << (system_properties.trackingProperties.orientationTracking ? "true" : "false") << std::endl;
    std::cout << "\tPosition Tracking   : " << (system_properties.trackingProperties.positionTracking ? "true" : "false") << std::endl;
}

// Pass vector by const reference
static void print_viewconfig_view_info(uint32_t view_count, const std::vector<XrViewConfigurationView>& viewconfig_views) {
    for (uint32_t i = 0; i < view_count; ++i) {
        std::cout << "View Configuration View " << i << ":" << std::endl;
        std::cout << "\tResolution       : Recommended " << viewconfig_views[i].recommendedImageRectWidth
                  << "x" << viewconfig_views[i].recommendedImageRectHeight << ", Max: "
                  << viewconfig_views[i].maxImageRectWidth << "x" << viewconfig_views[i].maxImageRectHeight << std::endl;
        std::cout << "\tSwapchain Samples: Recommended: " << viewconfig_views[i].recommendedSwapchainSampleCount
                  << ", Max: " << viewconfig_views[i].maxSwapchainSampleCount << ")" << std::endl; // Fixed indexing [0]->[i]
    }
}

// returns the preferred swapchain format if it is supported
// else:
// - if fallback is true, return the first supported format
// - if fallback is false, return -1
static int64_t get_swapchain_format(XrInstance instance,
                                    XrSession session,
                                    int64_t preferred_format,
                                    bool fallback) {
    XrResult result;

    uint32_t swapchain_format_count = 0;
    result = xrEnumerateSwapchainFormats(session, 0, &swapchain_format_count, nullptr);
    if (!xr_check(instance, result, "Failed to get number of supported swapchain formats"))
        return -1;

    std::cout << "Runtime supports " << swapchain_format_count << " swapchain formats" << std::endl;
    if (swapchain_format_count == 0) return -1; // Handle case with zero formats

    std::vector<int64_t> swapchain_formats(swapchain_format_count);
    result = xrEnumerateSwapchainFormats(session, swapchain_format_count, &swapchain_format_count, swapchain_formats.data());
    if (!xr_check(instance, result, "Failed to enumerate swapchain formats"))
        return -1;

    int64_t chosen_format = -1;
    bool found_preferred = false;

    for (int64_t format : swapchain_formats) {
        std::cout << "Supported GL format: 0x" << std::hex << format << std::dec << std::endl;
        if (format == preferred_format) {
            chosen_format = format;
            found_preferred = true;
            std::cout << "Using preferred swapchain format 0x" << std::hex << chosen_format << std::dec << std::endl;
            break;
        }
    }

    if (!found_preferred && fallback) {
        chosen_format = swapchain_formats[0]; // Fallback to the first format
        std::cout << "Falling back to non-preferred swapchain format 0x" << std::hex << chosen_format << std::dec << std::endl;
    } else if (!found_preferred && !fallback) {
         std::cout << "Preferred swapchain format not found and fallback disabled." << std::endl;
         return -1; // Return -1 if preferred not found and fallback is false
    }

    return chosen_format;
}


static void print_api_layers() {
    uint32_t count = 0;
    XrResult result = xrEnumerateApiLayerProperties(0, &count, nullptr);
    if (!xr_check(nullptr, result, "Failed to enumerate api layer count"))
        return;

    if (count == 0) {
        std::cout << "No API layers found." << std::endl;
        return;
    }

    std::vector<XrApiLayerProperties> props(count);
    // Initialize `type` and `next` for each element
    for (auto& prop : props) {
        prop.type = XR_TYPE_API_LAYER_PROPERTIES;
        prop.next = nullptr;
    }


    result = xrEnumerateApiLayerProperties(count, &count, props.data());
    if (!xr_check(nullptr, result, "Failed to enumerate api layers"))
        return;

    std::cout << "API layers:" << std::endl;
    for (const auto& prop : props) { // Use range-based for loop
        std::cout << "\t" << prop.layerName << " v" << prop.layerVersion << ": " << prop.description << std::endl;
    }
    // No need to free 'props', std::vector handles it.
}


// functions belonging to extensions must be loaded with xrGetInstanceProcAddr before use
static PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
static bool load_extension_function_pointers(XrInstance instance) {
    XrResult result =
        xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                              reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetOpenGLGraphicsRequirementsKHR)); // C++ cast
    if (!xr_check(instance, result, "Failed to get OpenGL graphics requirements function!"))
        return false;

    return true;
}


// --- Helper Function (You MUST implement this correctly!) ---
Sophus::SE3f ConvertXrPoseToSophusSE3f(const XrPosef& xrPose) {
    // xrPose is likely World-to-View (Twv) in OpenXR coords (Y-up, RH, -Z fwd)
    // GaussianMapper::renderFromPose expects Camera-to-World (Tcw) in its coordinate system
    // (potentially Y-down, RH, +Z fwd like OpenCV)

    // 1. Convert XrPosef to Eigen/GLM matrix (Twv)
    Eigen::Quaternionf orientation(xrPose.orientation.w, xrPose.orientation.x, xrPose.orientation.y, xrPose.orientation.z);
    Eigen::Vector3f position(xrPose.position.x, xrPose.position.y, xrPose.position.z);
    Eigen::Matrix4f Twv_xr = Eigen::Matrix4f::Identity();
    Twv_xr.block<3, 3>(0, 0) = orientation.toRotationMatrix();
    Twv_xr.block<3, 1>(0, 3) = position;

    // 2. Invert to get View-to-World (Tvw_xr)
    Eigen::Matrix4f Tvw_xr = Twv_xr.inverse();

    // 3. Apply coordinate system transformation matrix (if needed)
    // Example: Transform from OpenXR (Y-up, -Z fwd) to OpenCV (Y-down, +Z fwd)
    Eigen::Matrix4f xr_to_cv_coord = Eigen::Matrix4f::Identity();
    xr_to_cv_coord(1, 1) = -1.0f; // Flip Y
    xr_to_cv_coord(2, 2) = -1.0f; // Flip Z

    // Tcw_cv = xr_to_cv_coord * Tvw_xr * xr_to_cv_coord.inverse();
    // Simplified if rotation applied first: Tcw_cv = xr_to_cv_coord * Tvw_xr
    Eigen::Matrix4f Tcw_cv_coords = xr_to_cv_coord * Tvw_xr;


    // 4. Convert Eigen::Matrix4f to Sophus::SE3f
    Eigen::Matrix3f R = Tcw_cv_coords.block<3, 3>(0, 0);
    Eigen::Vector3f t = Tcw_cv_coords.block<3, 1>(0, 3);

    return Sophus::SE3f(R, t);
}

// =============================================================================
// OpenGL rendering code
// =============================================================================

static SDL_Window* desktop_window = nullptr;
static SDL_GLContext gl_context = nullptr;
static GLuint texture = 0; // Texture ID

#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

// OpenGL Debug Callback - signature must match GL spec (C linkage)
extern "C" void GLAPIENTRY MessageCallback(GLenum source,
                                           GLenum type,
                                           GLuint id,
                                           GLenum severity,
                                           GLsizei length,
                                           const GLchar* message,
                                           const void* userParam) {
    // Use std::cerr for error messages
    std::cerr << "GL CALLBACK: " << (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "")
              << " type = 0x" << std::hex << type << ", severity = 0x" << severity
              << ", message = " << message << std::dec << std::endl;
}


bool init_sdl_window(Display** xDisplay,
                     uint32_t* visualid,
                     GLXFBConfig* glxFBConfig,
                     GLXDrawable* glxDrawable,
                     GLXContext* glxContext,
                     int w,
                     int h) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Unable to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set OpenGL context attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3); // Usually 3.3 is well-supported
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG); // Request debug context

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0); // Disable SDL's double buffering if OpenXR manages swapchains

    /* Create our window centered at half the VR resolution */
    desktop_window =
        SDL_CreateWindow("OpenXR C++ Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w,
                         h / 2, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!desktop_window) {
        std::cerr << "Unable to create SDL window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    gl_context = SDL_GL_CreateContext(desktop_window);
    if (!gl_context) {
         std::cerr << "Unable to create OpenGL context: " << SDL_GetError() << std::endl;
         SDL_DestroyWindow(desktop_window);
         SDL_Quit();
         return false;
    }

    init_gl_funcs(); // Load OpenGL function pointers after context creation

    // Check if debug context was successfully created and setup callback
    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT && glDebugMessageCallback) { // Check if function pointer is loaded
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Optional: makes debugging easier
        glDebugMessageCallback(MessageCallback, nullptr);
        std::cout << "Registered OpenGL debug callback." << std::endl;
    } else {
         std::cout << "OpenGL debug context not available or callback function failed to load." << std::endl;
    }


    SDL_GL_SetSwapInterval(0); // Disable vsync for VR (latency is key)

    // HACK? OpenXR wants us to report these values, so "work around" SDL a
    // bit and get the underlying glx stuff. This remains platform-specific (X11).
    // Error checking could be added here.
    *xDisplay = XOpenDisplay(nullptr);
    if (!(*xDisplay)) {
        std::cerr << "Failed to open X Display." << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }
    *glxContext = glXGetCurrentContext();
    *glxDrawable = glXGetCurrentDrawable();
    // Note: Obtaining the GLXFBConfig used by SDL might be complex/unreliable.
    // OpenXR often needs the *specific* FBConfig. This part might require
    // more platform-specific GLX setup *before* SDL window creation if strict
    // FBConfig matching is needed by the runtime. For now, assume this is sufficient.
    // A placeholder or dummy value might be needed for glxFBConfig if not easily obtained.
    *glxFBConfig = nullptr; // Indicate we didn't reliably get this via SDL


    return true;
}


static const char* vertexshader =
    "#version 330 core\n"
    "#extension GL_ARB_explicit_uniform_location : require\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 5) in vec2 aTexCoord; // Changed location from 5 to 1 for convention\n"
    "layout(location = 2) uniform mat4 model;\n"
    "layout(location = 3) uniform mat4 view;\n"
    "layout(location = 4) uniform mat4 proj;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "   gl_Position = proj * view * model * vec4(aPos, 1.0);\n" // Simplified vec4 construction
    "   TexCoord = aTexCoord;\n"
    "}\n";

static const char* fragmentshader =
    "#version 330 core\n"
    "#extension GL_ARB_explicit_uniform_location : require\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "layout(location = 6) uniform sampler2D texture1; // Explicit location for sampler\n"
    "void main() {\n"
    "   FragColor = texture(texture1, TexCoord);\n"
    "}\n";



int init_gl(uint32_t view_count,
            const std::vector<uint32_t>& swapchain_lengths, // Use const&
            std::vector<std::vector<GLuint>>& framebuffers, // Use &
            GLuint& shader_program_id, // Use &
            GLuint& VAO) { // Use &

    /* Allocate resources that we use for our own rendering. */
    framebuffers.resize(view_count); // Resize outer vector
    for (uint32_t i = 0; i < view_count; ++i) {
        framebuffers[i].resize(swapchain_lengths[i]); // Resize inner vector
        glGenFramebuffers(swapchain_lengths[i], framebuffers[i].data()); // Get data pointer
    }

    // --- Shader Compilation ---
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader_id, 1, &vertexshader, nullptr);
    glCompileShader(vertex_shader_id);
    GLint success;
    glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader_id, 512, nullptr, info_log);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << info_log << std::endl;
        glDeleteShader(vertex_shader_id);
        return 1;
    } else {
        std::cout << "Successfully compiled vertex shader!" << std::endl;
    }


    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader_id, 1, &fragmentshader, nullptr);
    glCompileShader(fragment_shader_id);
    glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader_id, 512, nullptr, info_log);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << info_log << std::endl;
        glDeleteShader(vertex_shader_id); // Clean up previous shader
        glDeleteShader(fragment_shader_id);
        return 1;
    } else {
         std::cout << "Successfully compiled fragment shader!" << std::endl;
    }


    // --- Shader Program Linking ---
    shader_program_id = glCreateProgram();
    glAttachShader(shader_program_id, vertex_shader_id);
    glAttachShader(shader_program_id, fragment_shader_id);
    glLinkProgram(shader_program_id);
    glGetProgramiv(shader_program_id, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(shader_program_id, 512, nullptr, info_log);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << info_log << std::endl;
         // Clean up shaders even on link failure
        glDeleteShader(vertex_shader_id);
        glDeleteShader(fragment_shader_id);
        shader_program_id = 0; // Reset ID
        return 1;
    } else {
        std::cout << "Successfully linked shader program!" << std::endl;
    }

    // Shaders are linked, no longer needed
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);


    // --- Vertex Data and Buffers ---
    // Cube vertices (Pos: 3 floats, TexCoord: 2 floats)
    float vertices[] = {
        // positions         // texture Coords
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, 1.0f, 0.0f,

         0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
         0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 1.0f
    };

    GLuint VBO; // Only one VBO needed
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // Use STATIC_DRAW if data doesn't change

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr); // Use nullptr for offset 0
    glEnableVertexAttribArray(0);

    // Texture coord attribute (location = 5 in shader, corresponds to index 5 here)
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float))); // C++ cast
    glEnableVertexAttribArray(5);

    // Unbind VBO and VAO (optional but good practice)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    // --- Texture Loading ---
    int width, height;
    // Ensure "test.png" is in the correct path relative to the executable
    unsigned char* image = SOIL_load_image("test.png", &width, &height, 0, SOIL_LOAD_RGBA);
    if (image == nullptr) {
        std::cerr << "Failed to load texture 'test.png': " << SOIL_last_result() << std::endl;
        // Delete framebuffers
        for (uint32_t i = 0; i < view_count; ++i) {
             if (!framebuffers[i].empty()) {
                 glDeleteFramebuffers(framebuffers[i].size(), framebuffers[i].data());
             }
        }
        VAO = 0;
        shader_program_id = 0;
        return 1;
    }
     std::cout << "Loaded texture 'test.png' (" << width << "x" << height << ")" << std::endl;


    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    SOIL_free_image_data(image); // Free CPU memory after uploading to GPU

    // Set texture wrapping and filtering options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Use REPEAT instead of MIRRORED_REPEAT if desired
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Good quality filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture

    return 0; // Success
}

// Render a single block/cube with a specific pose and scale
static void render_block(const XrVector3f& position, // Use const&
                         const XrQuaternionf& orientation, // Use const&
                         const XrVector3f& scale, // Use const&
                         GLint modelLoc) { // Pass uniform location
    XrMatrix4x4f model_matrix;
    XrMatrix4x4f_CreateModelMatrix(&model_matrix, position, orientation, scale);
    // Pass matrix data directly using .m
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model_matrix.m);
    glDrawArrays(GL_TRIANGLES, 0, 36); // Draw the cube (36 vertices)
}

// Render a single cube using the math_3d library conventions
// Note: Assumes math_3d types vec3_t, mat4_t and functions m4_*, degrees_to_radians are available
void render_rotated_cube(vec3_t position, float cube_size, float rotation_degrees, GLint modelLoc) {
    // Assuming degrees_to_radians is available globally or via math_3d.h
    mat4_t rotationmatrix = m4_rotation_y(degrees_to_radians(static_cast<double>(rotation_degrees))); // Cast needed if degrees_to_radians expects double
    mat4_t scalematrix = m4_scaling(vec3(cube_size / 2.0f, cube_size / 2.0f, cube_size / 2.0f));
    mat4_t translationmatrix = m4_translation(position);

    // Model = Translation * Rotation * Scale
    mat4_t modelmatrix = m4_mul(translationmatrix, m4_mul(rotationmatrix, scalematrix));

    // Assuming mat4_t has a compatible .m member or can be cast
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(modelmatrix.m)); // Pass matrix data
    glDrawArrays(GL_TRIANGLES, 0, 36); // Draw the cube
}

void render_frame(int w,
                  int h,
                  GLuint shader_program_id,
                  GLuint VAO,
                  XrTime predictedDisplayTime,
                  int view_index,
                  const XrMatrix4x4f& projectionmatrix, // Pass const&
                  const XrMatrix4x4f& viewmatrix, // Pass const&
                  GLuint framebuffer,
                  GLuint image /* The GLuint handle of the OpenXR swapchain image */) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glViewport(0, 0, w, h);
    glScissor(0, 0, w, h);

    // Attach the swapchain image (texture) to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);

    // Check framebuffer status (optional but recommended for debugging)
    // GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    // if (status != GL_FRAMEBUFFER_COMPLETE) {
    //     std::cerr << "Framebuffer is not complete! Status: 0x" << std::hex << status << std::dec << std::endl;
    // }


    glClearColor(0.0f, 0.0f, 0.2f, 1.0f); // Dark blue background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_program_id);
    glBindVertexArray(VAO);


    // Activate texture unit 0 and bind our texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    // Set the sampler uniform to use texture unit 0 (matching the layout location)
    glUniform1i(6, 0); // Location 6 was specified in fragment shader


    // Get uniform locations (cache these outside the render loop for performance if possible)
    GLint modelLoc = glGetUniformLocation(shader_program_id, "model"); // Location 2
    GLint viewLoc = glGetUniformLocation(shader_program_id, "view");   // Location 3
    GLint projLoc = glGetUniformLocation(shader_program_id, "proj");   // Location 4
    // GLint colorLoc = glGetUniformLocation(shader_program_id, "uniformColor"); // This uniform isn't in the shader anymore

    // Set view and projection matrices (once per frame)
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewmatrix.m);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionmatrix.m);

    // --- Render Scene ---
    {
        // Render rotating cubes using the math_3d based function
        double display_time_seconds = static_cast<double>(predictedDisplayTime) / 1e9; // Convert nanoseconds to seconds
        const float rotations_per_sec = 0.25f;
        // Use fmod for floating point modulo
        float angle = std::fmod(static_cast<float>(display_time_seconds * 360.0 * rotations_per_sec), 360.0f);

        float dist = 1.5f;
        float height = 0.0f; // Place cubes at origin height for simplicity
        float cube_size = 0.33f;

        // Ensure vec3 takes floats if math_3d expects that
        render_rotated_cube(vec3(0.0f, height, -dist), cube_size, angle, modelLoc);
        render_rotated_cube(vec3(0.0f, height, dist), cube_size, angle + 90.0f, modelLoc); // Offset rotation for variety
        render_rotated_cube(vec3(dist, height, 0.0f), cube_size, angle + 180.0f, modelLoc);
        render_rotated_cube(vec3(-dist, height, 0.0f), cube_size, angle + 270.0f, modelLoc);

        // Example using the XrMath based render_block function:
        // XrVector3f pos = {0.f, 0.f, -2.f};
        // XrQuaternionf rot = {0.f, 0.f, 0.f, 1.f}; // Identity rotation
        // XrVector3f scale = {0.5f, 0.5f, 0.5f};
        // render_block(pos, rot, scale, modelLoc);
    }

    glBindVertexArray(0); // Unbind VAO
    glUseProgram(0); // Unbind shader program

    // --- Blit to Desktop Window (Mirror View) ---
    // Unbind the application framebuffer before blitting
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Blit from the framebuffer we just rendered *to* (which has the swapchain image attached)
    // to the default framebuffer (0, which corresponds to the SDL window).
    // Ensure glBlitNamedFramebuffer or glBlitFramebuffer is available and loaded.
    if (glBlitNamedFramebuffer) { // Check if function pointer is valid
        GLint srcX0 = 0;
        GLint srcY0 = 0;
        GLint srcX1 = w;
        GLint srcY1 = h;
        GLint dstX0 = (view_index == 0) ? 0 : w / 2;     // Left half for eye 0
        GLint dstY0 = 0;
        GLint dstX1 = (view_index == 0) ? w / 2 : w;     // Right half for eye 1
        GLint dstY1 = h / 2;                             // Draw to bottom half of window

        glBlitNamedFramebuffer(framebuffer, 0,          // Read from app's framebuffer, draw to default framebuffer (0)
                               srcX0, srcY0, srcX1, srcY1, // Source rect
                               dstX0, dstY0, dstX1, dstY1, // Destination rect
                               GL_COLOR_BUFFER_BIT,      // Mask
                               GL_LINEAR);               // Filter
    } else {
        // Fallback or error if blit function isn't available
        if (view_index == 0) std::cerr << "glBlitNamedFramebuffer not available!" << std::endl;
    }


    // Swap the SDL window buffer *only once* per frame, e.g., after rendering the right eye
    // Or, if not double buffering the SDL window, this might not be needed or do anything.
    // Since SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0) was called, SwapWindow might be less relevant
    // unless the driver implicitly triple-buffers or similar.
    // If mirroring is desired, ensure the blit happens correctly. Swapping might only be needed
    // once outside the per-eye render loop if the window isn't the primary render target.
    // For simplicity, let's swap after each eye's blit for now.
    SDL_GL_SwapWindow(desktop_window);

     // It's important *not* to unbind the texture from the framebuffer here,
     // as OpenXR needs it attached when the frame is submitted.
     // glBindFramebuffer(GL_FRAMEBUFFER, 0); // Already unbound before blit
}

// ==================================
// Modified OpenXRApp Class Definition
// ==================================
// class OpenXRApp {
//     public:
//         // +++ 中文注释：构造函数，接收 SLAM/Mapper 指针 +++
//         // +++ English Comment: Constructor taking SLAM/Mapper pointers +++
//         OpenXRApp(std::shared_ptr<ORB_SLAM3::System> pSLAM,
//                   std::shared_ptr<GaussianMapper> pGausMapper);
    
//         // +++ 中文注释：析构函数 +++
//         // +++ English Comment: Destructor +++
//         ~OpenXRApp();
    
//         // Prevent copying
//         OpenXRApp(const OpenXRApp&) = delete;
//         OpenXRApp& operator=(const OpenXRApp&) = delete;
    
//         // +++ 中文注释：初始化 OpenXR 和相关资源 +++
//         // +++ English Comment: Initializes OpenXR and related resources +++
//         bool Initialize();
    
//         // +++ 中文注释：运行主事件和渲染循环 +++
//         // +++ English Comment: Runs the main event and rendering loop +++
//         void Run();
    
//         // +++ 中文注释：清理所有资源 +++
//         // +++ English Comment: Cleans up all resources +++
//         void Shutdown();
    
    
//     private:
//         // --- Initialization Methods ---
//         bool CheckInstanceExtensions();
//         bool CreateInstance();
//         bool LoadExtensionFunctions(); // Assumes global function pointers or member storage
//         bool GetSystem();
//         bool GetViewConfigurations();
//         bool CheckGraphicsRequirements();
//         bool InitializePlatformGraphics(); // Sets up SDL/GLX, fills m_graphics_binding_gl
//         bool CreateSession();
//         bool CreateReferenceSpace();
//         bool CreateSwapchains();
    
//         // --- Main Loop Methods ---
//         void PollEvents();
//         void PollSdlEvents(); // Helper for SDL specific events
//         void ProcessEvent(const XrEventDataBuffer& event);
//         void HandleSessionStateChanged(const XrEventDataSessionStateChanged* event);
//         void HandleInteractionProfileChanged(); // Placeholder
    
//         // --- Frame Rendering Methods ---
//         bool RenderFrameCycle();
//         bool RenderViewToSwapchain(uint32_t view_index, const XrFrameState& frame_state, XrSwapchainSubImage& sub_image /* out */);
    
//         // --- Cleanup Methods ---
//         void CleanupPlatformGraphics(); // Cleans up SDL/GLX, blit FBO
//         void CleanupSwapchains();
    
    
//         // --- Member Variables ---
    
//         // +++ 中文注释：指向 SLAM 和 Mapper 对象的成员变量 +++
//         // +++ English Comment: Member variables pointing to SLAM and Mapper objects +++
//         std::shared_ptr<ORB_SLAM3::System> pSLAM_;
//         std::shared_ptr<GaussianMapper> pGausMapper_;
    
//         // --- OpenXR Configuration ---
//         XrFormFactor m_form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
//         XrViewConfigurationType m_view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
//         XrReferenceSpaceType m_play_space_type = XR_REFERENCE_SPACE_TYPE_LOCAL; // Or STAGE
//         float m_near_z = 0.1f; // Near clipping plane
//         float m_far_z = 100.0f; // Far clipping plane
    
//         // --- OpenXR Handles ---
//         XrInstance m_instance = XR_NULL_HANDLE;
//         XrSystemId m_system_id = XR_NULL_SYSTEM_ID;
//         XrSession m_session = XR_NULL_HANDLE;
//         XrSpace m_play_space = XR_NULL_HANDLE;
    
//         // --- Graphics Binding (Filled by InitializePlatformGraphics) ---
//         // Assumes Xlib/GLX platform
//         XrGraphicsBindingOpenGLXlibKHR m_graphics_binding_gl = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    
//         // --- View & Swapchain Data ---
//         uint32_t m_view_count = 0;
//         std::vector<XrViewConfigurationView> m_viewconfig_views;
//         std::vector<XrCompositionLayerProjectionView> m_projection_views; // For submission
//         std::vector<XrView> m_views; // Holds located view pose/fov per frame
//         std::vector<XrSwapchain> m_swapchains;
//         std::vector<uint32_t> m_swapchain_lengths;
//         std::vector<std::vector<XrSwapchainImageOpenGLKHR>> m_swapchain_images; // Holds GL texture IDs
    
//         // +++ 中文注释：用于伴侣窗口图像拷贝 (blit) 的 FBO +++
//         // +++ English Comment: Framebuffer Object for blitting to companion window +++
//         GLuint m_blit_fbo = 0;
    
//         // --- Main Loop State ---
//         bool m_quit_mainloop = false;
//         bool m_session_running = false;
//         bool m_run_framecycle = false;
//         XrSessionState m_state = XR_SESSION_STATE_UNKNOWN;
    
//         // --- Platform Specifics (Example: SDL Window) ---
//         // These should be managed by your platform graphics helper functions
//         SDL_Window* desktop_window = nullptr;
//         // Display*, GLXContext etc. are now within m_graphics_binding_gl
    
//     }; // End OpenXRApp Class



#endif // OPENXR_APP_H
