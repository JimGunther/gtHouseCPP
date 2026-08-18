//import os
#include "config.h"
#include "theDB.h"
#include <iostream>
//#include <typeinfo>
#include <cstring>

/*********************************************************************************************************************************
* theDB.cpp: support CPP file for Sentinel project. Handles the Functionality of data transfer using SQL between the code and
* the Sentinel database
* Version 0.3
* Last updated 18/08/2026 17:13
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
    if ( mysql_real_connect( _myConn, "localhost", _userid.c_str(), _passwd.c_str(), _dBase.c_str(), 0, NULL, 0 ) == nullptr)
    {
		_errMsg = "Open connection failed.";
        mysql_close(_myConn);
        return false;
    }
	else return true;	
}

bool theDB::preFetch(std::string sql, bool store) {
	int status = mysql_real_query(_myConn, sql.c_str(), sql.length());
    if (status !=0) {
		std::string sErr(mysql_error(_myConn));
        _errMsg = sErr;
		mysql_close();
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
		mysql_close();
		return false;
	}
	mysql_close();
    return true;
}

// Methods for writing to / reading from data tables

// 0. Log all fob touches
bool theDB::logTouch(std::string fobUID) {
    /*logTouch(): method to log every fob touch
    parameter: fobUID: string : UID of fob just touched
    returns: bool: true if successful
	*/
    sql = "INSERT INTO TouchLog (dtLog, FobUID) VALUES (NOW(), " + fobUID + ")";
    return changeData(fobUID);
}

