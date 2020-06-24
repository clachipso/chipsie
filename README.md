# chipsie
A Twitch chat bot in C/C++

Chipsie is a chatbot designed to work with the Twitch Messaging Interface (TMI).
Chipsie is designed to run and exist in a single channel (belonging to the 
streamer). It has many of the same capabilities as other popular Twitch bots,
and since it's open source, you have complete control over how Chipsie runs and
what it does.

## Features
- Adding/removing operators by channel host 
- Adding/removing commands by channel host and operators
- Dynamic command syntax with parameters
- Configurable message of the day

### Operators

Operators are privilidged users that are appointed and removed by the host that
is running Chipsie. Operators can access nearly all of the commands that a host
does. Being a channel moderator has no effect on a person's operator status.
This decision was made in order to be flexible for each channel's unique needs.

### Configuration

To run Chipsie, a JSON file named auth.json must be in the same folder as the
executable. This file contains account credentials for the account that Chipsie
will run as. The following variables are contained in the auth.json file:

#### token

token is the oauth token provided by Twitch for the user account that Chipsie 
will run as

#### client_id

The client_id is the application id for the chatbot application that is assigned
by Twitch. This is not currently used by Chipsie. You can put whatever you want 
in here (but don't leave it blank). Alternatively, if you get your bot verified
by Twitch, you may find it useful to have this.

#### nick

The nickname of the account that Chipsie will run as. This could be the host's 
name or the name of a separate account that the bot is run as.

#### channel

The name of the channel that Chipsie will join and exist in. This is the host's
name, and can be the same value that was assigned to nick.

### Database

The first time Chipsie runs, it will create a database to store operators,
commands, and the MOTD. If you ever wish to clear your database and start fresh, 
simply delete the existing database file.