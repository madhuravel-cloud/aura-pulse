# Aura Pulse

**Multi-Stream Windows WASAPI Audio Router** — stream your PC's system audio to multiple headphones and speakers simultaneously with independent per-device volume controls.

![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(x64)-0078D6?style=flat-square&logo=windows)
![Engine](https://img.shields.io/badge/Engine-C%2B%2B%20WASAPI%20%2B%20Node--API-blue?style=flat-square&logo=cplusplus)
![UI](https://img.shields.io/badge/Frontend-Electron-47848F?style=flat-square&logo=electron)

---

## Download

Grab the latest Windows build directly from **[GitHub Releases](https://github.com/madhuravel-cloud/aura-pulse/releases/latest)**:

* **[Aura Pulse Setup 1.1.0.exe](https://github.com/madhuravel-cloud/aura-pulse/releases/download/v1.1.0/Aura.Pulse.Setup.1.1.0.exe)** — Full Windows installer (creates Desktop & Start Menu shortcuts)
* **[Aura Pulse 1.1.0.exe](https://github.com/madhuravel-cloud/aura-pulse/releases/download/v1.1.0/Aura.Pulse.1.1.0.exe)** — Standalone portable executable (run directly with no installation)

> **Note:** Windows SmartScreen may show an *"Unknown Publisher"* prompt on first launch since the binary is self-packaged. Click **More info → Run anyway** to launch.

---

## Features

* **Real-Time Loopback Fan-Out:** Captures your default Windows playback stream via WASAPI Loopback and routes independent PCM ring buffers to multiple active output endpoints concurrently.
* **Per-Device Volume & Mute Controls:** Adjust volume (`0%–100%`) or mute individual headphones live during playback without interrupting other connected devices.
* **Automatic Sample Rate & Format Conversion:** Uses Windows `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM` so mixed endpoints (Bluetooth, USB DACs, 3.5mm wired, HDMI) stay synced automatically.
* **Native C++ Bridge:** Zero-overhead `node-addon-api` integration connecting multi-threaded C++ WASAPI renderers directly to an Electron glassmorphism interface.

---

## Supported Multi-Headphone Setups

Windows' built-in Classic Bluetooth driver (`BthA2dp.sys`) locks a single internal radio to one A2DP stereo stream at a time. Aura Pulse supports multi-device streaming across:

1. **Dual Bluetooth LE Audio (LC3):** Enable *Use LE Audio when available* in Windows Bluetooth settings with two LE Audio-capable headsets for dual wireless stereo over one built-in radio.
2. **Bluetooth + Wired / USB Hybrid:** Stream simultaneously to one Classic Bluetooth headphone alongside a 3.5mm wired headset, USB headset, or HDMI monitor speaker.
3. **Built-in Bluetooth + USB Bluetooth Transmitter:** Connect a USB Bluetooth audio dongle for the second headset—Windows exposes it as an independent USB sound card, bypassing the single-A2DP cap.
4. **Stereo (A2DP) + Hands-Free (AG Audio):** Run one headset on Stereo and route the second stream to the other headset's *Hands-Free AG Audio* endpoint.

---

## Build from Source

### Prerequisites
* **Windows 10 / 11 (x64)**
* **Node.js** (v18 or newer)
* **Visual Studio Build Tools** (with the *Desktop development with C++* workload)
