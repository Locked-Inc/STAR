# STAR infra — Pi5-side system files

Non-ROS infra I manage on the Pi5 itself (outside the ROS workspace). These
are mirrored here so they live in version control, but the authoritative
copies are the ones deployed at the paths below.

## Layout

```
infra/pi5/
├── opt/
│   └── star_cockpit_api/
│       └── server.py              → /opt/star_cockpit_api/server.py
└── etc/
    ├── caddy/
    │   └── Caddyfile              → /etc/caddy/Caddyfile
    └── systemd/system/
        ├── caddy.service          → /etc/systemd/system/caddy.service
        ├── star-cockpit-api.service → /etc/systemd/system/star-cockpit-api.service
        └── star-slam-mvp.service  → /etc/systemd/system/star-slam-mvp.service
```

## What each piece does

### `star_cockpit_api` — HTTP API backing the Grafana cockpit controls

A standalone Python rclpy node + HTTP server on `100.64.0.6:9102`.

Endpoints:

| Method | Path                       | Purpose                                               |
|--------|----------------------------|-------------------------------------------------------|
| GET    | `/api/state`               | `{autonomy, estop, speed, slam}` — all cached         |
| POST   | `/api/autonomy/toggle`     | flip `/star/autonomy_enable`                          |
| POST   | `/api/estop/toggle`        | flip `/star/estop`                                    |
| POST   | `/api/speed`               | body `{"value":0.75}` → publish `/star/speed_scale`   |
| POST   | `/api/cmd_vel`             | body `{"linear":0.5,"angular":0}` → publish `/cmd_vel` |
| POST   | `/api/slam/start`          | configure+activate `/slam_toolbox`                    |
| POST   | `/api/slam/stop`           | deactivate `/slam_toolbox`                            |

Subscribes to the latched `/star/state/*` topics so the toggle logic always
sees the supervisor's current view. A background thread refreshes the slam
lifecycle state every 4s (via `ros2 service call`) so `/api/state` is O(1).

### Systemd units

- **`caddy.service`** — Caddy (on Pi5) listens on `100.64.0.6:443`, terminates
  TLS with its own local CA (`tls internal`), fronts `star_simple_bridge` on
  `:9101` and `star_cockpit_api` on `:9102`. `After=tailscaled.service` so it
  waits for the tailscale IP to be assigned. `Restart=always`.

- **`star-cockpit-api.service`** — runs `server.py` as the `star` user, with
  ROS workspace sourced and Cyclone DDS config exported. Restart on failure.

- **`star-slam-mvp.service`** — runs `scripts/slam-mvp.sh start` at boot
  (after tailscale is up + an 8s USB-enumeration delay). Initial state is
  MANUAL mode, e-stop cleared — so the robot is safe on boot, operator
  flips to AUTONOMY via the Grafana cockpit toggle.

### `Caddyfile`

Pi5-side Caddy config. Uses `tls internal` — no ACME, no Cloudflare, no
external CA. Laptop/phone clients install Caddy's local root CA once (see
`docs/ARCHITECTURE.md` → "Client setup to use `star.local`"). No secrets.

## Installing on a fresh Pi5

```bash
# 1. Copy files (from repo root)
sudo cp infra/pi5/opt/star_cockpit_api/server.py /opt/star_cockpit_api/
sudo chmod +x /opt/star_cockpit_api/server.py

sudo cp infra/pi5/etc/caddy/Caddyfile /etc/caddy/Caddyfile
sudo cp infra/pi5/etc/systemd/system/*.service /etc/systemd/system/

# 2. Install Caddy binary with no plugins needed (tls internal only)
sudo curl -fsSL -o /usr/local/bin/caddy \
  "https://caddyserver.com/api/download?os=linux&arch=arm64"
sudo chmod +x /usr/local/bin/caddy
sudo useradd --system --shell /usr/sbin/nologin --home-dir /var/lib/caddy caddy 2>/dev/null || true
sudo mkdir -p /var/log/caddy /var/lib/caddy
sudo chown caddy:caddy /var/log/caddy /var/lib/caddy

# 3. Enable + start
sudo systemctl daemon-reload
sudo systemctl enable --now caddy star-cockpit-api star-slam-mvp
```

## What is deliberately NOT mirrored here

- **Cluster-side k8s manifests** (traefik, robot-gateway, lichtblick, grafana
  dashboard JSON). Those live in k3s's etcd; Grafana dashboard is pushed via
  API. They can be exported separately if needed.
- **Any hostname or credentials** — these system files use only `star.local`
  and `100.64.0.6` (tailscale IP), nothing that identifies the operator.
- **The Caddy-gateway Caddyfile block on pve** that defines
  `<PUBLIC_ROBOT_HOST>` — that contains the public hostname and a bcrypt
  password hash, and is managed on the homelab host, not on the Pi5.
