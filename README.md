# Mimo

Mimo is a cute DIY desktop robot built around the Seeed Studio XIAO ESP32-S3. It combines a custom PCB, a 3D-printed enclosure, an OLED display, touch interaction, Wi-Fi, and audio to create a small interactive desktop companion.

Mimo normally displays cute animated eyes and different emotions. The user can interact with Mimo by touching the TTP223 sensor, which cycles through different modes such as time, weather, and the normal animation mode.

<img width="1029" height="394" alt="SCHEMATIC 1" src="https://github.com/user-attachments/assets/add99e50-c9e0-4e2b-8fdd-ad9473f4257e" />
<img width="387" height="419" alt="SCHEMATIC 3" src="https://github.com/user-attachments/assets/fba7a5a6-e467-42c4-95ef-a1c63459a08d" />
<img width="447" height="506" alt="SChematic 2" src="https://github.com/user-attachments/assets/32961bbe-4a90-4163-aad1-a1bef523eef8" />
<img width="624" height="512" alt="PCB FINALLLLLLLLLLL" src="https://github.com/user-attachments/assets/32c484b4-45d3-4edb-b857-6cbf87a9d06c" />
<img width="489" height="332" alt="MIMO CASE FINAL 1" src="https://github.com/user-attachments/assets/2f8115e9-12a2-4b04-9807-ad0b7cf6d5d8" />
<img width="399" height="270" alt="MIMO CASE FINAL" src="https://github.com/user-attachments/assets/e9521f14-e99a-4da8-8321-98c3850c68a5" />

## Features

- Cute animated eyes and different emotions
- 2.42" SSD1309 OLED display
- TTP223 touch sensor for interaction
- Wi-Fi connectivity
- Online weather information
- Real-time clock display
- MAX98357A I2S audio amplifier
- Speaker for sounds and future voice playback
- Li-Po battery powered
- Physical ON/OFF switch
- Custom PCB designed in KiCad
- Custom 3D-printed enclosure designed in Fusion 360

## How It Works

Mimo starts in its normal animation mode, where the OLED continuously displays different eye expressions and emotions.

The TTP223 touch sensor is used to change between modes:

    First Touch  ->  Time
    Second Touch ->  Weather
    Third Touch  ->  Back to Animations

The ESP32-S3 connects to Wi-Fi and retrieves weather information from an online API. The MAX98357A amplifier is used to drive the speaker, allowing Mimo to produce sounds and eventually play recorded voice clips.

## Hardware

| Component | Purpose |
|-----------|---------|
| XIAO ESP32-S3 | Main microcontroller |
| 2.42" SSD1309 OLED | Eyes, animations and information |
| TTP223 | Touch interaction |
| MAX98357A | I2S audio amplifier |
| Speaker | Sounds and voice |
| Li-Po Battery | Power source |
| Physical Switch | Power ON/OFF |
| Custom PCB | Main electronics board |

## Software

The firmware is being developed using Arduino IDE and C++.

Main libraries and technologies used:

- ESP32 Arduino Core
- U8g2
- ArduinoJson
- Wi-Fi
- HTTP requests
- I2S audio
- Online weather API

## Project Structure

    Mimo/
    ├── firmware/
    │   └── mimo.ino
    ├── pcb/
    │   └── KiCad/
    ├── case/
    │   └── Fusion360/
    ├── README.md
    └── LICENSE

## PCB

The Mimo PCB was designed from scratch in KiCad. It connects the XIAO ESP32-S3 with the OLED, touch sensor, audio amplifier, power system and other components.

The PCB is designed to fit inside the custom 3D-printed robot enclosure.

## Enclosure

The enclosure was designed in Fusion 360 specifically for Mimo's electronics. It includes mounting points for the PCB, openings for the OLED, USB connection, power switch and speaker, along with external horns to give Mimo its recognizable appearance.

## Future Plans

- Add more eye animations and emotions
- Add more touch interactions
- Improve weather animations
- Add battery status
- Add more sound effects
- Add sleep and wake animations
- Add additional interactive features

## Project Status

This project is being built as a complete DIY robot from the electronics and PCB design to the firmware and 3D-printed enclosure.

## License

This project is open source. See the LICENSE file for details.
