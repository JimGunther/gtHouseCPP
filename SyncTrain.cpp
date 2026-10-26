#include "config.h"
#include <mysql/mysql.h>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <map>
#include <cstring>

/*********************************************************************************************************************************
* SyncTrain.cpp: utility app for Sentinel project. Reads the contents of "Trained.txt" and synchronizes the contents with the 
* MachTrain database table
* Version 0.7
* Last updated 26/10/2026 16:10
* Author: Jim Gunther
*********************************************************************************************************************************/

#define NUM_COLS 7
#define PATH "/home/mis/Documents/Sentinel/CPP/"

// Global variables
std::string _userid, _passwd, _dBase, _errMsg;
std::map<std::string, int> machLookup;
std::vector<trained> machTrained;
int memID, machNo, errorCount = 0;
MYSQL* _myConn;
MYSQL_RES* _result;

bool dbBegin() { // initiates MySQL database connection
    int status = mysql_library_init(0, NULL, NULL);
	_userid = "mis";
	_passwd = "mispwd";
	_dBase = "Sentinel";
	return (status == 0);
}

bool openDBConnection() { // opens MySQL connection
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

bool preFetch(std::string sql, bool store) { // common MySQL code befores fetching database data
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

bool changeData(std::string sql) {  // helper method for all "Data change" MySQL queries
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
	// shedderExists(): checks if a matching entry to memID exists in Shedders table
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
	std::ifstream fLookup("/home/mis/Documents/Sentinel/CPP/Machines.csv");
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

int readOldMaxSeq() { // method to calculate the maximum sequence number - known to be the last value as previous files validated (seq in ascending order)
	try {
		std::string line, sq;
		size_t ix;
		int rv = 0;
		std::ifstream fOld("Trained.txt");
		if (fOld.is_open()) {
			while (std::getline(fOld, line)) {
				ix = line.find(",");
				if (ix < 0) return 0;
				sq = line.substr(0, ix); // we know format is OK
				rv = std::stoi(sq);
			}
			fOld.close();
			return rv;
		}
		else {
			std::cout << "Failed to open Trained.txt file" << std::endl;
			return 0;
		}
	}
	catch (...) {
        std::cout << "Error in reading Trained.txt file" << std::endl;
        return 0;
	}
	return 0;
}

void addLogEntry(std::string prefix, bool bError, bool bNoWarn, std::string text) {
	if (bError || !bNoWarn) std::cout << prefix;
	if (bError) {
		errorCount++;
		std::cout << "Error: ";
	}
	else if (!bNoWarn) std::cout << "Warn:  ";
	if (bError || !bNoWarn) std::cout << text << std::endl;
}

bool isNumber(std::string &s) {
	for (char ch : s) {
		if (!isdigit(ch)) return false;
	}
	return true;
}

int validate(std::string entries, int numPrev, bool bNoWarn) {
	// validate(): method to validate any new "Trained.txt" entries before adding to MachTrained database table
	std::vector<std::string> vect;
	std::stringstream ss(entries);
	std::string itm, itmTrim;
	std::string abbrev, entriesClean;
	int len, maxSeq = numPrev;
	entriesClean = entries.substr(0, entries.length() - 1);
	
	// split entries into vector of strings
	while (getline(ss, itm, ',')) {
		vect.push_back(itm);
	}
		
	
	// Lookup machine number at the start
	len = vect[3].length();
	itmTrim = vect[3].substr(1, len - 2);
	if (machLookup.find(itmTrim) == machLookup.end()) machNo = 0; // no such key
	else machNo = machLookup[itmTrim];
	abbrev = vect[0] + ": \t[" + vect[1] + "," + std::to_string(machNo) + "]: \t";
	if (vect.size() != NUM_COLS) {
		addLogEntry(abbrev, true, bNoWarn, "Wrong number of CSV entries: " + std::to_string(NUM_COLS) + " expected; " + std::to_string(vect.size()) + " found.");
		return 0;
	}

	int i, nVal;
	for (i = 0; i < NUM_COLS; i++) {
		itm = vect[i];
		switch (i) {
			case 0: // sequence number
				if (!isNumber(itm)) {
					addLogEntry(abbrev, true, bNoWarn, entriesClean + " sequence format error");					
					return 0;
				}
				nVal = std::stoi(itm);
				if (nVal > maxSeq) {
					if (nVal > maxSeq + 1) addLogEntry(abbrev, false, bNoWarn, itm + " is not next number"); 
					maxSeq = nVal;
				}
				else {
					addLogEntry(abbrev, true, bNoWarn, itm + " is less than previous entry");
					return 0;
				}
				break;
			case 1: // member ID
				if (!isNumber(itm)) {
					addLogEntry(abbrev, true, bNoWarn, itm + " is not numeric");
					return 0;
				}
				memID = std::stoi(itm); // memID is global variable
				if (!shedderExists(memID)) {
					addLogEntry(abbrev, true, bNoWarn, "member no. " + itm + " does not exist");
					return 0;
				}				
				break;
			case 2: // "Good"
				if (itm != "\"Good\"") addLogEntry(abbrev, false, bNoWarn, "is " + itm + " not \"Good\"");
				break;
			case 3: // machine name
				if (machNo == 0) { // no such key
					addLogEntry(abbrev, true, bNoWarn, "machine: " + itm + " does not exist in lookup table");
					return 0;
				}			
				break;
			case 4: // trainer
				if (!isNumber(itm)) {
					addLogEntry(abbrev, true, bNoWarn, itm + "is not numeric");
					return 0;
				}
				if (!shedderExists(stoi(itm))) {
					addLogEntry(abbrev, false, bNoWarn, "trainer " + itm + " does not exist");
				}
				break;
			case 5: // boolean
				if ((itm != "0") && (itm != "1")) {
					addLogEntry(abbrev, true, bNoWarn, itm + " is not boolean");
					return 0;
				}
				break;
			case 6: // datetime "trained"
				struct tm tm;
				bool b = strptime(itm.c_str(), "%d/%m/%Y %H:%M:%S", &tm);
				if (!b) {
					addLogEntry(abbrev, true, bNoWarn, "date/time format invalid");
					return 0;
				}
				break;
		}
	}
	return maxSeq;
}

bool readTrained(std::string fileName, bool bNoWarn) { 
    //readTrained(): provisional method to read the machine "trained" permissions file and store the results in a vector of struct trained
    // This function checks first if the file has been changed since last read.
	// Parameters: none
	// returns: bool: true if successfully read and updated
    
	struct trained t;
	// Read new file with machine "trained" permissions
    try {
		std::string sPath = PATH;
		//std::cout << sPath << std::endl;
		std::ifstream fTrained(sPath + fileName);
		int sq = 0, prevCount = 0, newCount = 0; 
		std::vector<trained> mt;
		std::string line;
		if (fTrained.is_open()) {
			while (std::getline(fTrained, line)) {
				sq = validate(line, sq, bNoWarn);
//				std::cout << sq << std::endl;
				if (sq > 0) { // line is valid
					if (exists(memID, machNo)) prevCount++;
					else {
						trained t;
						t.mchNo = machNo;
						t.memID = memID;
						mt.push_back(t);
					}
				}
			}
			addLogEntry("", false, false, std::to_string(prevCount) + " entries already exist");
			if (errorCount > 0) {
				addLogEntry("", false, false, "Please correct errors in " + std::to_string(errorCount) + " entries and try again");
			}
			else {
				for (const trained& t : mt) {
					addMachTrain(t.memID, t.mchNo);
					newCount++;
				}
				addLogEntry("", false, false, std::to_string(newCount) + " new entries added");
			}
			fTrained.close();
			return true;
		}
		else {
			std::cout << "Failed to open " << fileName << " file" << std::endl;
			return false;
		}
	}
    catch (std::exception &ex) {
		std::cout << ex.what() << std::endl;
        std::cout << "Error in reading " << fileName << " file" << std::endl;
        return false;
	}
}

int main(int argc, char* argv[]) {
	std::string fileName;
	bool bNoWarn = false;
	if (argc > 1) {
		fileName = std::string(argv[1]);
		if (argc > 2) {
			bNoWarn = (strcmp(argv[2], "noWarn") == 0);
		}
	}
	else fileName = "Trained.txt";
	readMachLookup();
	bool dbOK = dbBegin();
	if (!dbOK) {
		std::cout << "Error opening database library." << std::endl;
		return 1;
	}
	bool b = readTrained(fileName, bNoWarn);
	return 0;
}
