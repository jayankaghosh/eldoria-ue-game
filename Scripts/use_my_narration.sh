#!/bin/zsh
# Puts recorded story narration into the game: converts Story/MyRecordings/Intro_01..09 (m4a, wav, mp3
# or aiff) to 48 kHz mono WAV, trims and levels them, and imports them over the narration assets.
# Close the game first (the import runs a second Unreal process).
# usage: Scripts/use_my_narration.sh [folder]
set -e
cd "$(dirname "$0")/.."
IN="${1:-Story/MyRecordings}"
OUT=Audio/Recorded/Narration
if pgrep -f "UnrealEditor.*-game" >/dev/null; then echo "Close the game first."; exit 1; fi
mkdir -p "$OUT"
found=0
for n in 01 02 03 04 05 06 07 08 09; do
  src=$(find "$IN" -maxdepth 1 -iname "Intro_$n.*" | head -1)
  if [[ -z "$src" ]]; then echo "Intro_$n: not recorded yet, keeping the current one"; continue; fi
  afconvert -f WAVE -d LEI16@48000 -c 1 "$src" "$OUT/Intro_$n.wav"
  python3 Scripts/level_audio.py "$OUT/Intro_$n.wav"
  found=$((found+1))
done
[[ $found -eq 0 ]] && { echo "No recordings found in $IN"; exit 1; }
"/Users/joy/Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Eldoria.uproject" -run=pythonscript \
  -script="$PWD/Scripts/import_narration.py" -unattended -nosplash -nopause -stdout 2>&1 | grep "ELDORIA_INFO" | sed 's/.*ELDORIA_INFO //'
