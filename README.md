# Birdnoise logcollector
This repository is part of the birdnoise project, in which ornithologists play sounds to birds in forests to make science.
The sounds are played on an esp8266 based device, more info: https://github.com/klucsik/noise-generator-for-bird-research

This is the code for the "logCollectorDevice" that collects the logs from these devices.

## The Device:
The device is an ESP8266 with an SD card module attached to it (the device can be configure to use LittleFS instead).

## Code Structure:
Everything is in the "src" folder. 
Here you can find the config and the secret files, fill these with the spesific options you will use. (<b>Keep the secret file secret!</b>)
### main.cpp
This is the hearth of the device.
Its basicly puts itself in a WiFi host mode, where you can only access servers run locally.
It starts running a HTTP webserver (on port 80) which listens to "/sendLog" get request where "chipID" and "log" are params.
It then saves the sent logs to an SD card attached to it (or can be set to use LittleFS also).
If the deviceLogCollector is than taken "home", into a WiFi enviroment it can connect to it will connect and upload all data from it's SD card (or LittleFS) to the webserver.

There's also a webupdate function which is basicly a way to update the firmware of the device from the web.