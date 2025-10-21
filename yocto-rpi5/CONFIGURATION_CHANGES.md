# STAR Pi5 Configuration Changes

This document summarizes the configuration changes made to the Yocto build.

## Changes Made

### 1. Hostname
- **Changed from**: `star-robot`
- **Changed to**: `star-pi5`
- **Files modified**:
  - `meta-star/recipes-core/base-files/base-files_%.bbappend`
  - `meta-star/recipes-core/images/star-minimal-image.bb`

### 2. Network Configuration
- **Ethernet (eth0)**: Changed from DHCP to static IP
- **Static IP**: `192.168.2.100`
- **Netmask**: `255.255.255.0`
- **Gateway**: `192.168.2.1`
- **File modified**:
  - `meta-star/recipes-connectivity/network/files/interfaces`

### 3. User Accounts

#### Root User
- **Username**: `root`
- **Password**: `star`
- **Previous**: No password (empty)

#### New User
- **Username**: `star`
- **Password**: `star`
- **UID**: 1000
- **Home**: `/home/star`
- **Shell**: `/bin/sh`
- **Groups**: `users`

#### Implementation
- **New recipe created**: `meta-star/recipes-core/star-users/star-users.bb`
- Uses `useradd` and `extrausers` classes to create user and set passwords
- Added to packagegroup: `meta-star/recipes-core/packagegroups/packagegroup-star-minimal.bb`

## Building the Image

After these changes, rebuild the image:

```bash
cd yocto-rpi5
source setup-environment.sh
bitbake star-minimal-image
```

## Connecting to the Pi

### Via Ethernet (Direct Connection)

1. Configure your computer's Ethernet interface to be in the same subnet:
   - IP: `192.168.2.1` (or any address in 192.168.2.x except .100)
   - Netmask: `255.255.255.0`

2. Connect Ethernet cable directly between your computer and Raspberry Pi 5

3. SSH to the Pi:
   ```bash
   ssh root@192.168.2.100
   # Password: star
   ```

   Or login as star user:
   ```bash
   ssh star@192.168.2.100
   # Password: star
   ```

### Via Router

If connected to a router at 192.168.2.x network, the Pi will have IP 192.168.2.100 and can be accessed as shown above.

## Default Credentials Summary

| User  | Password | Notes                    |
|-------|----------|--------------------------|
| root  | star     | Full system access       |
| star  | star     | Regular user (UID 1000)  |

## Network Summary

| Interface | Type   | IP Address      | Notes                          |
|-----------|--------|-----------------|--------------------------------|
| lo        | Loop   | 127.0.0.1       | Loopback                       |
| eth0      | Static | 192.168.2.100   | Ethernet, /24 subnet           |
| wlan0     | DHCP   | (disabled)      | WiFi - manual config required  |

## Security Notes

**WARNING**: These credentials are for development/testing only!

For production use:
1. Change passwords immediately after first boot
2. Consider disabling root SSH login
3. Use SSH keys instead of passwords
4. Configure firewall rules

## Hostname

The system will identify itself as `star-pi5`:
```bash
hostname
# Output: star-pi5
```

You may also be able to access it via mDNS (if avahi is installed):
```bash
ssh root@star-pi5.local
```
