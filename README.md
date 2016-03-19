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
- Threre are 4 external utils used: **git**, **7z**, **grep**, **wget**<br>
You have to install command line **git**<br>
**7z**, **grep** and **wget** already in repo.<br>
Also, Visual Studio 2013 must be installed.<br>
- run **1_get_libs.cmd** - it will download some libs (such as zlib, pnglib and so on) from external sources. All you need is internet access. **wget** and **git** used to download, **7z** used to unpack files<br>
- run **2_build_libs.cmd** - it builds downloaded external libs<br>
- run **3_make_libcmt_nomem.cmd** - it creates libcmt_nomem.lib - croped version of standard libcmt.lib without any memory-related functions. We want to use dlmalloc everywhere :) **grep** utility used at this step<br>
- run **4_build_isotoxin.cmd** - final build of isotoxin.exe, plghost.exe and proto.dll's<br>
- run **5_build_assets.cmd** - it creates isotoxin.data - zip archive with Isotoxin assets. Also, you can take it from https://github.com/Rotkaermota/Isotoxin/releases or http://isotoxin.im/files from latest version archive.<br>
<br>
- sometimes you have to run **6_update_libs.cmd** to update toxcore or/and download new external libs. Don't forget to run **2_build_libs.cmd** again.<br>
