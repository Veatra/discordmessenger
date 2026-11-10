# Discord Messenger for Windows

This fork of Discord Messenger targets **Windows 8.1**. It keeps the existing
32-bit Windows desktop client and quiet background reconnects. Windows versions
older than 8.1 are no longer supported by this fork.

Its motto: *It's time to ditch MSN and Yahoo.*

**NOTE**: This is beta software, so there may be issues which need to be fixed!

The project is licensed under the MIT license.

## Disclaimer

Using third party clients is against Discord's TOS! Although the risk to get banned is low, the
risk is there! The author of this software is not responsible for the status of your Discord
account.

See https://twitter.com/discord/status/1229357198918197248.

## Discord Server

A Discord server about this client can be joined here: https://discord.gg/cEDjgDbxJj

###### Note, you will need to use an official client to accept invitations currently. This may change in the future.

## Screenshots

![Windows 11 screenshot](doc/ss_11.png)
![Windows XP screenshot](doc/ss_xp.png)
![Windows 95 screenshot](doc/ss_95.png)
![Windows NT 3.1 screenshot](doc/ss_nt31.png)

## Minimum System Requirements

- Windows 8.1 (32-bit or 64-bit Windows, using the 32-bit MinGW executable).
- Matching 32-bit OpenSSL DLLs and compiler runtime DLLs beside the executable.
- Visual Studio builds also need their matching Visual C++ runtime, compatible
  with Windows 8.1. MinGW binaries do not use that runtime.

Screenshots above are from the upstream project and include historical platforms.

## Building

Before you can start the build process, after cloning the project (You should NOT download it as
ZIP, unless you know that you should also download the submodules individually and unzip them in
the correct locations), check out the submodules with the command:
`git submodule update --init`.

Then you can start the build process.

You can build this project with Visual Studio or MinGW on Linux.

### 1. Visual Studio

Use a Windows SDK and compiler/runtime that support Windows 8.1; do not select
a Windows 10-only runtime or APIs. The projects target `_WIN32_WINNT=0x0603`.

1. Compile OpenSSL for Win32, or find a distribution of OpenSSL 3.X.

You can acquire OpenSSL for Win32 from the following website if you don't want to bother with
compiling it:
https://slproweb.com/products/Win32OpenSSL.html

(Note: Do not download the "Light" versions, as they only contain the DLLs.)

(Note: Download Win64 if you want to compile for x64, Win32 if you want to compile for Win32).

2. Add an entry to your user/system environment variables called `OPENSSL_INSTALL`. (replace with
`OPENSSL_INSTALL64` everywhere if you are compiling for 64-bit)

Set its value to the place where your OpenSSL distribution is located.

3. If you want to use a later version of libwebp, acquire libwebp from the following web site:
https://developers.google.com/speed/webp/download.  Extract the archive and place "libwebp.lib" in
`vs/libs`.

4. Open the Visual Studio solution `vs/DiscordMessenger.sln`.

5. Click the big play button.  (Both x86 and x64 targets are supported.)

6. Enjoy!

### 2. MinGW (on Linux, targeting Windows)

(Note: x64 compilation with MinGW is currently not supported)

1. Acquire mingw-w64:
```
sudo apt install mingw-w64 gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix
```

2. Check out Discord Messenger's fork of [OpenSSL](https://github.com/DiscordMessenger/openssl).

