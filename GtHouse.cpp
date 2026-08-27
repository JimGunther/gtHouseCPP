#include <ctime>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <format>
#include <vector>
#include <map>
#include <thread>
#include <cstdlib>
#include <mqtt/async_client.h>

#include "config.h"
#include "theDB.h"
#include "qutie.h"


/*********************************************************************************************************************************
* GtHouse.cpp main CPP file for Sentinel project. Acts as intermediary between Sentry RFID terminals (using Wifi/MQTT) and the
* Sentinel database, running on the same RPi on the MariaDB database server: now with added support for machines!
* Version 0.8
* Last updated 27/08/2026 11:02
* Author: Jim Gunther
*********************************************************************************************************************************/
// Global variables (?)
const std::string svr_uri = "mqtt://localhost:1883";
const std::string client_id = "SentinelPi";
	
theDB db;
Qutie qt;
mqtt::async_client cli(svr_uri, client_id);

std::time_t ttNow = std::time(0);
struct tm* dt = gmtime(&ttNow);
int prevHour = dt->tm_hour;
std::time_t ttFire = ttNow + 99 * 24 * 3600; // time until fire timeout as epoch (time_t)
int prevDay = 0;
extern bool touchReceived;
extern bool machTouched;
extern std::string sender, icMessText;
std::chrono::time_point<std::filesystem::__file_clock> ttTrained = std::filesystem::__file_clock::now();
std::vector<std::string> prevCommands;
//std::vector<std::string> prevMessages;
std::map<std::string, int> machLookup;
std::vector<trained> machTrained;

// HELPER FUNCTIONS FOR STARTUP======================================================================================================

void setupCmds() {
    std::vector<sentry> rfids = qt.getRFIDs();
    for (sentry row : rfids) {
        prevCommands.push_back("ZZZ"); // start with a dummy value
		//prevMessages.push_back(row.LCDText);
	}
}

// HELPER FUNCTIONS FOR loopGH===================================================================================================

void handleIncomingTouch() {
    //handleIncomingTouch(): method called every time a fob touches the RFID screen - via MQTT from a Sentry
    //parameters: none
    //returns void, but publishes a reply on MQTT 
    
    if (!touchReceived && !machTouched) return;
    
    std::string fobuid = icMessText.substr(0, 8);// ASSUMES UID IS 8 CHARACTERS LONG
	std::string reply;

    if (machTouched) {
		machTouched = false;
		shedder s = db.matchShedder(fobuid);
        int machNo = std::stoi(icMessText.substr(10));    // ASSUMES UID IS 8 CHARACTERS LONG FOLLOWED BY ',M' and number
        bool matched = db.mtExists(s.memID, machNo);
		std::string yn;
        if (matched) yn = ",Y";
        else yn = ",N";
        reply = fobuid + yn;
        qt.postStuff(sender + "OUT", reply); // for machine authorisation "sender" is actually i/c topic minus "IN" at the end
        return;
	}
    
	touchReceived = false;
	bool isHex = true; // Check it's all hexadecimal
	for (char c : fobuid) {
		if (!std::isxdigit(c)) isHex = false;
	}
    db.logTouch(fobuid);
    if (fobuid == EMF) { // Emergency Fire Fob
        // Line below is my best guess at syntax: CHECK WITH RICHARD!
        system("/home/MIS/Sentinel/bin/InNow.sh");
        qt.messageAllSentries("Fire alarm from " + sender);
        db.logMessage("Fire alarm from " + sender);
		std::time_t ttNow = std::time(NULL);
		ttFire = ttNow + 3 * 3600; // time until fire timeout (3 hours)
        return;
	}
    
    // "Normal" fob in/out fgrom here...
	shedder mem = db.matchShedder(fobuid);
    if (mem.memID > 0) {  // matching fob in database: must be visit (in or out)
        if (db.isInShed(mem.memID)) {   // must be exit
            db.addExit(mem.memID);
            std::string reply = fobuid + ":B:" + mem.givenName;
		}
        else {
            db.addVisit(mem.memID);
            std::string reply = fobuid + ":W:" + mem.givenName;
		}
	}
    else { // no match: add to Fob Store unless already in use
        reply = fobuid + ":N";
        if (!db.isAPerm(fobuid)) {
            bool ok = db.addFob(fobuid); //false if already in fob store
            if (!ok) db.logMessage(fobuid + " is already in Fob Store");
        }
		else db.logMessage(fobuid + " is already in use");
	}

    qt.postStuff(sender + "/OUT", reply); // always reply to RFID (even if adding to fob store)
}

