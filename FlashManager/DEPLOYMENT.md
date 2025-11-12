# FlashManager Deployment Guide

Complete deployment instructions for FlashManager across different environments.

## Table of Contents

1. [Deployment Options](#deployment-options)
2. [Prerequisites](#prerequisites)
3. [Environment Configuration](#environment-configuration)
4. [Docker Compose Deployment](#docker-compose-deployment)
5. [Coder Workspace Deployment](#coder-workspace-deployment)
6. [Manual Deployment](#manual-deployment)
7. [Production Considerations](#production-considerations)
8. [Monitoring & Maintenance](#monitoring--maintenance)
9. [Backup & Recovery](#backup--recovery)
10. [Troubleshooting](#troubleshooting)

## Deployment Options

FlashManager supports three deployment models:

| Model | Use Case | Complexity | Scalability |
|-------|----------|------------|-------------|
| **Docker Compose** | Single server, development | Low | Limited |
| **Coder Workspace** | Cloud IDE environment | Medium | Medium |
| **Manual** | Custom infrastructure | High | High |

## Prerequisites

### All Deployments

- **Hardware**: 2+ CPU cores, 4GB+ RAM, 20GB+ storage
- **Operating System**: Linux (Ubuntu 20.04+ recommended), macOS, Windows WSL2
- **Network**: Outbound HTTPS access for builds and dependencies

### Docker Compose

- Docker 20.10+
- Docker Compose 2.0+
- 2GB+ available RAM for containers

### Coder Workspace

- Kubernetes cluster 1.19+
- Coder 1.0+ installed
- Container registry access
- Persistent volume provisioner

### Manual Deployment

- Java 17+ (OpenJDK or Oracle)
- Node.js 18+
- PostgreSQL 15+
- Gradle 8.8 (included via wrapper)
- Build toolchains (ESP-IDF, Android SDK, etc.)

## Environment Configuration

### Required Environment Variables

Create a `.env` file with the following variables:

```bash
# Database Configuration
DATABASE_URL=jdbc:postgresql://localhost:5433/flashmanager
DATABASE_USERNAME=flashmanager
DATABASE_PASSWORD=flashmanager123

# JWT Configuration (GENERATE NEW SECRET!)
JWT_SECRET=your-random-256-bit-base64-encoded-secret
JWT_EXPIRATION=86400000          # 24 hours
JWT_REFRESH_EXPIRATION=2592000000 # 30 days

# Server Configuration
PORT=8081
ALLOWED_ORIGINS=http://localhost:5174

# Build Configuration
BUILD_ARTIFACTS_DIR=/workspace/STAR/FlashManager/artifacts
ESP32_REPO_PATH=/workspace/STAR/ESP32
PI5_REPO_PATH=/workspace/STAR/Pi5
ANDROID_REPO_PATH=/workspace/STAR/Android
BACKENDS_REPO_PATH=/workspace/STAR/Backends
```

### Generate JWT Secret

```bash
# Linux/macOS
openssl rand -base64 32

# Or use Node.js
node -e "console.log(require('crypto').randomBytes(32).toString('base64'))"
```

**IMPORTANT**: Use a unique secret for each environment. Never commit secrets to version control.

## Docker Compose Deployment

### 1. Prepare Docker Compose File

Create `docker-compose.yml` in project root:

```yaml
version: '3.8'

services:
  postgres:
    image: postgres:15-alpine
    container_name: flashmanager-postgres
    environment:
      POSTGRES_DB: flashmanager
      POSTGRES_USER: flashmanager
      POSTGRES_PASSWORD: flashmanager123
    volumes:
      - postgres-data:/var/lib/postgresql/data
    ports:
      - "5433:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U flashmanager"]
      interval: 10s
      timeout: 5s
      retries: 5
    networks:
      - flashmanager-network

  backend:
    build:
      context: ./backend
      dockerfile: Dockerfile
    container_name: flashmanager-backend
    environment:
      DATABASE_URL: jdbc:postgresql://postgres:5432/flashmanager
      DATABASE_USERNAME: flashmanager
      DATABASE_PASSWORD: flashmanager123
      JWT_SECRET: ${JWT_SECRET}
      PORT: 8081
      BUILD_ARTIFACTS_DIR: /app/artifacts
    volumes:
      - build-artifacts:/app/artifacts
      - ./backend/src:/app/src  # For development
    ports:
      - "8081:8081"
    depends_on:
      postgres:
        condition: service_healthy
    networks:
      - flashmanager-network
    restart: unless-stopped

  frontend:
    build:
      context: ./frontend
      dockerfile: Dockerfile
    container_name: flashmanager-frontend
    environment:
      VITE_GRAPHQL_URL: http://localhost:8081/api/v1/graphql
      VITE_GRAPHQL_WS_URL: ws://localhost:8081/api/v1/graphql
    ports:
      - "5174:80"
    depends_on:
      - backend
    networks:
      - flashmanager-network
    restart: unless-stopped

volumes:
  postgres-data:
    driver: local
  build-artifacts:
    driver: local

networks:
  flashmanager-network:
    driver: bridge
```

### 2. Create Backend Dockerfile

Create `backend/Dockerfile`:

```dockerfile
FROM gradle:8.8-jdk17 AS build

WORKDIR /app

# Copy Gradle files
COPY build.gradle.kts settings.gradle.kts gradlew ./
COPY gradle ./gradle

# Download dependencies
RUN ./gradlew dependencies --no-daemon

# Copy source code
COPY src ./src

# Build application
RUN ./gradlew bootJar --no-daemon

# Production image
FROM eclipse-temurin:17-jre-alpine

WORKDIR /app

# Copy JAR from build stage
COPY --from=build /app/build/libs/*.jar app.jar

# Create artifacts directory
RUN mkdir -p /app/artifacts

# Expose port
EXPOSE 8081

# Run application
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### 3. Create Frontend Dockerfile

Create `frontend/Dockerfile`:

```dockerfile
FROM node:18-alpine AS build

WORKDIR /app

# Copy package files
COPY package*.json ./

# Install dependencies
RUN npm ci

# Copy source code
COPY . .

# Build application
RUN npm run build

# Production image with Nginx
FROM nginx:1.25-alpine

# Copy built files
COPY --from=build /app/dist /usr/share/nginx/html

# Copy Nginx configuration
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
```

### 4. Create Nginx Configuration

Create `frontend/nginx.conf`:

```nginx
server {
    listen 80;
    server_name localhost;
    root /usr/share/nginx/html;
    index index.html;

    # SPA routing
    location / {
        try_files $uri $uri/ /index.html;
    }

    # Proxy GraphQL requests to backend
    location /api {
        proxy_pass http://backend:8081;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }

    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;

    # Gzip compression
    gzip on;
    gzip_vary on;
    gzip_types text/plain text/css application/json application/javascript text/xml application/xml application/xml+rss text/javascript;
}
```

### 5. Deploy with Docker Compose

```bash
# Set JWT secret
export JWT_SECRET=$(openssl rand -base64 32)

# Start all services
docker-compose up -d

# View logs
docker-compose logs -f

# Check status
docker-compose ps

# Access application
# Frontend: http://localhost:5174
# Backend: http://localhost:8081/api/v1
# GraphQL: http://localhost:8081/api/v1/graphql
```

### 6. Verify Deployment

```bash
# Check backend health
curl http://localhost:8081/api/v1/health

# Check database connection
docker-compose exec postgres psql -U flashmanager -d flashmanager -c '\dt'

# Check Flyway migrations
docker-compose exec backend curl http://localhost:8081/actuator/flyway
```

## Coder Workspace Deployment

### 1. Build Custom Docker Image

```bash
cd .coder
./build.sh

# Tag for registry
docker tag flashmanager-dev:latest your-registry.com/flashmanager-dev:latest

# Push to registry
docker push your-registry.com/flashmanager-dev:latest
```

### 2. Update Coder Template

Edit `.coder/coder.yaml`:

```yaml
version: "0.2"
name: "flashmanager-dev"
description: "FlashManager development workspace"

workspace:
  image: your-registry.com/flashmanager-dev:latest

  resources:
    requests:
      cpu: "2"
      memory: "4Gi"
    limits:
      cpu: "4"
      memory: "8Gi"

  volumes:
    - name: workspace
      mountPath: /workspace
      persistentVolumeClaim:
        claimName: workspace-pvc
        storageClassName: fast-ssd
        size: 50Gi

    - name: postgres-data
      mountPath: /var/lib/postgresql/data
      persistentVolumeClaim:
        claimName: postgres-pvc
        storageClassName: fast-ssd
        size: 10Gi

  env:
    - name: DATABASE_URL
      value: "jdbc:postgresql://localhost:5433/flashmanager"
    - name: JWT_SECRET
      valueFrom:
        secretKeyRef:
          name: flashmanager-secrets
          key: jwt-secret

  services:
    - name: backend
      port: 8081
      protocol: http
      public: true

    - name: frontend
      port: 5174
      protocol: http
      public: true

    - name: postgres
      port: 5433
      protocol: tcp
      public: false

  scripts:
    - name: startup
      path: /workspace/STAR/FlashManager/.coder/startup.sh
      runOnStart: true
```

### 3. Create Kubernetes Secrets

```bash
# Generate JWT secret
JWT_SECRET=$(openssl rand -base64 32)

# Create secret in Kubernetes
kubectl create secret generic flashmanager-secrets \
  --from-literal=jwt-secret="$JWT_SECRET" \
  -n coder-workspaces
```

### 4. Create Workspace from Template

1. Log into Coder dashboard
2. Click "Create Workspace"
3. Select "flashmanager-dev" template
4. Choose workspace name
5. Click "Create"
6. Wait for initialization (startup script runs automatically)

### 5. Access Workspace

```bash
# Via Coder CLI
coder ssh flashmanager-workspace

# Or use VS Code with Coder extension
# Or access via browser at workspace URL
```

### 6. Verify Services

```bash
# Inside workspace
cd /workspace/STAR/FlashManager

# Check PostgreSQL
docker ps | grep postgres

# Start backend
cd backend
./gradlew bootRun

# In new terminal, start frontend
cd frontend
npm run dev

# Access services via Coder port forwarding
```

## Manual Deployment

### 1. Install Prerequisites

**Ubuntu/Debian:**
```bash
# Java 17
sudo apt update
sudo apt install openjdk-17-jdk

# Node.js 18
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs

# PostgreSQL 15
sudo sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt update
sudo apt install postgresql-15 postgresql-client-15
```

**macOS:**
```bash
# Using Homebrew
brew install openjdk@17 node@18 postgresql@15

# Link Java
sudo ln -sfn $(brew --prefix)/opt/openjdk@17/libexec/openjdk.jdk \
  /Library/Java/JavaVirtualMachines/openjdk-17.jdk
```

### 2. Setup PostgreSQL

```bash
# Start PostgreSQL
sudo systemctl start postgresql  # Linux
brew services start postgresql@15  # macOS

# Create database and user
sudo -u postgres psql << EOF
CREATE DATABASE flashmanager;
CREATE USER flashmanager WITH PASSWORD 'flashmanager123';
GRANT ALL PRIVILEGES ON DATABASE flashmanager TO flashmanager;
\c flashmanager
GRANT ALL ON SCHEMA public TO flashmanager;
EOF
```

### 3. Deploy Backend

```bash
cd backend

# Create .env file
cat > .env << EOF
DATABASE_URL=jdbc:postgresql://localhost:5432/flashmanager
DATABASE_USERNAME=flashmanager
DATABASE_PASSWORD=flashmanager123
JWT_SECRET=$(openssl rand -base64 32)
PORT=8081
BUILD_ARTIFACTS_DIR=/var/lib/flashmanager/artifacts
EOF

# Build application
./gradlew clean build

# Run migrations (automatic on startup)
# Start application
./gradlew bootRun

# Or run as systemd service (see below)
```

### 4. Deploy Frontend

```bash
cd frontend

# Install dependencies
npm install

# Build for production
npm run build

# Serve with Nginx or Node.js server
# Option 1: Using serve
npm install -g serve
serve -s dist -l 5174

# Option 2: Using Nginx (see Nginx configuration above)
sudo cp -r dist/* /var/www/flashmanager/
```

### 5. Setup Systemd Services

**Backend Service** (`/etc/systemd/system/flashmanager-backend.service`):

```ini
[Unit]
Description=FlashManager Backend
After=postgresql.service
Requires=postgresql.service

[Service]
Type=simple
User=flashmanager
Group=flashmanager
WorkingDirectory=/opt/flashmanager/backend
EnvironmentFile=/opt/flashmanager/backend/.env
ExecStart=/usr/bin/java -jar /opt/flashmanager/backend/build/libs/flashmanager-backend.jar
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**Frontend Service** (using serve):

```ini
[Unit]
Description=FlashManager Frontend
After=network.target

[Service]
Type=simple
User=flashmanager
Group=flashmanager
WorkingDirectory=/opt/flashmanager/frontend
ExecStart=/usr/bin/npm run preview
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start services:

```bash
sudo systemctl daemon-reload
sudo systemctl enable flashmanager-backend flashmanager-frontend
sudo systemctl start flashmanager-backend flashmanager-frontend
sudo systemctl status flashmanager-backend flashmanager-frontend
```

## Production Considerations

### Security

1. **HTTPS/TLS**
   - Use reverse proxy (Nginx, Traefik, Caddy)
   - Obtain SSL certificates (Let's Encrypt)
   - Redirect HTTP to HTTPS

2. **Firewall**
   - Close unnecessary ports
   - Allow only 80/443 for web traffic
   - Restrict database to localhost

3. **Environment Variables**
   - Use secrets management (Vault, AWS Secrets Manager)
   - Never commit secrets to version control
   - Rotate JWT secrets regularly

4. **Database**
   - Use strong passwords
   - Enable SSL connections
   - Regular backups
   - Restrict network access

### Performance

1. **Database Optimization**
   - Connection pooling (HikariCP configured)
   - Indexes on critical columns
   - Regular VACUUM and ANALYZE

2. **Backend**
   - JVM tuning: `-Xmx2G -Xms1G`
   - Enable compression
   - Use production profile

3. **Frontend**
   - Enable Nginx gzip compression
   - Configure CDN for static assets
   - Enable browser caching

### Scalability

1. **Horizontal Scaling**
   - Backend: Stateless, can run multiple instances
   - Frontend: Serve from CDN
   - Database: Use read replicas

2. **Load Balancing**
   - Nginx upstream for multiple backend instances
   - Sticky sessions for WebSocket subscriptions

3. **Caching**
   - Redis for session storage (future)
   - GraphQL query result caching

### High Availability

1. **Database**
   - PostgreSQL replication (streaming)
   - Automated failover (Patroni, repmgr)

2. **Application**
   - Multiple backend instances
   - Health checks and auto-restart

3. **Backup**
   - Automated database backups
   - Off-site backup storage
   - Tested restore procedures

## Monitoring & Maintenance

### Application Monitoring

**Spring Boot Actuator** (enabled in production):

```yaml
# application-prod.yml
management:
  endpoints:
    web:
      exposure:
        include: health,metrics,info,prometheus
  metrics:
    export:
      prometheus:
        enabled: true
```

Access metrics:
- Health: `http://localhost:8081/actuator/health`
- Metrics: `http://localhost:8081/actuator/metrics`
- Prometheus: `http://localhost:8081/actuator/prometheus`

### Log Management

**Configure logging in** `application-prod.yml`:

```yaml
logging:
  level:
    root: INFO
    com.star.flashmanager: INFO
  file:
    name: /var/log/flashmanager/application.log
  logback:
    rollingpolicy:
      max-file-size: 10MB
      max-history: 30
```

**Centralized logging** (optional):
- Logstash for log aggregation
- Elasticsearch for log storage
- Kibana for log visualization

### Database Maintenance

```bash
# Regular maintenance script
#!/bin/bash

# Vacuum database
psql -U flashmanager -d flashmanager -c "VACUUM ANALYZE;"

# Check database size
psql -U flashmanager -d flashmanager -c "SELECT pg_size_pretty(pg_database_size('flashmanager'));"

# Check table sizes
psql -U flashmanager -d flashmanager -c "
  SELECT schemaname, tablename,
         pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename))
  FROM pg_tables
  WHERE schemaname = 'public'
  ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;"
```

## Backup & Recovery

### Automated Backup Script

```bash
#!/bin/bash
# /usr/local/bin/backup-flashmanager.sh

BACKUP_DIR="/var/backups/flashmanager"
DATE=$(date +%Y%m%d_%H%M%S)
RETENTION_DAYS=30

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Backup database
pg_dump -U flashmanager flashmanager | gzip > "$BACKUP_DIR/db_$DATE.sql.gz"

# Backup build artifacts
tar -czf "$BACKUP_DIR/artifacts_$DATE.tar.gz" /var/lib/flashmanager/artifacts

# Backup configuration
tar -czf "$BACKUP_DIR/config_$DATE.tar.gz" /opt/flashmanager/backend/.env

# Remove old backups
find "$BACKUP_DIR" -type f -mtime +$RETENTION_DAYS -delete

echo "Backup completed: $DATE"
```

Schedule with cron:

```bash
# Edit crontab
sudo crontab -e

# Add daily backup at 2 AM
0 2 * * * /usr/local/bin/backup-flashmanager.sh >> /var/log/flashmanager/backup.log 2>&1
```

### Restore Procedure

```bash
# Stop services
sudo systemctl stop flashmanager-backend flashmanager-frontend

# Restore database
gunzip -c /var/backups/flashmanager/db_20240101_020000.sql.gz | \
  psql -U flashmanager flashmanager

# Restore artifacts
tar -xzf /var/backups/flashmanager/artifacts_20240101_020000.tar.gz -C /

# Restart services
sudo systemctl start flashmanager-backend flashmanager-frontend
```

## Troubleshooting

### Backend Issues

**Backend won't start:**

```bash
# Check logs
journalctl -u flashmanager-backend -n 100 -f

# Check database connection
psql -U flashmanager -d flashmanager -c "SELECT 1;"

# Check Java version
java -version  # Should be 17+

# Check port availability
sudo lsof -i :8081
```

**Database migration errors:**

```bash
# Check Flyway schema history
psql -U flashmanager -d flashmanager -c "SELECT * FROM flyway_schema_history ORDER BY installed_rank;"

# Manual repair (if needed)
./gradlew flywayRepair
./gradlew flywayMigrate
```

### Frontend Issues

**Frontend won't build:**

```bash
# Clear npm cache
npm cache clean --force

# Remove node_modules and reinstall
rm -rf node_modules package-lock.json
npm install

# Check Node version
node -v  # Should be 18+
```

**GraphQL connection errors:**

```bash
# Check backend is running
curl http://localhost:8081/api/v1/graphql

# Check CORS settings in backend application.yml
# Verify frontend .env has correct URLs
```

### Agent Issues

**Agent authentication failed:**

```bash
# Verify API key is correct
# Check device hasn't been deleted in UI

# Enable debug logging
RUST_LOG=debug ./flashmanager-agent --api-key YOUR_KEY

# Check server URL is correct
./flashmanager-agent --server-url http://localhost:8081 --api-key YOUR_KEY
```

**Flash operations fail:**

```bash
# ESP32: Check esptool installation
pip3 list | grep esptool
esptool.py version

# Pi5: Check device permissions
ls -l /dev/sdb
sudo chown $USER /dev/sdb

# Android: Check adb connection
adb devices
adb version
```

### Performance Issues

**Slow builds:**

```bash
# Check CPU and memory usage
top
htop

# Check disk I/O
iostat -x 1

# Check build executor logs
grep "Build duration" /var/log/flashmanager/application.log
```

**High database load:**

```bash
# Check active connections
psql -U flashmanager -d flashmanager -c "SELECT count(*) FROM pg_stat_activity;"

# Check slow queries
psql -U flashmanager -d flashmanager -c "
  SELECT query, mean_exec_time, calls
  FROM pg_stat_statements
  ORDER BY mean_exec_time DESC
  LIMIT 10;"
```

### Network Issues

**Agent can't reach server:**

```bash
# Check network connectivity
ping your-server.com
curl -v http://your-server.com:8081/api/v1/health

# Check firewall rules
sudo iptables -L -n

# Check proxy settings
echo $HTTP_PROXY
echo $HTTPS_PROXY
```

## Support & Additional Resources

- **Backend**: See `backend/README.md`
- **Frontend**: See `frontend/README.md`
- **Agent**: See `agent/README.md`
- **Coder**: See `.coder/README.md`
- **Architecture**: See `ARCHITECTURE.md`

For issues or questions, consult the main README.md or project documentation.
