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

namespace
{
PFNEGLCREATESTREAMKHRPROC eglCreateStreamKHR = nullptr;
PFNEGLDESTROYSTREAMKHRPROC eglDestroyStreamKHR = nullptr;
PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC eglGetStreamFileDescriptorKHR = nullptr;

bool initEGLStreamsExtension() noexcept
{
    if (!eglCreateStreamKHR)
    {
        eglCreateStreamKHR = reinterpret_cast<PFNEGLCREATESTREAMKHRPROC>(eglGetProcAddress("eglCreateStreamKHR"));
        if (!eglCreateStreamKHR)
            return false;
    }

    if (!eglDestroyStreamKHR)
    {
        eglDestroyStreamKHR = reinterpret_cast<PFNEGLDESTROYSTREAMKHRPROC>(eglGetProcAddress("eglDestroyStreamKHR"));
        if (!eglDestroyStreamKHR)
            return false;
    }

    if (!eglGetStreamFileDescriptorKHR)
    {
        eglGetStreamFileDescriptorKHR =
            reinterpret_cast<PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC>(eglGetProcAddress("eglGetStreamFileDescriptorKHR"));
        if (!eglGetStreamFileDescriptorKHR)
            return false;
    }

    return true;
}
} // namespace

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

ViewBackend::~ViewBackend()
{
    if (m_eglStreamFD != -1)
        close(m_eglStreamFD);

    if (m_display)
    {
        if (m_eglStream)
            eglDestroyStreamKHR(m_display, m_eglStream);

        eglTerminate(m_display);
    }
}

void ViewBackend::initialize() noexcept
{
    if (m_eglStream)
    {
        g_warning("ViewBackend is already initialized");
        return;
    }

    m_display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);

    EGLint major, minor;
    if (!eglInitialize(m_display, &major, &minor) || !initEGLStreamsExtension())
    {
        g_critical("Cannot initialize EGL or EGLStream extensions on ViewBackend side");
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return;
    }

    static constexpr const EGLint s_streamAttribs[] = {EGL_STREAM_FIFO_LENGTH_KHR, 1,
                                                       EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1, EGL_NONE};
    m_eglStream = eglCreateStreamKHR(m_display, s_streamAttribs);
    if (!m_eglStream)
    {
        g_critical("Cannot create EGLStream on ViewBackend side");
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return;
    }

    m_eglStreamFD = eglGetStreamFileDescriptorKHR(m_display, m_eglStream);
    if (m_eglStreamFD == -1)
    {
        g_critical("Cannot get EGLStream file descriptor on ViewBackend side");
        eglDestroyStreamKHR(m_display, m_eglStream);
        m_eglStream = EGL_NO_STREAM_KHR;
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return;
    }

    wpe_view_backend_dispatch_set_size(m_wpeViewBackend, m_viewParams.width, m_viewParams.height);
}

void ViewBackend::frameComplete() noexcept
{
    wpe_view_backend_dispatch_frame_displayed(m_wpeViewBackend);
    m_ipcChannel.sendMessage(IPC::FrameComplete());
}

void ViewBackend::handleMessage(IPC::Channel& /*channel*/, const IPC::Message& message) noexcept
{
    // Messages received on application process side from RendererBackendEGLTarget on WPEWebProcess side
    switch (message.getCode())
    {
    case IPC::EGLStream::MESSAGE_CODE:
        onRemoteEGLStreamStateChanged(static_cast<const IPC::EGLStream&>(message).getState());
        break;

    case IPC::FrameAvailable::MESSAGE_CODE:
        onFrameAvailable();
        break;

    default:
        break;
    }
}

void ViewBackend::onRemoteEGLStreamStateChanged(IPC::EGLStream::State state) noexcept
{
    switch (state)
    {
    case IPC::EGLStream::State::WaitingForFd:
        if (m_eglStreamFD != -1)
        {
            m_ipcChannel.sendMessage(IPC::EGLStreamFileDescriptor(m_eglStreamFD));
            close(m_eglStreamFD);
            m_eglStreamFD = -1;
        }
        else
        {
            if (m_eglStream)
            {
                if (m_display)
                    eglDestroyStreamKHR(m_display, m_eglStream);

                m_eglStream = EGL_NO_STREAM_KHR;
            }

            g_critical("EGLStream doesn't exist on ViewBackend side");
        }
        break;

    case IPC::EGLStream::State::Connected:
        g_info("EGLStream successfully connected");
        break;

    case IPC::EGLStream::State::Error:
        if (m_eglStreamFD != -1)
        {
            close(m_eglStreamFD);
            m_eglStreamFD = -1;
        }

        if (m_eglStream)
        {
            if (m_display)
                eglDestroyStreamKHR(m_display, m_eglStream);

            m_eglStream = EGL_NO_STREAM_KHR;
        }

        g_critical("Cannot connect EGLStream");
        break;
    }
}

void ViewBackend::onFrameAvailable() noexcept
{
    // TODO: extract frame from the EGLStream
    EGLImage frame = EGL_NO_IMAGE;

    if (m_viewParams.onFrameAvailableCB)
        m_viewParams.onFrameAvailableCB(this, frame, m_viewParams.userData);
    else
        frameComplete();
}
