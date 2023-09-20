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

#include <GLES2/gl2ext.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cassert>

namespace
{
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;

bool initGLExtensions() noexcept
{
    if (!glEGLImageTargetTexture2DOES)
    {
        glEGLImageTargetTexture2DOES =
            reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
        if (!glEGLImageTargetTexture2DOES)
            return false;
    }

    return true;
}
} // namespace

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
{
    if (width > m_width)
        m_width = width;

    if (height > m_height)
        m_height = height;
}

std::unique_ptr<NativeSurface> NativeSurface::createNativeSurface(uint32_t width, uint32_t height) noexcept
{
    Display* nativeDisplay = XOpenDisplay(nullptr);
    if (!nativeDisplay)
        return nullptr;

    std::unique_ptr<NativeSurface> nativeSurface(new NativeSurface(width, height));
    nativeSurface->m_nativeDisplay = nativeDisplay;

    int screen = DefaultScreen(nativeDisplay);
    XSetWindowAttributes attr = {};
    attr.background_pixel = BlackPixel(nativeDisplay, screen);
    Window nativeWindow = XCreateWindow(nativeDisplay, RootWindow(nativeDisplay, screen), 0, 0, nativeSurface->m_width,
                                        nativeSurface->m_height, 0, 0, InputOutput,
                                        DefaultVisual(nativeDisplay, screen), CWBackPixel, &attr);
    if (!nativeWindow)
        return nullptr;

    nativeSurface->m_nativeWindow = reinterpret_cast<EGLNativeWindowType>(nativeWindow);

    nativeSurface->m_deleteAtom = XInternAtom(nativeDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(nativeDisplay, nativeWindow, &nativeSurface->m_deleteAtom, 1);

    XSizeHints* hints = XAllocSizeHints();
    hints->flags = PMinSize | PMaxSize;
    hints->min_width = hints->max_width = nativeSurface->m_width;
    hints->min_height = hints->max_height = nativeSurface->m_height;
    XSetWMNormalHints(nativeDisplay, nativeWindow, hints);
    XFree(hints);

    XMapWindow(nativeDisplay, nativeWindow);
    XFlush(nativeDisplay);

    if (!nativeSurface->initializeGL())
        return nullptr;

    return nativeSurface;
}

void NativeSurface::draw(EGLImage frame) const noexcept
{
    if (!m_context)
        return;

    assert(m_display);
    assert(m_surface);

    eglMakeCurrent(m_display, m_surface, m_surface, m_context);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, frame);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(m_display, m_surface);
}

void NativeSurface::close() noexcept
{
    if (m_display)
    {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_surface)
            eglDestroySurface(m_display, m_surface);

        if (m_context)
            eglDestroyContext(m_display, m_context);

        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
    }

    m_context = EGL_NO_CONTEXT;
    m_surface = EGL_NO_SURFACE;

    m_program = 0;
    m_vbo = 0;
    m_texture = 0;

    if (m_nativeDisplay)
    {
        if (m_nativeWindow)
            XDestroyWindow(static_cast<Display*>(m_nativeDisplay), reinterpret_cast<Window>(m_nativeWindow));

        XCloseDisplay(static_cast<Display*>(m_nativeDisplay));
        m_nativeDisplay = nullptr;
    }

    m_nativeWindow = nullptr;
    m_deleteAtom = 0;
}

bool NativeSurface::isClosed() noexcept
{
    if (!m_nativeDisplay)
        return true;

    Display* display = static_cast<Display*>(m_nativeDisplay);
    XEvent event;
    while (XPending(display))
    {
        XNextEvent(display, &event);
        if ((event.type == ClientMessage) && (static_cast<Atom>(event.xclient.data.l[0]) == m_deleteAtom))
        {
            close();
            return true;
        }
    }

    return false;
}

bool NativeSurface::initializeGL() noexcept
{
    assert(m_nativeDisplay);
    assert(m_nativeWindow);
    assert(!m_display);

    EGLint major, minor;
    m_display = eglGetDisplay(m_nativeDisplay);
    if (!m_display || !eglInitialize(m_display, &major, &minor))
    {
        if (m_display)
        {
            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
        }
        return false;
    }

    static constexpr const EGLint s_configAttribs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT, EGL_NONE};
    EGLConfig config = {};
    EGLint numConfigs = 0;
    if (!eglChooseConfig(m_display, s_configAttribs, &config, 1, &numConfigs) || (numConfigs != 1) ||
        !eglBindAPI(EGL_OPENGL_ES_API) || !initGLExtensions())
    {
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    static constexpr const EGLint s_contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, s_contextAttribs);
    if (!m_context)
    {
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    m_surface = eglCreateWindowSurface(m_display, config, m_nativeWindow, nullptr);
    if (!m_surface)
    {
        eglDestroyContext(m_display, m_context);
        m_context = EGL_NO_CONTEXT;
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context))
    {
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
        eglDestroyContext(m_display, m_context);
        m_context = EGL_NO_CONTEXT;
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    static constexpr const char s_vxtSource[] = R"EOS(
attribute vec2 aVtxPosition;

varying vec2 vTexCoord;

void main()
{
    gl_Position = vec4(aVtxPosition, 0.0, 1.0);
    vTexCoord = 0.5 * (aVtxPosition + 1.0);
}
)EOS";
    static constexpr int s_vxtSourceLength = sizeof(s_vxtSource);

    GLuint vtxShader = glCreateShader(GL_VERTEX_SHADER);
    const char* source = s_vxtSource;
    glShaderSource(vtxShader, 1, &source, &s_vxtSourceLength);
    glCompileShader(vtxShader);

    static constexpr const char s_fragSource[] = R"EOS(
precision highp float;

uniform sampler2D uImageTex;

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
            std::printf("Shader compilation error:\n%s\n", buffer);
            delete[] buffer;
        }

        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        m_program = 0;

        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
        eglDestroyContext(m_display, m_context);
        m_context = EGL_NO_CONTEXT;
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
        return false;
    }

    static constexpr const float s_plane[] = {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_plane), s_plane, GL_STATIC_DRAW);

    glUseProgram(m_program);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glViewport(0, 0, m_width, m_height);
    return true;
}
