#!/bin/sh
# Mock ssh command for integration tests

if [ -z "$MOCK_SSH_OUTPUT_FILE" ]; then
    echo "MOCK_SSH_OUTPUT_FILE env var not set" >&2
    exit 1
fi

# Record all arguments to the file specified by the env var
echo "$@" > "$MOCK_SSH_OUTPUT_FILE"

# Block forever to simulate an active connection, until killed by the test
# script.
sleep 9999