// 1. Read IDs from Shedders table to validate fob and get given name
 shedder theDB::matchShedder(std::string fobUIDString) {
    /*matchShedder(): finds a shedder whose fob UID matches
    parameter: fobUIDString: string representation of fon hexadecimal UID number
    returns: shedder struct(ID, given name) if match found, ID == 0 otherwise
    */
    std::string sql = "SELECT ID, GivenName FROM Shedders WHERE EndDate IS NULL AND FobUID = '" + fobUIDString + "'";
	shedder s;
	s.memID = 0;
	if ((!openConnection()) || (!preFetch(sql), true) || (mysql_num_fields() == 0) {
		mysql_close();
		return s;
	}
    
	MYSQL_ROW row = mysql_fetch_row(_result);
    s.memID = atoi(row[0]);
	s.givenName = std::string(row[1]);
	mysql_close();
	
	return s;
 }
/*        
    # 1A. Get fobuid from membership number
    @classmethod
    def fobFromNo(cls, memNo: int) -> str:
        '''fobFromNo(): gets a shedder's fobUID from their membership ID
        parameter: memNo: int: shedder's unique membership number
        returns: str: the matching fob UID string
        '''
        sql = "SELECT FobUID FROM Shedders WHERE ID = " + str(memNo)
        result = cls.fetchOne(sql)
        if result is None:
            return "none"
        else:
            return result[0] # result is a tuple with one member
*/
// 2. Is person in or out of shed?
bool theDB::isInShed(int memID) {
    /*isInShed(): checks whether shedder is currently signed in
    parameter: memID: int: unique id for shedder
    returns: bool: true if signed in, false otherwise
    */
    std::string sql = "SELECT ShedderID FROM Visits WHERE EndTime IS NULL AND ShedderID = " + std::toString(memID);
	if (!openConnection()) return s;
	if (!preFetch(sql), true) return s;
	int nFields = mysql_num_fields();
	mysql_close();
    return nFields > 0;
}
   
// 3. Add entry to Visits table
bool theDB::addVisit(int memID) {
    /*addVisit(): adds a visit record, start time now, for given user
    parameter: shedder ID : int
    returns: bool: true if successfully added, else false
    */
    std::string sql = "INSERT INTO Visits (StartTime, ShedderID) VALUES (NOW(), " + std::toString(memID) + ")";
    return changeData(sql):
}

// 4. Modify visits table on exit
bool theDB::addExit(int memID) {
        /*addExit(): adds an exit time to shedder visit record
        parameters:
            shedderID: int; shedder's unique ID (NOT fob UID)
        returns: bool: True if successfully updated, False otherwise
        */
        sql = "UPDATE Visits SET EndTime = NOW() WHERE ShedderID = " + std::toString(memID) + " AND EndTime IS NULL";
        return changeData(sql);
}
/*
    # 5. Add to message log: WE NEED TO DISCUSS WHAT GETS LOGGED!
    # returns: none
    @classmethod
    def logMessage(cls, text : str) -> bool:
        '''logMessage: adds a message to log table
        parameter: text: string: message text
        returns: bool
        '''
        sql = "INSERT INTO MessageLog (dtMessage, MessageText) VALUES (NOW(), '" + text + "')"
        return cls.changeData(sql)

    # 6. List today's visitors for report
    @classmethod
    def listTodaysAll(cls) -> list:
        '''listTodaysAll(): returns list (by name) of shedders in today
        parameters: none
        returns: list of tuples: (given name, surname)
        '''
        visitors = []
        sql = "SELECT GivenName, Surname, Id, Visits.StartTime, Visits.EndTime FROM Shedders INNER JOIN Visits On Shedders.Id = Visits.ShedderID WHERE Visits.StartTime > DATE_SUB(NOW(), INTERVAL 16 HOUR) ORDER BY Surname, GivenName"
        results = cls.fetchAll(sql)
        if results is not None:
            for result in results:
                visitors.append(result)
        return visitors
    
    # 7. List today's forced exits for report
    @classmethod
    def listTodaysForcees(cls) -> list:
        '''listTodaysForcees(): lists those still not logged out today
        parameters: none
        returns: list of tuples(given name, surname)
        '''
        forcees = []
        sql = "SELECT GivenName, Surname FROM Shedders INNER JOIN Visits on Shedders.ID = Visits.ShedderID WHERE (Visits.StartTime > DATE_SUB(NOW(), INTERVAL 16 HOUR)) AND (Visits.EndTime IS NULL) ORDER BY Surname, GivenName"
        results = cls.fetchAll(sql)
        if results is not None:
            for result in results:
                forcees.append(result)
        return forcees
    
    # 8. Add a newly detected Fob (for assignment by administrator)
    @classmethod
    def addFob(cls, uid : str) -> bool:
        '''addFob(): adds a new fob's UID to the "fob store" table
        parameter: uid: string: new fob's UID
        returns: boolean: True if successfully added, otherwise False
        '''
        # Check not in Fob Store
        sql = "SELECT FobUID FROM NewFobs WHERE FobUID = '" + uid + "'"
        row = cls.fetchOne(sql)
        if row is not None:
            return False
        
        # Check not already assigned to Shedder
        sql = "SELECT ID FROM Shedders WHERE FobUID = '" + uid + "'"
        row = cls.fetchOne(sql)
        if row is None:
            sql = "INSERT INTO NewFobs (FobUID) VALUES('" + uid + "')"
            return cls.changeData(sql)
        else:
            return False

    # 9. procedure for generating data for monthly report (count of visits by day) : runs on 1st of month only!
    @classmethod
    def monthlyVisits(cls) -> list:
        '''monthlyVisits(): produces monthly stats in a list for conversion to CSV data
        parameters: none
        returns: list of tuples (visitor count, date)
        '''
        sql = "SELECT COUNT(StartTime), DAY(StartTime) as DoM FROM Visits WHERE StartTime > DATE_SUB(NOW(), INTERVAL 1 MONTH) GROUP BY DoM"
        results = cls.fetchAll(sql)
        if results is None:
            return []   # empty list
        else:
            return results
    */    

   //10. procedure for listing terminals
    
std::vector<sentry> theDB::listTerminals() {
    /*listTerminals(): produces list of terminal names as two-item tuples (name, LCDtext)
    parameters: none
    returns: list of sentry structs (terminal names, LCDTexts, commds)
    */
  	std::string sql = "SELECT TerminalName, IsAdmin, LCDText, Commd FROM Terminals ORDER BY TerminalName";
	std::vector<sentry> rfids;
	rfids.clear();
	if (!openConnection()) return rfids; // empty vector at this stage
	if (!preFetch(sql), false) return rfids;

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
    		
	//std::cout << "End ListTerminals()" << rfids.size() << std::endl;
    return rfids;
}
    /*
    #11. procedure to reset the Commd field in Terminals table
    @classmethod
    def resetCommand(cls) -> bool:
        '''resetCommand(): resets Terminal command after notifying RFID
        parameters: none
        returns: bool: True if OK
        '''
        sql = "UPDATE Terminals SET Commd = 'XXX' WHERE Commd != 'XXX' "
        return cls.changeData(sql)
*/    
// 12. procedure for checking if fob UID is in use
bool theDB::isAPerm(std::string fobUID) {
    /*isAPerm: checks if uid is in use in PrmUID column of Shedders table
    parameter: fobUID: str: fobUID to check
    returns: bool: true if found
    */
    sql = "SELECT PrmUID FROM Shedders WHERE (EndDate IS NULL) AND (PrmUID IS NOT NULL) AND (PrmUID = '" + fobUID + "')";
	if (!openConnection()) return false;
	if (!preFetch(sql), true) return false;
	int nFields = mysql_num_fields();
	mysql_close();
    return (nFields > 0);
}
/*
    #13. procedure to list "DayFobbers" for daily report
    @classmethod
    def dayFobbers(cls) -> list:   
        '''dayFobbers(): list those who forgot their fobs and use a "day fob"
        parameters: none
        returns: list of tuples'''
        sql = "SELECT CONCAT(GivenName, ' ', Surname) AS FullName, FobUID FROM Shedders WHERE PrmUID IS NOT NULL ORDER BY Surname, GivenName"
        df = cls.fetchAll(sql)
        if df is None:
            return []   # empty list
        else:
            return df
        
    #14. procedure to get monthly visitors data
    @classmethod
    def monthVisitors(cls) -> list:
        '''monthVisitors() list with names of all visitors in the past month
        parameters: none
        returns: list of tuples
        '''
        sql = "SELECT CONCAT(Shedders.GivenName, ' ', Shedders.Surname) AS FullName, Shedders.ID, Shedders.FobUID, Visits.StartTime "
        sql += "FROM Shedders INNER JOIN Visits ON Shedders.ID = Visits.ShedderID "
        sql += "WHERE Visits.StartTime > DATE_SUB(NOW(), INTERVAL 1 MONTH) ORDER BY Visits.StartTime"
        print (sql)
        mv = cls.fetchAll(sql)
        if mv is None:
            return []
        else:
            return mv
	*/
			