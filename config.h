#ifndef CONFIG_H
#define CONFIG_H
#include <string>

/*********************************************************************************************************************************
* config.h: support header file for Sentinel project. Mainly used for defining constants and structs
* Version 1.0
* Last updated 25/08/2026 23:17
* Author: Jim Gunther
*********************************************************************************************************************************/
#define DB_USER "mis"
#define PASSWORD "mispwd"
#define DBASE "Sentinel"
#define EMF "ce864161"

#define FORCEE_HR 18

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

struct visitor {
	std::string fullName;
	int id;
	std::string startTime;
	std::string endTime;
};

struct dayFobber {
	std::string fullName;
	std::string fobuid;
};

struct mthVisit {
	int countVisits;
	int dom;
};

struct mthVisitor {
	std::string fullName;
	int id;
	std::string fobuid;
	std::string startTime;
};

#endif