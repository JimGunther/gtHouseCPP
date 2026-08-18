#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>
#include <thread>
#include <cstdlib>
#include <mqtt/async_client.h>

//import string
#include "config.h"
#include "theDB.h"
#include "qutie.h"


/*********************************************************************************************************************************
* GtHouse.cpp main CPP file for Sentinel project. Acts as intermediary between Sentry RFID terminals (using Wifi/MQTT) and the
* Sentinel database, running on the same RPi on the MariaDB database server: now with added support for machines!
* Version 0.5
* Last updated 18/08/2026 15:07
* Author: Jim Gunther
*********************************************************************************************************************************/
// Global variables (?)
	//config.init()
const std::string svr_uri = "mqtt://localhost:1883";
const std::string client_id = "JimPi5";
	
theDB db;
Qutie qt;
mqtt::async_client cli(svr_uri, client_id);

std::time_t ttNow = std::time(NULL);
struct tm* dt = gmtime(&ttNow);
int prevHour = dt->tm_hour;
std::time_t ttFire = ttNow + 99 * 24 * 3600; // time until fire timeout as epoch (time_t)
int prevDay = 0;
extern bool touchReceived;
extern bool machTouched;
extern std:string sender, icMessText;
std::chrono::time_point<std::filesystem::__file_clock> ttTrained = std::filesystem::__file_clock::now();
std::vector<std::string> prevCommands;
std::vector<std::string> prevMessages;
std::vector<trained> machTrained;

