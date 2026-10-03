#!/bin/sh
# Convert video to MP4 (H.264 + AAC) playable on any browser/TV, and place it in www/
# Usage: scripts/make-video.sh original_movie.mkv [destination_filename.mp4]
set -e
in="$1"; out="${2:-video.mp4}"
[ -n "$in" ] || { echo "Usage: $0 input [output.mp4]"; exit 1; }
cd "$(dirname "$0")/.."
# +faststart moves index data to the beginning of the file, allowing instant playback/seeking via HTTP Range
ffmpeg -y -i "$in" -c:v libx264 -preset medium -crf 23 -pix_fmt yuv420p -vf "scale='min(1280,iw)':-2" \
       -c:a aac -b:a 128k -movflags +faststart "www/$out"
echo "Created www/$out. Next, run: make disk && make run"