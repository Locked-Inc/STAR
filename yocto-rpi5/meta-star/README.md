# meta-star - Custom Yocto Layer for STAR Robot

This layer provides a minimal, debloated Linux distribution for the STAR robot running on Raspberry Pi 5.

## Description

meta-star is a custom Yocto layer that creates an ultra-minimal headless Linux system with:
- Runit init system (lightweight service supervision)
- Dropbear SSH server
- Basic WiFi and Ethernet networking
- opkg package manager
- Minimal busybox utilities

## Dependencies

This layer depends on:
- meta (from poky)
- meta-raspberrypi
- meta-oe (from meta-openembedded)
- meta-python (from meta-openembedded)
- meta-networking (from meta-openembedded)

## Layer Structure

```
meta-star/
├── conf/
│   └── layer.conf                          # Layer configuration
├── recipes-core/
│   ├── base-files/
│   │   └── base-files_%.bbappend          # Hostname customization
│   ├── images/
│   │   └── star-minimal-image.bb          # Main image recipe
│   └── packagegroups/
│       └── packagegroup-star-minimal.bb   # Package group
├── recipes-init/
│   ├── runit/
│   │   ├── runit_2.1.2.bb                 # Runit init system
│   │   └── files/
│   │       ├── runit-init                 # Init wrapper
│   │       ├── runit-1                    # Stage 1: boot
│   │       ├── runit-2                    # Stage 2: services
│   │       └── runit-3                    # Stage 3: shutdown
│   └── runit-services/
│       ├── runit-services.bb              # Service definitions
│       └── files/
│           ├── dropbear/run               # SSH service
│           └── networking/run             # Network service
└── recipes-connectivity/
    └── network/
        ├── network-config.bb              # Network configuration
        └── files/
            └── interfaces                 # Network interfaces file
```

## Recipes

### star-minimal-image.bb

Main image recipe that creates the bootable system. Includes:
- Core boot packages
- Custom package group
- Minimal rootfs size
- Cleanup tasks to remove docs and unused locales

### packagegroup-star-minimal.bb

Defines the set of packages installed in the minimal image:
- System utilities (busybox, util-linux)
- Networking (WiFi firmware, wpa-supplicant, dropbear)
- Init system (runit)
- Package management (opkg)
- I2C tools (for sensors)

### runit_2.1.2.bb

Provides the runit init system with:
- Service supervision
- Lightweight process management
- Three-stage boot process

### runit-services.bb

Service definitions for:
- dropbear (SSH server)
- networking (interface management)

## Customization

### Adding Packages

Edit `recipes-core/packagegroups/packagegroup-star-minimal.bb`:

```bitbake
RDEPENDS:${PN} = " \
    ...
    your-new-package \
"
```

### Creating New Services

1. Create service directory: `mkdir -p recipes-init/runit-services/files/myservice`
2. Create run script: `recipes-init/runit-services/files/myservice/run`
3. Add to `runit-services.bb`
4. Enable in image

### Modifying Network Configuration

Edit `recipes-connectivity/network/files/interfaces` to change network settings.

## Building

From the yocto-rpi5 directory:

```bash
source setup-environment.sh
bitbake star-minimal-image
```

## Image Output

Built images are located in:
```
build/tmp/deploy/images/raspberrypi5/
```

Look for:
- `star-minimal-image-raspberrypi5.wic.bz2` - Compressed SD card image
- `star-minimal-image-raspberrypi5.wic` - Uncompressed SD card image

## Layer Configuration

### layer.conf

- **BBFILE_PRIORITY**: Set to 10 (higher priority than base layers)
- **LAYERSERIES_COMPAT**: kirkstone
- **LAYERDEPENDS**: core, raspberrypi

## Design Philosophy

This layer follows these principles:

1. **Minimal by default**: Only include what's absolutely necessary
2. **Easy to extend**: Simple structure for adding packages
3. **Service supervision**: All services managed by runit
4. **Network-ready**: WiFi and Ethernet work out of box
5. **Debuggable**: Includes basic tools for troubleshooting

## Future Enhancements

Potential additions (not included to keep minimal):
- ROS2 integration layer
- LiDAR driver recipes
- Camera utilities
- Real-time kernel patches
- Custom boot splash

## Maintenance

Layer maintainer: STAR Robot Project

Compatible with: Yocto Project 4.0 (Kirkstone LTS)

## License

This layer is licensed under MIT. See main STAR project for details.

## Contributing

When adding new recipes:
1. Follow Yocto naming conventions
2. Include license information
3. Document dependencies
4. Test build before committing
5. Update this README

## Support

For issues or questions:
- Check build logs in `build/tmp/work/`
- Review Yocto documentation
- File issue in STAR repository
