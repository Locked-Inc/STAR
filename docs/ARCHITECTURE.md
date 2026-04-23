# STAR Robot — Full Operations Stack

End-to-end architecture for the STAR robot, from the Pi5 compute brick to the
remote browser-based cockpit. Generic hostnames/IPs are used throughout.
Substitute your own values where you see `<LIKE_THIS>` placeholders.

## Topology

```
┌────────────────────────────────────────────────────────────────────────┐
│                         USER (browser, laptop)                         │
│  on tailnet, trusts Pi5 Caddy internal CA, /etc/hosts: 100.64.0.6 →     │
│  star.local                                                            │
└──────────────┬────────────────────────────────────┬────────────────────┘
               │                                    │
       https://<cockpit>/                    https://<grafana>/
  (public Caddy → Traefik → Lichtblick)    (public Caddy → k3s → Grafana)
               │                                    │
               │ ws://100.64.0.6:8765 (tailnet)     │  iframe-like: pulls
               │ (foxglove bridge)                  │  https://star.local/map.*
               │                                    │  directly from browser
               │                                    ▼
               │                      ┌─────────────────────────────┐
               │                      │  Pi5 Caddy (:443, bind      │
               │                      │  100.64.0.6)                │
               │                      │  tls internal (Caddy CA)    │
               │                      │  star.local → localhost:9101│
               │                      └──────────┬──────────────────┘
               │                                 │
               ▼                                 ▼
    ┌──────────────────────────────────────────────────────────────┐
    │                      Pi5 (100.64.0.6)                        │
    │                                                              │
    │  :8765  foxglove_bridge      → ROS topics over WS protocol   │
    │  :9100  node_exporter        → scraped by Alloy              │
    │  :9101  star_simple_bridge   → /map.{png,pgm,yaml},          │
    │                                POST /map/reset?confirm=yes   │
    │                                + custom metrics              │
    │                                                              │
    │  Alloy agent → remote_write → http://100.64.0.1:30909/api/v1/write │
    │                                                              │
    │  ROS topics (supervisor-gated):                              │
    │    /star/autonomy_enable (Bool, latched)                     │
    │    /star/estop (Bool, latched)                               │
    │    /star/speed_scale (Float32 0-1)                           │
    │    /cmd_vel (Twist, manual input)                            │
    │    /nav2/cmd_vel (Twist, Nav2 output)                        │
    │    /goal_pose (PoseStamped, user click)                      │
    │    /cmd_vel_out (Twist, the gated output bridge consumes)    │
    └──────────────────────────────────────────────────────────────┘

    ┌──────────────────────────────────────────────────────────────┐
    │                 k3s cluster (100.64.0.1 on tailnet)          │
    │                                                              │
    │  monitoring/                                                 │
    │    prometheus       --web.enable-remote-write-receiver       │
    │                     NodePort 30909 on tailscale interface    │
    │    grafana          dashboard "STAR robot telemetry"         │
    │                     GF_PANELS_DISABLE_SANITIZE_HTML=true     │
    │                     GF_DASHBOARDS_MIN_REFRESH_INTERVAL=1s    │
    │                                                              │
    │  robot/                                                      │
    │    lichtblick       web cockpit (Caddy-served static bundle) │
    │                     ConfigMap-injected default layout        │
    │                     ingress: <COCKPIT_HOST>                  │
    │                                                              │
    │  kube-system/                                                │
    │    rollout-restarter CronJob, 0 4 * * *                      │
    │                     rolls every workload w/ keel.sh/policy    │
    │                     =force label → forces :latest re-pull    │
    └──────────────────────────────────────────────────────────────┘

    ┌──────────────────────────────────────────────────────────────┐
    │     Caddy LXC (on homelab, public entry point, LAN-only)     │
    │                                                              │
    │  Cloudflare → Caddy → backends:                              │
    │    <COCKPIT_HOST>   → 192.168.1.221:80 (k3s traefik)         │
    │    <GRAFANA_HOST>   → 192.168.1.221:80 (k3s traefik)         │
    │    ...                                                       │
    │  ACME via Cloudflare DNS-01 (public certs)                   │
    └──────────────────────────────────────────────────────────────┘
```

## The supervisor safety model

