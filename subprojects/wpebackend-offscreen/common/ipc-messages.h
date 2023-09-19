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

#include "ipc.h"

namespace IPC
{
class FrameAvailable final : public Message
{
  public:
    static constexpr uint16_t MESSAGE_CODE = 1;

    FrameAvailable() : Message(MESSAGE_CODE)
    {
    }
};

class FrameComplete final : public Message
{
  public:
    static constexpr uint16_t MESSAGE_CODE = 2;

    FrameComplete() : Message(MESSAGE_CODE)
    {
    }
};

class EGLStreamFileDescriptor final : public Message
{
  public:
    static constexpr uint16_t MESSAGE_CODE = 3;

    EGLStreamFileDescriptor(int fd) : Message(MESSAGE_CODE, 1)
    {
        *getPayload<int>() = fd;
    }

    int getFD() const noexcept
    {
        return *getPayload<int>();
    }
};

class EGLStreamState final : public Message
{
  public:
    static constexpr uint16_t MESSAGE_CODE = 4;

    enum class State
    {
        WaitingForFd,
        Connected,
        Error
    };

    EGLStreamState(State state) : Message(MESSAGE_CODE)
    {
        *getPayload<State>() = state;
    }

    State getState() const noexcept
    {
        return *getPayload<State>();
    }
};
} // namespace IPC
