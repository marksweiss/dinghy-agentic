---
name: hub-submission
description: Get an FM-1 firmware listed on the FM-1 Firmware Hub (thefirmwarehub.com) - identity, the two required tests, the README notes, the email. Use when the user wants their firmware listed.
---
# The FM-1 Firmware Hub

Requirements (https://thefirmwarehub.com/submit/): open licence with full source (or a link card); a GitHub release with
the .fwsc; an `FM-1_` identity no other firmware uses (check https://thefirmwarehub.com/catalog.json, the `product` fields);
the update loader in the package (Dinghy builds have Felucca's); tested on a real FM-1.

Tests before the email (a device test plan section):
1. Install from the official firmware (put V15 / V16 on through the hub, then install yours).
2. Leave again: from yours, install another firmware (the hub's V15); it must start and finish.
3. `tools/install_fw.sh --info` reports your identity.
README notes they ask for: that your firmware does not read other firmwares' projects or samples; that switching
firmware erases your patches (give the backup in the web tool).

The email (to zven@thefirmwarehub.com; links, no attachments): name of the firmware; your name as shown; repository;
identity; licence; two or three sentences; a picture link (a 240 x 240 screen, e.g. a raw.githubusercontent.com PNG);
optional browser tool / video; "Tested on a real FM-1, leaving it again works: yes". Draft it; the user sends it.
After listing: the hub picks up each new GitHub release every few hours.
