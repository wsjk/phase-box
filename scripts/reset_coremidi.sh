#!/usr/bin/env bash
# Rebuilds CoreMIDI's per-user state on macOS without a reboot.
#
# What it does:
#   1. quits Audio MIDI Setup and the Phase Box simulator (they hold CoreMIDI connections)
#   2. MOVES (never deletes) these into a timestamped backup folder:
#        ~/Library/Preferences/com.apple.audio.AudioMIDISetup.plist  (+ any ByHost copy)
#        ~/Library/Audio/MIDI Configurations/                       (saved MIDI Studio layouts)
#   3. flushes cached preferences (cfprefsd) and restarts MIDIServer and coreaudiod
#   4. tells you how to check that the device tree came back, and how to undo everything
#
# It does not touch MIDI drivers (it only lists them) or any audio device settings.
#
#   bash scripts/reset_coremidi.sh --dry-run    show what would happen, change nothing
#   bash scripts/reset_coremidi.sh              show the plan, ask, then do it
#   bash scripts/reset_coremidi.sh -y           do it without asking
#   bash scripts/reset_coremidi.sh --no-sudo    skip the coreaudiod restart (the only step that needs your password)
set -u -o pipefail

DRY=0; YES=0; SUDO=1
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY=1 ;;
    -y|--yes) YES=1 ;;
    --no-sudo) SUDO=0 ;;
    -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $arg (try --help)"; exit 2 ;;
  esac
done

if [ "$(uname)" != "Darwin" ]; then echo "This script is for macOS (CoreMIDI)."; exit 1; fi

STAMP="$(date +%Y%m%d_%H%M%S)"
BACKUP="$HOME/phase_box_midi_reset_$STAMP"
PREFS="$HOME/Library/Preferences/com.apple.audio.AudioMIDISetup.plist"
CONFIGS="$HOME/Library/Audio/MIDI Configurations"

shopt -s nullglob
BYHOST=("$HOME"/Library/Preferences/ByHost/com.apple.audio.AudioMIDISetup.*.plist)
shopt -u nullglob

say()  { printf '%s\n' "$*"; }
run()  { printf '+ %s\n' "$*"; if [ "$DRY" = 0 ]; then "$@"; fi; }
try()  { printf '+ %s\n' "$*"; if [ "$DRY" = 0 ]; then "$@" >/dev/null 2>&1 || say "  (that did not succeed; carrying on)"; fi; }

say "CoreMIDI reset plan"
say "  backup folder : $BACKUP"
[ -e "$PREFS" ]   && say "  will move     : $PREFS"
for f in "${BYHOST[@]}"; do say "  will move     : $f"; done
[ -d "$CONFIGS" ] && say "  will move     : $CONFIGS"
if [ ! -e "$PREFS" ] && [ ! -d "$CONFIGS" ] && [ "${#BYHOST[@]}" = 0 ]; then
  say "  (no Audio MIDI Setup preferences or saved configurations found; only the restarts will run)"
fi
say "  will restart : cfprefsd, MIDIServer$([ "$SUDO" = 1 ] && echo ', coreaudiod (asks for your password)')"
say ""
say "Please quit any other MIDI apps first (DAWs, MIDI Monitor, Max, VCV Rack...)."
say ""

if [ "$DRY" = 0 ] && [ "$YES" = 0 ]; then
  read -r -p "Continue? [y/N] " answer
  case "$answer" in y|Y|yes|YES) ;; *) say "Cancelled. Nothing was changed."; exit 0 ;; esac
fi
[ "$DRY" = 1 ] && say "(dry run: showing commands only)"

say ""; say "1/4 Quitting apps that hold CoreMIDI connections"
try osascript -e 'tell application "Audio MIDI Setup" to quit'
try pkill -x "Audio MIDI Setup"
try pkill -x phase_box_native_sim

say ""; say "2/4 Moving preferences and saved configurations to the backup folder"
run mkdir -p "$BACKUP"
[ -e "$PREFS" ]   && run mv "$PREFS" "$BACKUP/"
for f in "${BYHOST[@]}"; do run mv "$f" "$BACKUP/"; done
[ -d "$CONFIGS" ] && run mv "$CONFIGS" "$BACKUP/"

say ""; say "3/4 Restarting the services"
try killall cfprefsd
if launchctl print "gui/$(id -u)/com.apple.midiserver" >/dev/null 2>&1; then
  try launchctl kickstart -k "gui/$(id -u)/com.apple.midiserver"
else
  try killall MIDIServer
fi
if [ "$SUDO" = 1 ]; then try sudo killall coreaudiod; else say "  (skipping coreaudiod: --no-sudo)"; fi
[ "$DRY" = 0 ] && sleep 3

say ""; say "4/4 Check the result"
say "MIDI drivers installed (a broken third-party driver can also freeze MIDI Studio):"
for d in "/Library/Audio/MIDI Drivers" "$HOME/Library/Audio/MIDI Drivers"; do
  if [ -d "$d" ]; then say "  $d:"; ls -1 "$d" 2>/dev/null | sed 's/^/    /'; fi
done
say ""
say "Next steps:"
say "  a) Unplug the FM-1's USB cable and plug it back in (this forces a fresh enumeration)."
say "  b) Confirm macOS sees it on the USB bus:"
say "       system_profiler SPUSBDataType | grep -i -A8 melodee"
say "  c) Confirm CoreMIDI sees it (works even if the MIDI Studio window is blank):"
say "       ./build-native/phase_box_native_sim --list-midi"
say "  d) Open Audio MIDI Setup > Window > Show MIDI Studio: it builds a fresh layout."
say "     Phase Box sends straight to the 'Melodee FM-1' destination, so the IAC Driver is not needed."
say "  e) If it is still stuck, watch what MIDIServer complains about:"
say "       log stream --predicate 'process == \"MIDIServer\"' --info"
say ""
say "To undo: quit Audio MIDI Setup, then move the files from"
say "  $BACKUP"
say "back to ~/Library/Preferences/ (the .plist files) and ~/Library/Audio/ (the 'MIDI Configurations' folder)."
