#!/bin/bash

# Set the names for your server/module here
MODNAME=MyModule

status=$(./mod-status.sh)
if [ "$status" -eq "1" ]; then
    echo "$MODNAME is already running"
    exit;
fi

if [ -f /home/nwn/.mod-maintenance ]; then
    echo "$MODNAME maintenance in progress (.mod-maintenance file exists), not starting"
    exit;
fi

# Make a backup of all characters each time the server restarts
./mod-savechars.sh

pushd ~/nwn/bin/linux-x86

# Advanced plugins most people won't use, so skip them by default.
export NWNX_JVM_SKIP=y
export NWNX_MONO_SKIP=y
export NWNX_THREADWATCHDOG_SKIP=y
export NWNX_PROFILER_SKIP=y
export NWNX_METRICS_INFLUXDB_SKIP=y
export NWNX_RUBY_SKIP=y
export NWNX_REDIS_SKIP=y
export NWNX_LUA_SKIP=y
export NWNX_SPELLCHECKER_SKIP=y
export NWNX_TRACKING_SKIP=y

# Set you DB connection info here
export NWNX_SQL_TYPE=mysql
export NWNX_SQL_HOST=localhost
export NWNX_SQL_USERNAME=nwn
export NWNX_SQL_PASSWORD=pass
export NWNX_SQL_DATABASE=mymodulename
export NWNX_SQL_QUERY_METRICS=true

# Log levels go from 2 (only fatal errors) to 7 (very verbose). 6 is recommended
export NWNX_CORE_LOG_LEVEL=6

#
# Custom behavior tweaks from the NWNX_Tweaks plugin. Uncomment/modify to enable
#
# HP players need to reach to be considered dead
#export NWNX_TWEAKS_PLAYER_DYING_HP_LIMIT=-10
# Disable pausing by players and DMs
#export NWNX_TWEAKS_DISABLE_PAUSE=y
# Disable DM quicksave ability
export NWNX_TWEAKS_DISABLE_QUICKSAVE=y
# Stackable items can only be merged if all local variables are the same
#export NWNX_TWEAKS_COMPARE_VARIABLES_WHEN_MERGING=y
# Parry functions as per description, instead of blocking max 3 attacks per round
#export NWNX_TWEAKS_PARRY_ALL_ATTACKS=y
# Immunity to Critical Hits does not confer immunity to sneak attack
#export NWNX_TWEAKS_SNEAK_ATTACK_IGNORE_CRIT_IMMUNITY=y
# Items are not destroyed when they reach 0 charges
#export NWNX_TWEAKS_PRESERVE_DEPLETED_ITEMS=y
# Fix some intel crashes by disabling some shadows on areas
#export NWNX_TWEAKS_DISABLE_SHADOWS=y


# Keep all logs in this directory
LOGFILE=~/logs/mod-`date +%s`.txt
echo "Starting $MODNAME. Log is $LOGFILE"

# Set game options below

export NWNX_CORE_LOAD_PATH=~/nwnx/Binaries
LD_PRELOAD=~/nwnx/Binaries/NWNX_Core.so \
 ./nwserver-linux \
  -module '$MODNAME' \
  -maxclients 96 \
  -minlevel 1 \
  -maxlevel 20 \
  -pauseandplay 0 \
  -pvp 2 \
  -servervault 1 \
  -elc 0 \
  -ilr 0 \
  -gametype 3 \
  -oneparty 0 \
  -difficulty 3 \
  -autosaveinterval 0 \
  -dmpassword 'dmpass' \
  -servername 'myservername' \
  -publicserver 1 \
  -reloadwhenempty 0 \
  -port 5121 \
  "$@" >> $LOGFILE 2>&1 &

echo $! > ~/.modpid 
popd 
