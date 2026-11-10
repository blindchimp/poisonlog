#!/usr/bin/env bash
set -euo pipefail

# Output directory
OUT_DIR="opencode-sessions"

# Optional --sanitize argument
SANITIZE=()
if [[ "${1:-}" == "--sanitize" ]]; then
    SANITIZE=(--sanitize)
fi

# Dependencies
command -v opencode >/dev/null || {
    echo "Error: opencode not found"
    exit 1
}
command -v jq >/dev/null || {
    echo "Error: jq is required"
    exit 1
}

mkdir -p "$OUT_DIR"

echo "Fetching OpenCode sessions..."

# Get session list as JSON
opencode session list --format json > "$OUT_DIR/session-list.json"

# Extract session IDs
mapfile -t SESSION_IDS < <(
    jq -r '.[].id' "$OUT_DIR/session-list.json"
)

if [[ ${#SESSION_IDS[@]} -eq 0 ]]; then
    echo "No sessions found."
    exit 0
fi

echo "Found ${#SESSION_IDS[@]} sessions."

COUNT=0

for ID in "${SESSION_IDS[@]}"; do
    FILE="$OUT_DIR/${ID}.json"
    TMP="${FILE}.tmp"

    echo "Exporting $ID..."

    if opencode export "${SANITIZE[@]}" "$ID" > "$TMP"; then
        mv "$TMP" "$FILE"
        ((COUNT+=1))
    else
        echo "WARNING: Failed to export $ID"
        rm -f "$TMP"
    fi
done

echo
echo "Export complete."
echo "Successfully exported: $COUNT / ${#SESSION_IDS[@]}"
echo "Output directory: $OUT_DIR"