void onTimeout() {
    std::time_t ttNow = std::time(0);
	ttFire = ttNow + 99 * 24 *3600;
    qt.messageAllSentries("MiS Tag Identity");
    db.logMessage("MiS Tag Identity");
}
/*
void forcees() { // runs dailyReport()
    //forcees(): now just runs daily report and checks for Traioned.txt changed
    //parameters: none
    //returns: none, but calls dailyReport method just before return
	std::time_t now = std::time(0);
	struct tm* dt = gmtime(&now);
    int currHour = dt->tm_hour;
    //if ((currHour != prevHour) && (currHour == FORCEE_HR)) {
        //dailyReport();
        //readTrained(false);
	//}
    prevHour = currHour;
}

void dailyReport() {
    //dailyReport(): creates text file of daily visitors and those who didn't log out
    //parameters: none
    //returns: none
    
	// First, construct name of file to create
	std::string dir = "../Reports/";	
	std::time_t now = std::time(0);
	struct tm* dt = gmtime(&now);
	char buf[22];
	strftime(buf, 22, "D%Y-%m-%d.txt", dt);
    std::string fName = dir + std::string(buf);
	std::cout << fName << std::endl;

	// Now, open new file and write to it
    
	std::ofstream f(fName);
	std::string tdy = fName.substr(1, 10);

    f << "Men In Sheds Bedford: Daily Report for " + tdy + "\n";
    f << "================================================\n\n";
    f << "Visitors Today:\n";
    f << "Name------------ID-In----Out--------------------\n";
    std::vector<visitor> visitors;// = db.listTodaysAll();
    for (visitor v : visitors) { 
        f << std::setw(16) << v.fullName;
		f << std::setw(6) << v.startTime;
		f << std::setw(6) << v.endTime;
		f << std::endl;
	}
 	f << std::endl << std::endl;
	
    f << "Auto Checkouts Today\n:";
    f << "Name--------------------------------------------\n";
    std::vector<std::string> forcees;// = db.listTodaysForcees();
    for (std::string fe : forcees) {
        f << fe[0] << std::endl;
	}
	f << std::endl << std::endl;
    f << "People with Day Fobs:\n";
    f << "FobID--Name--------------------------------------\n";
	std::vector<dayFobber> dayFobbers = db.dayFobbers();
    for (dayFobber df : dayFobbers) {
        f << df.fobuid << " " << df.fullName;
	}
    f << std::endl << std::endl;
	strftime(buf, 22, "%A, %d %B %Y %H:%M", dt);
    f << "Report created " << std::string(buf) << std::endl;	
}
*/

void monthlyReport(){
    //monthlyReport(): creates a report as CSV file of daily attenders in calendar format
    //parameters: none (but code assumes it is run on 1st of each month)
    //returns: none
	std::time_t ttNow = std::time(0);
	struct tm* dt = gmtime(&ttNow);
	int today = dt->tm_mday;
    assert(today == 1);// "Today is not 1st of Month"
   
    // First, construct name of file to create
    std::string dir = "../Reports/";
	std::time_t tt7 = ttNow - 7 * 24 * 3600; // 7 days ago
    struct tm* dt2 = gmtime(&tt7); // report runs on 1st of next month, so any day "safely" last month
	char buf[22];
	strftime(buf, 22, "%Y-%m", dt2);
	std::string sdt = std::string(buf);
    std::string fName = dir + "M" + sdt + ".txt";
    
    int tbl[6][7];
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 7; j++) tbl[i][j] = 0; // initialize 2-d array
	}

    // calculate offset in first row from day of week for 1st of last month
	std::string sdtM = sdt + "-01";
	struct tm tmF;
	strptime(sdtM.c_str(), "%Y-%m-%d", &tmF); // tmF contains tm strct for 1st of month
	std::time_t first = mktime(&tmF); 
    int wd = tmF.tm_wday;
    int adjust = (wd + 6) % 7;
    std::time_t ttMon = first - 7 * 24 * 3600;
	struct tm* dtMon = gmtime(&ttMon); // dtMon points to struct tm for Monday in first week of the (previous) month

    std::vector<mthVisit> visits = db.monthlyVisits();
    
    int x, y, n;
	// Populate tbl calendar-fashion
    for (mthVisit v : visits) {
        n = v.dom + adjust;   // n is absolute position in table; convert to x,y
        y = (int)(n / 7);
        x = n % 7;
        tbl[y][x] = v.countVisits;
	}

    // Now write to csv file
	int count, sum;
	float ave;
	std::string line;
    std::ofstream f(fName);
	std::stringstream ss;
    f << sdt << std::endl;
    f << "w/c,Mon,Tue,Wed,Thu,Fri,Sat,Sun,Ave" << std::endl; //header row
    for (y = 0; y < 6; y++) {
        line = "";
        if (ttMon < ttNow) {    // don't print 6th row if not needed
			strftime(buf, 22, "%d/%m", dtMon);
			f << std::string(buf);;
            count = 0;
            sum = 0;
            for (x = 0; x < 7; x++) {
                f << ", " << std::to_string(tbl[y][x]);
                if (tbl[y][x] > 0) count++;
                sum += tbl[y][x];
			}
            if (count == 0) ave = 0.0;
            else ave = sum / count;
			ss << std::fixed << std::setprecision(1) << ave;
            line += "," + ss.str();
            f << line << std::endl;
		}
		ttMon += 7 * 24 * 3600;
        dtMon = gmtime(&ttMon); // move on one week
	}
    
	// Add "averages" row
	line="Ave";
    int count2 = 0;
    int sum2 = 0;
	float ave2;
    for (x = 0; x < 7; x++) {  // column averages
        count = 0;
        sum = 0;
        for (y = 0; y < 6; y++) { //# 6th row may be all zeros (ok)
            if (tbl[y][x] > 0) {
                count++;
                count2++;
			}
            sum += tbl[y][x];
            sum2 += tbl[y][x];
		}
        if (count == 0) ave = 0.0;
        else ave = sum / count;
		ss << std::fixed << std::setprecision(1) << ave;
        line += "," + ss.str();
	}
    if (count2 == 0) ave2 = 0.0;
    else ave2 = sum2 / count2;
	ss << std::fixed << std::setprecision(1) << ave2;
    line += "," + ss.str();
    f << line << std::endl;
    f.close();
}