void handleIncomingTouch() {
    /*handleIncomingTouch(): method called every time a fob touches the RFID screen - via MQTT from a Sentry
    parameters: none
    returns void, but publishes a reply on MQTT 
    */
    
    if (!touchReceived && !machTouched) return;
    
    std::string fobuid = icMessText.substr(0, 8);// ASSUMES UID IS 8 CHARACTERS LONG
    //config.touchReceived = False    # reply only once

    if (machTouched) {
		machTouched = false;
        int machNo = stoi(icMess.substr(10));    // ASSUMES UID IS 8 CHARACTERS LONG FOLLOWED BY ',M' and number
        bool matched = matchBoth(fobuid, machNo);
		std::string yn;
        if (matched) yn = ",Y";
        else yn = ",N";
        std::string reply = fobuid + yn;
        qt.postStuff(sender + "OUT", reply); // for machine authorisation "sender" is actually i/c topic minus "IN" at the end
        return;
	}
    
	touchReceived = false;
	bool isHex = true; // Check it's all hexadecimal
	for char (c : fobUID) {
		if (!std::isxdigit(c)) isHex = false;
	}
    theDB.logTouch(fobUID);
    if (fobUID == EMF) { // Emergency Fire Fob
        // Line below is my best guess at syntax: CHECK WITH RICHARD!
        system("/home/MIS/Sentinel/bin/InNow.sh");
        qt.messageAllSentries("Fire alarm from " + sender);
        theDB.logMessage("Fire alarm from " + sender);
		std::time_t ttNow = std::time(NULL);
		ttFire = ttNow + 3 * 3600; // time until fire timeout (3 hours)
        return;
	}
    
    // "Normal fob in/out fgrom here...
	shedder mem = theDB.matchShedder(fobUID);
    if (mem.memID > 0) {  // matching fob in database: must be visit (in or out)
        if (theDB.isInShed(mem.memID) {   // must be exit
            theDB.addExit(mem.memID);
            std::string reply = fobUID + ":B:" + mem.givenName;
		}
        else:
            theDB.addVisit(mem.memID);
            std::string reply = fobUID + ":W:" + mem.givenName;
    else: // no match: add to Fob Store unless already in use
        reply = fobUID + ":N";
        if (!theDB.isAPerm(fobUID)) {
            bool ok = theDB.addFob(fobUID); //false if already in fob store
            if (!ok) theDB.logMessage(fobUID + " is already in Fob Store");
        }
		else theDB.logMessage(fobUID + " is already in use");

    qt.postStuff(sender + "/OUT", reply); // always reply to RFID (even if adding to fob store)
}
/*
def onTimeout() -> None:
    global fireTimeout
    fireTimeout = datetime.now() + timedelta(days=99)
    qt.messageAllSentries("MiS Tag Identity" )
    theDB.logMessage("MiS Tag Identity")

def forcees() -> None:   # runs dailyReport()
    '''forceExits(): now just runs daily report
    parameters: none
    returns: none, but calls dailyReport method just before return
    '''
    global prevHour
    currHour = datetime.now().hour
    if (currHour != prevHour) and (currHour == config.forceeHour):
        dailyReport();
        readTrained(false);
    prevHour = currHour

def dailyReport() -> None:
    '''dailyReport(): creates text file of daily visitors and those who didn't log out
    parameters: none
    returns: none
    '''
    
    # First, construct name of file to create
    dir = "/home/MIS/Reports/Sentinel/"
    dt = datetime.now().strftime("%Y-%m-%d")
    fName = dir + "D" + dt + ".txt" 

    # Now, open new file and write to it
    f = open(fName, 'wt')
    print("Men In Sheds Bedford: Daily Report for " + dt, file=f)
    print("================================================\n\n", file=f)
    print("Visitors Today:", file=f)
    print("Name------------ID-In----Out--------------------\n", file=f)
    visitors = theDB.listTodaysAll()
    for v in visitors:
        fullName = v[0] + " " + v[1]
        inTime = v[3].strftime("%H:%M")
        if (v[4] is None):
            outTime = "-----"
        else:
            outTime = v[4].strftime("%H:%M")
        print(f'{fullName:<16} {v[2]:>3} {inTime:<6}{outTime:>6}', file=f)
    print("\n\n", file=f)
    print("Auto Checkouts Today:", file=f)
    print("Name--------------------------------------------\n", file=f)
    forcees = theDB.listTodaysForcees()
    for fe in forcees:
        print(fe[0] + " " + fe[1], file=f)
    print("\n\n", file=f)
    print("People with Day Fobs:", file=f)
    print("FobID--Name--------------------------------------\n", file=f)
    dayFobbers = theDB.dayFobbers()
    for df in dayFobbers:
        print(df[1] + " " + df[0], file=f)
    print("\n\n", file=f)
    print("Report created " + datetime.now().strftime("%A, %d %B %Y %H:%M"), file=f)

def monthlyReport() -> None:
    '''monthlyReport(): creates a report as CSV file of daily attenders in calendar format
    parameters: none (but code assumes it is run on 1st of each month)
    returns: none
    '''
    assert datetime.now().day == 1, "Today is not 1st of Month"
    
    # First, construct name of file to create
    dir = "/home/MIS/Reports/Sentinel/"
    td = timedelta(days=-7) # report runs on 1st of next month, so any day "safely" last month
    dt = (datetime.now() + td).strftime("%Y-%m")
    fName = dir + "M" + dt + ".csv"
    
    tbl = [[0]*7 for i in range(6)] ## initialize 2-d array

    # calculate offset in first row from day of week for 1st of last month
    first = datetime.fromisoformat(dt + "-01")
    wd = first.weekday()
    adjust = (wd + 6) % 7
    dtMon = first + timedelta(days=-wd)

    visits = theDB.monthlyVisits()
    
    # populate tbl calendar-fashion
    for v in visits:
        n = v[1] + adjust   # n is absolute position in table; convert to x,y
        y = (int)(n / 7)
        x = n % 7
        tbl[y][x] = v[0]

    # Now write to csv file
    f = open(fName, 'wt')
    print(dt, file=f)
    print("w/c,Mon,Tue,Wed,Thu,Fri,Sat,Sun,Ave", file=f)  #header row
    for y in range(0, 6):
        line = ""
        if dtMon < datetime.today():    # don't print 6th row if not needed
            print(dtMon.strftime("%d/%m"), file=f, end="")
            count = 0
            sum = 0
            for x in range(0, 7):
                print(", " + str(tbl[y][x]), file=f, end="")
                if tbl[y][x] > 0:
                    count += 1
                sum += tbl[y][x]
            if count == 0:
                ave = 0.0
            else:
                ave = sum / count
            line += ",{:.1f}".format(ave)
            print(line, file=f)   # row average
        dtMon = dtMon + timedelta(days=7) # move on one week
    line="Ave"
    count2 = 0
    sum2 = 0
    for x in range(0, 7):   # column averages
        count = 0
        sum = 0
        for y in range(0, 6): # 6th row may be all zeros (ok)
            if tbl[y][x] > 0:
                count += 1
                count2 += 1
            sum += tbl[y][x]
            sum2 += tbl[y][x]
        if count == 0:
            ave = 0.0
        else:
            ave = sum / count
        line += ",{:.1f}".format(ave)
    if count2 == 0:
        ave2 = 0.0
    else:
        ave2 = sum2 / count2
    line += ",{:.1f}".format(ave2) + "\n"
    print(line, file=f)
    f.close()

def monthlyVisitors() -> None:
    '''monthlyVisitors(): creates a report as CSV file of daily attenders in list format, including names
    parameters: none (but code assumes it is run on 1st of each month)
    returns: none
    '''
    assert datetime.now().day == 1, "Today is not 1st of Month"
    
    # First, construct name of file to create
    dir = "/home/MIS/Reports/Sentinel/"
    td = timedelta(days=-7) # report runs on 1st of next month, so any day "safely" last month
    dt = (datetime.now() + td).strftime("%Y-%m")
    fName = dir + "V" + dt + ".csv"    

    visitors = theDB.monthVisitors()

    # Now write to csv file
    f = open(fName, 'wt')
    print (fName[27:39], file=f)
    print ("Name, ID, fobUID, Date", file=f)
    for vis in visitors:
        line = vis[0] + "," + str(vis[1]) + "," + vis[2] + "," + vis[3].strftime("%d-%m-%Y")
        print (line, file=f)
    f.close()


# creates monthly report file on 1st day of month at midnight
def checkMonthly() -> None:
    ''' checkMonthly(): method to run monthly report at midnight on 1st day of month (checks for this)
    parameters: none
    returns: none
    '''
    global prevDay
    dt = datetime.now().day
    if dt != prevDay:
        if dt == 1:
            monthlyReport()
            monthlyVisitors()
    prevDay = dt
# =================================================================================================================
*/    
bool readTrained(bool isStart) { 
    /*readTrained(): provisional method to read the machine "trained" permissions file and store the results in a vector of struct trained
        This function checks first if the file has been changed since last read.
		parameters: none
		returns: bool: true if successfully read and updated
    */
	struct trained t;
	std::filesystem::file_time_type mod_time = std::filesystem::last_write_time("Trained.txt");
	if (isStart || (ttTrained < mod_time)) { // CHANGE HERE!
        // Read file with machine "trained" permissions
        try {
			std::ifstream fTrained("Trained.txt");
			std::string comma = ",";
			std::vector<trained> mt;
			if (fTrained.is_open()) {
				std::string line, mch, shdr;
				std::size_t ix, ix2;
				int mNo, shID;
				while (std::getline(fTrained, line)) {
					ix = line.find(comma);
					mch = line.substr(0, ix);
					mNo = std::stoi(mch);
					ix2 = line.find(comma, ix + 1);
					shdr = line.substr(ix + 1, ix2 - ix - 1);
					shID = std::stoi(shdr);
					t.mchNo = mNo;
					t.memID = shID;
					mt.push_back(t);
				}
				fTrained.close();
				machTrained.clear();
				for (trained tt : mt) machTrained.push_back(tt);
				ttTrained = std::filesystem::__file_clock::now();
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
	else { // Trained.txt latest version already read.
        return false;
	}
	return false;
}

void setupCmdsAndMsgs() {
    std::vector<sentry> rfids = qt.getRFIDs();
    for (sentry row : rfids) {
        prevCommands.push_back("ZZZ"); // start with a dummy value
		prevMessages.push_back(row.LCDText);
	}
}

/*    
def checkForCommands() -> None:
    '''checkForCommands: handles commands from administrator to be carried out by Sentry terminals
        parameters: none
        returns: none
    '''
    global prevCommands
    results = theDB.listTerminals()
    i = 0
    for row in results:
        cmd = row[2]
        if not (cmd == prevCommands[i]):
            #print(cmd)
            prevCommands[i] = cmd
            target = row[0]
            if cmd == "RST":
                qt.rebootSentry(target)
            if cmd == "LCD":
                qt.switchLCD(cmd)
            if (cmd[0:3] == "RED") or (cmd[0:3] == "GRN"):
                qt.setIntvl(cmd)
        i = i + 1
    theDB.resetCommand()

def checkForMessages() -> None:
    '''checkForMessages: handles admin messages from website to be passed on to Sentry terminals
        parameters: none
        returns: none
    '''
    global prevMessages
    results = theDB.listTerminals()
    i = 0
    for row in results:
        mess = row[1]
        if not (mess == prevMessages[i]):
            prevMessages[i] = mess

            target = row[0]
            if target == "ALL":
                qt.messageAllSentries(mess)
                return
            qt.messageSentry(target, mess)
        i = i + 1
*/
bool matchBoth(std::string uid, int mNo) {
    bool matched = false;
    for ( trained mp : machTrained) {
        //if ((mp.mchNo == mNo) && (mp.memID == uid)) {
          //  matched = true;
            //break;
		//}
	}
    return matched;
}
    
// ====================================================================================

void setup() {
    bool ok = readTrained(true);
    setupCmdsAndMsgs();
    std::cout << "**Setup done**" << std::endl;
}

void loopGH() {
    //global fireTimeout
    while (true) {
        handleIncomingTouch();
            //forcees();    # runs at set time: currently 18:00: now includes readPermissions() if file changed
            //checkMonthly();
            //checkForCommands();
            //checkForMessages();
        //if datetime.now() > fireTimeout:
            //  onTimeout();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
		std::cout << "loop" << std::endl;
	}
	
}

int main() {
	bool dbOK = db.begin();
	if (!dbOK) {
		std::cout << "Error opening database library." << std::endl;
		return 1;
	}
	qt.init(&db, &cli);
	setup();
	loopGH();
	return 0;
}

        