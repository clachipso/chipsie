
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

#ifndef CHIPSIE_NETWORKING_HPP
#define CHIPSIE_NETWORKING_HPP

#include <string>
#include <queue>
using namespace std;

#include "winsock2.h"

enum NetStatus
{
    NET_OK,
    NET_ERROR,
    NET_CONNECT_FAILED
};

struct AuthData
{
    string oauth;
    string client_id;
    string nick;
    string channel;
};

typedef queue<string> MsgQueue;

NetStatus InitNetworking(const AuthData &auth_data);
NetStatus UpdateNetworking(MsgQueue *rx_queue, MsgQueue *tx_queue);
void StopNetworking();

#endif // CHIPSIE_NETWORKING_HPP