# eStation_Solder (v1.0)
An open-source electronic soldering station (eStation) for JBC C245 tips, powered by STM32. Features custom firmware and hardware design. Part of Bit &amp; Solder.


This is the initial stable release of the custom-built soldering station. It is currently fully functional and performs reliably for soldering tasks.

# Channels
Please follow/subcribe for new updates
https://www.youtube.com/@bitnsolder
https://facebook.com/bitnsolder
www.tiktok.com/@bitnsolder

## Future Roadmap & Development
While the core functionality is solid, I am planning several upgrades and new features to enhance its capabilities:

### Hardware Upgrades
* **Op-Amp Optimization:** Explore upgrading the external Op-Amp or utilizing the high-performance internal Op-Amps of the STM32G474.
* **Display:** Evaluating the integration of a larger TFT screen for a better user interface.

### Software & Firmware
* **Energy-Optimized PID Controller:** Transitioning from traditional temperature-error-based PID to an energy-balance control strategy. This approach calculates the necessary energy input based on thermal mass and heat dissipation, allowing for ultra-fast ramp-up times without the risk of overshooting or thermal oscillation.
* **Settings Menu:** Implementing a comprehensive UI menu to customize profiles, sleep timers, display brightness, and calibration parameters.
* **Machine Learning Integration:** Exploring on-device ML/AI models to predict thermal load requirements, optimize tip temperature recovery, and detect soldering tip status (idle/active/dry burn).

### Planned Features
* **Integrated Multimeter:** Basic voltage/current measurement capabilities.
* **Oscilloscope:** Adding low-frequency signal monitoring.
* **Battery Management:** Integrated charging support.
* **Programmable Power Supply:** Implementing constant voltage (CV) and constant current (CC) modes.

---
*Contributions and suggestions are highly welcome!*

# DIY video
Part1 (Old solder iron's problem):
https://youtu.be/2gBfrUGCxDQ

Part2 (Hardware): https://youtu.be/oAlwus3iCWY

Part3 (Software - optimize PID) 
TBD (soon...)

# PCB
https://oshwlab.com/tuanta11/project_hqokojnx

# Note
This repository temporarily reuses a few functions from the AxxSolder project to accelerate initial development. These dependencies fully comply with the GPLv3 license and may be independently optimized or rewritten in future updates to better fit this specific hardware architecture.
