# Mimo - Bill of Materials

This is the complete Bill of Materials (BOM) for the Mimo desktop robot. It includes the electronics, custom PCB, 3D-printed enclosure, mounting hardware, and wiring required to build the project.

## Bill of Materials

| # | Component | Qty | Estimated Unit Price | Estimated Total | Purchase Link |
|---|-----------|-----|----------------------|-----------------|---------------|
| 1 | XIAO ESP32-S3 | 1 | $8–12 | $8–12 | https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html |
| 2 | 2.42" SSD1309 OLED 128×64 I2C | 1 | $8–12 | $8–12 | https://www.aliexpress.com/wholesale?SearchText=2.42+SSD1309+128x64+I2C+OLED |
| 3 | TTP223 Capacitive Touch Sensor | 1 | Rs. 40 | Rs. 40 | https://www.robochacha.pk/ |
| 4 | MAX98357A I2S Audio Amplifier | 1 | Rs. 799 | Rs. 799 | https://www.robochacha.pk/product/max98357a-i2s-audio-amplifier-module/ |
| 5 | 3W 4Ω Speaker | 1 | Rs. 250–500 | Rs. 250–500 | https://www.daraz.pk/catalog/?q=3w+4ohm+speaker |
| 6 | 3.7V Li-Po Battery | 1 | Rs. 800–1,500 | Rs. 800–1,500 | https://www.daraz.pk/catalog/?q=3.7v+lipo+battery |
| 7 | Slide ON/OFF Switch | 1 | Rs. 50–100 | Rs. 50–100 | https://www.daraz.pk/catalog/?q=slide+switch+SPDT |
| 8 | Custom Mimo PCB | 1–2 | $5–15 | $5–15 | https://jlcpcb.com/ |
| 9 | PCB Shipping | 1 | $10–20 | $10–20 | https://jlcpcb.com/ |
| 10 | 3D-Printed Mimo Enclosure | 1 | Rs. 1,000–2,500 | Rs. 1,000–2,500 | Local 3D Printing Service |
| 11 | M2 Screws + Nuts/Standoffs | 1 set | Rs. 150–300 | Rs. 150–300 | https://www.daraz.pk/catalog/?q=M2+screws+nuts |
| 12 | JST-PH 2-Pin Connectors | 2–3 | Rs. 100–200 | Rs. 100–200 | https://www.daraz.pk/catalog/?q=JST+PH+2+pin |
| 13 | Connecting Wires | 1 pack | Rs. 200–400 | Rs. 200–400 | https://www.daraz.pk/catalog/?q=jumper+wire+electronics |
| 14 | 2.54mm Pin Headers | 1 pack | Rs. 100–200 | Rs. 100–200 | https://www.daraz.pk/catalog/?q=2.54mm+pin+header |

## Main Electronics

The main electronics of Mimo are built around the XIAO ESP32-S3. The OLED provides the animated face, while the TTP223 allows Mimo to respond to touch.

The MAX98357A provides I2S audio output to the speaker for sounds and future voice playback.

## Fabrication

The project requires a custom PCB designed in KiCad and a custom 3D-printed enclosure designed in Fusion 360.

The enclosure includes openings for the OLED, USB connection, power switch and speaker, along with mounting points for the PCB.

## Estimated Budget

The final cost will depend on the supplier, shipping method, PCB quantity and local 3D-printing cost.

The estimated total project cost is approximately:

**$55–75 USD**

This estimate includes the electronics, PCB manufacturing and shipping, enclosure fabrication, and required mounting/connectors.

## Notes

- Prices are estimates and may change depending on the supplier.
- PCB shipping can vary significantly depending on the selected shipping method.
- The weather functionality can use Open-Meteo, so no paid weather API subscription is required.
- The project does not require a separate DAC because the MAX98357A handles the I2S audio output.
