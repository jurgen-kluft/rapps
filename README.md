# rapps

- Air Quality Monitoring   (Functional)
- Human Presence Detection (Functional)
- Bed Occupancy Detection

Uses the following `rdno` libraries:

- rcore
- rnetwork
- rwifi
- rsensors

## Overview

- AirQuality (CO2, Temperature, Humidity, Pressure)
  - Status: Working
- BedPresence
  - Status: Not working
  - Need to think of a more clever way to detect bed occupancy
- HumanPresence (mmWave, Distance)
  - Status: Working
- Magnet
  - Status: Working
- RD03D
  - Status: Working 


## Silent Mode (WIP)

During a specific time window (e.g., 10 PM to 6 AM), the device can enter a silent mode where it minimizes data transmission to conserve power and reduce noise. In this mode, the device can either stop sending data altogether or send data at a much lower frequency (e.g., every hour instead of every minute).

info = 4 bytes, 7 sensor values = 17 bytes, total = 21 bytes, rounded up to 24 bytes (multiple of 4).

e.g. one packet per second, size of one packet = 24 bytes:
- per second = 24 bytes
- per minute = 1.44 KB
- per hour = 86.4 KB
- 8 hours = 691.2 KB
- per day = 2.06 MB

For the 'human presence' detection application, this means that we will not send 'distance' measurements during silent mode, as they are not critical for monitoring human presence. Instead, we will only send 'info' packets that indicate whether a person is detected or not.

