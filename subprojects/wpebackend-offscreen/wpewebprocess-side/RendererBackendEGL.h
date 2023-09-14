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

#include "../common/ipc.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wpe/wpe-egl.h>

class RendererBackendEGL final : private IPC::MessageHandler
{
  public:
    static wpe_renderer_backend_egl_interface* getWPEInterface() noexcept;

    ~RendererBackendEGL() = default;

    RendererBackendEGL(RendererBackendEGL&&) = delete;
    RendererBackendEGL& operator=(RendererBackendEGL&&) = delete;
    RendererBackendEGL(const RendererBackendEGL&) = delete;
    RendererBackendEGL& operator=(const RendererBackendEGL&) = delete;

    EGLNativeDisplayType getDisplay() const noexcept
    {
        return EGL_DEFAULT_DISPLAY;
    }

    EGLenum getPlatform() const noexcept
    {
        return EGL_PLATFORM_SURFACELESS_MESA;
    }

  private:
    RendererBackendEGL(int rendererHostClientFd) noexcept;
    void handleMessage(IPC::Channel& channel, const IPC::Message& message) noexcept override;

    IPC::Channel m_ipcChannel;
};
