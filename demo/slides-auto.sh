#!/bin/bash
#
# Auto-advancing slides for CES 2026 demo video
#
# Simple version - runs slides and sends keystrokes via osascript (macOS)
# Works with iTerm2, Terminal.app, or any macOS terminal
#
# Usage:
#   ./slides-auto.sh [seconds-per-slide]
#
# Default: 6 seconds per slide (10 slides = 60 seconds total loop)
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SLIDES_FILE="${SCRIPT_DIR}/slides.md"
SECONDS_PER_SLIDE="${1:-6}"
NUM_SLIDES=7

# Check for slides
if ! command -v slides &> /dev/null; then
    echo "Install slides first:"
    echo "  brew install slides"
    exit 1
fi

# Background process to send keystrokes
(
    sleep 2  # Wait for slides to start

    while true; do
        # Advance through all slides
        for ((i=1; i<NUM_SLIDES; i++)); do
            sleep "$SECONDS_PER_SLIDE"
            osascript -e 'tell application "System Events" to keystroke " "'
        done

        # Last slide - wait then go back to start
        sleep "$SECONDS_PER_SLIDE"
        osascript -e 'tell application "System Events" to keystroke "q"'
        sleep 1
    done
) &
KEYSTROKE_PID=$!

# Cleanup on exit
trap "kill $KEYSTROKE_PID 2>/dev/null; exit" INT TERM EXIT

# Run slides (this blocks)
slides "$SLIDES_FILE"
