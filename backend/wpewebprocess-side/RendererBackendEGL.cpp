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

#include "RendererBackendEGL.h"

#include <X11/Xlib.h>

wpe_renderer_backend_egl_interface* RendererBackendEGL::getWPEInterface() noexcept
{
    static wpe_renderer_backend_egl_interface s_interface = {
        // void* create(int peerFd)
        +[](int peerFd) -> void* { return new RendererBackendEGL(peerFd); },
        // void destroy(void* data)
        +[](void* data) { delete static_cast<RendererBackendEGL*>(data); },
        // EGLNativeDisplayType get_native_display(void* data)
        +[](void* data) -> EGLNativeDisplayType { return static_cast<RendererBackendEGL*>(data)->getDisplay(); },
        // uint32_t get_platform(void* data)
        +[](void* data) -> uint32_t { return static_cast<RendererBackendEGL*>(data)->getPlatform(); }, nullptr, nullptr,
        nullptr};

    return &s_interface;
}

RendererBackendEGL::RendererBackendEGL(int rendererHostClientFd) noexcept : m_ipcChannel(*this, rendererHostClientFd)
{
    Display* x11Display = XOpenDisplay(nullptr);
    if (x11Display)
    {
        m_display = x11Display;
        m_platform = EGL_PLATFORM_X11_KHR;
        g_message("Using X11 platform");
    }
}

RendererBackendEGL::~RendererBackendEGL()
{
    if (m_display)
    {
        if (m_platform == EGL_PLATFORM_X11_KHR)
            XCloseDisplay(static_cast<Display*>(m_display));
    }

    m_display = EGL_NO_DISPLAY;
    m_platform = 0;
}

void RendererBackendEGL::handleMessage(IPC::Channel& /*channel*/, const IPC::Message& /*message*/) noexcept
{
    // Messages received on WPEWebProcess side from RendererHostClient on application process side
}
