#!/bin/bash

# Start PostgreSQL for FlashManager (Coder workspace compatible)
set -e

CONTAINER_NAME="coder-bsikar-bsikar-ballsack-flashmanager-postgres"
PG_USER="flashmanager"
PG_PASSWORD="flashmanager123"
PG_DB="flashmanager"
PG_PORT="5433"  # Use different port from Projectum (5432)

echo "🐳 Starting Flash Manager PostgreSQL container..."

# Check if container exists
if [ "$(docker ps -aq -f name=${CONTAINER_NAME})" ]; then
    if [ "$(docker ps -q -f name=${CONTAINER_NAME})" ]; then
        echo "✅ Container already running"
    else
        echo "▶️  Starting existing container..."
        docker start ${CONTAINER_NAME}
    fi
else
    echo "📦 Creating new container..."
    docker run --name ${CONTAINER_NAME} \
        -e POSTGRES_USER=${PG_USER} \
        -e POSTGRES_PASSWORD=${PG_PASSWORD} \
        -e POSTGRES_DB=${PG_DB} \
        -p ${PG_PORT}:5432 \
        -d postgres:15
fi

echo "✅ FlashManager PostgreSQL is running on port ${PG_PORT}"
echo "📊 Connection: jdbc:postgresql://localhost:${PG_PORT}/${PG_DB}"