The robot hardware bridge reads `/cmd_vel_out` — **not** `/cmd_vel`. Everything
arrives at `/cmd_vel_out` only through `star_supervisor`, which arbitrates:

| Source topic        | Forwarded to `/cmd_vel_out` when                     |
|---------------------|------------------------------------------------------|
| `/cmd_vel` (manual) | `autonomy_enable = false` AND `estop = false`        |
| `/nav2/cmd_vel`     | `autonomy_enable = true`  AND `estop = false`        |
| `/goal_pose` → `/nav2/goal_pose` | `autonomy_enable = true`  AND `estop = false`  |

E-stop is **dual-enforced**: supervisor publishes zero Twist AND the hardware
bridge independently zeros outputs when `/star/estop=true` — a bug in either
layer cant override the other.

Nav2 is configured to emit on `/nav2/cmd_vel` (via `collision_monitor.cmd_vel_out_topic`
override) and receive on `/nav2/goal_pose` (via `SetRemap` in the launch file)
so that its outputs flow through the mux rather than driving the wheels directly.

## Auto-updates

| Layer              | Updater                 | Cadence     |
|--------------------|-------------------------|-------------|
| Host OS (k3s node) | `unattended-upgrades`   | daily       |
| k3s itself         | manual                  | —           |
| Container images   | `rollout-restarter`     | daily 04:00 |
|                    | CronJob restarts every  |             |
|                    | workload labeled        |             |
|                    | `keel.sh/policy=force`  |             |
|                    | → re-pulls `:latest`    |             |

keel was removed — with `:latest` + `imagePullPolicy: Always` + these labels,
a nightly rollout-restart is enough and doesnt risk mis-ordering tags.

## TLS / hostname strategy

Three zones, three different cert paths:

1. **Public hosts** (cockpit, grafana) — Caddy on LAN ingress host, ACME DNS-01
   via Cloudflare token. Cert is Lets Encrypt. Browser trusts automatically.

2. **Pi5 bridge** (`star.local`) — Caddy on Pi5, `tls internal`. Binds to the
   tailscale interface (100.64.0.6) only, so literally unreachable off-tailnet.
   Cert is issued by Caddys local CA which you install on each client laptop
   once.

3. **Lichtblick → foxglove_bridge ws://** — still plaintext ws://. Tailscale
   encrypts it end-to-end, but browsers show a "Not Secure" badge on the page
   because they cant see the tailscale tunnel. Cosmetic; ignore or add
   another Caddy block for wss:// if the badge bothers you.

## Client setup to use `star.local`

One-time on each laptop you want to use with the robot:

1. Add to `/etc/hosts` (needs sudo):
   ```
   100.64.0.6  star.local
   ```
2. Copy the Pi5 Caddy root CA to your trust store:
   - On Pi5: `sudo cat /var/lib/caddy/.local/share/caddy/pki/authorities/local/root.crt`
   - macOS: save as `caddy-ca.crt`, `sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain caddy-ca.crt`
   - Linux: `sudo cp caddy-ca.crt /usr/local/share/ca-certificates/ && sudo update-ca-certificates`
   - Firefox: Preferences → Privacy & Security → Certificates → View → Authorities → Import, tick "trust for websites"

After that, `https://star.local/map.pgm`, `https://star.local/map/reset?confirm=yes`, etc. all work with no browser warnings, and the Grafana map panels hold-to-reset / download buttons stop being blocked.

## File inventory (on the Pi5, outside this repo)

Nothing in this repo is touched by the Caddy/TLS setup. All state is at:

- `/usr/local/bin/caddy` — Caddy binary (with cloudflare DNS module, unused after switch to `tls internal`)
- `/etc/caddy/Caddyfile` — 4-line config, no secrets
- `/etc/systemd/system/caddy.service` — systemd unit
- `/var/lib/caddy/.local/share/caddy/pki/authorities/local/` — local CA + key
- `/var/log/caddy/` — logs (currently unused)

Previously the Caddyfile also held a Cloudflare API token for DNS-01 issuance;
that was deliberately removed once we switched to `tls internal`, since public
ACME is no longer needed for the Pi5 endpoint.

## Endpoint reference

From a client on the tailnet with `/etc/hosts` + CA installed:

