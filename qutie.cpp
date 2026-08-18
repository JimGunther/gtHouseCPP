//import time
//import config
#include <thread>
#include <chrono>
#include "qutie.h"
#include "theDB.h"

/*********************************************************************************************************************************
* qutie.cpp: support CPP file for Sentinel project. Handles MQTT functionality, using the paho.mqtt.client library
* Version 0.3
* Last updated 18/08/2026 15:2601
* Author: Jim Gunther
*********************************************************************************************************************************/
//#using namespace std::chrono_literals;

//msgWaiting = False
bool machTouched = false, touchReceived = false;
std::string sender, icMessText;

// Callback class------------------------------------------------------------------------------------------------------

class ceebee : public virtual mqtt::callback {
	
	void connected(const std::string& cause) override {
		std::cout << "connected!" << std::endl;
	}
	
	void message_arrived(mqtt::const_message_ptr msg) override {
		std::string topic = msg->get_topic();
		icMessText = msg->get_payload_str();
		std::string mach = topic.substr(4, 4);
		if (mach == "MACH") machTouched = true;
		else touchReceived = true;
		if (machTouched) sender = topic.substr(9, 6);
		else sender = topic.substr(4, 6);
		
		std::cout << "message!" << std::endl; // TEMP!!
	}

};
        
   
// -----------------------------End of MQTT callbacks -----------------------------------
    
// Qutie class starts here...

Qutie::Qutie() { // constructor
		//_cli = mqtt:async_client(svr_uri, client_id);
	}
	
void Qutie::init(theDB* dBase, mqtt::async_client* pCli) { 
    /*init(): class initiations: sets up MQTT client connection
		parameters: dBase : reference to theDB object
		returns: none
    */

	_dbPtr = dBase;
    _rfids = _dbPtr->listTerminals();
	_cliPtr = pCli;
	std::cout << "Connecting to broker" << std::endl;
	_connOpts = mqtt::connect_options_builder::v3()
		.keep_alive_interval(std::chrono::seconds(30))
		.clean_session(false)
		.automatic_reconnect()
		.finalize();
		
	ceebee cb;
	_cliPtr->set_callback(cb);
	mqtt::token_ptr tok = _cliPtr->connect(_connOpts);
	tok->wait();
	std::string top;
	//std::cout << "after connect" << std::endl;
	//std::cout << tok << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
	//std::cout << "after response" << std::endl;
	std::cout << "Subscribing to topics..." << std::endl;
	for (sentry rfid : _rfids) {
		top = "MiS/" + rfid.name + "/IN";
		_cliPtr->subscribe(top, 0);
		//std::cout << "  " << top << std::endl;
		top = "MiS/MACH/" + rfid.name + "/IN";
		_cliPtr->subscribe(top, 0);
		//std::cout << "  " << top << std::endl;
	}
    std::cout << "MQTT started" << std::endl;
}

void Qutie::postStuff(std::string topic, std::string payload) {
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
	auto pubMsg = mqtt::make_message("MiS/" + topic, payload);
	pubMsg->set_qos(0);
    _cliPtr->publish(pubMsg);
    std::cout << topic << "::" << payload << std::endl;
}
// =>CPP HERE!
    
void Qutie::messageSentry(std::string rfid, std::string msg) { //currently not in use: use and format to be agreed
        std:string tpc = rfid + "/MSG";
        postStuff(tpc, msg):
}

void Qutie::messageAllSentries(std::string msg) {
    std::string tpc = "MSG";
    postStuff(tpc, msg);
}
/*
    def rebootSentry(self, rfid : str) -> None:
        tpc = rfid + "/RST"
        self.postStuff(tpc, "-")

    def switchLCD(self, msg :str) -> None:
        tpc = "LCD"
        self.postStuff(tpc, msg)

    def setIntvl(self, msg : str) -> None:
        tpc = "DELAY"
        self.postStuff(tpc, msg)
*/

