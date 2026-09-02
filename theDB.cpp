#include "config.h"
#include "theDB.h"
#include <iostream>
#include <cstring>

/*********************************************************************************************************************************
* theDB.cpp: support CPP file for Sentinel project. Handles the Functionality of data transfer using SQL between the code and
* the Sentinel database
* Version 0.6
* Last updated 27/08/2026 10:35
* Author: Jim Gunther
*********************************************************************************************************************************/

theDB::theDB() { };

bool theDB::begin() {
    int status = mysql_library_init(0, NULL, NULL);
	_userid = "mis";
	_passwd = "mispwd";
	_dBase = "Sentinel";
	_emFob = "ce864161"; // Query: are there 2 email fobs? CHECK!
	return (status == 0);
}

bool theDB::openConnection() {
    _myConn = mysql_init(NULL);
	//std::cout << "mysql open: " << typeid(_myConn).name() << std::endl;
    if (mysql_real_connect( _myConn, "localhost", _userid.c_str(), _passwd.c_str(), _dBase.c_str(), 0, NULL, 0 ) == nullptr)
    {
		_errMsg = "Open connection failed.";
        mysql_close(_myConn);
        return false;
    }
	else return true;	
}

bool theDB::preFetch(std::string sql, bool store) {
	int status = mysql_real_query(_myConn, sql.c_str(), sql.length());
    if (status > 2) {
		std::string sErr(mysql_error(_myConn));
        _errMsg = sErr;
		mysql_close(_myConn);
		return false;
    }
	if (store) _result = mysql_store_result(_myConn);
	else _result = mysql_use_result(_myConn);
	return true;
}

bool theDB::changeData(std::string sql) {
	if (!openConnection()) return false;
	int status = mysql_real_query(_myConn, sql.c_str(), sql.length());
	if (status != 0) {
		std::string s(mysql_error(_myConn));
        _errMsg = s;
		mysql_close(_myConn);
		return false;
	}
	int nRows = mysql_affected_rows(_myConn);
	std::cout << "No. of rows affected: " << nRows << std::endl;
	mysql_close(_myConn);
    return true;
}

// Methods for writing to / reading from data tables

// 0. Log all fob touches
bool theDB::logTouch(std::string fobUID) {
    //*logTouch(): method to log every fob touch
    //parameter: fobUID: string : UID of fob just touched
    //returns: bool: true if successful
    std::string sql = "INSERT INTO TouchLog (dtLog, FobUID) VALUES (NOW(), " + fobUID + ")";
    return changeData(fobUID);
}

// 1. Read IDs from Shedders table to validate fob and get given name
shedder theDB::matchShedder(std::string fobUIDString) {
    //matchShedder(): finds a shedder whose fob UID matches
    //parameter: fobUIDString: string representation of fon hexadecimal UID number
    //returns: shedder struct(ID, given name) if match found, ID == 0 otherwise
    std::string sql = "SELECT ID, GivenName FROM Shedders WHERE EndDate IS NULL AND FobUID = '" + fobUIDString + "'";
	shedder s;
	s.memID = 0;
	if (!openConnection() || !preFetch(sql, true) || (mysql_num_fields(_result) == 0)) {
		mysql_close(_myConn);
		return s;
	}
    
	MYSQL_ROW row = mysql_fetch_row(_result);
    s.memID = atoi(row[0]);
	s.givenName = std::string(row[1]);
	std::cout << s.memID << s.givenName << std::endl;
	mysql_free_result(_result);
	mysql_close(_myConn);
	
	return s;
 }
        