| Purpose                   | URL                                                    |
|---------------------------|--------------------------------------------------------|
| Map image (live PNG)      | `https://star.local/map.png`                           |
| Map for Nav2 load         | `https://star.local/map.pgm` and `/map.yaml`           |
| Reset map (POST)          | `https://star.local/map/reset?confirm=yes`             |
| Bridge Prom metrics       | `http://100.64.0.6:9100/metrics` (scraped by Alloy)    |
| Foxglove ws for Lichtblick| `ws://100.64.0.6:8765`                                 |
| Lichtblick cockpit UI     | `https://<COCKPIT_HOST>/`                              |
| Grafana dashboards        | `https://<GRAFANA_HOST>/d/star-robot-telemetry`        |
| Prometheus write endpoint | `http://100.64.0.1:30909/api/v1/write` (from Pi5 Alloy)|

## Reboot recovery

Order of things that need to come back after a Pi5 reboot. Auto-started things
are marked 🟢; manual-restart ones 🔴.

| Component            | How it comes back                                              | Pass check                                         |
|----------------------|-----------------------------------------------------------------|----------------------------------------------------|
| tailscale            | 🟢 `tailscaled.service` (auto)                                  | `tailscale ip -4` returns `100.64.0.6`             |
| Caddy (TLS frontend) | 🟢 `caddy.service` (auto, `After=tailscaled.service`)           | `systemctl is-active caddy` → `active`             |
| ROS workspace        | 🔴 needs sourcing in each shell                                 | `source /workspaces/STAR/star-ros2/install/setup.bash` |
| SLAM stack           | 🟢 `star-slam-mvp.service` (auto); initial state manual+no-estop | `pgrep -f supervisor_node` returns a PID           |
| Nav2 (when wanted)   | 🔴 `bash scripts/slam-mvp.sh start-nav` on Pi5                  | `/goal_pose` topic has a subscriber                |
| Alloy (metrics push) | 🟢 `alloy.service` (was *disabled*; now enabled for boot)       | `systemctl is-active alloy` → `active`             |

### Gotchas I hit while setting Caddy up — must stay fixed

1. **Caddy binds to `100.64.0.6`**, which only exists after tailscale has
   assigned the interface. Caddy originally came up before tailscaled →
   `bind: cannot assign requested address` → service stopped and never retried.
   Fix is in `/etc/systemd/system/caddy.service`:
   ```
   [Unit]
   After=network-online.target tailscaled.service
   Wants=network-online.target tailscaled.service
   [Service]
   Restart=always
   RestartSec=5s
   ```
   If you see Caddy down after reboot, `sudo journalctl -u caddy -b` and look
   for `cannot assign requested address` — thats this race.

2. **Caddys `tls internal` tries to install its root CA into the system
   trust store on first start** — that needs `sudo` and the `caddy` user isnt
   in sudoers. It logs a scary-looking error but the cert is still served to
   clients fine. Silenced in the global block via:
   ```
   {
     local_certs
     skip_install_trust
     admin off
   }
   ```
   (The CA install is for letting the Pi5 itself trust the cert, not clients.
   We dont need it.)

3. **SLAM stack is not a systemd service.** It runs via `slam-mvp.sh start`
   which backgrounds each node but is tied to the invoking shell session
   (ctrl-C on that shell kills all of them; closing the vscode terminal does
   the same). After reboot you will always need to restart it manually. If
   you want it to auto-start, wrap `slam-mvp.sh start` in a user-level
   systemd unit, but dont do that until youre comfortable with it — SLAM
   auto-starting on a robot you didnt intend to drive is a safety hazard.

### Known benign log noise

- Caddy: `"Caddyfile input is not formatted"` — ignore or run
  `sudo /usr/local/bin/caddy fmt --overwrite /etc/caddy/Caddyfile`.
- Caddy: `"certutil is not available"` — would be needed for Firefox trust
  store on the Pi5 itself, which we dont care about.

### Post-reboot sanity commands

```
# On the Pi5:
tailscale ip -4                     # expect 100.64.0.6
systemctl is-active caddy           # expect active
systemctl is-active alloy           # expect active
curl -sS -o /dev/null -w "%{http_code}\n" https://star.local/ --resolve star.local:443:100.64.0.6 -k
#   expect 502 until SLAM stack is running, then 200/redirect

bash scripts/slam-mvp.sh start      # bring up SLAM stack
pgrep -f supervisor_node            # expect a PID
curl -sS -o /dev/null -w "%{http_code}\n" https://star.local/map.png --resolve star.local:443:100.64.0.6 -k
#   expect 200

# On a laptop (once /etc/hosts + CA trust are set up):
curl -sS -o /dev/null -w "%{http_code}\n" https://star.local/map.png
#   expect 200
```


