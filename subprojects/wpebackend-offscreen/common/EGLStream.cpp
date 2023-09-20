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

#include "EGLStream.h"

namespace
{
PFNEGLCREATESTREAMKHRPROC eglCreateStreamKHR = nullptr;
PFNEGLDESTROYSTREAMKHRPROC eglDestroyStreamKHR = nullptr;
PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC eglGetStreamFileDescriptorKHR = nullptr;
PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC eglStreamConsumerGLTextureExternalKHR = nullptr;
PFNEGLSTREAMCONSUMERACQUIREKHRPROC eglStreamConsumerAcquireKHR = nullptr;
PFNEGLSTREAMCONSUMERRELEASEKHRPROC eglStreamConsumerReleaseKHR = nullptr;
PFNEGLQUERYSTREAMKHRPROC eglQueryStreamKHR = nullptr;
PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC eglCreateStreamFromFileDescriptorKHR = nullptr;
PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC eglCreateStreamProducerSurfaceKHR = nullptr;

bool initEGLStreamsExtensions() noexcept
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

    if (!eglStreamConsumerGLTextureExternalKHR)
    {
        eglStreamConsumerGLTextureExternalKHR = reinterpret_cast<PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC>(
            eglGetProcAddress("eglStreamConsumerGLTextureExternalKHR"));
        if (!eglStreamConsumerGLTextureExternalKHR)
            return false;
    }

    if (!eglStreamConsumerAcquireKHR)
    {
        eglStreamConsumerAcquireKHR =
            reinterpret_cast<PFNEGLSTREAMCONSUMERACQUIREKHRPROC>(eglGetProcAddress("eglStreamConsumerAcquireKHR"));
        if (!eglStreamConsumerAcquireKHR)
            return false;
    }

    if (!eglStreamConsumerReleaseKHR)
    {
        eglStreamConsumerReleaseKHR =
            reinterpret_cast<PFNEGLSTREAMCONSUMERRELEASEKHRPROC>(eglGetProcAddress("eglStreamConsumerReleaseKHR"));
        if (!eglStreamConsumerReleaseKHR)
            return false;
    }

    if (!eglQueryStreamKHR)
    {
        eglQueryStreamKHR = reinterpret_cast<PFNEGLQUERYSTREAMKHRPROC>(eglGetProcAddress("eglQueryStreamKHR"));
        if (!eglQueryStreamKHR)
            return false;
    }

    if (!eglCreateStreamFromFileDescriptorKHR)
    {
        eglCreateStreamFromFileDescriptorKHR = reinterpret_cast<PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC>(
            eglGetProcAddress("eglCreateStreamFromFileDescriptorKHR"));
        if (!eglCreateStreamFromFileDescriptorKHR)
            return false;
    }

    if (!eglCreateStreamProducerSurfaceKHR)
    {
        eglCreateStreamProducerSurfaceKHR = reinterpret_cast<PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC>(
            eglGetProcAddress("eglCreateStreamProducerSurfaceKHR"));
        if (!eglCreateStreamProducerSurfaceKHR)
            return false;
    }

    return true;
}
} // namespace

EGLStream::~EGLStream()
{
    if (m_eglStream)
        eglDestroyStreamKHR(m_display, m_eglStream);
}

EGLStream::StreamStatus EGLStream::getStatus() const noexcept
{
    EGLint status = 0;
    if (eglQueryStreamKHR(m_display, m_eglStream, EGL_STREAM_STATE_KHR, &status))
        return static_cast<StreamStatus>(status);
    else
        return StreamStatus::Error;
}

std::unique_ptr<EGLConsumerStream> EGLConsumerStream::createEGLStream(EGLDisplay display) noexcept
{
    if (!display || !initEGLStreamsExtensions())
        return nullptr;

    std::unique_ptr<EGLConsumerStream> stream(new EGLConsumerStream(display));

    static constexpr const EGLint s_streamAttribs[] = {
        EGL_STREAM_FIFO_LENGTH_KHR, 1, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, ACQUIRE_MAX_TIMEOUT_USEC, EGL_NONE};
    stream->m_eglStream = eglCreateStreamKHR(display, s_streamAttribs);
    if (!stream->m_eglStream)
        return nullptr;

    stream->m_streamFD = eglGetStreamFileDescriptorKHR(display, stream->m_eglStream);
    if (stream->m_streamFD == -1)
        return nullptr;

    return stream;
}

EGLConsumerStream::~EGLConsumerStream()
{
    if (m_streamFD != -1)
        close(m_streamFD);
}

void EGLConsumerStream::closeStreamFD() noexcept
{
    if (m_streamFD != -1)
    {
        close(m_streamFD);
        m_streamFD = -1;
    }
}

bool EGLConsumerStream::bindStreamToCurrentExternalTexture() const noexcept
{
    return eglStreamConsumerGLTextureExternalKHR(m_display, m_eglStream);
}

bool EGLConsumerStream::acquireFrame() const noexcept
{
    return eglStreamConsumerAcquireKHR(m_display, m_eglStream);
}

bool EGLConsumerStream::releaseFrame() const noexcept
{
    return eglStreamConsumerReleaseKHR(m_display, m_eglStream);
}

std::unique_ptr<EGLProducerStream> EGLProducerStream::createEGLStream(EGLDisplay display, EGLContext ctx, EGLint width,
                                                                      EGLint height, int consumerFD) noexcept
{
    if (!display || !ctx || (consumerFD == -1) || !initEGLStreamsExtensions())
        return nullptr;

    std::unique_ptr<EGLProducerStream> stream(new EGLProducerStream(display));
    stream->m_eglStream = eglCreateStreamFromFileDescriptorKHR(display, consumerFD);
    if (!stream->m_eglStream)
        return nullptr;

    stream->m_eglContext = ctx;
    EGLint configId = 0;
    if (!eglQueryContext(display, ctx, EGL_CONFIG_ID, &configId))
        return nullptr;

    const EGLint configAttribs[] = {EGL_CONFIG_ID, configId, EGL_NONE};
    EGLConfig config = {};
    EGLint numConfigs = 0;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || (numConfigs != 1))
        return nullptr;

    const EGLint surfaceAttribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
    stream->m_eglSurface = eglCreateStreamProducerSurfaceKHR(display, config, stream->m_eglStream, surfaceAttribs);
    if (!stream->m_eglSurface)
        return nullptr;

    return stream;
}

EGLProducerStream::~EGLProducerStream()
{
    if (m_eglSurface)
    {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(m_display, m_eglSurface);
    }
}

bool EGLProducerStream::makeCurrent() const noexcept
{
    return eglMakeCurrent(m_display, m_eglSurface, m_eglSurface, m_eglContext);
}

bool EGLProducerStream::swapBuffers() const noexcept
{
    return eglSwapBuffers(m_display, m_eglSurface);
}
