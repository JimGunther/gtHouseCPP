#ifndef QUTIE_H
#define QUITIE_H
#include <mqtt/async_client.h>
#include <mqtt/connect_options.h>
#include <vector>
#include <string>
#include "config.h"
#include "theDB.h"
/**************************************************************************************************
* qutie.h: header file for Qutie class
* Version of 18/08/2026 17:02
* Written by Jim Gunther
***************************************************************************************************/

//const std::string svr_uri = "mqtt://localhost:1883";
//const std::string client_id = "JimPi5";
//const std::string pers = "./persist";

class Qutie {
	public:
		Qutie();
		void init(theDB* dBase, mqtt::async_client* pCli);
		std::vector<sentry> getRFIDs() { return _rfids; }
		void postStuff(std::string topic, std::string payload);
		void messageSentry(std::string rfid, std::string msg);
		void messageAllSentries(std::string msg);
	private:
		theDB* _dbPtr;
		mqtt::async_client* _cliPtr;
		mqtt::connect_options _connOpts;
		std::vector<std::string> _topics;
		std::vector<sentry> _rfids;
};
#endif