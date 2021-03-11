#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <SimConnect.h>
#include <MSFS\Legacy\gauges.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include "Module.h"

HANDLE g_hSimConnect;
const char* version = "0.0.1";
const char* OpenA3xxEventPrefix = "OpenA3XX.";
const char* FileEventsOpenA3xx = "modules/events.txt";
const char* FileEventsUser = "modules/events.user.txt";

std::vector<std::pair<std::string, std::string>> CodeEvents;


enum OPENA3XX_GROUP
{
	DEFAULT
};

enum eEvents
{
	EVENT_FLIGHT_LOADED
};

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

std::pair<std::string, std::string> splitIntoPair(std::string value, char delimiter) {
	auto index = value.find(delimiter);
	std::pair<std::string, std::string> result;
	if (index != std::string::npos) {

		// Split around ':' character
		result = std::make_pair(
			value.substr(0, index),
			value.substr(index + 1)
		);

		// Trim any leading ' ' in the value part
		// (you may wish to add further conditions, such as '\t')
		while (!result.second.empty() && result.second.front() == ' ') {
			result.second.erase(0, 1);
		}
	}
	else {
		// Split around ':' character
		result = std::make_pair(
			value,
			std::string("(>H:" + value + ")")
		);
	}

	return result;
} 

void LoadEventDefinitions(const char * fileName) {
	std::ifstream file(fileName);
	std::string line;

	while (std::getline(file, line)) {
		if (line.find("//") != std::string::npos) continue;

		std::pair<std::string, std::string> codeEvent = splitIntoPair(line, '#');
		CodeEvents.push_back(codeEvent);
	}

	file.close();
}

void RegisterEvents() {
	DWORD eventID = 0;

	for (const auto& value : CodeEvents) {
		std::string eventCommand = value.second;
		std::string eventName = std::string(OpenA3xxEventPrefix) + value.first;

		HRESULT hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, eventID, eventName.c_str());
		hr = SimConnect_AddClientEventToNotificationGroup(g_hSimConnect, OPENA3XX_GROUP::DEFAULT, eventID, false);

#if _DEBUG
		if (hr != S_OK) fprintf(stderr, "OpenA3XX: Error on registering Event %s with ID %u for code %s", eventName.c_str(), eventID, eventCommand.c_str());
		else fprintf(stderr, "OpenA3XX: Success on registering Event %s with ID %u for code %s", eventName.c_str(), eventID, eventCommand.c_str());
#endif

		eventID++;
	}

	SimConnect_SetNotificationGroupPriority(g_hSimConnect, OPENA3XX_GROUP::DEFAULT, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
}

extern "C" MSFS_CALLBACK void module_init(void)
{
	// load defintions
	LoadEventDefinitions(FileEventsOpenA3xx);
	LoadEventDefinitions(FileEventsUser);
	
	g_hSimConnect = 0;
	HRESULT hr = SimConnect_Open(&g_hSimConnect, "OpenA3XX Module", (HWND) NULL, 0, 0, 0);
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not open SimConnect connection.\n");
		return;
	}
	hr = SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_FLIGHT_LOADED, "FlightLoaded");
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not subscribe to \"FlightLoaded\" system event.\n");
		return;
	}
	
	RegisterEvents();

	hr = SimConnect_CallDispatch(g_hSimConnect, MyDispatchProc, NULL);
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not set dispatch proc.\n");
		return;
	}

	fprintf(stderr, "OpenA3XX: Module Init Complete. Version: %s", version);
	fprintf(stderr, "OpenA3XX: Loaded %u event defintions.", CodeEvents.size());
}

extern "C" MSFS_CALLBACK void module_deinit(void)
{

	if (!g_hSimConnect)
		return;
	HRESULT hr = SimConnect_Close(g_hSimConnect);
	if (hr != S_OK)
	{
		fprintf(stderr, "Could not close SimConnect connection.\n");
		return;
	}

}

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	switch (pData->dwID)
	{
	case SIMCONNECT_RECV_ID_EVENT: {
		SIMCONNECT_RECV_EVENT* evt = (SIMCONNECT_RECV_EVENT*)pData;
		int eventID = evt->uEventID;

		if (eventID < CodeEvents.size()) {
			// We got a Code Event or a User Code Event
			 int CodeEventId = eventID;
			std::string command = std::string(CodeEvents[CodeEventId].second);
			fprintf(stderr, "execute %s\n", command.c_str());
			execute_calculator_code(command.c_str(), nullptr, nullptr, nullptr);
		} 
		else {
			fprintf(stderr, "OpenA3XX: OFF! - EventID out of range:%u\n", eventID);
		}
		
		break;
	}
	default:
		break;
	}
}
