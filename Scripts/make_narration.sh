#!/bin/zsh
# Records the new-game story narration with macOS text-to-speech, one clip per line of
# Story/intro_narration.txt, into Audio/Recorded/Narration (then run Scripts/import_sounds.py).
# usage: Scripts/make_narration.sh [voice]   e.g. "Daniel (Enhanced)" once installed in
# System Settings > Accessibility > Spoken Content > System Voice > Manage Voices.
set -e
cd "$(dirname "$0")/.."
VOICE="${1:-Daniel}"
OUT=Audio/Recorded/Narration
mkdir -p "$OUT"
find "$OUT" -name "Intro_*.wav" -delete
i=0
while IFS= read -r line; do
  [[ -z "$line" ]] && continue
  i=$((i+1))
  n=$(printf "%02d" $i)
  say -v "$VOICE" -r 148 -o "/tmp/eldoria_intro_$n.aiff" "$line"
  afconvert -f WAVE -d LEI16@48000 -c 1 "/tmp/eldoria_intro_$n.aiff" "$OUT/Intro_$n.wav"
  rm -f "/tmp/eldoria_intro_$n.aiff"
  echo "Intro_$n: $(afinfo "$OUT/Intro_$n.wav" | grep -E 'estimated duration' | awk '{print $3}')s"
done < Story/intro_narration.txt
