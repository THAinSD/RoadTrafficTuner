ROAD TRAFFIC TUNER for X-Plane 12
=================================

Rebalances X-Plane's own road traffic between road types - for example,
making residential streets mostly empty while main roads and freeways stay
busy. X-Plane's own files are never modified.

Requirements: X-Plane 12 on Windows (64-bit) or macOS 12 or newer
(Apple silicon or Intel). A Linux (64-bit) version is included but has not
yet been tested in X-Plane; reports from Linux users are welcome.


INSTALLING
----------
1. Unzip the download.
2. Copy the RoadTrafficTuner folder into  X-Plane 12/Resources/plugins/
3. Start X-Plane. "Road Traffic Tuner" appears in the Plugins menu.

MAC ONLY - ALLOWING THE PLUGIN
The plugin is not signed by Apple, so the first time X-Plane starts with it,
macOS shows a warning:

    "RoadTrafficTuner.xpl" Not Opened
    Apple could not verify "RoadTrafficTuner.xpl" is free of malware...

Click DONE - not "Move to Trash", which deletes the plugin. Then:

  a. Open System Settings > Privacy & Security and scroll down to the
     Security section. Next to the message about "RoadTrafficTuner.xpl",
     click Allow Anyway.
  b. Restart X-Plane. macOS warns once more, over the X-Plane splash
     screen; this time click Open Anyway.

The plugin then loads normally and macOS will not ask again.

Alternative (before starting X-Plane): open Terminal, type the following
with a space at the end, drag the RoadTrafficTuner folder from the plugins
folder onto the Terminal window, and press Return:

    xattr -dr com.apple.quarantine


USING IT
--------
1. Open  Plugins > Road Traffic Tuner > Open...
2. Under "Apply to", tick the road networks to change:
     Rest of the world (roads.net)  - used by most of the world
     Europe (roads_EU.net)          - used by European scenery
3. Set the four sliders: Local (residential), Secondary (collector roads),
   Primary (arterials) and Highway. x1.00 is X-Plane's default and x0
   removes cars from that road type. The right-hand column shows the typical
   spacing between cars in a lane, default -> new.
4. Press  Build pack.
5. Restart X-Plane. Changes take effect only after a restart.

Build pack creates  Custom Scenery/zzz_Road_Traffic_Tuner, holding edited
copies of the road files plus the road objects and textures they need.

Your graphics settings still control the total number of cars; the plugin
changes how those cars are shared between road types. Traffic builds up
over a few minutes after loading, so give it time before judging.

Other buttons:
  Remove pack        Deletes the pack (press twice to confirm). Restart
                     X-Plane to return to default traffic.
                     See "Warnings until you restart" below.
  Reset to defaults  Returns the sliders and checkboxes to their starting
                     values. Press Build pack to apply.

Plugins > Road Traffic Tuner > About...  explains what the plugin does.

Warnings until you restart: X-Plane keeps using the pack's files while it
runs. If you press Remove pack, or rebuild with a road network unticked,
during a flight, the files X-Plane was using for that network are gone, and
it may show "Some scenery textures could not be loaded" or similar errors
onscreen and in Log.txt as new scenery loads. This is expected and harmless: click
Understood and restart X-Plane. Building or rebuilding with the same
networks ticked does not cause this.


OTHER SCENERY THAT REPLACES ROAD FILES
--------------------------------------
Some add-ons (xRoads, for example) also replace X-Plane's road files. Only
one version of each road file is used: the one from the pack highest in
Custom Scenery/scenery_packs.ini. The plugin window lists any such packs it
finds. To use this plugin's settings, move its line above theirs in
scenery_packs.ini or remove the other pack.


AFTER AN X-PLANE UPDATE
-----------------------
If an update changes X-Plane's road files, the plugin window opens at
startup with your saved settings loaded. Press Build pack and restart.


UNINSTALLING
------------
1. Open the plugin window and press Remove pack (twice).
2. Quit X-Plane and delete  X-Plane 12/Resources/plugins/RoadTrafficTuner


TROUBLESHOOTING
---------------
The plugin writes its messages to  X-Plane 12/Log.txt  on lines starting
with "Road Traffic Tuner:". Please include those lines with any report.


TERMS
-----
Road Traffic Tuner is free, open-source software released under the MIT
License (see LICENSE.txt). It is provided "as is", without warranty of any
kind; use it at your own risk. The source code is available at
https://github.com/THAinSD/RoadTrafficTuner

The override pack is built on your own computer from your own copy of
X-Plane's road files. This download contains no Laminar Research files.

Road Traffic Tuner is an independent add-on for X-Plane. It is not made,
endorsed or supported by Laminar Research. X-Plane is a trademark of
Laminar Research.
