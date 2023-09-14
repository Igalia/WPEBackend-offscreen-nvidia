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

#include "../common/ipc-messages.h"
#include "../wpebackend-offscreen.h"

#include <EGL/eglext.h>

struct wpe_offscreen_view_backend
{
    // Empty struct used to hide the internal implementation from the public C interface
};

class ViewBackend final : public wpe_offscreen_view_backend, private IPC::MessageHandler
{
  public:
    static wpe_view_backend_interface* getWPEInterface() noexcept;

    struct ViewParams
    {
        wpe_offscreen_on_frame_available_callback onFrameAvailableCB;
        void* userData;
        uint32_t width;
        uint32_t height;
    };

    ~ViewBackend();

    ViewBackend(ViewBackend&&) = delete;
    ViewBackend& operator=(ViewBackend&&) = delete;
    ViewBackend(const ViewBackend&) = delete;
    ViewBackend& operator=(const ViewBackend&) = delete;

    wpe_view_backend* getWPEViewBackend() const noexcept
    {
        return m_wpeViewBackend;
    }

    void initialize() noexcept;
    void frameComplete() noexcept;

  private:
    const ViewParams m_viewParams;
    wpe_view_backend* m_wpeViewBackend = nullptr;
    IPC::Channel m_ipcChannel;

    ViewBackend(const ViewParams& viewParams, wpe_view_backend* wpeViewBackend) noexcept
        : m_viewParams(viewParams), m_wpeViewBackend(wpeViewBackend), m_ipcChannel(*this)
    {
    }

    void handleMessage(IPC::Channel& channel, const IPC::Message& message) noexcept override;
    void onRemoteEGLStreamStateChanged(IPC::EGLStream::State state) noexcept;
    void onFrameAvailable() noexcept;

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLStreamKHR m_eglStream = EGL_NO_STREAM_KHR;
    int m_eglStreamFD = -1;
};