### Alloy gotcha from a reboot

Alloys systemd unit ships with `disabled: preset enabled` — meaning if you
installed it via the deb and never ran `systemctl enable alloy`, it will
*not* start at boot, even though its the metrics agent the whole Grafana
experience depends on. Symptom: Grafana panels are empty after a reboot
except for the k3s-side metrics (which come from node_exporter scraped by
Prometheus inside the cluster).

Fix, one-time:
```
sudo systemctl enable --now alloy
```

Verify after future reboots:
```
systemctl is-enabled alloy     # expect: enabled
systemctl is-active alloy      # expect: active
curl -sS http://localhost:12345/metrics | grep -E '^prometheus_remote_write_wal_samples_appended_total'
# expect: counter going up
```

### Updated post-reboot sanity pass

```bash
# On Pi5, from a fresh terminal:
tailscale ip -4                                 # → 100.64.0.6
systemctl is-active caddy                       # → active
systemctl is-active alloy                       # → active
curl -sS --resolve star.local:443:100.64.0.6 -k -o /dev/null -w "%{http_code}\n" https://star.local/
#   expect 502 (bridge not started) or 200 (bridge up)

bash /workspaces/STAR/scripts/slam-mvp.sh start
# Note: if /dev/star-mcu is missing at boot, the script auto-reflashes the
# RX72N to recover it. Wait for "recovered" then it continues. Takes ~30-60s.

pgrep -f supervisor_node                         # → pid
curl -sS --resolve star.local:443:100.64.0.6 -k -o /dev/null -w "%{http_code}\n" https://star.local/map.png
#   expect 200 once slam_toolbox has published /map (needs at least one scan cycle)
```


## SLAM stack auto-start

`scripts/slam-mvp.sh start` runs under `/etc/systemd/system/star-slam-mvp.service`:

- **Type=forking** so systemd tracks the detached child nodes the script backgrounds.
- **ExecStartPre=/bin/sleep 8** to give USB enumeration time after boot (the LiDAR/MCU serial-by-id symlinks must exist before `slam-mvp.sh` tries to open them).
- **User=star**, no sudo needed; the script is self-contained.
- **RemainAfterExit=yes** so the unit shows `active` even though the launcher process exits.
- **After=tailscaled.service** so the stack waits for networking; the supervisor publishes latched state which downstream viewers (Lichtblick/Grafana) pick up on connect.

Initial state after boot is **MANUAL mode, no e-stop** — supervisors hardcoded defaults. Operator flips to AUTONOMY via the Grafana toggle or Lichtblick publish panel.

To stop/restart/disable:
```bash
sudo systemctl stop star-slam-mvp
sudo systemctl start star-slam-mvp
sudo systemctl disable star-slam-mvp   # if you want boot-time launch off
```

Logs per node remain in `/tmp/slam-mvp-logs/` (wiped by reboot). Service-level output: `journalctl -u star-slam-mvp --no-pager`.

### slam_toolbox autostart backstop

slam_toolbox's `autostart: true` event-handler occasionally drops its initial
Configure event under boot-time DDS-discovery load, leaving the node in
`unconfigured` and `/map` unpublished. `scripts/slam-mvp.sh start` (and
`start-nav`) now run a 10-attempt service-call retry loop that drives the
lifecycle node to `active` idempotently -- equivalent to running the two
`ChangeState` calls by hand but without a human in the loop.

If `https://star.local/map.png` ever returns 503 after boot, the fallback is
still the same two calls:

```bash
source /opt/ros/jazzy/setup.bash
source /workspaces/STAR/star-ros2/install/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI=file:///workspaces/STAR/config/cyclonedds.xml
ros2 service call /slam_toolbox/change_state lifecycle_msgs/srv/ChangeState "{transition: {id: 1}}"
ros2 service call /slam_toolbox/change_state lifecycle_msgs/srv/ChangeState "{transition: {id: 3}}"
```
