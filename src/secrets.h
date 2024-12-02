#include <Arduino.h>
/*
    In this file you have to store your wifi's SSID and password, and the update server's URL
    This should only exist on your device and noone should be able to access it
*/

class Secrets{
    public:
        String update_server = "http://arduino-webupdate.klucsik.fun";
        const char *SSID_1 = "EXAMPLE"; // Write here the name (SSID) of your wifi network
        const char *pass_1 = "12345"; // Write here the password of your wifi network
        const char *SSID_2 = "EXAMPLE2"; // Write here the name (SSID) of your wifi network, or "" if you don't want to use it
        const char *pass_2 = "23456"; // Write here the password of your wifi network, or "" if you don't want to use it
        // The device will first try to connect to the wifi labeled 1, and if it fails it will try to connect to the wifi labeled 2
};