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

#include <cstdint>
#include <gio/gio.h>

namespace IPC
{
class Message
{
  public:
    static constexpr size_t MESSAGE_SIZE = 32;
    static constexpr size_t PAYLOAD_SIZE = MESSAGE_SIZE - sizeof(uint64_t);

    Message() noexcept = default;
    Message(uint64_t code) noexcept : m_code(code)
    {
    }

    uint64_t getCode() const noexcept
    {
        return m_code;
    }

    const uint8_t* getPayload() const noexcept
    {
        return m_payload;
    }

    uint8_t* getPayload() noexcept
    {
        return m_payload;
    }

    const char* exposeAsBuffer() const noexcept
    {
        return reinterpret_cast<const char*>(this);
    }

    char* exposeAsBuffer() noexcept
    {
        return reinterpret_cast<char*>(this);
    }

  private:
    uint64_t m_code = 0;
    uint8_t m_payload[PAYLOAD_SIZE] = {};
};
static_assert(sizeof(Message) == Message::MESSAGE_SIZE, "IPC Message class size is wrong");

class MessageHandler;
class Channel
{
  public:
    Channel(MessageHandler& handler) noexcept;
    Channel(MessageHandler& handler, int peerFd) noexcept;

    Channel(Channel&& other) noexcept;
    Channel& operator=(Channel&& other) noexcept;

    virtual ~Channel()
    {
        closeChannel();
    }

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    bool sendMessage(const Message& message) const noexcept;
    int detachPeerFd() noexcept;
    void closeChannel() noexcept;

  private:
    bool configureLocalSocketFromFd(int localFd) noexcept;

    MessageHandler& m_handler;
    GSocket* m_localSocket = nullptr;
    GSource* m_socketSource = nullptr;
    int m_peerFd = -1;
};

class MessageHandler
{
  public:
    virtual ~MessageHandler() = default;

    virtual void handleMessage(Channel& channel, const Message& message) noexcept = 0;

    virtual void handleError(Channel& channel, const GError* /*error*/) noexcept
    {
        channel.closeChannel();
    }

    virtual void handlePeerClosed(Channel& channel) noexcept
    {
        channel.closeChannel();
    }
};
} // namespace IPC