// 1A. Get fobuid from membership number
std::string theDB::fobFromNo(int memNo) {
	//fobFromNo(): gets a shedder's fobUID from their membership ID
    //parameter: memNo: int: shedder's unique membership number
    //returns: str: the matching fob UID string
    std::string sql = "SELECT FobUID FROM Shedders WHERE ID = " + std::to_string(memNo);
	if (!openConnection() || !preFetch(sql, true) || (mysql_num_fields(_result) == 0)) {
		mysql_close(_myConn);
		return "";
	}
    MYSQL_ROW row = mysql_fetch_row(_result);
	if (row == nullptr) return std::string("NO-ONE");
	if (row[0] == nullptr) return std::string("NULL");
	return std::string(row[0]);
}

// 1B. Does a matching entry exist in MachTrain table?
bool theDB::mtExists(int memID, int mchNo) {
	// mtExists(): checks if a matching entry to (memID, mchNo) exists in MachTain table
	//parameters:
	//	memID: int: member's ID number
	//	mchNo: int: unique machine number
	// returns true if already in table, otherwise false
	std::string sql = "SELECT COUNT(IDShedder) AS MatchCount FROM MachTrain WHERE IDShedder = " + std::to_string(memID) + " AND IDMachine = " + std::to_string(mchNo);
	int n;
	if (openConnection() && preFetch(sql, true)) {
		MYSQL_ROW row = mysql_fetch_row(_result);
		n = atoi(row[0]);
		mysql_free_result(_result);
		mysql_close(_myConn);
		return n > 0;
	}
	return false;
}


// 2. Is person in or out of shed?
bool theDB::isInShed(int memID) {
    //isInShed(): checks whether shedder is currently signed in
    //parameter: memID: int: unique id for shedder
    //returns: bool: true if signed in, false otherwise
    std::string sql = "SELECT ShedderID FROM Visits WHERE EndTime IS NULL AND ShedderID = " + std::to_string(memID);
	if (!openConnection()) return false;
	if (!preFetch(sql, true)) return false;
	MYSQL_ROW row = mysql_fetch_row(_result);
	mysql_free_result(_result);
	mysql_close(_myConn);
	return row != NULL;
}
   
// 3. Add entry to Visits table
bool theDB::addVisit(int memID) {
    //addVisit(): adds a visit record, start time now, for given user
    //parameter: shedder ID : int
    //returns: bool: true if successfully added, else false
    std::string sql = "INSERT INTO Visits (StartTime, ShedderID) VALUES (NOW(), " + std::to_string(memID) + ")";
	std::cout << sql << std::endl;
    return changeData(sql);
}

// 4. Modify visits table on exit
bool theDB::addExit(int memID) {
    //addExit(): adds an exit time to shedder visit record
    //parameters:
    //shedderID: int; shedder's unique ID (NOT fob UID)
    //returns: bool: True if successfully updated, False otherwise
    std::string sql = "UPDATE Visits SET EndTime = NOW() WHERE ShedderID = " + std::to_string(memID) + " AND EndTime IS NULL";
	std::cout << sql << std::endl;
    return changeData(sql);
}

// 5. Add to message log: WE NEED TO DISCUSS WHAT GETS LOGGED!
bool theDB::logMessage(std::string text) {
    //logMessage: adds a message to log table
    //parameter: text: string: message text
    //returns: bool
    std::string sql = "INSERT INTO MessageLog (dtMessage, MessageText) VALUES (NOW(), '" + text + "')";
    return changeData(sql);
}

// 6. List today's visitors for report
std::vector<visitor> theDB::listTodaysAll() {
    //listTodaysAll(): returns list of shedders in today
    //parameters: none
    //returns: list of visitor structs
    
    std::vector<visitor> visitors;
    std::string sql = "SELECT GivenName, Surname, Id, Visits.StartTime, Visits.EndTime FROM Shedders INNER JOIN Visits On Shedders.Id = Visits.ShedderID WHERE Visits.StartTime > DATE_SUB(NOW(), INTERVAL 16 HOUR) ORDER BY Surname, GivenName";
	if (!openConnection()) return visitors;
	if (!preFetch(sql, false)) return visitors;

	MYSQL_ROW row;
	while(row = mysql_fetch_row(_result)) {
		unsigned long* lengths = mysql_fetch_lengths(_result); 
		visitor v;
		v.fullName = std::string(row[0]) + " " + std::string(row[1]);
		v.id = atoi(row[2]);
		v.startTime = std::string(row[3]).substr(11, 5);
		if (lengths[4] < 10) v.endTime = std::string("-----");
		else v.endTime = std::string(row[4]).substr(11, 5);	
		visitors.push_back(v);
	}
	mysql_free_result(_result);
    mysql_close(_myConn);
    return visitors;
}
   
