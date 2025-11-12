#!/bin/bash

# Clean start PostgreSQL for FlashManager (Coder workspace compatible)
set -e

CONTAINER_NAME="coder-bsikar-bsikar-ballsack-flashmanager-postgres"
PG_USER="flashmanager"
PG_PASSWORD="flashmanager123"
PG_DB="flashmanager"
PG_PORT="5433"

echo "🗑️  Removing existing FlashManager PostgreSQL container..."

docker rm -f ${CONTAINER_NAME} 2>/dev/null || true

echo "📦 Creating fresh container..."
docker run --name ${CONTAINER_NAME} \
    -e POSTGRES_USER=${PG_USER} \
    -e POSTGRES_PASSWORD=${PG_PASSWORD} \
    -e POSTGRES_DB=${PG_DB} \
    -p ${PG_PORT}:5432 \
    -d postgres:15

echo "✅ FlashManager PostgreSQL is running on port ${PG_PORT}"
echo "📊 Connection: jdbc:postgresql://localhost:${PG_PORT}/${PG_DB}"
