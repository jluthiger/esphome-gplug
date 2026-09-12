# Migrating a gPlug from Tasmota to this firmware

This replaces the Tasmota firmware on a gPlug with the ESPHome firmware in this repository. It
takes about 20 minutes, most of it the first compile, and needs physical access to the device and
a USB cable.

There is no over-the-air path from Tasmota. The two firmwares use different flash layouts, so the
device has to be erased and written over USB once. Afterwards, every further update is over the
air from the device's own web app.

---

## Read this before you unplug anything

**Write down the decryption key first.** On an encrypted meter the key (GUEK) lives only in the
Tasmota script and in your grid operator's records. Erasing the device destroys the only copy you
control, and getting another one means asking the operator and waiting. This is the single step
of the whole migration that cannot be undone.

In Tasmota, open the web interface, go to **Consoles → Edit script**, and copy the whole script
into a text file. The key is the line

```
dKEY="0123456789ABCDEF0123456789ABCDEF"
```

32 hexadecimal characters. An empty `dKEY=""` means the meter is unencrypted and there is nothing
to save. While you are in there, note the first comment line of the script, for example
`; gPlugK KAMSTRUP DLMS Push1, (6.9.26)`. It tells you which profile to pick later.

You also want your Wi-Fi network name and password to hand, since the device forgets them.

### What you keep and what you lose

| | |
|---|---|
| Meter readings and totals | **Kept.** They live in the meter, not in the gPlug. The new firmware reads the same registers |
| The decryption key | **Lost**, unless you copy it out now |
| Tasmota's own stored data, rules and MQTT topics | **Lost.** This firmware has no Tasmota compatibility layer, by design |
| Home Assistant entities from Tasmota | **Replaced.** The device is adopted again as an ESPHome device, with new entity names |
| Your Wi-Fi credentials | **Lost**, re-entered during setup |

### Which devices this works on

gPlugD, gPlugD-E, gPlugK and gPlugM, all of which are ESP32-C3. **gPlugE is not supported**: it is
Ethernet hardware (WT32-ETH01) and out of scope for this firmware. Leave those on Tasmota.

### Two ways to get the firmware onto the device

