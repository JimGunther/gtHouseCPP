This repository contains CPP files to create 2 executables and is written in C++
1. GtHouse: the CPP replacement for Python code to run the RPi functionality for the Sentinel RFID entry control system
2. SyncTrain: a small CPP coded utility app to synchronize the contents of Trained.txt (from Steve Conway) with the MachTain table in the Sentinel database.
As of 27/08/2026, SyncTrain is fully tested and GtHouse is functional but needs more extensive testing.
SyncTrain should be run every time the contents of Trained.txt is updated. GtHouse will replace the GtHouse Python once it has been "fully" tested and will become the operational service for Sentinel.
To compile and link the executables, run the following on the RPi:
SyncTrain: g++ SyncTrain.cpp -lmysqlclient -o SyncTrain
GtHouse: g++ GtHouse.cpp theDB.cpp quti.cpp -lpaho-mqttp3 -lpaho-mqtt3a -lmysqlclient -o GtHouse
