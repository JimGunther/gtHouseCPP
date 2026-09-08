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
* Version 0.92
* Last updated 08/09/2026 12:31
* Author: Jim Gunther
*********************************************************************************************************************************/
// Global variables (?)
const std::string SVR_URI = "mqtt://localhost:1883";
const std::string CLIENT_ID = "SentinelPi";
const std::string TOPIC1("MiS/RFID/+/IN");
const std::string TOPIC2("MiS/MACH/RFID/+");
const int QOS = 0;
const int N_RETRY_ATTEMPTS = 5;
const int NUM_RFIDs = 5;
	
theDB db;

std::time_t ttNow = std::time(0);
struct tm* dt = gmtime(&ttNow);
int prevHour = dt->tm_hour;
std::time_t ttFire = ttNow + 99 * 24 * 3600; // time until fire timeout as epoch (time_t)
int prevDay = 0;
bool touchReceived, machTouched;
int sender;
std::string inTopic;
std::string icMessText;
std::chrono::time_point<std::filesystem::__file_clock> ttTrained = std::filesystem::__file_clock::now();
std::vector<std::string> prevCommands;
std::vector<std::string> prevMessages;
std::map<std::string, int> machLookup;
std::vector<trained> machTrained;

// HELPER CLASSES FOR MQTT=======================================================================================================
/////////////////////////////////////////////////////////////////////////////
// Callbacks for the success or failures of requested actions.

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override
    {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0) std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
    }

    void on_success(const mqtt::token& tok) override
    {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0) std::cout << " for token: [" << tok.get_message_id() << "]. ";
        auto top = tok.get_topics();
        if (top && !top->empty()) std::cout << "token topic: '" << (*top)[0] << "', ..." << std::endl;
    }

public:
    action_listener(const std::string& name) : name_(name) {}
};

