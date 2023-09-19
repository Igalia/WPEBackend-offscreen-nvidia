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

#include <GLES2/gl2ext.h>

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
        +[](void* data) { static_cast<ViewBackend*>(data)->initialize(); },
        // int get_renderer_host_fd(void* data)
        +[](void* data) -> int { return static_cast<ViewBackend*>(data)->m_ipcChannel.detachPeerFd(); }, nullptr,
        nullptr, nullptr, nullptr};

    return &s_interface;
}

ViewBackend::~ViewBackend()
{
    if (m_display)
    {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_eglImage)
            eglDestroyImage(m_display, m_eglImage);

        if (m_eglContext)
            eglDestroyContext(m_display, m_eglContext);

        eglTerminate(m_display);
    }
}

void ViewBackend::initialize() noexcept
{
    if (m_display)
    {
        g_warning("ViewBackend is already initialized");
        return;
    }

    EGLint major, minor;
    m_display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if (!m_display || !eglInitialize(m_display, &major, &minor))
    {
        if (m_display)
        {
            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
        }
        g_critical("Cannot initialize EGL on ViewBackend side");
        return;
    }

    m_consumerStream = EGLConsumerStream::createEGLStream(m_display);
    if (!m_consumerStream)
    {
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        g_critical("Cannot create the consumer EGLStream on ViewBackend side");
        return;
    }

    if (!createGLESRenderer() || !m_consumerStream->bindStreamToCurrentExternalTexture())
    {
        m_consumerStream.reset();
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        g_critical("Cannot create the GLES renderer on ViewBackend side");
        return;
    }

    wpe_view_backend_dispatch_set_size(m_wpeViewBackend, m_viewParams.width, m_viewParams.height);
}

void ViewBackend::frameComplete() noexcept
{
    if (m_consumerStream)
        m_consumerStream->releaseFrame();

    wpe_view_backend_dispatch_frame_displayed(m_wpeViewBackend);
    m_ipcChannel.sendMessage(IPC::FrameComplete());
}

void ViewBackend::handleMessage(IPC::Channel& /*channel*/, const IPC::Message& message) noexcept
{
    // Messages received on application process side from RendererBackendEGLTarget on WPEWebProcess side
    switch (message.getCode())
    {
    case IPC::EGLStreamState::MESSAGE_CODE:
        onRemoteEGLStreamStateChanged(static_cast<const IPC::EGLStreamState&>(message).getState());
        break;

    case IPC::FrameAvailable::MESSAGE_CODE:
        onFrameAvailable();
        break;

    default:
        break;
    }
}

void ViewBackend::onRemoteEGLStreamStateChanged(IPC::EGLStreamState::State state) noexcept
{
    switch (state)
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
        m_consumerStream.reset();
        g_critical("Cannot connect EGLStream");
        break;
    }
}

void ViewBackend::onFrameAvailable() noexcept
{
    if (!m_viewParams.onFrameAvailableCB || !m_consumerStream || !m_consumerStream->acquireFrame())
    {
        frameComplete();
        return;
    }

    assert(m_eglImage);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, m_viewParams.width, m_viewParams.height, 0);
    glFlush();
    m_viewParams.onFrameAvailableCB(this, m_eglImage, m_viewParams.userData);
}

bool ViewBackend::createGLESRenderer() noexcept
{
    assert(m_display);
    assert(!m_eglContext);

    static constexpr const EGLint s_configAttribs[] = {EGL_SURFACE_TYPE,
                                                       EGL_STREAM_BIT_KHR,
                                                       EGL_RED_SIZE,
                                                       8,
                                                       EGL_GREEN_SIZE,
                                                       8,
                                                       EGL_BLUE_SIZE,
                                                       8,
                                                       EGL_ALPHA_SIZE,
                                                       8,
                                                       EGL_RENDERABLE_TYPE,
                                                       EGL_OPENGL_ES_BIT,
                                                       EGL_NONE};
    EGLConfig config = {};
    EGLint numConfigs = 0;
    if (!eglChooseConfig(m_display, s_configAttribs, &config, 1, &numConfigs) || (numConfigs != 1))
        return false;

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        return false;

    static constexpr const EGLint s_contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
    m_eglContext = eglCreateContext(m_display, config, EGL_NO_CONTEXT, s_contextAttribs);
    if (!m_eglContext)
        return false;

    if (!eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
    {
        eglDestroyContext(m_display, m_eglContext);
        m_eglContext = EGL_NO_CONTEXT;
        return false;
    }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_srcTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_srcTexture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_EXTERNAL_OES, m_srcTexture, 0);

    glGenTextures(1, &m_destTexture);
    glBindTexture(GL_TEXTURE_2D, m_destTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_viewParams.width, m_viewParams.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    assert(!m_eglImage);
    m_eglImage = eglCreateImage(m_display, m_eglContext, EGL_GL_TEXTURE_2D,
                                reinterpret_cast<EGLClientBuffer>(m_destTexture), nullptr);
    if (!m_eglImage)
    {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        m_fbo = 0;
        m_srcTexture = 0;
        m_destTexture = 0;
        eglDestroyContext(m_display, m_eglContext);
        m_eglContext = EGL_NO_CONTEXT;
        return false;
    }

    return true;
}
