# Build images
docker compose -f "$COMPOSE_FILE" build
docker compose -f "$COMPOSE_FILE" build --no-cache

# Create + start containers (sender, receiver) in background
docker compose -f "$COMPOSE_FILE" up -d

# Create + start in foreground (see logs live)
docker compose -f "$COMPOSE_FILE" up

# Start already-created stopped containers
docker compose -f "$COMPOSE_FILE" start
docker compose -f "$COMPOSE_FILE" start sender
docker compose -f "$COMPOSE_FILE" start receiver

# "Launch into" a running container shell
docker compose -f "$COMPOSE_FILE" exec sender bash
docker compose -f "$COMPOSE_FILE" exec receiver bash

# Run one-off shell in a new container (does not require existing running container)
docker compose -f "$COMPOSE_FILE" run --rm sender bash
docker compose -f "$COMPOSE_FILE" run --rm receiver bash

# Check status / list containers
docker compose -f "$COMPOSE_FILE" ps

# Logs
docker compose -f "$COMPOSE_FILE" logs
docker compose -f "$COMPOSE_FILE" logs -f
docker compose -f "$COMPOSE_FILE" logs -f sender
docker compose -f "$COMPOSE_FILE" logs -f receiver

# Stop containers (keep them for later start)
docker compose -f "$COMPOSE_FILE" stop
docker compose -f "$COMPOSE_FILE" stop sender receiver

# Restart containers
docker compose -f "$COMPOSE_FILE" restart
docker compose -f "$COMPOSE_FILE" restart sender

# Remove containers + default network
docker compose -f "$COMPOSE_FILE" down

# Remove containers + images built by compose + anonymous volumes
docker compose -f "$COMPOSE_FILE" down --rmi local --volumes --remove-orphans

# Force recreate containers
docker compose -f "$COMPOSE_FILE" up -d --force-recreate