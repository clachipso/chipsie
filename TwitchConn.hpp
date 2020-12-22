/*
 * MIT License
 *
 * Copyright (c) 2020 Aaron C. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CHIPSIE_TWITCH_CONNECTION_HPP
#define CHIPSIE_TWITCH_CONNECTION_HPP

#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#endif // _WIN32

#include <string>
#include <queue>

enum TwitchConnStatus {
    TWC_ERROR,
    TWC_NOT_CONNECTED,
    TWC_CONNECTED
};

struct AuthData {
    std::string oauth;
    std::string client_id;
    std::string nick;
    std::string channel;
};

class TwitchConn {
public:
    TwitchConn();
    TwitchConnStatus Init(const AuthData &auth_data);
    void Update();
    TwitchConnStatus GetConnectionStatus() const;
    int GetNumRxMsgs() const;
    std::string GetNextRxMsg();
    void SendMsg(const std::string &msg);
    void Shutdown();

private:
    static const int TX_BUFFER_SIZE = 2048;
    static const int RX_BUFFER_SIZE = 2048;
    static const int LINE_BUFFER_SIZE = 2048;
    
    char tx_buffer[TX_BUFFER_SIZE];
    char rx_buffer[RX_BUFFER_SIZE];
    char line_buffer[LINE_BUFFER_SIZE];
    int line_length;

    SOCKET sock;
    struct addrinfo *hint_results;
    TwitchConnStatus cstatus;
    AuthData credentials;
    std::queue<std::string> rx_queue;
    std::queue<std::string> tx_queue;

    void Connect();
    void Close();
    void Receive();
    void Send();
};

#endif // SAT_TWITCH_CONNECTION_HPP