// 7. List today's forced exits for report
std::vector<std::string> theDB::listTodaysForcees() {
    //listTodaysForcees(): lists those still not logged out today
    //parameters: none
    //returns: list of full names(given name + surname)
    std::vector<std::string> forcees;
    std::string sql = "SELECT CONCAT(GivenName, " ", Surname) AS FullName FROM Shedders INNER JOIN Visits on Shedders.ID = Visits.ShedderID WHERE (Visits.StartTime > DATE_SUB(NOW(), INTERVAL 16 HOUR)) AND (Visits.EndTime IS NULL) ORDER BY Surname, GivenName";
	if (!openConnection()) return forcees;
	if (!preFetch(sql, false)) return forcees;
	
	MYSQL_ROW row;
	while (row = mysql_fetch_row(_result)) {
		forcees.push_back(std::string(row[0]));
	}
	mysql_free_result(_result);
    mysql_close(_myConn);
    return forcees;
}
   
// 8. Add a newly detected Fob (for assignment by administrator)
bool theDB::addFob(std::string uid) {
    //addFob(): adds a new fob's UID to the "fob store" table
    //parameter: uid: string: new fob's UID
    //returns: boolean: True if successfully added, otherwise False
	
	// Check not already in Fob Store
    std::string sql = "SELECT FobUID FROM NewFobs WHERE FobUID = '" + uid + "'";
	if (!openConnection()) return false;
	if (!preFetch(sql, true)) return false;
	int nFields = mysql_num_fields(_result);
    if (nFields != 0) return false;
        
    // Check not already assigned to Shedder
    sql = "SELECT ID FROM Shedders WHERE FobUID = '" + uid + "'";
	if (!openConnection()) return false;
	if (!preFetch(sql, true)) return false;
	nFields = mysql_num_fields(_result);
	mysql_free_result(_result);
    if (nFields == 0) {
        sql = "INSERT INTO NewFobs (FobUID) VALUES('" + uid + "')";
        return changeData(sql);
	}
    else return false;
}

// 9. procedure for generating data for monthly report (count of visits by day) : runs on 1st of month only!
std::vector<mthVisit> theDB::monthlyVisits() {
    //monthlyVisits(): produces monthly stats in a list for conversion to CSV data
    //parameters: none
    //returns: list of tuples (visitor count, date)
	std::vector<mthVisit> mvs;
    std::string sql = "SELECT COUNT(StartTime), DAY(StartTime) as DoM FROM Visits WHERE StartTime > DATE_SUB(NOW(), INTERVAL 1 MONTH) GROUP BY DoM";
	if (!openConnection()) return mvs; // empty vector at this stage
	if (!preFetch(sql, false)) return mvs;

	MYSQL_ROW row;
	while(row = mysql_fetch_row(_result)) {
		mthVisit mv;
		mv.countVisits = atoi(row[0]);
		mv.dom = atoi(row[1]);
		mvs.push_back(mv);
	}
	mysql_free_result(_result);
    mysql_close(_myConn);
    return mvs;
}    

//10. procedure for listing terminals
    
