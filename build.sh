#!/bin/bash

# Set the path to the docker-compose file
COMPOSE_FILE="containers/pktreplay-tester/docker-compose.yml"

docker compose -f "$COMPOSE_FILE" build