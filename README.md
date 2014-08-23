XBMC LiveStreams PVR Add-on
===========================

This is a PVR add-on for XBMC. It add support for Live TV watching and
EPG TV Guide.

------------------------------------------

Written by: Zoltan Csizmadia

Based on: 

          XBMC IPTV Simple Add-on by Anton Fedchin 
          https://github.com/afedchin/xbmc-addon-iptvsimple

          XBMC PVR Demo Add-on by Pulse-Eight 
          http://www.pulse-eight.com

          XBMC LiveStreams Video Add-on by divingmule
          https://github.com/divingmule/plugin.video.live.streams

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
See the file COPYING for more information.

---------------------------------------------

Precompiled binaries

Precompiled zip packages can be found in "release" folder.

---------------------------------------------

How to compile

=============================
       Linux, OS-X, BSD
=============================

- Clone the GIT repository
- cd xbmc-pvr-livestreams
- sh autogen.sh
- ./configure
- make dist-zip

This will prepare zip file in current folder to install the plugin via XBMC interface.

=============================
           Windows
=============================

- Install Visual C++ Express 2010 (follow the instructions on the wiki for XBMC itself)
- Clone the GIT repository
- cd xbmc-pvr-livestreams
- make.cmd dist-zip

This will prepare zip file in current folder to install the plugin via XBMC interface.

IMPORTANT:
Please disable *all* PVR addons *before* installing the LiveStreams PVR addon!
