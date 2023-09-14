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

#include "NativeSurface.h"

#include <EGL/eglext.h>
#include <X11/Xlib.h>
#include <glib.h>

namespace
{
class NativeX11Surface final : public NativeSurface
{
  public:
    static std::unique_ptr<NativeSurface> createNativeX11Surface(EGLNativeDisplayType display, unsigned int width,
                                                                 unsigned int height) noexcept;
    ~NativeX11Surface();

  protected:
    void resizeUnderlyingWindow() noexcept override
    {
        XResizeWindow(static_cast<Display*>(m_display), reinterpret_cast<Window>(m_window), m_width, m_height);
    }

  private:
    NativeX11Surface(EGLNativeDisplayType display, unsigned int width, unsigned int height)
        : NativeSurface(display, width, height)
    {
    }
};

std::unique_ptr<NativeSurface> NativeX11Surface::createNativeX11Surface(EGLNativeDisplayType display,
                                                                        unsigned int width,
                                                                        unsigned int height) noexcept
{
    std::unique_ptr<NativeX11Surface> surface(new NativeX11Surface(display, width, height));

    int screen = DefaultScreen(display);
    XSetWindowAttributes attr = {};
    attr.background_pixel = BlackPixel(display, screen);
    Window wnd =
        XCreateWindow(static_cast<Display*>(display), RootWindow(display, screen), 0, 0, surface->m_width,
                      surface->m_height, 0, 0, InputOutput, DefaultVisual(display, screen), CWBackPixel, &attr);
    surface->m_window = reinterpret_cast<EGLNativeWindowType>(wnd);
    if (!surface->m_window)
    {
        g_critical("Cannot create X11 window for the native surface");
        return nullptr;
    }

    Atom deleteAtom = XInternAtom(static_cast<Display*>(display), "WM_DELETE_WINDOW", False);
    XSetWMProtocols(static_cast<Display*>(display), wnd, &deleteAtom, 1);

    XMapWindow(static_cast<Display*>(display), wnd);
    XFlush(static_cast<Display*>(display));

    g_message("Native X11 surface created (%dx%d)", surface->m_width, surface->m_height);
    return surface;
}

NativeX11Surface::~NativeX11Surface()
{
    if (m_display)
    {
        if (m_window)
        {
            XDestroyWindow(static_cast<Display*>(m_display), reinterpret_cast<Window>(m_window));
            m_window = 0;
        }

        XCloseDisplay(static_cast<Display*>(m_display));
        m_display = nullptr;
    }
}
} // namespace

NativeSurface::NativeSurface(EGLNativeDisplayType display, unsigned int width, unsigned int height) : m_display(display)
{
    if (width > m_width)
        m_width = width;

    if (height > m_height)
        m_height = height;
}

std::unique_ptr<NativeSurface> NativeSurface::createNativeSurface(EGLenum platform, EGLNativeDisplayType display,
                                                                  unsigned int width, unsigned int height) noexcept
{
    if (platform == EGL_PLATFORM_X11_KHR)
        return NativeX11Surface::createNativeX11Surface(display, width, height);

    return nullptr;
}

void NativeSurface::resize(unsigned int width, unsigned int height) noexcept
{
    if (width < 1)
        width = 1;

    if (height < 1)
        height = 1;

    if ((width != m_width) || (height != m_height))
    {
        m_width = width;
        m_height = height;
        resizeUnderlyingWindow();
    }
}
