# Introduction #

Sylverant login server can serve up downloadable quests to clients on all supported versions of PSO, much like Sega's servers did. In addition, as should be expected the ship server supports online playable quests, which are configured in pretty much the same manner. This page will describe how to set this up and get it working properly.

# Directory Layout #

The login server configuration (as described in ConfiguringSylverant) specifies where the root of the download quests tree will be based on the `<quests>` tag. This directory will contain several subdirectories for all versions and languages of PSO that you wish to serve up downloadable quests for. Any missing directories will be ignored. The subdirectory names are based on an abbreviation of the version of PSO (dc (for Dreamcast PSOv1/PSOv2), pc (for PSOPC), and gc (for Gamecube)), and the language (en (English), sp (Spanish), fr (French), de (German), jp (Japanese), ct (Chinese, Traditional), cs (Chinese, Simplified), kr (Korean)). Note that some of these pairs (such as dc-ct, dc-cs, dc-kr) don't actually exist in practice (the Dreamcast versions did not support Chinese or Korean), so should be omitted.

The ship server configuration (as described in ConfiguringSylverantShips) specifies where the root of the online quests tree will be based on the `<quests>` tag. This directory shall contain several subdirectories, much like for downloadable quests on the login server. There is one important difference (which eventually should get propagated to the login server), namely that PSOv1 and PSOv2 are separated out into their own directories. So, the valid version parts of the directory names are v1 (for Dreamcast PSOv1), v2 (for Dreamcast PSOv2), pc (for PSOPC), gc (for Gamecube), and bb (in the future for Blue Burst). The language codes are the same as for the login server's downloadable quests section.

In order for a directory to be checked, it must exist inside the root of the tree, contain a quests.xml file that will specify what quests are available, and of course it must contain quest files. The quests.xml file format is detailed below.

