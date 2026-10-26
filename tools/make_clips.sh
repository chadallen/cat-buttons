#!/usr/bin/env bash
# Converts every recording in clips/ (m4a, wav, aiff, mp3, ...) into data/<name>.wav:
# 16 kHz mono 16-bit PCM, silence trimmed from both ends, loudness normalized.
# Then upload with: pio run -t uploadfs
#
#   tools/make_clips.sh          convert clips/* into data/
#   tools/make_clips.sh --say    first speak a placeholder for every pad in
#                                include/config.h that has no recording yet
#
# A recording's file name must match its pad's clip name in include/config.h,
# e.g. clips/treat.m4a for "/treat.wav".
set -euo pipefail

cd "$(dirname "$0")/.."

# Pad table lines look like:  {13, "TREAT", "/treat.wav"},   Commented-out lines are skipped.
pads() {
  sed -nE 's#^[[:space:]]*\{[0-9]+, "([^"]+)", "/([^"]+)\.wav"\}.*#\1 \2#p' include/config.h
}

if [[ "${1:-}" == "--say" ]]; then
  mkdir -p clips
  while read -r name stem; do
    if ! ls clips/"$stem".* >/dev/null 2>&1; then
      spoken="$(echo "$name" | tr '_' ' ' | tr '[:upper:]' '[:lower:]')"
      say -o "clips/$stem.aiff" "$spoken"
      echo "placeholder: clips/$stem.aiff (\"$spoken\")"
    fi
  done < <(pads)
fi

mkdir -p data
rm -f data/*.wav
shopt -s nullglob
n=0
for src in clips/*; do
  [[ -f "$src" ]] || continue
  name="$(basename "${src%.*}" | tr '[:upper:]' '[:lower:]')"
  if [[ -f "data/$name.wav" ]]; then
    echo "skipping $src: another recording already made data/$name.wav. Delete one of them." >&2
    continue
  fi
  # Trim silence below -45 dB at the start, reverse, trim again, reverse back.
  # loudnorm targets -16 LUFS with -1.5 dB true peak, so every word plays at
  # the same loudness before VOLUME scales it.
  ffmpeg -hide_banner -loglevel error -y -i "$src" \
    -af "silenceremove=start_periods=1:start_threshold=-45dB,areverse,silenceremove=start_periods=1:start_threshold=-45dB,areverse,loudnorm=I=-16:TP=-1.5:LRA=11" \
    -ar 16000 -ac 1 -c:a pcm_s16le -map_metadata -1 -fflags +bitexact \
    "data/$name.wav"
  printf '%-28s -> data/%s.wav  %6d bytes\n' "$src" "$name" "$(stat -f%z "data/$name.wav")"
  n=$((n + 1))
done

if [[ $n -eq 0 ]]; then
  echo "no recordings in clips/. Add some, or run: tools/make_clips.sh --say" >&2
  exit 1
fi

while read -r name stem; do
  if [[ ! -f "data/$stem.wav" ]]; then
    echo "WARNING: pad $name has no clip (expected clips/$stem.<any audio type>)" >&2
  fi
done < <(pads)
total=$(du -ck data/*.wav | tail -1 | cut -f1)
echo "$n clips, ${total} KB in data/ (filesystem holds about 1900 KB)"
