# Drone Simulator

A drone simulator made with ESP32, Unity and a browser-based dashboard (website). You fly a drone in Unity and watch data like battery, altitude and flight events (armed or disarmed) update in real time on a webpage that is served directly from the microcontroller. Built as a starting point toward a real camera-equipped drone.

```
Unity 3D Simulation  →  ESP32 Web Server  →  Browser Dashboard
  (flies the drone)       (stores data)        (displays it live)
```
---
## Setup

### ESP32

1. Install [Arduino IDE](https://www.arduino.cc/en/software) and add ESP32 board support
2. Install the **ArduinoJson** library via Library Manager
3. Open `drone_dashboard.ino`
4. Fill in your WiFi credentials:
```cpp
const char* WIFI_SSID = "network_name";
const char* WIFI_PASS = "password";
```
5. Upload the code to your ESP32
6. Open the Serial Monitor and set **115200 baud** -> it will print the ESP32's IP address once connected

### Unity

1. Open the Unity project
2. Select the Drone object in the scene
3. In the `DroneTelemetry` script, set **Esp32 Url** to your ESP32's IP:
4. Make sure your PC is on the **same WiFi network as the ESP32**
5. In **Edit → Project Settings → Player → Other Settings**, set **Allow downloads over HTTP** to `Always Allowed`
6. Press Play

### Viewing the Dashboard

Open a browser on any device on the same WiFi and go to the IP address of your ESP32
```
http://192.168.x.x
```
---
## Controls (Unity)
| `Space` | Arm the drone/Start flying |

| `W / S` | Throttle up / down |

| `Arrow keys` | Fly forward / back / strafe |

| `A / D` | Fly left / right |

| `Q / E` | Rotate left / right |

