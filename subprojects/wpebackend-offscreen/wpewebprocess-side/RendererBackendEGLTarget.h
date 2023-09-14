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

#pragma once

#include "RendererBackendEGL.h"

class RendererBackendEGLTarget final : private IPC::MessageHandler
{
  public:
    static wpe_renderer_backend_egl_target_interface* getWPEInterface() noexcept;

    ~RendererBackendEGLTarget()
    {
        shut();
    }

    RendererBackendEGLTarget(RendererBackendEGLTarget&&) = delete;
    RendererBackendEGLTarget& operator=(RendererBackendEGLTarget&&) = delete;
    RendererBackendEGLTarget(const RendererBackendEGLTarget&) = delete;
    RendererBackendEGLTarget& operator=(const RendererBackendEGLTarget&) = delete;

    void init(RendererBackendEGL* backend, uint32_t width, uint32_t height) noexcept;
    void shut() noexcept;

    void resize(uint32_t width, uint32_t height) noexcept;

    void frameWillRender() const noexcept;
    void frameRendered() noexcept;

  private:
    wpe_renderer_backend_egl_target* m_wpeTarget = nullptr;
    IPC::Channel m_ipcChannel;

    RendererBackendEGLTarget(wpe_renderer_backend_egl_target* wpeTarget, int viewBackendFd) noexcept
        : m_wpeTarget(wpeTarget), m_ipcChannel(*this, viewBackendFd)
    {
    }

    void handleMessage(IPC::Channel& channel, const IPC::Message& message) noexcept override;
    bool connectEGLStream(int fd) noexcept;

    RendererBackendEGL* m_backend = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_bResized = false;

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLStreamKHR m_eglStream = EGL_NO_STREAM_KHR;
};
