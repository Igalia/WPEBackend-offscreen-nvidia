/*
 * Copyright (C) 2023 Igalia S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../wpebackend-offscreen.h"

#include "../application-side/RendererHost.h"
#include "../application-side/ViewBackend.h"
#include "../wpewebprocess-side/RendererBackendEGL.h"
#include "../wpewebprocess-side/RendererBackendEGLTarget.h"

#include <cstring>

/**
 * Components architecture per process:
 *
 *                                             ___________________________
 *  ____________________________              | Application Process       |
 * | WPEWebProcess              |             |                           |
 * |                            |     IPC     | RendererHost              |
 * | RendererBackendEGL       <-------------------> |- RendererHostClient |
 * |                            |    /------------> |- RendererHostClient |
 * |                            |    |        |                           |
 * |                            |    |        |                           |
 * | RendererBackendEGLTarget <------|----------> ViewBackend             |
 * | RendererBackendEGLTarget <------|----------> ViewBackend             |
 * | RendererBackendEGLTarget <------|----------> ViewBackend             |
 * |____________________________|    |        |                           |
 *                                   |    /-----> ViewBackend             |
 *  ____________________________     |    | /---> ViewBackend             |
 * | WPEWebProcess              |    |    | | |___________________________|
 * |                            |    |    | |
 * | RendererBackendEGL       <------/    | |
 * |                            |         | |
 * | RendererBackendEGLTarget <-----------/ |
 * | RendererBackendEGLTarget <-------------/
 * |____________________________|
 *
 */

extern "C"
{
    __attribute__((visibility("default"))) wpe_loader_interface _wpe_loader_interface = {
        +[](const char* name) -> void* {
            if (std::strcmp(name, "_wpe_renderer_host_interface") == 0)
                return RendererHost::getWPEInterface();

            if (std::strcmp(name, "_wpe_renderer_backend_egl_interface") == 0)
                return RendererBackendEGL::getWPEInterface();

            if (std::strcmp(name, "_wpe_renderer_backend_egl_target_interface") == 0)
                return RendererBackendEGLTarget::getWPEInterface();

            if (std::strcmp(name, "_wpe_renderer_backend_egl_offscreen_target_interface") == 0)
            {
                static wpe_renderer_backend_egl_offscreen_target_interface s_interface = {
                    +[]() -> void* { return nullptr; },
                    +[](void*) {},
                    +[](void*, void*) {},
                    +[](void*) -> EGLNativeWindowType { return nullptr; },
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr};
                return &s_interface;
            }

            return nullptr;
        },
        nullptr, nullptr, nullptr, nullptr};
}

__attribute__((visibility("default"))) wpe_offscreen_view_backend* wpe_offscreen_view_backend_create(
    wpe_offscreen_on_frame_available_callback cb, void* user_data, uint32_t width, uint32_t height,
    EGLNativeDisplayType display)
{
    ViewBackend::ViewParams viewParams = {cb, user_data, width, height, display};
    wpe_view_backend_create_with_backend_interface(ViewBackend::getWPEInterface(), &viewParams);
    return static_cast<wpe_offscreen_view_backend*>(viewParams.userData);
}

__attribute__((visibility("default"))) wpe_view_backend* wpe_offscreen_view_backend_get_wpe_backend(
    wpe_offscreen_view_backend* offscreen_backend)
{
    return static_cast<ViewBackend*>(offscreen_backend)->getWPEViewBackend();
}

__attribute__((visibility("default"))) void wpe_offscreen_view_backend_dispatch_frame_complete(
    wpe_offscreen_view_backend* offscreen_backend)
{
    static_cast<ViewBackend*>(offscreen_backend)->frameComplete();
}