3. Build it: `./buildit` or `TOOLCHAIN_PATH=[custom toolchain path] ./buildit`
(if you're using the Pentium toolchain you should specify the TOOLCHAIN_PATH)

4. Set `OPENSSL_DIR` in your environment variables to your OpenSSL checkout directory.  If you want
to remember the path, edit the Makefile to use it as your default(but make sure to not check in your
change when sending a PR!), or `export` it.

5. Check out Discord Messenger's fork of [LibWebP](https://github.com/DiscordMessenger/libwebp).

You can skip this step and steps #6 and #7.

6. Run the following commands:
```
mkdir build && cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE=../win32.cmake
-or-
TOOLCHAIN_PATH=[custom toolchain path] cmake .. -DCMAKE_TOOLCHAIN_FILE=../win32.cmake

make -j12
```

7. Set `LIBWEBP_DIR` to `[libwebp checkout dir]/build`.

8. Finally, you are ready to compile Discord Messenger.

Use the following command line:
```
make -j4 all DEBUG=no UNICODE=yes
```

If you did not compile LibWebP, add both `DISABLE_WEBP=1` and
`USER_DEFINES=-DWEBP_DISABLED`. This disables WebP image decoding; the standard
build retains it. Always use the explicit `all` target.

For an ANSI build, use `UNICODE=no` with separate `BUILD_DIR` and `BIN_DIR`
values to avoid reusing Unicode objects. Keep the default Windows 8.1 API and
subsystem baseline; do not lower `WINVER`.

The finished binary will be placed in `./bin/DiscordMessenger.exe`. Enjoy!

### Validation and Windows 8.1 behavior

Run the portable expiry regression check with a native C++ compiler:

```sh
g++ -std=c++11 -Wall -Wextra -Isrc/core tests/typing_expiry.cpp -o /tmp/dm-typing-expiry
/tmp/dm-typing-expiry
python3 tests/status_bar_timers.py
```

The Win32 timer test uses doubles for the platform calls; it is not a Windows GUI
test. On Windows 8.1, smoke-test login, server/DM switching, attachments, typing
indicators, tray hide/restore, minimize/restore, and a network interruption. Check
that reconnects stay quiet and the restored window keeps its previous size.

This fork removes legacy listbox fallback views, low-color icon/bitmap variants,
and duplicate NT 3.x dialogs. It retains the shared API/thread/network wrappers
required by the bundled dependencies. Typing animation stops while hidden or
minimized, but expiry and connection maintenance continue. The typing indicator
also releases temporary text buffers and expires each user after ten seconds.

Compilation and the portable checks do not establish live Windows 8.1 readiness.

## Features
### Implemented

- Viewing and interacting with servers and direct messages
- Viewing and downloading images and attachments
- Uploading attachments
- Editing messages
- Deleting messages
- Replying to messages
- Typing indicator
- URL hotlinks with untrusted link warning dialog (1)
- Viewing member list in servers (2)
- Viewing pinned messages in server
- Embeds (6)
- Showing profile pictures in DM list
- User notes

### Unimplemented but planned

- Friends list
- Viewing member list in group messages and DMs
- Dark mode on modern systems (3)
- Using an asynchronous HTTP library (4)
- Entering voice channels (5)
- Blocking, closing DMs, removing as friend
- Muting channels
- Changing nickname
- More options in the "Preferences" menu
- Assigning a custom status

### Unplanned Features

- Sending friend requests
- Creating DM channels
- Logging in using QR code (7) (8)
- Logging in using e-mail address and password (7)
- Joining servers (7)

### Note
1. You may need a modern browser to actually access most links.

2. Only the first 100 users. I plan on changing it.

3. Would take a lot of effort, but theoretically it is possible. No, it's not as simple as hooking
   certain APIs.

4. Currently, we are using a synchronous HTTP library, with threads to simulate async behavior.

5. Planned for far in the future.

6. Embeds are incomplete, for example, fields don't function properly.

7. Action is weighted by Discord's anti-spam measures. It could cause the target to get autobanned.

8. Some code already exists, but this feature is unfinished and will probably never be finished.

## Attributions

Discord Messenger is powered by the following external libraries:

- [JSON for Modern C++](https://github.com/nlohmann/json)
- [Boost](https://www.boost.org)
- [Libwebp](https://github.com/webmproject/libwebp)
- [Httplib](https://github.com/yhirose/cpp-httplib)
- [Asio](https://think-async.com/Asio)
- [Websocketpp](https://github.com/zaphoyd/websocketpp)

Although these libraries are vendored, you can replace them with the latest version, and the MSVC
build will keep working.  Adjustments were made to certain libraries to make them compile on MinGW.
See `doc/` for details.
