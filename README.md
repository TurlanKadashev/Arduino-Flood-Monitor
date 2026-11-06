# Arduino-Based Flood Monitoring System

## Project Overview

This is a low-cost Arduino system that monitors rainfall and the water level in a container to help identify possible flood risk. It measures how fast the water is rising, compares it with the rainfall, and sends the data over Bluetooth. An LED turns on when a flood alert is active.

Status: prototype, currently being tested.

## Hardware Components

- Arduino UNO R3
- HC-SR04 ultrasonic sensor
- Tipping-bucket rain gauge
- HC-05 Bluetooth module
- LED indicator
- Breadboard and jumper wires

| Component | Arduino pin |
|-----------|-------------|
| HC-SR04 TRIG | 2 |
| HC-SR04 ECHO | 4 |
| Rain gauge | 3 |
| LED | 5 |
| HC-05 TX | 10 (Arduino RX) |
| HC-05 RX | 11 (Arduino TX) |

## How It Works

1. The ultrasonic sensor is placed above the container and measures the distance to the water surface. A reading is taken every 3 seconds (average of 5 measurements).
2. The rate of water rise (cm/s) is calculated from the last 10 readings.
3. The rain gauge counts bucket tips (0.279 mm per tip). The tips from the last 60 minutes are added up to get the rainfall in mm/h.
4. A flood alert is raised when the water rises faster than 0.005 cm/s and the rainfall is 15 mm/h or more, for 10 readings in a row. The LED then turns on.
5. The time until overflow is estimated from the current distance and the rate of rise.
6. After every reading, the data is sent as one line over Bluetooth and USB serial.

Example output:

```
Distance ≈ 24.50 cm, Rise rate ≈ 0.0120 cm/s, Rainfall ≈ 16.74 mm/h, Possible overflow in ≈ 0.57 hours, Flood risk: high chance
```

## Software

- Arduino IDE
- C++ (Arduino)
- NewPing library
- SoftwareSerial library

To run it, install the NewPing library, upload the sketch to the Arduino UNO, and open the Serial Monitor (9600 baud) or connect to the HC-05 with a Bluetooth terminal app. The thresholds and timings are defined at the top of the sketch.

## Limitations

- The system gives approximate measurements and is not a certified flood-warning device. Sensor accuracy and environmental conditions may affect the results.
- The overflow time assumes that the container overflows when the water reaches the level of the sensor.
- The rainfall value (mm/h) is only complete after the system has been running for one hour.

## Future Improvements

- Use the median of the measurements instead of the average to improve robustness against invalid readings.
- Calibrate the thresholds using field data from real rainfall events.
- Extended long-term testing under outdoor conditions.
