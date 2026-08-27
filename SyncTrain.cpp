#include "config.h"
#include <mysql/mysql.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>

/*********************************************************************************************************************************
* SyncTrain.cpp: utility app for Sentinel project. Reads the contents of "Trained.txt" and synchronizes the contents wit the MachTrain database table
* Version 0.5
* Last updated 27/08/2026 12:27
* Author: Jim Gunther
*********************************************************************************************************************************/

// Global variables
std::string _userid, _passwd, _dBase, _errMsg;
std::map<std::string, int> machLookup;
std::vector<trained> machTrained;
MYSQL* _myConn;
MYSQL_RES* _result;

bool dbBegin() {
    int status = mysql_library_init(0, NULL, NULL);
	_userid = "mis";
	_passwd = "mispwd";
	_dBase = "Sentinel";
	return (status == 0);
}

bool openDBConnection() {
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

bool preFetch(std::string sql, bool store) {
	int status = mysql_real_query(_myConn, sql.c_str(), sql.length());
    if (status !=0) {
		std::string sErr(mysql_error(_myConn));
        _errMsg = sErr;
		mysql_close(_myConn);
		return false;
    }
	if (store) _result = mysql_store_result(_myConn);
	else _result = mysql_use_result(_myConn);
	return true;
}

bool changeData(std::string sql) {
	if (!openDBConnection()) return false;
	int status = mysql_real_query(_myConn, sql.c_str(), sql.length());
	if (status != 0) {
		std::string s(mysql_error(_myConn));
        _errMsg = s;
		std::cout << s << std::endl;
		mysql_close(_myConn);
		return false;
	}
	mysql_close(_myConn);
    return true;
}

bool exists(int memID, int mchNo) {
	// exists(): checks if a matching entry to (memID, mchNo) exists in MachTained table
	//parameters:
	//	memID: int: member's ID number
	//	mchNo: int: unique machine number
	// returns true if already in table, otherwise false
	std::string sql = "SELECT COUNT(IDShedder) AS MatchCount FROM MachTrain WHERE IDShedder = " + std::to_string(memID) + " AND IDMachine = " + std::to_string(mchNo);
	int n;
	if (openDBConnection() && preFetch(sql, true)) {
		MYSQL_ROW row = mysql_fetch_row(_result);
		n = atoi(row[0]);
		mysql_close(_myConn);
		return n > 0;
	}
	return false;
}

bool shedderExists(int memID) {
	// shedderExists(): checks if a matching entry to (memID, mchNo) exists in MachTained table
	//parameters:
	//	memID: int: member's ID number
	// returns true if already in table, otherwise false
	std::string sql = "SELECT COUNT(ID) AS MatchCount FROM Shedders WHERE ID = " + std::to_string(memID);
	int n;
	if (openDBConnection() && preFetch(sql, true)) {
		MYSQL_ROW row = mysql_fetch_row(_result);
		n = atoi(row[0]);
		mysql_close(_myConn);
		return n > 0;
	}
	return false;
}

bool addMachTrain(int memID, int mchNo) {
	//addMachTrain(): adds a row to the MachTrain table//parameters:
	//memID: int :shed member's ID number
	//mchNo: int: machine number
	// returns: true if added successfully, false otherwise
	std::string sql = "INSERT INTO MachTrain VALUES (" + std::to_string(memID) + ", " + std::to_string(mchNo) + ")";
	return changeData(sql);
}

 bool readMachLookup() {
    //readMachLookup(): reads machine numbers CSV and converts to a (key,value) map
	// Parameters: none
	// returns: bool: true if successfully read and updated
	std::ifstream fLookup("Machines.csv");
	std::string comma = ",";
	if (fLookup.is_open()) {
		std::string line, k;
		int val;
		std:size_t ix;
		while (std::getline(fLookup, line)) {
			ix = line.find(comma);
			k = line.substr(0, ix);
			val = stoi(line.substr(ix + 1));
			machLookup.insert({k, val});
		}
		fLookup.close();
	}
	return true;
}
    
bool readTrained() { 
    //readTrained(): provisional method to read the machine "trained" permissions file and store the results in a vector of struct trained
    // This function checks first if the file has been changed since last read.
	// Parameters: none
	// returns: bool: true if successfully read and updated
    
	struct trained t;
	// Read file with machine "trained" permissions
    try {
			std::ifstream fTrained("Trained.txt");
			std::string comma = ",", sid;
			size_t ix, ix2;
			std::vector<trained> mt;
			if (fTrained.is_open()) {
				std::string line, mch, s, fobID;
				int mNo, count = 0;
				while (std::getline(fTrained, line)) {
					//std::cout << line << std::endl;
					ix = line.find(comma);
					ix2 = line.find(comma, ix + 1);
					sid = line.substr(ix + 1, ix2 - ix - 1);
					ix = line.find(comma, ix2 + 1);
					ix2 = line.find(comma, ix + 1);
					mch = line.substr(ix + 1, ix2 - ix - 1);
					mch = mch.erase(0, 1);
					mch = mch.erase(mch.size() - 1);
					//std::cout << mch << ":" << sid << std::endl;
					mNo = machLookup[mch];
					t.memID = std::stoi(sid);
					t.mchNo = mNo;
					if (!exists(t.memID, t.mchNo)) {
						 if (shedderExists(t.memID)) {
							 if (addMachTrain(t.memID, t.mchNo)) count++;
						 }
						 else {
							std::cout << "Shedder no. " << t.memID << " is not in the Sentinel database" << std::endl;
						 }
					}
				}
				fTrained.close();
				std::cout << count << " records added." << std::endl;
				return true;
			}
			else {
				std::cout << "Failed to open Trained.txt file" << std::endl;
				return false;
			}
	}
    catch (...) {
        std::cout << "Error in reading Trained.txt file" << std::endl;
        return false;
	}
}

int main() {
	readMachLookup();
	bool dbOK = dbBegin();
	if (!dbOK) {
		std::cout << "Error opening database library." << std::endl;
		return 1;
	}
	bool b = readTrained();
	return 0;
}
