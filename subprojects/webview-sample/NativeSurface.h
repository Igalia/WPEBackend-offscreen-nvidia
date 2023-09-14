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
#include <memory>

class NativeSurface
{
  public:
    static std::unique_ptr<NativeSurface> createNativeSurface(EGLenum platform, EGLNativeDisplayType display,
                                                              unsigned int width, unsigned int height) noexcept;
    virtual ~NativeSurface() = default;

    NativeSurface(NativeSurface&&) = delete;
    NativeSurface& operator=(NativeSurface&&) = delete;
    NativeSurface(const NativeSurface&) = delete;
    NativeSurface& operator=(const NativeSurface&) = delete;

    unsigned int getWidth() const noexcept
    {
        return m_width;
    }

    unsigned int getHeight() const noexcept
    {
        return m_height;
    }

    EGLNativeWindowType getWindow() const noexcept
    {
        return m_window;
    }

    void resize(unsigned int width, unsigned int height) noexcept;

  protected:
    NativeSurface(EGLNativeDisplayType display, unsigned int width, unsigned int height);

    unsigned int m_width = 1;
    unsigned int m_height = 1;

    EGLNativeDisplayType m_display = nullptr;
    EGLNativeWindowType m_window = nullptr;

    virtual void resizeUnderlyingWindow() noexcept = 0;
};