**The short way — the web installer.** Every release is published as a ready-made image, and
<https://jluthiger.github.io/esphome-gplug/> flashes it straight from the browser over USB. It
needs Chrome or Edge on a computer (Safari and phones cannot talk to serial ports) and nothing
else: no Python, no ESPHome, no toolchain, no compile. Plug the gPlug in, press *Install*, choose
*Erase device*, and continue at [Step 6](#step-6-set-the-device-up). Steps 1 to 5 below exist for
the other case.

**The long way — build it yourself.** Do this if you want to change the firmware, if the browser
route is not available to you, or if you would rather flash a binary you compiled. That is the
rest of this guide. The single ready-made image also lives on each
[release page](https://github.com/jluthiger/esphome-gplug/releases/latest) as
`gplug-<version>.factory.bin` if you want the file but not the browser.

Either way, the meter key has to be out of Tasmota before you start.

---

## Step 1: install the software

You need ESPHome. It brings its own copy of esptool, the flashing tool, so that is one install
rather than two. Python 3.11, 3.12, 3.13 or 3.14 is required; 3.15 is not supported yet.

### macOS

```
brew install esphome
```

Without Homebrew, use pipx instead, which keeps ESPHome in its own environment:

```
python3 -m pip install --user pipx
python3 -m pipx ensurepath
pipx install esphome
```

No driver is needed. The ESP32-C3 in a gPlug presents itself as a USB serial device that macOS
supports out of the box.

### Linux

```
python3 -m pip install --user pipx
python3 -m pipx ensurepath
pipx install esphome
```

Your user needs permission to use serial ports, otherwise flashing fails with "permission denied"
on `/dev/ttyACM0`. On Debian, Ubuntu and Raspberry Pi OS:

```
sudo usermod -aG dialout $USER
```

On Arch and Fedora the group is `uucp` rather than `dialout`. Log out and back in afterwards, or
the new group membership will not apply.

### Windows

Install Python from [python.org](https://www.python.org/downloads/) and tick **Add python.exe to
PATH** in the installer. Then, in PowerShell:

```
py -m pip install --user pipx
py -m pipx ensurepath
pipx install esphome
```

Close and reopen PowerShell so the new PATH takes effect. Windows 10 and 11 recognise the device
without a driver. If you are on an older Windows, or the port never appears, install the
[CP210x / USB serial drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
for your machine.

### Check it worked, on any system

```
esphome version
```

---

## Step 2: build the firmware

```
git clone https://github.com/jluthiger/esphome-gplug
cd esphome-gplug/firmware
esphome compile dev.yaml
```

The first run downloads the ESP-IDF toolchain, roughly a gigabyte, and takes several minutes. Later
builds take under a minute. You do **not** need Node.js: the web app is committed to the repository
in built form, and is only rebuilt if you change it.

A successful build ends with `SUCCESS` and leaves the images under
`.esphome/build/gplug/.pioenvs/gplug/`.

---

## Step 3: connect the device and find its port

Plug the gPlug into the computer with a USB cable that carries data. Charge-only cables are a
common and confusing cause of "no port appeared".

| System | How to find the port | What it looks like |
|---|---|---|
| macOS | `ls /dev/cu.*` | `/dev/cu.usbmodem1101` |
| Linux | `ls /dev/ttyACM*` | `/dev/ttyACM0` |
| Windows | Device Manager → Ports (COM & LPT) | `COM5` |

If no port appears, hold down the button on the gPlug while plugging the cable in, then release it.
That is the same button this firmware later uses to reopen the setup access point, and holding it
during power-up forces the chip into its flashing mode.

Confirm the computer can talk to the chip before erasing anything:

```
esptool --chip esp32c3 --port <PORT> chip-id
```

It should report an ESP32-C3. If this fails, nothing further will work, so fix it here.

---

## Step 4: erase Tasmota completely

```
esptool --chip esp32c3 --port <PORT> erase-flash
```

This is deliberate rather than cautious. Tasmota divides the flash differently, and leftovers from
it would sit inside the regions this firmware uses for its settings and its 15-minute history,
where they would be read as corrupt data. A full erase is the clean start.

If your esptool is older than version 5, the subcommands are spelled with underscores:
`erase_flash` and `write_flash`.

---

## Step 5: write the new firmware

```
esphome run dev.yaml --device <PORT>
```

This compiles if needed, writes the firmware and then shows the device's log. Press Ctrl+C to stop
watching the log; the device keeps running. Add `--no-logs` if you would rather it return
immediately.

The equivalent with esptool alone, if you prefer:

```
esptool --chip esp32c3 --port <PORT> write-flash 0x0 .esphome/build/gplug/.pioenvs/gplug/firmware.factory.bin
```

Writing `firmware.factory.bin` to address 0 is right **here**, on a device you have just erased.
It is the wrong thing to do later: on a configured gPlug it wipes the Wi-Fi credentials and the
meter settings along with everything else. Later updates go through the app or over the air. See
`firmware/README.md` for the details.

---

## Step 6: set the device up

The device has no Wi-Fi credentials yet, so it opens its own access point called **gPlug-Setup**.

1. Join that network from a phone or laptop. A setup page opens by itself. If it does not, browse
   to `http://192.168.4.1/`.
2. Enter your home Wi-Fi and save. The device restarts and joins it.
3. Open `http://gplug.local/`. If your network does not resolve that name, look for the device's IP
   address in your router, or read it from the USB log.
4. The wizard asks for the device type, then for the meter profile and the key.

Pick the profile that matches the Tasmota script you copied in the beginning:

| Tasmota script | Device | Profile in the wizard | Key needed |
|---|---|---|---|
| `gplugd/p1-dsmr` | gPlugD | P1 DSMR | no |
| `gplugd/p1-hdlc_dlms` | gPlugD | P1 HDLC/DLMS | yes |
| `gplugde/p1-dsmr` | gPlugD-E | P1 DSMR | no |
| `gplugde/p1-hdlc_dlms` | gPlugD-E | P1 HDLC/DLMS Romande Energie | yes |
| `gplugk/dlms-push-1` | gPlugK | Kamstrup DLMS Push | yes |
| `gplugm/romande-energie` | gPlugM | CII HDLC/DLMS Romande Energie | yes |
| `gplugm/universal` | gPlugM | CII HDLC/DLMS universal (L+G E450) | yes |

You will usually not have to choose at all. The device listens to the meter while you are still on
the hardware step and proposes the profile that matches what it hears. Paste the key from your
saved script when asked; it is stored on the device and never shown again.

The pin settings on the hardware step are filled in from the device type and match the Tasmota
script's `rxPin`, `rL`, `gL`, `bL` and `butA` values. Leave them alone unless your script differs.

---

## Step 7: check that it works

The last wizard step waits for the meter and tells you what it sees. Within a minute you should
have live values and a green LED.

Open the app at `http://gplug.local/` and confirm:

- **Live** shows current power and the two counters, and they match the meter's own display.
- **History** fills up over the following hours. A new record is written every 15 minutes.
- **Setup → event log** shows the restart you just caused, and nothing alarming after it.

If there are no values, the app says why rather than leaving you guessing: a silent line, a wrong
profile, a key that does not decrypt, or a cable problem each get their own message and a button
back to the step that fixes it.

---

## If something goes wrong

| Symptom | What to do |
|---|---|
| No serial port | Another cable, another port, or hold the button while plugging in |
| "Permission denied" on Linux | Add yourself to `dialout` (or `uucp`) and log out and back in |
| Flashing stops partway | Erase again and retry; a short or unpowered hub is a common cause |
| No meter data after setup | Follow the message on screen. It distinguishes a dead line from a wrong profile from a wrong key |
| Wrong profile or key entered | Setup tab, then back into the wizard. The key can be re-entered without redoing anything else |
| Device unreachable, no idea why | Hold the button for at least 3 seconds. It erases the Wi-Fi credentials and reopens **gPlug-Setup**, leaving the meter settings intact |

---

## Going back to Tasmota

Nothing here is one-way as long as you kept the key.

1. Download the Tasmota image for ESP32-C3 (`tasmota32c3.factory.bin`) from the Tasmota project.
2. `esptool --chip esp32c3 --port <PORT> erase-flash`
3. `esptool --chip esp32c3 --port <PORT> write-flash 0x0 tasmota32c3.factory.bin`
4. Set up Wi-Fi again, paste your saved script back in, and put the key back into `dKEY`.

---

## What is different afterwards

The device now serves its own app and keeps its own history, so it is useful on its own, without a
server. Home Assistant is optional: if you run it, the ESPHome integration finds the device on the
network and adopts it, and you get the values as entities as before, under new names.

There is no MQTT and no Tasmota HTTP API. What replaces them is the app on the device, the CSV
export of the 15-minute load profile under **History**, and the ESPHome native API for Home
Assistant.

Updates from here on need no cable: **Setup → firmware update**, or `esphome upload dev.yaml
--device gplug.local` from this repository.
