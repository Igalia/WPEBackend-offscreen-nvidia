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

    if (m_viewParams.nativeDisplay)
        m_eglDisplay = eglGetDisplay(m_viewParams.nativeDisplay);

    if (!m_eglDisplay)
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

    if (!createGLESRenderer() || !m_consumerStream->bindStreamToCurrentExternalTexture())
    {
        shut();
        g_critical("Cannot create the GLES renderer on ViewBackend side");
        return;
    }

    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_eglImage)
            eglDestroyImage(m_eglDisplay, m_eglImage);

        if (m_eglContext)
            eglDestroyContext(m_eglDisplay, m_eglContext);

        eglTerminate(m_eglDisplay);
        m_eglDisplay = EGL_NO_DISPLAY;
    }

    m_eglImage = EGL_NO_IMAGE;
    m_eglContext = EGL_NO_CONTEXT;

    m_program = 0;
    m_vbo = 0;
    m_fbo = 0;
    m_srcTexture = 0;
    m_destTexture = 0;
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

bool ViewBackend::createGLESRenderer() noexcept
{
    assert(m_eglDisplay);
    assert(!m_eglContext);

    static constexpr const EGLint s_configAttribs[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,   EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT, EGL_NONE};
    EGLConfig config = {};
    EGLint numConfigs = 0;
    if (!eglChooseConfig(m_eglDisplay, s_configAttribs, &config, 1, &numConfigs) || (numConfigs != 1))
        return false;

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        return false;

    static constexpr const EGLint s_contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
    m_eglContext = eglCreateContext(m_eglDisplay, config, EGL_NO_CONTEXT, s_contextAttribs);
    if (!m_eglContext || !eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
        return false;

    static constexpr const char s_vxtSource[] = R"EOS(
attribute vec2 aVtxPosition;

varying vec2 vTexCoord;

void main()
{
    gl_Position = vec4(aVtxPosition, 0.0, 1.0);
    vTexCoord = 0.5 * (aVtxPosition + 1.0);
    vTexCoord.y = 1.0 - vTexCoord.y;
}
)EOS";
    static constexpr int s_vxtSourceLength = sizeof(s_vxtSource);

    GLuint vtxShader = glCreateShader(GL_VERTEX_SHADER);
    const char* source = s_vxtSource;
    glShaderSource(vtxShader, 1, &source, &s_vxtSourceLength);
    glCompileShader(vtxShader);

    static constexpr const char s_fragSource[] = R"EOS(
#extension GL_OES_EGL_image_external : require

precision highp float;

uniform samplerExternalOES uImageTex;

varying vec2 vTexCoord;

void main()
{
    gl_FragColor = texture2D(uImageTex, vTexCoord);
}
)EOS";
    static constexpr int s_fragSourceLength = sizeof(s_fragSource);

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    source = s_fragSource;
    glShaderSource(fragShader, 1, &source, &s_fragSourceLength);
    glCompileShader(fragShader);

    m_program = glCreateProgram();
    glAttachShader(m_program, vtxShader);
    glAttachShader(m_program, fragShader);
    glLinkProgram(m_program);

    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);

    GLint status = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &status);
    if (!status)
    {
        GLsizei bufferSize = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &bufferSize);
        if (bufferSize > 0)
        {
            GLchar* buffer = new GLchar[bufferSize];
            glGetProgramInfoLog(m_program, bufferSize, nullptr, buffer);
            g_warning("Shader compilation error:\n%s\n", buffer);
            delete[] buffer;
        }

        return false;
    }

    static constexpr const float s_plane[] = {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_plane), s_plane, GL_STATIC_DRAW);

    glUseProgram(m_program);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glGenTextures(1, &m_srcTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_srcTexture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &m_destTexture);
    glBindTexture(GL_TEXTURE_2D, m_destTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_viewParams.width, m_viewParams.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_destTexture, 0);
    glViewport(0, 0, m_viewParams.width, m_viewParams.height);

    assert(!m_eglImage);
    m_eglImage = eglCreateImage(m_eglDisplay, m_eglContext, EGL_GL_TEXTURE_2D,
                                reinterpret_cast<EGLClientBuffer>(m_destTexture), nullptr);
    if (!m_eglImage)
        return false;

    return true;
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
    assert(m_eglDisplay);
    assert(m_eglContext);

    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext);

    while (!m_stopConsumer)
    {
        if (!m_consumerStream->acquireFrame())
            continue;

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glFinish();
        m_consumerStream->releaseFrame();

        m_availableFrame = m_eglImage;

        std::unique_lock<std::mutex> lock(m_consumerMutex);
        m_fetchNextFrame = false;
        m_consumerCondition.wait(lock, [this] { return m_fetchNextFrame; });
    }
}
