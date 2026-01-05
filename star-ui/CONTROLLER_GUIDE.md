# STAR PWA Controller Guide

The STAR PWA Controller allows for real-time tactile control of the robot using a physical gamepad.

## Supported Devices
- **Retroid Pocket 2S** (Optimized)
- Any Android/iOS device with a connected Bluetooth/USB gamepad.
- Desktop browsers (Chrome/Firefox/Edge).

## Setup Instructions

1.  **Connect Gamepad:** Pair your controller via Bluetooth or plug it in via USB-OTG.
2.  **Enable Gamepad Mode:** On Retroid Pocket 2S, ensure the device is in "Gamepad Mode" (not Mouse Mode) in the quick settings.
3.  **Access the UI:** Open Chrome and navigate to the STAR UI URL (e.g., `http://star-robot.local:5173`).
4.  **Activate:** The UI will display "Gamepad Disconnected". **Press any button** (e.g., the 'A' button) on the controller to activate the browser's Gamepad API.

## Controls (Arcade Drive)

- **Left Stick (Vertical):** Controls Forward/Backward speed ($v$).
- **Left Stick (Horizontal):** Controls Steering/Angular velocity ($\omega$).

## Safety Features

- **Watchdog:** If the connection to the robot is lost for more than 200ms, the robot will automatically stop.
- **Dead Man's Switch:** If the gamepad is unplugged or the browser tab is closed, a final "stop" command is sent.

## Troubleshooting

- **No input detected:** Refresh the page and press a button on the gamepad.
- **Latency issues:** Ensure you are on a 5GHz Wi-Fi network for optimal performance.
