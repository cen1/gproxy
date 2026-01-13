# GProxy++
[![build](https://github.com/cen1/gproxy/actions/workflows/build.yml/badge.svg)](https://github.com/cen1/gproxy/actions/workflows/build.yml)

This is a modernized GProxy++ fork used on Eurobattle.net platform.  
It includes several improvements:
- Modern CMake support.
- Conan v2 support for automatic dependency resolution.
- Automatic lobby spoof-check.
- Additional "version" field in GPSC_INIT to uniquely identify a GProxy build.
- A "cmdbuffer" feature which allows you to walk up and down your chat history with your arrow key (like in a Linux terminal).
- Playing custom sounds on various events like "Game Started".
- GPS protocol extension to communicate the locally detected W3 version to GHost and enforce same W3 patch lobbies to avoid desyncs.
- Boost library updates.

Functionality that was removed:
- LAN games.
- Interactive commands/stdin.

While this fork is primarily PvPGN focused, LAN functionality could be re-introduced if there is ever a need for it.

# Run

```
./gproxy.exe --w3exe="D:/local_games/Warcraft III 1.26/war3.exe" --mode=restricted --pvpgn-server=server.eurobattle.net
```

All start args are optional. If `--w3exe` is not set and pointing to your base game directory, the base path to your game installation will be read from the registry `InstallPath` entry.  
GProxy will not acknowledge the game as started unless the paths of the running process match to avoid any kind of patch version confusion. 

Be aware that you also need to add an additional registry entry under `Computer\HKEY_CURRENT_USER\Software\Blizzard Entertainment\Warcraft III`, key `Battle.net Gateways`, for example:

``` 
127.0.0.1
-1
GProxy
```

This will add an additional realm to your game which you can then choose with the magnifying glass before connecting to bnet.

# Build 

## On Windows

Use Visual Studio CMD (x86 Native Tools COmmand Prompt).

**Important:** GProxy++ must be built as 32-bit (x86) to properly interact with the 32-bit Warcraft III process.

```
conan profile detect --force
conan install . -of build_conan -s compiler.cppstd=20 -s build_type=Release -s arch=x86 -o *:shared=True --build=missing
cmake --preset conan -G "Visual Studio 17 2022" -A Win32
cmake --build build_conan --config Release
```
## On Debian 13 (Cross-Compilation to Windows)

**Prerequisites:**
```bash
sudo apt-get install -y cmake build-essential mingw-w64 gcc-mingw-w64-i686 g++-mingw-w64-i686 python3 python3-pip
pip3 install conan --upgrade
conan remote update conancenter --url="https://center2.conan.io"
```

**Build:**
```bash
conan profile detect --force
conan install . -of build_mingw -s compiler.cppstd=20 -s build_type=Release -s arch=x86 -s os=Windows -s compiler=gcc -s compiler.version=14 -o '*:shared=True' --build=missing
cmake --preset linux-mingw-cross
cmake --build build_mingw --config Release
```

Output: `build_mingw/gproxy/gproxy.exe`

**Run:**
```bash
WINEPATH="/usr/lib/gcc/i686-w64-mingw32/14-win32;/usr/i686-w64-mingw32/lib" wine build_mingw/gproxy/gproxy.exe --help
```

# gproxy.ini

You can place an .ini file in the same directory as `gproxy.exe`.

```
pvpgn_server = pvpgn.example.com
game_port = 6125
console = 1
option_sounds = 1
sound_1 = 1
sound_2 = 1
sound_3 = 1
sound_4 = 1
sound_5 = 1
sound_6 = 1
sound_7 = 1
sound_8 = 1
sound_9 = 1
sound_10 = 1
sound_11 = 1
sound_12 = 1
chatbuffer = 0
debug = 0
telemetry = 0
autojoin = 0
autojoin_gndelay = 2
autojoin_delay = 2
```

# Description

## About
GProxy++ is a "disconnect protection" tool for use with Warcraft III when connecting to specially configured GHost++ servers.
It allows the game to recover from a temporary internet disruption which would normally cause a disconnect.
The game will typically pause and display the lag screen while waiting for a player to reconnect although this is configurable on the server side.
There is a limit to how long the game will wait for a player to reconnect although this is also configurable on the server side.
Players can still vote to drop a player while waiting for them to reconnect and if enough players vote they will be dropped and the game will resume.

You can use GProxy++ to connect to ANY games, GHost++ hosted or otherwise.
Players can also connect to reconnect enabled servers even without using GProxy++, they just won't be protected if they get a disconnect.
You can also use GProxy++ for regular chat or channel monitoring.
Any games created by a reconnect-enabled host will be listed in blue in your game list on the LAN screen.

## Help

Type !help at any time for help when using GProxy++.

## Technical Details

GProxy++, as you may have guessed, proxies the connection from Warcraft III to the GHost++ server.
This is done because Warcraft III is not able to handle a lost connection but, with some trickery, GProxy++ can.
We assume that the (local) connection from Warcraft III to GProxy++ is stable and we permit the (internet) connection from GProxy++ to GHost++ to be unstable.

Warcraft III <----local----> GProxy++ <----internet----> GHost++

When GProxy++ connects to a GHost++ server it sends a GPS_INIT message which signals that the client is using GProxy++.
The server responds with some required information and marks the client as being "reconnectable".
Since the GPS_INIT message is not recognized by non-GHost++ hosts and we want GProxy++ to be able to connect to either, we need some way of identifying GHost++ hosts.
This is done by setting the map dimensions in the stat string to 1984x1984 as these map dimensions are not valid.
The map dimensions are ignored by Warcraft III so there don't seem to be any side effects to this method.
If GProxy++ connects to a game with map dimensions of 1984x1984 it will send the GPS_INIT message, otherwise it will act as a dumb proxy.

After the connection is established both GProxy++ and GHost++ start counting and buffering all the W3GS messages both sent and received.
This is necessary because a broken connection might be detected at different points by either side.
In order to resume the connection we need to ensure the data stream is synchronized which requires starting from a known-good-point.

If GProxy++ detects a broken connection it starts attempting to reconnect to the GHost++ server every 10 seconds.
When a reconnection occurs GProxy++ sends a GPS_RECONNECT message which includes the last message number that it received from the server.
GHost++ then responds with its own GPS_RECONNECT message which includes the last message number that it received from the client.
Both sides then stream all subsequent messages buffered since the requested message numbers and the game resumes.

As an optimization a GPS_ACK message is sent by both sides periodically which allows the other side to remove old messages from the buffer.

Unfortunately there is a quirk in Warcraft III that complicates the reconnection process.
Warcraft III will disconnect from GProxy++ if it doesn't receive a W3GS action at least every 65 seconds or so.
This puts a hard limit on the time we can take to reconnect to the server if the connection is broken.
I decided that 65 seconds was too short so I needed some way to send additional W3GS actions to extend the reconnect time.
However, W3GS actions (even empty ones) are "desyncable" which means we must ensure that every player receives the same actions in the same order.
We can't just have GProxy++ start sending empty actions to the disconnected player because the other players didn't receive those same actions.
And since the broken connection will be detected at different points, we can't go back in time to send empty actions to the connected players after someone disconnects.
The solution, while inelegant, is to send a defined number of empty actions between every single real action.
GProxy++ holds these empty actions in reserve, only sending them to the client when a subsequent real action is received.
Note that as an optimization, GHost++ only sends these empty actions to non-GProxy++ clients.
The number of empty actions is negotiated in the GPS_INIT message and it is assumed that GProxy++ will generate the correct number of empty actions as required.

If GProxy++ doesn't receive a W3GS action for 60 seconds it will "use up" one of the available empty actions to keep the Warcraft III client interested.
If it doesn't have any more empty actions available it will give up and allow Warcraft III to disconnect.
This is because generating another empty action for the Warcraft III client would result in a desync upon reconnection.
After reconnecting GProxy++ knows how many empty actions it had to use while the connection was broken and only sends the remaining number to the Warcraft III client.

GHost++ itself will only bring up the lag screen for a disconnected GProxy++ client when the client falls behind by bot_synclimit messages.
This means it's important for bot_synclimit to be set to a "reasonable value" when allowing GProxy++ reconnections.
Otherwise the game will not be paused while waiting and when GProxy++ reconnects the game will play in fast forward for the player until catching up.
During the catchup period the player will not be able to control their units.

Note that very occasionally the game temporarily enters a state where empty actions cannot be generated.
If the connection is broken while in this state GProxy++ will be limited to 65 seconds to reconnect regardless of the server's configuration.
In most games this will only happen a couple of times for a fraction of a second each time, but note that the lower the server's bot_latency the less chance of this happening.

# License

Project is licensed under Apache 2.0 unless otherwise specified in individual file headers.