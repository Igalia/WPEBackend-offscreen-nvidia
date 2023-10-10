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

#include <glib.h>

#include <cstdint>

namespace IPC
{
class Message
{
  public:
    static constexpr size_t MESSAGE_SIZE = 32;
    static constexpr size_t PAYLOAD_SIZE = MESSAGE_SIZE - 2 * sizeof(uint16_t);

    Message() noexcept = default;

    uint16_t getCode() const noexcept
    {
        return m_code;
    }

    uint16_t getFDCount() const noexcept
    {
        return m_fdCount;
    }

    template <typename T> T* getPayload() noexcept
    {
        return reinterpret_cast<T*>(m_payload);
    }

    template <typename T> const T* getPayload() const noexcept
    {
        return reinterpret_cast<const T*>(m_payload);
    }

  protected:
    Message(uint16_t code, uint16_t fdCount = 0) noexcept : m_code(code), m_fdCount(fdCount)
    {
    }

  private:
    uint16_t m_code = 0;
    uint16_t m_fdCount = 0;
    uint8_t m_payload[PAYLOAD_SIZE] = {};
};
static_assert(sizeof(Message) == Message::MESSAGE_SIZE, "IPC Message class size is wrong");

class MessageHandler;
class Channel
{
  public:
    Channel(MessageHandler& handler) noexcept;
    Channel(MessageHandler& handler, int peerFd) noexcept;

    Channel(Channel&& other) = delete;
    Channel& operator=(Channel&& other) = delete;
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    virtual ~Channel()
    {
        closeChannel();
    }

    bool sendMessage(const Message& message) noexcept;

    int detachPeerFd() noexcept;
    void closeChannel() noexcept;

  private:
    int m_localFd = -1;
    int m_peerFd = -1;
    bool configureLocalEndpoint(int localFd) noexcept;

    static gboolean idleCallback(Channel* channel) noexcept;
    guint m_idleSourceId = 0;
    MessageHandler& m_handler;

    bool readNextMessage(Message& message) noexcept;
    bool writeFileDescriptor(int fd) noexcept;
    int readFileDescriptor() noexcept;
};

class MessageHandler
{
  public:
    virtual ~MessageHandler() = default;

    virtual void handleMessage(Channel& channel, const Message& message) noexcept = 0;

    virtual void handleError(Channel& /*channel*/, int /*errnoValue*/) noexcept
    {
    }

    virtual void handlePeerClosed(Channel& /*channel*/) noexcept
    {
    }
};
} // namespace IPC
