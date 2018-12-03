# nwn-misc
Miscellaneous standalone NWN tools

## NWNLTR

A tool for displaying and generating LTR files - used for the game random name generator.

 - Generate random names from .ltr files like the game does
 - Print .ltr file Markov chain tables in a human readable format
 - Build a new .ltr file from a set of names

## NWNX Server setup

Instructions on how to setup a NWNX server and a collection of useful scripts to run/maintain it:

- mod-start.sh - starts the server unless already running
- mod-stop.sh - kills the server
- mod-disable.sh - disables server auto restart
- mod-enable.sh - enables server auto restart
- mod-savechars.sh - saves servervault/ to git
- mod-status.sh - returns 1 if server is running, 0 if not
