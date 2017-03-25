# Isotoxin
Multiprotocol messenger for windows XP and later with tox support

## Features
  * Standard features like:
    * Nickname customization
    * Status customization
    * Friendlist
    * One to one conversations
    * Message history
    * Unread messages notification
  * Autoupdate
  * Audio/Video calls
  * Desktop sharing via video calls
  * File transfers
  * File transfer resuming
  * UI Themes
  * Multilangual
  * Multiprofile
  * Metacontacts
  * Avatars
  * Faux offline messaging
  * DNS discovery (tox1 and tox3) 
  * Tox group chats (and audio group chats)
  * Send message create time (only Isotoxin to Isotoxin)
  * Unlimited message length (only Isotoxin to Isotoxin)
  * Emoticons
  * Inline images
  * Spell checker + Spelling dictionary build-in downloader

## Screenshots

![Main Window (Dark Theme)](http://isotoxin.im/screens/screenshot8.png)
![Main Window (Light Theme)](http://isotoxin.im/screens/screenshot5.png)
![Settings)](http://isotoxin.im/screens/screenshot7.png)
![Prepare image)](http://isotoxin.im/screens/screenshot6.jpg)

## Build instruction

### Windows (Visual Studio 2015)
- Visual Studio 2015 must be installed.
- Command line **git** must be installed (it used to get some libs)
- Enter **build-win** folder. Threre are 3 utils: **7z**, **grep**, **wget**
- run **1-get-libs.cmd** - it will download some libs (such as zlib, pnglib and so on) from external sources. All you need is internet access. **wget** and **git** used to download, **7z** used to unpack files.
- run **2-build-libs-2015.cmd** - it (re)builds external libs.
- run **3-build-isotoxin-2015.cmd** - build of isotoxin.exe, plghost.exe and proto.dll's.
- run **4-build-assets.cmd** - it creates isotoxin.data - zip archive with Isotoxin assets. Also, you can take it from https://github.com/isotoxin/isotoxin/releases or http://isotoxin.im/files from latest version archive.

- sometimes you have to run **5-update-libs.cmd** to update toxcore or/and download new external libs. Don't forget to run **2_build_libs.cmd** again.

- If you want to build 64-bit version of Isotoxin, just run **2-build-libs-2015.cmd** and **3-build-isotoxin-2015.cmd** with **64** parameter:
**2-build-libs-2015.cmd 64**
**3-build-isotoxin-2015.cmd 64**

### Linux
(work in progress)

