#ifndef CONFIG_H
#define CONFIG_H
#include <string>

/*********************************************************************************************************************************
* config.h: support header file for Sentinel project. Mainly used for defining constants and structs
* Version 1.0
* Last updated 18/08/2026 16:45
* Author: Jim Gunther
*********************************************************************************************************************************/
#define DB_USER "mis"
#define PASSWORD "mispwd"
#define DBASE "Sentinel"
#define EMF "ce864161"

struct sentry {
	std::string name;
	bool isAdmin;
	std::string LCDText;
	std::string commd;
};

struct trained {
	int mchNo;
	int memID;
};

struct shedder {
	int memID;
	std::string givenName;
};

/*def init():
    global sender
    sender = "none"
    global icBuffer
    icBuffer = "(empty)"
    global ogBuffer
    ogBuffer = "(empty)"
    global touchReceived
    touchReceived = False
    global forceeHour
    forceeHour = 18
	*/
	#endif
