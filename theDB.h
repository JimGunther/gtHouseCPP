#ifndef THEDB_H
#define THEDB_H

#include <string>
#include <mysql/mysql.h>
//#include <map>
#include <vector>
#include "config.h"

/***********************************************************************************************
* theDB.h: header file for theDB class                                                         *
*                                                                                              *
* Version: 0.2                                                                                 *
* Last updated: 18/08/2026 19:27                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/

class theDB {
	
public:
	theDB();
	bool begin();
	std::vector<sentry> listTerminals();
	bool logTouch(std::string fobUID);
	bool logMessage(std::string text);
	shedder matchShedder(std::string fobUIDString);
	bool isInShed(int memID);
	bool addVisit(int memID);
	bool addExit(int memID);
	bool isAPerm(std::string fobUID);

private:
	bool openConnection();
	bool preFetch(std::string sql, bool store);
	bool changeData(std::string sql);
	std::string _userid;
	std::string _passwd;
	std::string _dBase;
	std::string _emFob;
	std::string _errMsg;
	MYSQL* _myConn;
	MYSQL_RES* _result;
	MYSQL_ROW _row;
//	std::vector<MYSQL_ROW> _rows;
//	std::vector<unsigned long*> _lengths;
// ADD HERE...
};

#endif