/////////////////////////////////////////////////////////////////////////////
/**
 * Local callback & listener class for use with the client connection. Primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener

{
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;

    // This deomonstrates manually reconnecting to the broker by calling connect() again. This is a possibility for an application that keeps
    // a copy of its original connect_options, or if the app wants to reconnect with different options.
    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override
    {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > N_RETRY_ATTEMPTS)
            exit(1);
        reconnect();
    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override {}

    // (Re)connection success
    void connected(const std::string& cause) override
    {
        std::cout << "\nConnection success" << std::endl;
    //    std::cout << "\nSubscribing to topic '" << TOPIC1 << " and " << TOPIC2 << "'\n"
      //            << "\tfor client " << CLIENT_ID << " using QoS" << QOS << "\n"
        //          << std::endl;

        cli_.subscribe(TOPIC1, QOS, nullptr, subListener_);
		cli_.subscribe(TOPIC2, QOS, nullptr, subListener_);
		// REMOVE 2 LINES BELOW PER RFID ONCE "WILDCARD_FRIENDLY"
		cli_.subscribe("MiS/RFID_1/IN", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/MACH/RFID_1", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/RFID_2/IN", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/MACH/RFID_2", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/RFID_4/IN", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/MACH/RFID_4", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/RFID_5/IN", QOS, nullptr, subListener_);
		cli_.subscribe("MiS/MACH/RFID_5", QOS, nullptr, subListener_);
		
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override
    {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override
    {
        std::cout << "\nMessage arrived" << std::endl;
		inTopic = msg->get_topic().substr(4); // Removes "MiS/"
		icMessText = msg->to_string();
        std::cout << "\ttopic: '" << inTopic << "'" << std::endl;
        std::cout << "\tpayload: '" << icMessText << std::endl;
		std::string topicType = inTopic.substr(0, 4);
		if (topicType == "MACH") machTouched = true;
		else touchReceived = true;
		sender = stoi(msg->get_topic().substr(5, 1));
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
        : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription")
    {
    }
};

// HELPER FUNCTIONS FOR STARTUP==================================================================================================

void setupCmds() {
    int n = db.listTerminals().size();
    for (int i = 0; i < n; i++) prevCommands.push_back("XXX"); // start with a dummy value
}

void setupMessages() {
	std::vector<sentry> rfids = db.listTerminals();
	for (sentry t : rfids) prevMessages.push_back(t.LCDText);
}

// HELPER FUNCTIONS FOR loopGH===================================================================================================

void postStuff(mqtt::async_client& cli, std::string topic, std::string payload) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
	auto pubMsg = mqtt::make_message("MiS/" + topic, payload);
	pubMsg->set_qos(0);
    cli.publish(pubMsg);
    std::cout << "Post " << topic << "::" << payload << std::endl;
}

void messageSentry(mqtt::async_client& cli, int rfidNo, std::string msg) { // Uses both _ and / formats for topic: for now
    std::string tpc = "RFID/" + std::to_string(rfidNo) + "/MSG";
    postStuff(cli, tpc, msg);
	// *** REMOVE NEXT 2 LINES WHEN ALL RFIDs ARE "WILDCARD-FRIENDLY" ***
	tpc = "RFID_" + std::to_string(rfidNo) = "/MSG";
	postStuff(cli, tpc, msg);
}

void messageAllSentries(mqtt::async_client& cli, std::string msg) {
    std::string tpc = "/MSG";
    postStuff(cli, tpc, msg);
}
void onTimeout(mqtt::async_client& cli) {
    std::time_t ttNow = std::time(0);
	ttFire = ttNow + 99 * 24 *3600;
    messageAllSentries(cli, "MiS Tag Identity");
    db.logMessage("MiS Tag Identity");
}

void rebootSentry(mqtt::async_client& cli, int rfidNo) { // Uses both _ and / formats for topic: for now
    std::string tpc = "RFID/" + std::to_string(rfidNo) + "/RST";
    postStuff(cli, tpc, "-");
	// *** REMOVE NEXT 2 LINES WHEN ALL RFIDs ARE "WILDCARD-FRIENDLY" ***
	tpc = "RFID_" + std::to_string(rfidNo) + "/RST";
	postStuff(cli, tpc, "-");
}

void handleIncomingTouch(mqtt::async_client& cli) {
    //handleIncomingTouch(): method called every time a fob touches the RFID screen - via MQTT from a Sentry
    //parameters: async_client: cli: reference to MQTT client object
    //returns void, but publishes a reply on MQTT 
    
    if (!touchReceived && !machTouched) return;
    
    std::string fobuid = icMessText.substr(0, 8);// ASSUMES UID IS 8 CHARACTERS LONG
	std::string reply;
	std::string outTopic;

    if (machTouched) { // Machine authorization
		//std::cout << "Machine touch" << std::endl;
		machTouched = false;
		shedder s = db.matchShedder(fobuid);
		std::string mColon = icMessText.substr(9, 2);
		if (mColon != "M:") {
			db.logMessage("Malformed machine no.");
			return;
		}
        try {
			int machNo = std::stoi(icMessText.substr(11));    // ASSUMES UID IS 8 CHARACTERS LONG FOLLOWED BY ',M:' and number
			bool matched = db.mtExists(s.memID, machNo);
			std::string yn;
			if (matched) yn = ",Y";
			else yn = ",N";
			reply = fobuid + "," + icMessText.substr(9) + yn;
			db.addMach(s.memID, machNo, !matched);
		}
		catch (const std::invalid_argument& e) {
			std::cout << e.what() << std::endl;
			reply = fobuid + "," + icMessText.substr(9) + ",N";
		}
		outTopic = inTopic + "/OUT";
        postStuff(cli, outTopic, reply); 
        return;
	}
    
	// All other cases (except machine auth'n)
	touchReceived = false;
	bool isHex = true; // Check it's all hexadecimal
	for (char c : fobuid) {
		if (!std::isxdigit(c)) isHex = false;
	}
    db.logTouch(fobuid);
    if (fobuid == EMF) { // Emergency Fire Fob
        // Line below is my best guess at syntax: CHECK WITH RICHARD!
        system("/home/MIS/Sentinel/bin/InNow.sh");
        messageAllSentries(cli, "Fire alarm from RFID_" + std::to_string(sender));
        db.logMessage("Fire alarm from RFID_" + std::to_string(sender));
		std::time_t ttNow = std::time(NULL);
		ttFire = ttNow + 3 * 3600; // time until fire timeout (3 hours)
        return;
	}
    
    // "Normal" fob in/out from here...
	size_t ix = inTopic.find("/IN");
	outTopic = inTopic.substr(0, ix) + "/OUT"; // replaces "/IN" with "/OUT"
	shedder mem = db.matchShedder(fobuid);
    if (mem.memID > 0) {  // matching fob in database: must be visit (in or out)
        if (db.isInShed(mem.memID)) {   // must be exit
            db.addExit(mem.memID);
            reply = fobuid + ":B:" + mem.givenName;
		}
        else {
            db.addVisit(mem.memID);
            reply = fobuid + ":W:" + mem.givenName;
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

    postStuff(cli, outTopic, reply); // always reply to RFID (even if adding to fob store)
}

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
			f << std::string(buf);
            count = 0; sum = 0;
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
	line= "Ave";
    int count2 = 0, sum2 = 0;
	float ave2;
    for (x = 0; x < 7; x++) {  // column averages
        count = 0; sum = 0;
        for (y = 0; y < 6; y++) { //# 6th row may be all zeros (ok)
            if (tbl[y][x] > 0) {
                count++; count2++;
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

void checkForCommands(mqtt::async_client& cli) {
    //checkForCommands: handles commands from administrator to be carried out by Sentry terminals
    //parameters: none
    //returns: none
    std::vector<sentry> results = db.listTerminals();
    int i = 0;
    for (sentry row : results) {
        std::string cmd = row.commd;
        if (!(cmd == prevCommands[i])) {
            prevCommands[i] = cmd;
            std::string target = row.name;
			int tNo = stoi(target.substr(5));
            if (cmd == "RST") rebootSentry(cli, tNo);
        }
	i++;
	}
    db.resetCommand();
}

void checkForMessages(mqtt::async_client& cli) {
    //checkForMessages: handles admin messages from website to be passed on to Sentry terminals
    //  parameters: none
    // returns: none
    std::vector<sentry> results = db.listTerminals();
    int i = 0;
    for (sentry row : results) {
        std::string mess = row.LCDText;
//		std::cout << mess << std::endl;
        if (mess != prevMessages[i]) {
			std::cout << "Message was: " << prevMessages[i] << "; Now: " << mess << std::endl; // Remove this line later (06/09/2026)
            prevMessages[i] = mess;
            std::string target = row.name;
			int tNo = stoi(target.substr(5));
            messageSentry(cli, tNo, mess);
		}
    i++;		
	}
}
    
// ====================================================================================

bool setup() {
	bool dbOK = db.begin();
	if (!dbOK) {
		std::cout << "Error opening database library." << std::endl;
		return false;
	}
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    setupCmds();
	setupMessages();
    std::cout << "**Setup done**" << std::endl;
	return true;
}

void loopGH(mqtt::async_client& cli) {
    while (true) {
        handleIncomingTouch(cli);
        checkMonthly();
        checkForCommands(cli);
        checkForMessages(cli);
        if (std::time(0) > ttFire) onTimeout(cli);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

	}	
}

int main() {
	mqtt::async_client cli(SVR_URI, CLIENT_ID);

	mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);
    callback cb(cli, connOpts);
    cli.set_callback(cb);

    // Start the connection.
    try {
        std::cout << "Connecting to the MQTT server '" << SVR_URI << "'..." << std::flush;
        cli.connect(connOpts, nullptr, cb);
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "\nERROR: Unable to connect to MQTT server: '" << SVR_URI << "'" << exc << std::endl;
        return 1;
    }

	if (setup()) loopGH(cli);
	return 0;
}

        