void monthlyVisitors() {
    //monthlyVisitors(): creates a report as CSV file of daily attenders in list format, including names
    //parameters: none (but code assumes it is run on 1st of each month)
    //returns: none
	
	std::time_t ttNow = std::time(0);
	struct tm* dt = gmtime(&ttNow);
	int today = dt->tm_mday;
    assert(today == 1);// "Today is not 1st of Month"
    
    // First, construct name of file to create
    std::string dir = "../Reports/";
	std::time_t tt7 = ttNow - 7 * 24 * 3600; // 7 days ago
    struct tm* dt2 = gmtime(&tt7); // report runs on 1st of next month, so any day "safely" last month
	char buf[22];
	strftime(buf, 22, "%Y-%m", dt2);
	std::string sdt = std::string(buf);
    std::string fName = dir + "M" + sdt + ".csv";
    fName = dir + "V" + sdt + ".csv";

    std::vector<mthVisitor> visitors = db.monthVisitors();

    // Now write to csv file
    std::ofstream f(fName);
    f << "M" << sdt << ".csv" << std::endl;
    f << "Name, ID, fobUID, Date" << std::endl;
	std::string line;
    for (mthVisitor vis : visitors) {
        line = std::string(vis.fullName) + "," + std::to_string(vis.id) + "," + std::string(vis.fobuid) + "," + std::string(vis.startTime);
        f << line << std::endl;
	}
    f.close();
}

// creates monthly report file on 1st day of month at midnight
void checkMonthly() {
    //checkMonthly(): method to run monthly report at midnight on 1st day of month (checks for this)
    //parameters: none
    //returns: none
	std::time_t now = std::time(0);
	struct tm* dt = gmtime(&now);
    int dy = dt->tm_mday;
    if (dy != prevDay) {
        if (dy == 1) {
            monthlyReport();
            monthlyVisitors();
		}
	}
    prevDay = dy;
}
// =================================================================================================================

void checkForCommands() {
    //checkForCommands: handles commands from administrator to be carried out by Sentry terminals
    //parameters: none
    //returns: none
    std::vector<sentry> results = db.listTerminals();
    int i = 0;
    for (sentry row : results) {
        std::string cmd = row.LCDText;
        if (!(cmd == prevCommands[i])) {
            //print(cmd)
            prevCommands[i] = cmd;
            std::string target = row.name;
            if (cmd == "RST") qt.rebootSentry(target);
            //if (cmd == "LCD") qt.switchLCD(cmd);
            //if ((cmd.substr(0:3) == "RED") || (cmd.substr(0:3) == "GRN")) qt.setIntvl(cmd);
        }
		i++;
	}
    db.resetCommand();
}
/*
void checkForMessages() {
    //checkForMessages: handles admin messages from website to be passed on to Sentry terminals
    //  parameters: none
    // returns: none
    vector<sentry> results = db.listTerminals();
    int i = 0;
    for (sentry: row : results) {
        std::string mess = row[1];
        if (mess != prevMessages[i]) {
            prevMessages[i] = mess;
            std::string target = row[0];
            if (target == "ALL") {
                qt.messageAllSentries(mess);
                return;
			}
            qt.messageSentry(target, mess);
        i++;
		}
	}
}
*/
    
// ====================================================================================

bool setup() {
	bool dbOK = db.begin();
	if (!dbOK) {
		std::cout << "Error opening database library." << std::endl;
		return false;
	}
	qt.init(&db, &cli);
    setupCmds();
    std::cout << "**Setup done**" << std::endl;
	return true;
}

void loopGH() {
    while (true) {
        handleIncomingTouch();
        //forcees(); // runs at set time: currently 18:00: NOT NEEDED? All done by Richard's code controlled by cron??
        checkMonthly();
        checkForCommands();
        //checkForMessages();
        if (std::time(0) > ttFire) onTimeout();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
		std::cout << "loop" << std::endl;
	}	
}

int main() {
	if (setup()) loopGH();
	return 0;
}

        