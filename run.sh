#!/bin/bash

# Set the path to the docker-compose file
COMPOSE_FILE="containers/pktreplay-tester/docker-compose.yml"

# Check if a container name is provided as an argument
if [ -z "$1" ]; then
  echo "Usage: $0 <container_name>"
  echo "Example: $0 sender"
  echo "Options: s|sender, r|receiver"
  exit 1
fi

CONTAINER="$1"

# Allow short aliases for convenience.
case "$CONTAINER" in
  s)
    CONTAINER="sender"
    ;;
  r)
    CONTAINER="receiver"
    ;;
esac

# Build images (if not already built)
docker compose -f "$COMPOSE_FILE" run --rm "$CONTAINER" bash