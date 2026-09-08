#ifndef THEDB_H
#define THEDB_H

#include <string>
#include <vector>
#include "config.h"
#include <mysql/mysql.h>


/***********************************************************************************************
* theDB.h: header file for theDB class                                                         *
*                                                                                              *
* Version: 0.6                                                                                 *
* Last updated: 07/09/2026 10:27                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/

class theDB {
	
public:
	theDB();
	bool begin();
	std::vector<sentry> listTerminals();
	int topRFID();
	bool logTouch(std::string fobUID);
	std::string fobFromNo(int memNo);
	bool logMessage(std::string text);
	bool addFob(std::string uid);
	std::vector<mthVisit> monthlyVisits();
	shedder matchShedder(std::string fobUIDString);
	bool mtExists(int memID, int mchNo);
	bool isInShed(int memID);
	bool addVisit(int memID);
	bool addExit(int memID);
	bool addMach(int memID, int mchNo, bool refused);
	bool resetCommand();
	bool isAPerm(std::string fobUID);
	std::vector<mthVisitor> monthVisitors();

private:
	bool openConnection();
	bool preFetch(std::string sql, bool store);
	bool changeData(std::string sql, bool reportZero);
	std::string _userid;
	std::string _passwd;
	std::string _dBase;
	std::string _emFob;
	std::string _errMsg;
	MYSQL* _myConn;
	MYSQL_RES* _result;
	MYSQL_ROW _row;
// ADD HERE...
};

#endif