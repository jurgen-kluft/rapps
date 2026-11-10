# rapps

- Air Quality Monitoring   (Functional)
- Human Presence Detection (Functional)
- Door/Window              (Functional)
- Bed Occupancy Detection  (Design)
- Cube                     (Design)

Uses the following `Arduino` libraries:

- rcore
- rnetwork
- rwifi
- rsensors

## Overview

- AirQuality Basic (CO2, Temperature, Humidity, Pressure, Presence)
  - Status: Working
- AirQuality Elite (CO2, Temperature, Humidity, Pressure, Presence, VOC, PM2.5)
  - Status: Design
  - Note: A more advanced version of the AirQuality sensor with additional capabilities such as VOC and PM2.5 measurements.
- BedPresence
  - Status: Design, current idea is to use a Human Presence sensor aimed at the bed, but this
            requires it to be wall powered and not battery operated.
            Will turn on in the evening before bed time and will turn off at a certain moment when sure there is presence or absence.
- Presence (mmWave (e.g. RD03D), with distance)
  - Status: Can work but requires wall power and not using battery 
- Door/Window (magnet switch)
  - Status: Working, battery powered
- Cube
  - Status: Design, current idea is to use a cube-shaped device for controlling the house. 
            It can detect single and double taps and knows which face is up.
- Wall Switch Panel with 4 inch touch display
  - Status: Design, current idea is to use a wall-mounted touch display as a light switch panel that also can control all other panels in the house.

## Air Quality Basic

This will be a wall powered device that continuously monitors air quality parameters such as CO2, temperature, humidity, and pressure. It will provide real-time data to the central system for analysis and control. It has a touch display



## Silent Mode (Proposal)

During a specific time window (e.g., 10 PM to 6 AM), the device can enter a silent mode where it minimizes data transmission to conserve power and reduce noise. In this mode, the device can either stop sending data altogether or send data at a much lower frequency (e.g., every hour instead of every minute).

info = 4 bytes, 7 sensor values = 17 bytes, total = 21 bytes, rounded up to 24 bytes (multiple of 4).

e.g. one packet per second, size of one packet = 24 bytes:
- per second = 24 bytes
- per minute = 1.44 KB
- per hour = 86.4 KB
- 8 hours = 691.2 KB
- per day = 2.06 MB

For the 'human presence' detection application, this means that we will not send 'distance' measurements during silent mode, as they are not critical for monitoring human presence. Instead, we will only send 'info' packets that indicate whether a person is detected or not.

