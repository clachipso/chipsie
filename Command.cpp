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
#include <stdlib.h>
#include <time.h>

void HandlePrivateMsg(const IrcMessage &msg, MsgQueue *tx_queue);
void HandleUserCmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd);

void InitCmdProcessing()
{
    srand(time(NULL));
}

bool ConvertLineToMsg(const string &line, IrcMessage *msg)
{
    int cursor = 0;
    while ((line[cursor] == ' ' || line[cursor] == '\t')) 
    {
        cursor++;
        if (cursor >= line.size()) return false;
    }
    
    if (line[cursor] == '@') // Line has tags
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
        msg->source = "";
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
    if (msg.command == "PRIVMSG") // Private message
    {
        HandlePrivateMsg(msg, tx_queue);
    }
    else if (msg.command == "PING") // Ping message
    {
        // Always reply with a pong so we don't get booted
        string line = "PONG " + msg.parameters;
        tx_queue->push(line);
    }
    else if (msg.command == "JOIN") // Join command reply
    {

    }
    else if (msg.command == "001") // Welcome message
    {

    }
    else if (msg.command == "002") // Server host ID
    {

    }
    else if (msg.command == "003") // Server creation time
    {

    }
    else if (msg.command == "004") // User info, not currently used by Twitch
    {

    }
    else if (msg.command == "353") // NAME command reply
    {

    }
    else if (msg.command == "366") // End of NAME list reply
    {

    }
    else if (msg.command == "375") // Message of the day start
    {

    }
    else if (msg.command == "372") // Message of the day line
    {
        
    }
    else if (msg.command == "376") // Message of the day end
    {

    }
    else
    {
        printf("ALERT: Unknown command %s\n", msg.command.c_str());
    }
}

void HandlePrivateMsg(const IrcMessage &msg, MsgQueue *tx_queue)
{
    int cursor = 0;
    while (msg.parameters[cursor] == ' ' || msg.parameters[cursor] == '\t')
    {
        cursor++;
        if (cursor >= msg.parameters.length())
        {
            cursor = msg.parameters.length() - 1;
            break;
        }
    }
    if (msg.parameters[cursor] != '#')
    {
        printf("WARNING: Received malformed PRIVMSG command\n");
        return;
    }
    int end = msg.parameters.find(' ', cursor);
    cursor++;
    string channel = msg.parameters.substr(cursor, end - cursor);
    cursor = end + 1;

    cursor = msg.parameters.find(':', cursor);
    if (cursor == string::npos)
    {
        printf("WARNING: Received PRIVMSG with no text\n");
        return;
    }
    cursor++;
    string priv_msg = msg.parameters.substr(cursor);
    
    // Is the sender trying to issue a command?
    cursor = 0;
    while (priv_msg[cursor] == ' ' || priv_msg[cursor] == '\t')
    {
        cursor++;
        if (cursor >= priv_msg.length())
        {
            cursor = priv_msg.length() - 1;
            break;
        }
    }
    if (priv_msg[cursor] == '!')
    {
        // User is attempting to send command. Extract it
        cursor++;
        end = cursor;
        while (priv_msg[end] != ' ' && priv_msg[end] != '\t')
        {
            end++;
            if (end >= priv_msg.length()) 
            {
                end = priv_msg.length();
                break;
            }
        }
        string user_cmd = priv_msg.substr(cursor, end - cursor);
        HandleUserCmd(msg, tx_queue, channel, user_cmd);
        
    }
    else
    {
        printf("private message started with %c\n", priv_msg[cursor]);
    }
    
}

void HandleUserCmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd)
{
    if (cmd == "dice")
    {
        int d6 = rand() % 6 + 1;
        string resp = "PRIVMSG #" + chan + " :You rolled a " + to_string(d6);
        tx_queue->push(resp);
    }
}