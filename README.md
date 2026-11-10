This repository contains CPP files to create 2 executables and is written in C++
1. GtHouse: the CPP replacement for Python code to run the RPi functionality for the Sentinel RFID entry control system
2. SyncTrain: a small CPP coded utility app to synchronize the contents of Trained.txt (or any .txt or .csv file)(from Steve Conway) with the MachTain table in the Sentinel database.
As of 10/11/2026, SyncTrain has been updated in line with Richard's suggested logic, i.e. Trained.txt creates a fully-synchronized and "refreshed-from-scratch" MachTrain database table.
SyncTrain is fully tested and GtHouse is functional but needs more extensive testing. Minor logic changes to SyncTrain 10/11/2026; confirmed working OK on Shed RPi.
Uploads should be run every time a new .txt or .csv file with updated training data is created.
GtHouse will replace the GtHouse Python once it has been more fully tested at the Shed and will become the operational service for Sentinel.
To compile and link the executables, run the following on the RPi:
SyncTrain: g++ SyncTrain.cpp -lmysqlclient -o SyncTrain
GtHouse: g++ GtHouse.cpp theDB.cpp -lpaho-mqttp3 -lpaho-mqtt3a -lmysqlclient -o GtHouse
