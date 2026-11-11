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
make -j4 all DEBUG=no
```

If you did not compile LibWebP, add both `DISABLE_WEBP=1` and
`USER_DEFINES=-DWEBP_DISABLED`. This disables WebP image decoding; the standard
build retains it. Always use the explicit `all` target.

ANSI builds have been removed; all builds use Unicode Windows APIs.
Keep the Windows 8.1 API baseline;
do not lower `WINVER`. The PE loader header uses a conservative 6.0 version,
independent of the Windows 8.1 API target.

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

## Startup failures and diagnostic logging

`0xc000007b` means Windows rejected an executable or DLL image. Check a fresh,
fully extracted package first: this MinGW build and its bundled OpenSSL and
thread-runtime DLLs are all x86, including on 64-bit Windows. Do not mix them with
x64 DLLs or download replacement system DLLs from DLL download sites.

Runtime diagnostics default to enabled. Disable them under File > Preferences >
Connection > Save diagnostic log beside the executable. The setting is saved
immediately. `DiscordMessenger-diagnostics.txt` contains timestamps, thread IDs,
startup stages, gateway lifecycle/heartbeats, and numeric HTTP/transport failures.
It excludes message bodies, tokens, URLs, and raw gateway payloads. Rotation keeps
one previous file after approximately 1 MiB. The executable's folder must be
writable; inability to write a log does not stop the application.

Windows loader failures can occur before the app runs, so they cannot be captured
by the runtime logger. `tools/Start-Diagnostics.ps1` is included beside the EXE in
the diagnostic package. Run it from PowerShell there with
`& .\Start-Diagnostics.ps1`; it records OS details, local PE architectures,
checksums, and startup errors in `DiscordMessenger-loader-report.txt`. If script
execution is restricted, inspect the script and use your organization's approved
way of running PowerShell scripts. Compare file hashes with `SHA256SUMS.txt` in
the package. If a clean package still fails, a Windows loader trace (for example
Process Monitor) is needed to identify the rejected DLL; the launcher alone
cannot establish which system dependency failed.

## Unicode-only builds and package checks

CI produces only the Unicode `PRE-MinGW` artifact. The `PRE-MinGWA` artifact and
Visual Studio `DebugAnsi`/`ReleaseAnsi` configurations are removed. MinGW rejects
`UNICODE=no` rather than silently producing an unsupported binary. This does not
remove explicit ANSI APIs needed by third-party libraries or change text stored
as UTF-8 in Discord payloads and settings.

The build retains the working PE loader header of 6.0 and the Windows 8.1 API
target of 0x0603. Do not raise the loader header merely to match the API target.
`python3 tools/check_windows_package.py bin/prepared` verifies package bitness,
the executable header, and bundled compiler/library DLL dependencies. CI includes
the pthread runtime DLL, launcher report script, and SHA-256 manifest as well as
the OpenSSL DLLs. These checks do not replace testing on real Windows.

## Dark appearance on Windows 8.1

Enable **File > Preferences > Appearance > Dark appearance**, then fully exit
(including the tray icon) and restart. The choice is saved immediately. Light
appearance remains the default. Disable the option and restart to return to the
Windows system palette.

The app uses a slate palette with light text, brighter links/mentions, distinct
selected rows, and contrast-adjusted role names. It themes its message surfaces,
inputs, lists/trees, dialogs, checkboxes/radio buttons, buttons, tabs, progress and
status controls. Photos and attachment images retain their original colors.
The implementation uses documented Win32/GDI/common-controls APIs available on
Windows 8.1, rather than Windows 10 dark-mode ordinals or registry assumptions.

Dark appearance also supplies a flat caption with wider window buttons, dark
menu bars/popups, borderless list panels and slightly tighter panel spacing.
The server rail has a 4-pixel scroll indicator with an 8-pixel drag target
(scaled with DPI); click its track or drag it, use the mouse wheel, or focus the
rail and use arrows, Page Up/Down and Home/End. Its redundant scrollbar-toggle
button is replaced by a wider server chooser. Off-screen server avatars and
mention counts are skipped during painting, and tooltips are rebuilt after a
thumb drag rather than on every pointer movement.

Other list/tree/message scrollbars retain native tracking and hit targets with
a slim painted thumb and dark gutter. Native menus remain attached: Alt/F10,
mnemonics, enabled/checked commands and the system menu still use Windows menu
handling. Native frame geometry, resizing, maximize/restore and caption dragging
are preserved. Painting responds to window/input events; there are no added
animation or polling timers. The caption uses the documented Vista+ DWM
non-client rendering policy, without immersive dark-mode attributes.

OS-owned message boxes and file pickers retain their Windows appearance.
If Windows High Contrast is active when starting the app, the system palette
wins over the dark preference. Restart after changing High Contrast while the
app is running. UI theme changes also require restart so existing windows and
cached brushes cannot end up with mismatched palettes.

Portable contrast regression check:

```sh
g++ -std=c++11 -Wall -Wextra -Isrc/windows tests/theme_palette.cpp -o /tmp/dm-theme-palette
/tmp/dm-theme-palette
```

The portable scrollbar regression check covers empty/shrinking pages, dragging
and large ranges:

```sh
g++ -std=c++11 -Wall -Wextra -Isrc/windows tests/scroll_geometry.cpp -o /tmp/dm-scroll-geometry
/tmp/dm-scroll-geometry
```

`tests/theme_controls.cpp` is an offline Win32 fixture linking `Theme.cpp` and
`ThemeChrome.cpp`. It checks dialog colors, checkbox behavior, native scrolling,
caption hit targets/cancellation/minimize/maximize/restore, menu state/cleanup,
GDI handle stability across 100 menu cycles and keyboard menu commands. It captures
`theme-controls.bmp` in the working directory. Run it in separate processes for
dark (default) and light (`light` argument); `popup` captures an open dark popup
instead of testing Alt+F. The keyboard test uses SendInput and needs the fixture
to be foreground on an interactive desktop. It does not load account data or
contact Discord; its icon-classification/drawing helpers are stubs, while its image-list test
uses a generated black icon. The fixture can be built with
MinGW using the same Windows version/Unicode defines and include paths as the
application, linking `comctl32`, `uxtheme`, `dwmapi` and `gdi32`. Compile and link
`tests/theme_controls.rc` with windres too, so the fixture uses the same
common-controls activation context as the app.

The shared-palette approach was informed by upstream PR
<https://github.com/DiscordMessenger/dm/pull/267>. This fork does not import its
unfinished scroll-container changes or upgrade the compatibility dependencies.

## Refined dark layout and chat appearance

In dark mode the main menu is hidden by default. Click the three-line button in
the caption, press Alt/F10, or use Alt plus a menu mnemonic to open it. It hides
after a command or cancellation. The title includes the server and channel names.
The channel header is larger/bold. Category names use bold text and extra spacing;
click a category heading to fold/unfold it. The old square category icons are omitted.

File > Preferences > Appearance now offers a six-digit channel hover color
(default `#842a52`), rounded/square chat avatars, and a chat font/size selection
(10–24 logical pixels). Font changes require a full restart; message sizing is
independent of sidebar/profile sizing. Choices are Noto Sans, Segoe UI, Calibri,
Candara, Corbel, Verdana, Tahoma, Trebuchet MS, and gg sans. gg sans requires an
existing licensed installation; unavailable choices fall back to Noto Sans.
Noto Sans, part of Discord's fallback font stack, is bundled as a private memory
font: it does not install fonts system-wide. Keep `NotoSans-LICENSE.txt` with
redistributed builds (source and license: `res/NotoSans-*`, upstream
https://github.com/notofonts/noto-fonts).

Today’s message timestamps show only the time; yesterday’s show “Yesterday at”
and the time. Older dates retain the localized date format. Dates are smaller,
regular-weight and muted; author names are larger, with matching hover sizing.
Role headers use contrast-adjusted Discord role colors and a small accent marker
instead of the native horizontal separator.

Dark scrollbars use separate, clipped drawing surfaces over the native gutters.
Mouse input is forwarded to native tracking, retaining native ranges and keyboard
behavior; no polling or animation timers are added. The server indicator keeps a
steady color while dragging. Member refreshes capture a visible member and its
pixel offset before rebuilding rows, then restore that location after layout.

`python3 tests/relative_dates.py` exercises the production formatter around
midnight, year rollover and daylight-saving changes. The control fixture also
supports `hidden` to test the on-demand menu, and drives a real native scrollbar
with mouse input while checking gutter colors during tracking. These offline
checks do not replace live Discord and Windows 8.1 testing.

The GitHub Actions workflow **Build Windows 8.1 x86 (MinGW)** runs on an Ubuntu
host but produces only the Windows x86 application package (including WebP).
Its native host executables are temporary regression-test runners, not a Linux
application release. Compiler setup uses explicit official Ubuntu repositories,
bounded download retries, and strict index-refresh errors to avoid an unavailable
Azure mirror or stale package indexes. This does not change runtime requirements
or the Windows 8.1 API target / 6.0 loader header.
