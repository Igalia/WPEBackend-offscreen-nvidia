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

#include "RendererBackendEGLTarget.h"

#include "../common/ipc-messages.h"

namespace
{
PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC eglCreateStreamFromFileDescriptorKHR = nullptr;
PFNEGLDESTROYSTREAMKHRPROC eglDestroyStreamKHR = nullptr;

bool initEGLStreamsExtension() noexcept
{
    if (!eglCreateStreamFromFileDescriptorKHR)
    {
        eglCreateStreamFromFileDescriptorKHR = reinterpret_cast<PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC>(
            eglGetProcAddress("eglCreateStreamFromFileDescriptorKHR"));
        if (!eglCreateStreamFromFileDescriptorKHR)
            return false;
    }

    if (!eglDestroyStreamKHR)
    {
        eglDestroyStreamKHR = reinterpret_cast<PFNEGLDESTROYSTREAMKHRPROC>(eglGetProcAddress("eglDestroyStreamKHR"));
        if (!eglDestroyStreamKHR)
            return false;
    }

    return true;
}
} // namespace

wpe_renderer_backend_egl_target_interface* RendererBackendEGLTarget::getWPEInterface() noexcept
{
    static wpe_renderer_backend_egl_target_interface s_interface = {
        // void* create(wpe_renderer_backend_egl_target* target, int peerFd)
        +[](wpe_renderer_backend_egl_target* target, int peerFd) -> void* {
            return new RendererBackendEGLTarget(target, peerFd);
        },
        // void destroy(void* data)
        +[](void* data) { delete static_cast<RendererBackendEGLTarget*>(data); },
        // void initialize(void* targetData, void* backendData, uint32_t width, uint32_t height)
        +[](void* targetData, void* backendData, uint32_t width, uint32_t height) {
            static_cast<RendererBackendEGLTarget*>(targetData)
                ->init(static_cast<RendererBackendEGL*>(backendData), width, height);
        },
        // EGLNativeWindowType get_native_window(void* data)
        +[](void*) -> EGLNativeWindowType { return nullptr; },
        // void resize(void* data, uint32_t width, uint32_t height)
        +[](void* data, uint32_t width, uint32_t height) {
            static_cast<RendererBackendEGLTarget*>(data)->resize(width, height);
        },
        // void frame_will_render(void* data)
        +[](void* data) { static_cast<RendererBackendEGLTarget*>(data)->frameWillRender(); },
        // void frame_rendered(void* data)
        +[](void* data) { static_cast<RendererBackendEGLTarget*>(data)->frameRendered(); },
        // void deinitialize(void* data)
        +[](void* data) { static_cast<RendererBackendEGLTarget*>(data)->shut(); }, nullptr, nullptr, nullptr};

    return &s_interface;
}

void RendererBackendEGLTarget::init(RendererBackendEGL* backend, uint32_t width, uint32_t height) noexcept
{
    if (m_backend)
    {
        g_warning("RendererBackendEGLTarget is already initialized");
        return;
    }

    if (!initEGLStreamsExtension())
    {
        g_critical("EGLStream extensions are not available");
        return;
    }

    m_backend = backend;
    m_width = width;
    m_height = height;
    m_bResized = true;

    m_display = eglGetPlatformDisplay(backend->getPlatform(), backend->getDisplay(), nullptr);
    m_ipcChannel.sendMessage(IPC::EGLStream(IPC::EGLStream::State::WaitingForFd));
}

void RendererBackendEGLTarget::shut() noexcept
{
    m_backend = nullptr;
    m_width = 0;
    m_height = 0;
    m_bResized = false;

    if (m_display)
    {
        if (m_eglStream)
            eglDestroyStreamKHR(m_display, m_eglStream);

        eglTerminate(m_display);
        m_display = nullptr;
    }

    m_eglStream = nullptr;
}

void RendererBackendEGLTarget::resize(uint32_t width, uint32_t height) noexcept
{
    if ((width != m_width) || (height != m_height))
    {
        m_width = width;
        m_height = height;
        m_bResized = true;
    }
}

void RendererBackendEGLTarget::frameWillRender() const noexcept
{
    // Frame drawing started in ThreadedCompositor::renderLayerTree() from WPEWebProcess

    // TODO: prepare FBO and render to texture
}

void RendererBackendEGLTarget::frameRendered() noexcept
{
    // Frame drawing finished (and buffers swapped) in ThreadedCompositor::renderLayerTree() from WPEWebProcess

    // TODO: convert FBO texture to EGLImage and push it to the EGLStream

    m_ipcChannel.sendMessage(IPC::FrameAvailable());
}

void RendererBackendEGLTarget::handleMessage(IPC::Channel& /*channel*/, const IPC::Message& message) noexcept
{
    // Messages received on WPEWebProcess side from ViewBackend on application process side
    switch (message.getCode())
    {
    case IPC::EGLStreamFileDescriptor::MESSAGE_CODE:
        connectEGLStream(static_cast<const IPC::EGLStreamFileDescriptor&>(message).getFD());
        break;

    case IPC::FrameComplete::MESSAGE_CODE:
        wpe_renderer_backend_egl_target_dispatch_frame_complete(m_wpeTarget);
        break;

    default:
        break;
    }
}

bool RendererBackendEGLTarget::connectEGLStream(int fd) noexcept
{
    if (fd == -1)
    {
        m_ipcChannel.sendMessage(IPC::EGLStream(IPC::EGLStream::State::Error));
        g_critical("Invalid EGLStream file descriptor received on RendererBackendEGLTarget side");
        return false;
    }

    if (m_eglStream)
    {
        close(fd);
        m_ipcChannel.sendMessage(IPC::EGLStream(IPC::EGLStream::State::Error));
        g_warning("EGLStream is already connected");
        return false;
    }

    m_eglStream = eglCreateStreamFromFileDescriptorKHR(m_display, fd);
    close(fd);

    if (!m_eglStream)
    {
        m_ipcChannel.sendMessage(IPC::EGLStream(IPC::EGLStream::State::Error));
        g_critical("Cannot connect EGLStream on RendererBackendEGLTarget side");
        return false;
    }

    m_ipcChannel.sendMessage(IPC::EGLStream(IPC::EGLStream::State::Connected));
    return true;
}
