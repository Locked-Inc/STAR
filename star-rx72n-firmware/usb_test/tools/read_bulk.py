import usb.core, usb.util, sys
dev = usb.core.find(idVendor=0x1209, idProduct=0x0002)
if dev is None:
    print("NOT FOUND"); sys.exit(1)
try:
    if dev.is_kernel_driver_active(0):
        dev.detach_kernel_driver(0)
except Exception as e:
    print("detach:", e)
dev.set_configuration()
cfg = dev.get_active_configuration()
print("cfg OK, reading EP 0x81...")
total = 0
for i in range(20):
    try:
        data = dev.read(0x81, 64, timeout=500)
        print(f"[{i}] got {len(data)} bytes: {bytes(data)!r}")
        total += len(data)
    except usb.core.USBError as e:
        print(f"[{i}] err: {e}")
        break
print(f"TOTAL: {total}")
