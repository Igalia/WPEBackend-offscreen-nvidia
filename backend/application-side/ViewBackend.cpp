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

#include "ViewBackend.h"

#include "../common/ipc-messages.h"

wpe_view_backend_interface* ViewBackend::getWPEInterface() noexcept
{
    static wpe_view_backend_interface s_interface = {
        // void* create(void* params, wpe_view_backend* backend)
        +[](void* params, wpe_view_backend* backend) -> void* {
            auto* viewParams = static_cast<ViewBackend::ViewParams*>(params);
            auto* viewBackend = new ViewBackend(*viewParams, backend);
            viewParams->userData = viewBackend;
            return viewBackend;
        },
        // void destroy(void* data)
        +[](void* data) { delete static_cast<ViewBackend*>(data); },
        // void initialize(void* data)
        +[](void* data) { static_cast<ViewBackend*>(data)->initialize(); },
        // int get_renderer_host_fd(void* data)
        +[](void* data) -> int { return static_cast<ViewBackend*>(data)->m_ipcChannel.detachPeerFd(); }, nullptr,
        nullptr, nullptr, nullptr};

    return &s_interface;
}

void ViewBackend::initialize() noexcept
{
    wpe_view_backend_dispatch_set_size(m_wpeViewBackend, m_viewParams.width, m_viewParams.height);
}

void ViewBackend::frameComplete() noexcept
{
    m_ipcChannel.sendMessage(IPC::FrameComplete());
    wpe_view_backend_dispatch_frame_displayed(m_wpeViewBackend);
}

void ViewBackend::handleMessage(IPC::Channel& /*channel*/, const IPC::Message& message) noexcept
{
    // Messages received on application process side from RendererBackendEGLTarget on WPEWebProcess side
    switch (message.getCode())
    {
    case IPC::FrameAvailable::MESSAGE_CODE:
        if (m_viewParams.onFrameAvailableCB)
            m_viewParams.onFrameAvailableCB(this, m_viewParams.userData);
        else
            frameComplete();
        break;

    default:
        break;
    }
}
