#!/bin/sh

set -e

# The scrcpy binary is in the app/ subdirectory in the meson test env
SCRCPY_BIN=./app/scrcpy

# Provide the absolute path to the mock ssh script via an envvar
MOCK_SSH_SCRIPT=$(readlink -f "$(dirname "$0")/mock_ssh.sh")
export SCRCPY_MOCK_SSH_PATH="$MOCK_SSH_SCRIPT"

TMP_FILE=$(mktemp)
export MOCK_SSH_OUTPUT_FILE="$TMP_FILE"
trap 'rm -f "$TMP_FILE"' EXIT

# Run scrcpy in the background. It is expected to fail because there is no
# device, so ignore the error code.
set +e
"$SCRCPY_BIN" --ssh-tunnel=test@mockhost &
SCRCPY_PID=$!
set -e

# Give scrcpy time to start the ssh process
sleep 2

# Kill the scrcpy process, which should also trigger cleanup of the ssh process
# It may already be terminated, so ignore the error code.
kill "$SCRCPY_PID" 2>/dev/null || true
# Wait for the process to be fully terminated
wait "$SCRCPY_PID" 2>/dev/null || true

# Check if the mock ssh was called with the correct arguments
if [ ! -f "$TMP_FILE" ]; then
    echo "ERROR: Mock ssh was not called (output file not found)"
    exit 1
fi

EXPECTED_ARGS="-CN -L5038:localhost:5037 -R27183:localhost:27183 test@mockhost"
ACTUAL_ARGS=$(cat "$TMP_FILE")

if [ "$EXPECTED_ARGS" != "$ACTUAL_ARGS" ]; then
    echo "ERROR: Mock ssh called with wrong arguments"
    echo "  Expected: $EXPECTED_ARGS"
    echo "  Actual:   $ACTUAL_ARGS"
    exit 1
fi

echo "SUCCESS: Mock ssh called with correct arguments"

exit 0