For downloadable quests on the login server, you must provide the quests in a schtserv-style .qst format (which is what Qedit will give you if you pick the Downloadable quest file option while saving). The ship server supports .qst and .bin/.dat format quests (so either the default online quest format from Qedit or the option for compressed .bin/.dat format.

An example of the directory layout for a machine hosting both a login server and a ship server is as follows (note that not all quests have to be provided for all pairs, even within a platform, different languages can have different options):
  * /usr/local/share/sylverant
    * offline\_quests (as specified in the sylverant\_config.xml)
      * dc-en
        * quests.xml
        * quest15.qst
        * quest16.qst
        * quest19.qst
      * dc-sp
        * quests.xml
        * quest15.qst
      * pc-en
        * quests.xml
        * quest16.qst
      * gc-en
        * quests.xml
        * quest284.qst
        * quest477.qst
    * online\_quests (as specified in the ship\_config.xml)
      * v1-en
        * quests.xml
        * q777.bin
        * q777.dat
        * quest54.qst
      * v2-en
        * quests.xml
        * battle01.qst
        * battle02.qst
        * chl01.qst
        * q777.qst
        * quest54.qst
      * pc-en
        * quests.xml
        * quest54.qst
        * quest54v1.qst
      * gc-de
        * quests.xml
        * q777.qst

All of that should be relatively easy to follow, except perhaps the quest54v1.qst in the pc-en directory. For all PSOv1 compatible quests that are supported on PSOPC, you should provide a version 1 compatible version of the quest for PSOPC. This pretty much only involves opening up the Dreamcast v1 version of the quest in Qedit and re-saving it in the PC format. This is required for allowing PSOPC players to play quests with Dreamcast v1 players.

# quests.xml File Format #

The full format of the quests.xml file (current as of 2013-10-09) is as follows:

```
<?xml version="1.0" encoding="UTF-8"?>

<!DOCTYPE quests PUBLIC "-//Sylverant//DTD Quest List 1.1//EN" 
  "http://sylverant.net/dtd/quests1.1/quests.dtd">

<quests>
    <category name="Category Name" type="Category Type">
        <description>Multi-line
category description.</description>
        <quest name="Quest Name" prefix="Quest Filename Prefix" id="Quest ID"
         format="Format" episode="Episode Number" event="Event Number"
         minpl="Minimum Player Count" maxpl="Maximum Player Count"
         v1="Enable for PSOv1" v2="Enable for PSOv2">
            <short>Multi-line
short description.</short>
            <long>Multi-line
Long quest description.</long>
        </quest>
    </category>
</quests>
```

Here's a description of each of the fields above:
  * Category Name: The name of the category that appears on the quest select menu. Only relevant for online quests. Must still be included for downloadable quests, but is effectively ignored.
  * Category Type: One of "normal", "challenge", or "battle". If this is not specified, defaults to "normal". This specifies what mode the quests are available in. Only relevant for online quests.
  * Multi-line category description: A description of the category that shows up in the quest selection screen. Can be multiple lines. There's no hard-limit on what will fit in the box, so check it to make sure it looks OK on whatever version the file is setting up.
  * Quest Name: Self-explanatory.
  * Quest Filename Prefix: The beginning of the filename that represents the file. Normally this will be the last component of the filename excluding the extension (.qst, .bin, or .dat). The exception to this rule is for quests for v1-compatible games (where this won't include the "v1" part).
  * Quest ID: Integer ID for this quest. Only relevant for online quests. This should be set to the same value across all version/language pairs. So, for the quest54.qst file in the above example, this would always be set to "54" in the v1-en, v2-en, and pc-en directories' quests.xml file.
  * Format: Either "qst" or "bindat". Only relevant for online quests. For downloadable quests, this is ignored and assumed to be "qst". For online quests, the default is "bindat". If your quest is in .qst format, set this to "qst" for the quest. If it is in the compressed bin/dat format, set it to "bindat".
  * Episode Number: What episode is the quest for. Only relevant for online quests. Valid values are "1", "2", and "4".
  * Event Number: This is only relevant for online quests. If set to any number other than "-1", this limits the quest to only be available during the specified events. This is treated as a bitfield. The default is "-1". For instance, to enable the quest to be visible during events 0, 1, and 4, set this value to "19" (calculated from ((1 << 0) | (1 << 1) | (1 << 4)) ).
  * Minimum Player Count: Only display this quest if there are at least this many players in the team. Only relevant for online quests. The default for this setting is "1".
  * Maximum Player Count: Only display this quest if there are at most this many players in the team. Only relevant for online quests. The default for this setting is "4".
  * Enable for PSOv1/Enable for PSOv2: Only relevant for downloadable quests and only in Dreamcast-related directories. If set to true, then this quest should be shown to the relevant version of the game on the Download quests menu.
  * Multi-line short description: Short description of the quest that is shown in the small window while highlighting it in the quest menu.
  * Multi-line long description: Long description of the quest that is shown after you select the quest (while waiting to confirm that is the quest you wish to play). Only relevant for online quests.

# Example quests.xml Files #

Below is an example quests.xml file corresponding to the offline\_quests/dc-en directory in the above tree example layout.

```
<?xml version="1.0" encoding="UTF-8"?>

<!DOCTYPE quests PUBLIC "-//Sylverant//DTD Quest List 1.0//EN" 
  "http://sylverant.net/dtd/quests1/quests.dtd">

<quests>
    <category name="Offline Quests">
        <description>Offline quests</description>
        <quest name="Soul of a Blacksmith" prefix="quest15" v2="true">
            <short>A blacksmith
wants to make
a weapon using
Ragol's unknown
materials.</short>
        </quest>
        <quest name="Letter from Lionel" prefix="quest16" v2="true">
            <short>A mysterious letter
was delivered to
the Hunter's Guild.</short>
        </quest>
        <quest name="The Retired Hunter" prefix="quest19" v2="true">
            <short>I will kill
10000 monsters
before I die!</short>
        </quest>
    </category>
</quests>
```

As another example, here's the corresponding file from the online\_quests/gc-de directory:

```

<?xml version="1.0" encoding="UTF-8"?>

<!DOCTYPE quests PUBLIC "-//Sylverant//DTD Quest List 1.1//EN" 
  "http://sylverant.net/dtd/quests1.1/quests.dtd">

<quests>
    <category name="Welcome to Sylverant">
        <description>Sylverant-exclusive
quests.
Made by BlueCrab</description>
        <quest name="Globale Erwärmung" prefix="q777" id="777" format="qst">
            <short>Ragol
unterliegt einer
Hitze-welle!
Finde heraus warum.</short>
            <long>Auftraggeber: Wissentschaftler
Mission:
  Ragol unterliegt einer
  ungewühnlich langen
  Hitze-Periode. Finde die
  Ursache heraus.
Belohnung: ??? Meseta</long>
        </quest>
    </category>
</quests>
```