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

#include "Command.hpp"

bool ConvertLineToMsg(const string &line, IrcMessage *msg)
{
    int cursor = 0;
    while ((line[cursor] == ' ' || line[cursor] == '\t')) 
    {
        cursor++;
        if (cursor >= line.size()) return false;
    }
    
    if (line[cursor] == '@')
    {
        int end = line.find(' ', cursor);
        cursor++;
        msg->tags = line.substr(cursor, end - cursor);
        cursor = end;
        while ((line[cursor] == ' ' || line[cursor] == '\t')) 
        {
            cursor++;
            if (cursor >= line.size()) return false;
        }
    }
    else
    {
        msg->tags = "";
    }

    if (line[cursor] == ':')
    {
        int end = line.find(' ', cursor);
        cursor++;
        msg->source = line.substr(cursor, end - cursor);
        cursor = end;
        while ((line[cursor] == ' ' || line[cursor] == '\t')) 
        {
            cursor++;
            if (cursor >= line.size()) return false;
        }
    }
    else
    {
        msg->tags = "";
    }
    

    int end = line.find(' ', cursor);
    msg->command = line.substr(cursor, end - cursor);
    cursor = end;
    while (line[cursor] == ' ' || line[cursor] == '\t') 
    {
        cursor++;
        if (cursor >= line.size())
        {
            msg->parameters = "";
            return true;
        }
    }

    msg->parameters = line.substr(cursor, line.size() - cursor);
    return true;
}

void ProcessMsg(const IrcMessage &msg, MsgQueue *tx_queue)
{
    if (msg.command == "PING")
    {
        string line = "PONG " + msg.parameters;
        tx_queue->push(line);
    }
    else
    {
        printf("ALERT: Unknown command %s\n", msg.command.c_str());
    }
}