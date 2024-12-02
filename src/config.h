#include <Arduino.h>

/* In this file we stor generic data need for the device to run.*/

class Config{
    public:
        const String server_url = "http://www.dev.birdnoise.klucsik.fun/api"; // The server url where the logs will be sent
        const String name = "birdnoiseLogCollector"; // The name of the device, used for the webupdate
};
