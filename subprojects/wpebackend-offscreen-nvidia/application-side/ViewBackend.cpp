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

#include <cassert>

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
        +[](void* data) { static_cast<ViewBackend*>(data)->init(); },
        // int get_renderer_host_fd(void* data)
        +[](void* data) -> int { return static_cast<ViewBackend*>(data)->m_ipcChannel.detachPeerFd(); }, nullptr,
        nullptr, nullptr, nullptr};

    return &s_interface;
}

void ViewBackend::init() noexcept
{
    if (m_eglDisplay)
    {
        g_warning("ViewBackend is already initialized");
        return;
    }

    m_eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);

    EGLint major, minor;
    if (!m_eglDisplay || !eglInitialize(m_eglDisplay, &major, &minor))
    {
        shut();
        g_critical("Cannot initialize EGL on ViewBackend side");
        return;
    }

    m_idleSourceId = g_idle_add(G_SOURCE_FUNC(idleCallback), this);

    m_consumerStream = EGLConsumerStream::createEGLStream(m_eglDisplay);
    if (!m_consumerStream)
    {
        shut();
        g_critical("Cannot create the consumer EGLStream on ViewBackend side");
        return;
    }

    m_consumerThread = std::thread(&ViewBackend::consumerThreadFunc, this);
    wpe_view_backend_dispatch_set_size(m_wpeViewBackend, m_viewParams.width, m_viewParams.height);
}

void ViewBackend::shut() noexcept
{
    if (m_idleSourceId)
    {
        g_source_remove(m_idleSourceId);
        m_idleSourceId = 0;
    }

    if (m_consumerThread.joinable())
    {
        m_stopConsumer = true;

        std::unique_lock<std::mutex> lock(m_consumerMutex);
        m_fetchNextFrame = true;
        lock.unlock();
        m_consumerCondition.notify_all();

        m_consumerThread.join();
    }
    m_stopConsumer = false;
    m_fetchNextFrame = false;

    m_availableFrame = EGL_NO_IMAGE;
    m_consumerStream.reset();

    if (m_eglDisplay)
    {
        eglTerminate(m_eglDisplay);
        m_eglDisplay = EGL_NO_DISPLAY;
    }
}

void ViewBackend::frameComplete() noexcept
{
    std::unique_lock<std::mutex> lock(m_consumerMutex);
    m_fetchNextFrame = true;
    lock.unlock();
    m_consumerCondition.notify_all();

    wpe_view_backend_dispatch_frame_displayed(m_wpeViewBackend);
}

void ViewBackend::handleMessage(IPC::Channel& /*channel*/, const IPC::Message& message) noexcept
{
    // Messages received on application process side from RendererBackendEGLTarget on WPEWebProcess side
    switch (message.getCode())
    {
    case IPC::EGLStreamState::MESSAGE_CODE:
        switch (static_cast<const IPC::EGLStreamState&>(message).getState())
        {
        case IPC::EGLStreamState::State::WaitingForFd:
            if (m_consumerStream)
            {
                int fd = m_consumerStream->getStreamFD();
                if (fd != -1)
                {
                    m_ipcChannel.sendMessage(IPC::EGLStreamFileDescriptor(fd));
                    m_consumerStream->closeStreamFD();
                    return;
                }
            }
            g_critical("EGLStream doesn't exist on ViewBackend side");
            break;

        case IPC::EGLStreamState::State::Connected:
            g_info("EGLStream successfully connected");
            break;

        case IPC::EGLStreamState::State::Error:
            g_critical("Error on EGLStream");
            break;
        }
        break;

    default:
        break;
    }
}

gboolean ViewBackend::idleCallback(ViewBackend* backend) noexcept
{
    EGLImage frame = backend->m_availableFrame.exchange(EGL_NO_IMAGE);
    if (frame)
    {
        if (backend->m_viewParams.onFrameAvailableCB)
            backend->m_viewParams.onFrameAvailableCB(backend, frame, backend->m_viewParams.userData);
        else
            backend->frameComplete();
    }

    return G_SOURCE_CONTINUE;
}

void ViewBackend::consumerThreadFunc() noexcept
{
    assert(m_consumerStream);

    while (!m_stopConsumer)
    {
        EGLImage frame = m_consumerStream->acquireFrame();
        if (!frame)
            continue;

        m_availableFrame = frame;

        std::unique_lock<std::mutex> lock(m_consumerMutex);
        m_consumerCondition.wait(lock, [this] { return m_fetchNextFrame; });
        m_fetchNextFrame = false;

        m_consumerStream->releaseFrame();
    }
}