std::vector<sentry> theDB::listTerminals() {
    //listTerminals(): produces list of terminal names as two-item tuples (name, LCDtext)
    //parameters: none
    //returns: list of sentry structs (terminal names, LCDTexts, commds)
  	std::string sql = "SELECT TerminalName, IsAdmin, LCDText, Commd FROM Terminals ORDER BY TerminalName";
	std::vector<sentry> rfids;
	rfids.clear();
	if (!openConnection()) return rfids; // empty vector at this stage
	if (!preFetch(sql, false)) return rfids;

	MYSQL_ROW row;
	while(row = mysql_fetch_row(_result)) {
		sentry s;
		s.name = std::string(row[0]);
		s.isAdmin = atoi(row[1]) == 1;
		s.LCDText = std::string(row[2]);
		s.commd = std::string(row[3]);
		rfids.push_back(s);
	}
	mysql_free_result(_result);
    mysql_close(_myConn);
    		
    return rfids;
}

// #11. procedure to reset the Commd field in Terminals table
bool theDB::resetCommand() {
    //resetCommand(): resets Terminal command after notifying RFID
    //parameters: none
    //returns: bool: true if OK
    std::string sql = "UPDATE Terminals SET Commd = 'XXX' WHERE Commd != 'XXX' ";
    return changeData(sql);
}

    
// 12. procedure for checking if fob UID is in use
bool theDB::isAPerm(std::string fobUID) {
    //isAPerm: checks if uid is in use in PrmUID column of Shedders table
    //parameter: fobUID: str: fobUID to check
    //returns: bool: true if found
    std::string sql = "SELECT PrmUID FROM Shedders WHERE (EndDate IS NULL) AND (PrmUID IS NOT NULL) AND (PrmUID = '" + fobUID + "')";
	if (!openConnection()) return false;
	if (!preFetch(sql, true)) return false;
	int nFields = mysql_num_fields(_result);
	mysql_free_result(_result);
    mysql_close(_myConn);
    return (nFields > 0);
}

// 13. procedure to list "DayFobbers" for daily report
std::vector<dayFobber> theDB::dayFobbers() {   
    //dayFobbers(): list those who forgot their fobs and use a "day fob"
    //parameters: none
    //returns: vector of dayFobber structs
	std::vector<dayFobber> dfs;
    std::string sql = "SELECT CONCAT(GivenName, ' ', Surname) AS FullName, FobUID FROM Shedders WHERE PrmUID IS NOT NULL ORDER BY Surname, GivenName";
	if (!openConnection()) return dfs;
	if (!preFetch(sql, false)) return dfs;
	
	MYSQL_ROW row;
	while (row = mysql_fetch_row(_result)) {
		dayFobber df;
		df.fullName = std::string(row[0]);
        df.fobuid = std::string(row[1]);
		dfs.push_back(df);
	}
	mysql_free_result(_result);
    mysql_close(_myConn);
    return dfs;
}
        
// 14. procedure to get monthly visitors data
std::vector<mthVisitor> theDB::monthVisitors() {
    //monthVisitors() list with names of all visitors in the past month
    //parameters: none
    //returns: vector of mthVisit structs
	std::vector<mthVisitor> mvs;
    std::string sql = "SELECT CONCAT(Shedders.GivenName, ' ', Shedders.Surname) AS FullName, Shedders.ID, Shedders.FobUID, Visits.StartTime ";
    sql += "FROM Shedders INNER JOIN Visits ON Shedders.ID = Visits.ShedderID ";
    sql += "WHERE Visits.StartTime > DATE_SUB(NOW(), INTERVAL 1 MONTH) ORDER BY Visits.StartTime";
	if (!openConnection()) return mvs;
	if (!preFetch(sql, false)) return mvs;
	
	MYSQL_ROW row;
	while (row = mysql_fetch_row(_result)) {
		mthVisitor mv;
		mv.fullName = std::string(row[0]);
		mv.id = atoi(row[1]);
		mv.fobuid = std::string(row[2]);
		mv.startTime = std::string(row[3]);
		mvs.push_back(mv);
	}
	mysql_free_result(_result);
    mysql_close(_myConn);
    return mvs;
}

			