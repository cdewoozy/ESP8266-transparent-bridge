#!/bin/sh

if [ -f config_wifi.h ]
then
	cp config_wifi.h user
else
	if [ -f user/config_wifi.h ]
	then
		rm user/config_wifi.h
	fi
	echo '#define STA_SSID "ssid"
#define STA_PASSWORD "pass"
#define AP_SSID "myssid"
#define AP_PASSWORD "mypass"' > user/config_wifi.h

fi
