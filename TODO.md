# Divoom Ditoo Pro Controller - Feature Status

## Implemented

- [x] Scan for Bluetooth devices
- [x] Send animation/image to display
- [x] Set date/time
- [x] Toggle alarm on/off
- [x] Convert GIF to Divoom 16x16 format
- [x] Convert Divoom 16x16 format to GIF
- [x] Debug/inspect Divoom format images
- [x] Keyboard backlight control (next/prev/toggle)
- [x] Screen brightness control
- [x] Scrolling text
- [x] Clock face selection
- [x] Get/set volume
- [x] Play/pause control
- [x] Set language
- [x] Light mode selection (light, hot, special, music via 0x45)
- [x] Read and validate device responses (ACK/NAK)

## Not Implemented

### Display & Images
- [ ] Drawing pad control
- [ ] Sand painting mode
- [ ] GIF speed control
- [ ] Screen direction configuration
- [ ] Watch face mode (ext cmd 0x14 / JSON)
- [ ] Score mode (JSON)
- [ ] Set box color / sleep color

### Audio & Music
- [ ] EQ control
- [ ] Microphone on/off
- [ ] SD card music playback (list, play by ID, next/prev, play mode)
- [ ] Mix music mode
- [ ] Power-on voice control / volume

### Alarms (extended)
- [ ] Alarm with custom scene/GIF
- [ ] Alarm volume control
- [ ] Alarm voice control

### Sleep Mode
- [ ] Sleep timer
- [ ] Sleep light effect
- [ ] Sleep scene
- [ ] Sleep color

### Device Settings
- [ ] Set device name
- [ ] Auto power-off timer
- [ ] Energy control

### Notifications
- [ ] Android notification forwarding (ANCS)
- [ ] Custom notification pictures

### Games
- [ ] Game control
- [ ] Game key input

### File Management & Updates
- [ ] Firmware update over Bluetooth
- [ ] Hot-swap content updates
- [ ] SD/TF card management

### Work Modes
- [ ] Switch between Bluetooth / Line-in / SD Card / USB Audio modes

### Protocol
- [ ] JSON-based command protocol (SPP_JSON)
- [ ] Configurable Bluetooth adapter selection
