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
#include <GLES2/gl2.h>

#include <memory>

class NativeSurface final
{
  public:
    static std::unique_ptr<NativeSurface> createNativeSurface(uint32_t width, uint32_t height) noexcept;

    ~NativeSurface()
    {
        close();
    }

    NativeSurface(NativeSurface&&) = delete;
    NativeSurface& operator=(NativeSurface&&) = delete;
    NativeSurface(const NativeSurface&) = delete;
    NativeSurface& operator=(const NativeSurface&) = delete;

    uint32_t getWidth() const noexcept
    {
        return m_width;
    }

    uint32_t getHeight() const noexcept
    {
        return m_height;
    }

    EGLNativeDisplayType getNativeDisplay() const noexcept
    {
        return m_nativeDisplay;
    }

    void draw(EGLImage frame) const noexcept;
    void close() noexcept;
    bool isClosed() noexcept;

  private:
    NativeSurface(uint32_t width, uint32_t height);
    bool initializeGL() noexcept;

    uint32_t m_width = 1;
    uint32_t m_height = 1;

    EGLNativeDisplayType m_nativeDisplay = nullptr;
    EGLNativeWindowType m_nativeWindow = nullptr;
    unsigned long m_deleteAtom = 0;

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;

    GLuint m_program = 0;
    GLuint m_vbo = 0;
    GLuint m_texture = 0;
};
