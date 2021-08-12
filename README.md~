# ODrive
**NOTE!! This library is fork of [johnkok/ros_odrive](https://github.com/johnkok/ros_odrive) library**

Odrive controller library written in C++


- License URL: https://www.gnu.org/licenses/gpl-3.0.html
- License GNU General Public License v3.0

[![Generic badge](https://img.shields.io/badge/C%2B%2B-14-brightgreen)](https://shields.io/)
[![Generic badge](https://img.shields.io/badge/CMake-3.15-yellow)](https://shields.io/)

## Requirements
- [Libusb-1.0](https://github.com/libusb/libusb)

- [jsoncpp](https://github.com/open-source-parsers/jsoncpp)

## Usage
The libaray consists of an odrive class and a thin wrapper functions that make working wiht the object easier.
In order to use the libarary, you first need to create and initialize an odrive object:
```cpp
int main(){
...
dhr:odrive od;
uint64_t serial_number = 0x000000000001;
od.init(serial_number);
...
}
```
From there you need to request the Odrive's json config file:
```cpp
Json::Value json;
dhr::getJson(&od, &json);
```
Finally you can read and write from and to the Odrive the following way:
```cpp
...
//WRITE
uint32_t state1 = 6; //Odrive's parameters' classes should be set appropriately to the documentation
dhr::writeOdriveData(&od, json, "axis0.requested_state", state1);

//READ
float vel_es;
dhr::readOdriveData(&od, json, "axis0.encoder.vel_estimate",vel_es);

...
```
Exmaple usage you can [here](https://github.com/DHREngineering/Odrive/blob/master/main.cpp)

