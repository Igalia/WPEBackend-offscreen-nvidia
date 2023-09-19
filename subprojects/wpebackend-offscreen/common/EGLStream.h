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

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <memory>

class EGLStream
{
  public:
    virtual ~EGLStream();

    EGLStream(EGLStream&&) = delete;
    EGLStream& operator=(EGLStream&&) = delete;
    EGLStream(const EGLStream&) = delete;
    EGLStream& operator=(const EGLStream&) = delete;

    enum class Status : EGLint
    {
        Error = 0,
        Created = EGL_STREAM_STATE_CREATED_KHR,
        Connecting = EGL_STREAM_STATE_CONNECTING_KHR,
        Empty = EGL_STREAM_STATE_EMPTY_KHR,
        NewFrameAvailable = EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR,
        OldFrameAvailable = EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR,
        Disconnected = EGL_STREAM_STATE_DISCONNECTED_KHR
    };

    Status getStatus() const noexcept;

  protected:
    const EGLDisplay m_display;
    EGLStream(EGLDisplay display) : m_display(display)
    {
    }

    EGLStreamKHR m_eglStream = EGL_NO_STREAM_KHR;
};

class EGLConsumerStream final : public EGLStream
{
  public:
    static std::unique_ptr<EGLConsumerStream> createEGLStream(EGLDisplay display) noexcept;

    ~EGLConsumerStream() override;

    int getStreamFD() const noexcept
    {
        return m_streamFD;
    }

    void closeStreamFD() noexcept;

    bool bindStreamToCurrentExternalTexture() const noexcept;

    bool acquireFrame() const noexcept;
    bool releaseFrame() const noexcept;

  private:
    EGLConsumerStream(EGLDisplay display) : EGLStream(display)
    {
    }

    int m_streamFD = -1;
};

class EGLProducerStream final : public EGLStream
{
  public:
    static std::unique_ptr<EGLProducerStream> createEGLStream(EGLDisplay display, EGLContext ctx, EGLint width,
                                                              EGLint height, int consumerFD) noexcept;

    ~EGLProducerStream() override;

    bool makeCurrent() const noexcept;
    bool swapBuffers() const noexcept;

  private:
    EGLProducerStream(EGLDisplay display) : EGLStream(display)
    {
    }

    EGLContext m_eglContext = EGL_NO_CONTEXT;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
};
