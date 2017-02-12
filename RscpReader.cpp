//============================================================================
// Name        : RscpReader.cpp
// Author      : Ralf Lehmann
// Version     : 1.0
// Copyright   : GPL
// Description : Reads historical data from a s10 solar power station (E3DC)
// 			   : via rscp protocol; based on source code released by E3DC
//============================================================================

#define RLOG_COMPONENT S10read
#include <rlog/rlog.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "RscpProtocol.h"
#include "RscpTags.h"
#include "SocketConnection.h"
#include "AES.h"

#define AES_KEY_SIZE        32
#define AES_BLOCK_SIZE      32

//
// globals
//
SRscpTimestamp start, interval, span;
SRscpTag spanTag = TAG_DB_REQ_HISTORY_DATA_DAY;	// e.g. TAG_DB_REQ_HISTORY_DATA_MONTH

//#define SERVER_IP
const char * ip_addr;
//#define SERVER_PORT         5033
int port_number = 5033;

//#define E3DC_USER
const char * e3dc_user;
//#define E3DC_PASSWORD       ""
const char * e3dc_password;
//#define AES_PASSWORD        ""
const char * aes_password;

static int iSocket = -1;
static int iAuthenticated = 0;
static AES aesEncrypter;
static AES aesDecrypter;
static uint8_t ucEncryptionIV[AES_BLOCK_SIZE];
static uint8_t ucDecryptionIV[AES_BLOCK_SIZE];

bool brief = false;	// brief report; sum only

//
// functions

const char * db_value_prefix() {
	switch (spanTag) {
	case TAG_DB_REQ_HISTORY_DATA_DAY:
		return "Hour";
	case TAG_DB_REQ_HISTORY_DATA_MONTH:
		return "Day";
	case TAG_DB_REQ_HISTORY_DATA_YEAR:
		return "Month";
	default:
		return "unknown span";
	}
}

int graph_index = 0;
int db_value_container(RscpProtocol *protocol, std::vector<SRscpValue> *dbVal) {
	// does not make sense for year, because months have not the same length but only one interval is possible
	if (spanTag == TAG_DB_REQ_HISTORY_DATA_YEAR || brief) {
		return 0;
	}
	const char * value_prefix = db_value_prefix();
	struct val_t {
		float bat_in, bat_out;
		float production; 	// production
		float grid_in, grid_out;
		float consumption;
		float bat_charge_level, bat_cycle_count;
		float consumed_prod, autarky;
	} val;
	// Day show Watts all others energy (Watt Hours)
	const char * W;
	W = "Wh";
	if (spanTag == TAG_DB_REQ_HISTORY_DATA_DAY) {
		W = "W";
	}
	graph_index++;
	time_t d = start.seconds + ((graph_index - 1) * interval.seconds);
	printf("[%d]-%s Date: %d - %s", graph_index, value_prefix, (int) d, ctime(&d));

	for (size_t i = 0; i < dbVal->size(); ++i) {
		switch ((*dbVal)[i].tag) {
		case TAG_DB_GRAPH_INDEX: {
			float fgraph_index = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s graph index: %0.1f \n", graph_index, value_prefix, fgraph_index);
			break;
		}
		case TAG_DB_BAT_POWER_IN: {
			float bat_power_in = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s battery in: %0.1f %s\n", graph_index, value_prefix, bat_power_in, W);
			val.bat_in = bat_power_in;
			break;
		}
		case TAG_DB_BAT_POWER_OUT: {
			float bat_power_out = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s battery out: %0.1f %s\n", graph_index, value_prefix, bat_power_out, W);
			val.bat_out = bat_power_out;
			break;
		}
		case TAG_DB_DC_POWER: {
			float dc_power = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s production: %0.1f %s\n", graph_index, value_prefix, dc_power, W);
			val.production = dc_power;
			break;
		}
		case TAG_DB_GRID_POWER_IN: {
			float grid_power_in = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s grid in: %0.1f %s\n", graph_index, value_prefix, grid_power_in, W);
			val.grid_in = grid_power_in;
			break;
		}
		case TAG_DB_GRID_POWER_OUT: {
			float grid_power_out = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s grid out: %0.1f %s\n", graph_index, value_prefix, grid_power_out, W);
			val.grid_out = grid_power_out;
			break;
		}
		case TAG_DB_CONSUMPTION: {
			float db_consumption = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s consumption: %0.1f %s\n", graph_index, value_prefix, db_consumption, W);
			val.consumption = db_consumption;
			break;
		}
		case TAG_DB_PM_0_POWER: {
			float pm0_power = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s pm 0 power: %0.1f %s\n", graph_index, value_prefix, pm0_power, W);
			break;
		}
		case TAG_DB_PM_1_POWER: {
			float pm1_power = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s pm 1 power: %0.1f %s\n", graph_index, value_prefix, pm1_power, W);
			break;
		}
		case TAG_DB_BAT_CHARGE_LEVEL: {
			float bat_level = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s bat charge level: %0.1f %%\n", graph_index, value_prefix, bat_level);
			val.bat_charge_level = bat_level;
			break;
		}
		case TAG_DB_BAT_CYCLE_COUNT: {
			float cycle = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s bat cycle count: %f \n", graph_index, value_prefix, cycle);
			val.bat_cycle_count = cycle;
			break;
		}
		case TAG_DB_CONSUMED_PRODUCTION: {
			float prod = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s consumed production: %0.1f \n", graph_index, value_prefix, prod);
			val.consumed_prod = prod;
			break;
		}
		case TAG_DB_AUTARKY: {
			float aut = protocol->getValueAsFloat32(&((*dbVal)[i]));
			printf("[%d]-%s autarky: %f \n", graph_index, value_prefix, aut);
			val.autarky = aut;
			break;
		}
		default:
			rWarning("Unknown dbVal tag %08X\n", (*dbVal)[i].tag);
		}
	}
	if (graph_index == 1) {
		printf("[%d]-%s-CSV-head: date;batin;batout;batsoc;pro;netin;netout;con\n", graph_index, value_prefix);
	}
	printf("[%d]-%s-CSV: %d;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n", graph_index, value_prefix, (int) d, val.bat_in, val.bat_out, val.bat_charge_level, val.production, val.grid_in,
			val.grid_out, val.consumption);
	return 0;
}

const char * db_sum_prefix() {
	switch (spanTag) {
	case TAG_DB_REQ_HISTORY_DATA_DAY:
		return "Day";
	case TAG_DB_REQ_HISTORY_DATA_MONTH:
		return "Month";
	case TAG_DB_REQ_HISTORY_DATA_YEAR:
		return "Year";
	default:
		return "unknown span";
	}
}

int db_sum_container(RscpProtocol *protocol, std::vector<SRscpValue> *dbSum) {
	graph_index = 0;
	time_t d = start.seconds;
	const char * sum_prefix = db_sum_prefix();
	printf("%s start: %d - %s", sum_prefix, (int) d, ctime(&d));
	d = start.seconds + span.seconds;
	printf("%s end: %d - %s", sum_prefix, (int) d, ctime(&d));
	struct sum_t {
		float bat_in, bat_out;
		float production; 	// production
		float grid_in, grid_out;
		float consumption;
		float bat_charge_level, bat_cycle_count;
		float consumed_prod, autarky;
	} sum;
	for (size_t i = 0; i < dbSum->size(); ++i) {
		switch ((*dbSum)[i].tag) {
		case TAG_DB_GRAPH_INDEX: {
			float graph_index = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s graph index: %0.1f \n", sum_prefix, graph_index);
			break;
		}
		case TAG_DB_BAT_POWER_IN: {
			float bat_power_in = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s battery in: %0.1f Wh\n", sum_prefix, bat_power_in);
			sum.bat_in = bat_power_in;
			break;
		}
		case TAG_DB_BAT_POWER_OUT: {
			float bat_power_out = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s battery out: %0.1f Wh\n", sum_prefix, bat_power_out);
			sum.bat_out = bat_power_out;
			break;
		}
		case TAG_DB_DC_POWER: {
			float dc_power = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s production: %0.1f Wh\n", sum_prefix, dc_power);
			sum.production = dc_power;
			break;
		}
		case TAG_DB_GRID_POWER_IN: {
			float grid_power_in = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s grid in: %0.1f Wh\n", sum_prefix, grid_power_in);
			sum.grid_in = grid_power_in;
			break;
		}
		case TAG_DB_GRID_POWER_OUT: {
			float grid_power_out = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s grid out: %0.1f Wh\n", sum_prefix, grid_power_out);
			sum.grid_out = grid_power_out;
			break;
		}
		case TAG_DB_CONSUMPTION: {
			float db_consumption = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s consumption: %0.1f Wh\n", sum_prefix, db_consumption);
			sum.consumption = db_consumption;
			break;
		}
		case TAG_DB_PM_0_POWER: {
			float pm0_power = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s pm 0 power: %0.1f Wh\n", sum_prefix, pm0_power);
			break;
		}
		case TAG_DB_PM_1_POWER: {
			float pm1_power = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s pm 1 power: %0.1f Wh\n", sum_prefix, pm1_power);
			break;
		}
		case TAG_DB_BAT_CHARGE_LEVEL: {
			float bat_level = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s bat charge level: %0.1f %%\n", sum_prefix, bat_level);
			sum.bat_charge_level = bat_level;
			break;
		}
		case TAG_DB_BAT_CYCLE_COUNT: {
			float cycle = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s bat cycle count: %f \n", sum_prefix, cycle);
			sum.bat_cycle_count = cycle;
			break;
		}
		case TAG_DB_CONSUMED_PRODUCTION: {
			float prod = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s consumed production: %0.1f \n", sum_prefix, prod);
			sum.consumed_prod = prod;
			break;
		}
		case TAG_DB_AUTARKY: {
			float aut = protocol->getValueAsFloat32(&((*dbSum)[i]));
			printf("%s autarky: %f \n", sum_prefix, aut);
			sum.autarky = aut;
			break;
		}
		default:
			rWarning("Unknown dbSum tag %08X\n", (*dbSum)[i].tag);
		}
	}
	printf("%s-CSV-head: date;batin;batout;batsoc;pro;netin;netout;con\n", sum_prefix);
	printf("%s-CSV: %d;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n", sum_prefix, (int) start.seconds, sum.bat_in, sum.bat_out, sum.bat_charge_level, sum.production, sum.grid_in, sum.grid_out,
			sum.consumption);
	return 0;
}

int db_history_container(RscpProtocol *protocol, std::vector<SRscpValue> *c) {
	for (size_t i = 0; i < c->size(); ++i) {
		if ((*c)[i].dataType == RSCP::eTypeError) {
			// handle error for example access denied errors
			uint32_t uiErrorCode = protocol->getValueAsUInt32(&(*c)[i]);
			rError("Tag 0x%08X received error code %u.\n", (*c)[i].tag, uiErrorCode);
			return -1;
		}
		// db sub tags
		switch ((*c)[i].tag) {
		case TAG_DB_SUM_CONTAINER: {
			std::vector<SRscpValue> dbSum = protocol->getValueAsContainer(&((*c)[i]));
			db_sum_container(protocol, &dbSum);
			break;
		}
		case TAG_DB_VALUE_CONTAINER: {
			std::vector<SRscpValue> dbValue = protocol->getValueAsContainer(&((*c)[i]));
			db_value_container(protocol, &dbValue);
			break;
		}
		default:
			rWarning("Unknown db tag %08X\n", (*c)[i].tag);
		}
	}
	return 0;
}

//
int handleResponseValue(RscpProtocol *protocol, SRscpValue *response) {
	// check if any of the response has the error flag set and react accordingly
	if (response->dataType == RSCP::eTypeError) {
		// handle error for example access denied errors
		uint32_t uiErrorCode = protocol->getValueAsUInt32(response);
		rError("Tag 0x%08X received error code %u.\n", response->tag, uiErrorCode);
		return -1;
	}

	// check the SRscpValue TAG to detect which response it is
	switch (response->tag) {
	case TAG_RSCP_AUTHENTICATION: {
		// It is possible to check the response->dataType value to detect correct data type
		// and call the correct function. If data type is known,
		// the correct function can be called directly like in this case.
		uint8_t ucAccessLevel = protocol->getValueAsUChar8(response);
		if (ucAccessLevel > 0) {
			iAuthenticated = 1;
		}
		rInfo("RSCP authentitication level %i\n", ucAccessLevel);
		break;
	}
	case TAG_EMS_POWER_PV: {    // response for TAG_EMS_REQ_POWER_PV
		int32_t iPower = protocol->getValueAsInt32(response);
		printf("EMS PV power is %i W\n", iPower);
		break;
	}
	case TAG_EMS_POWER_BAT: {    // response for TAG_EMS_REQ_POWER_BAT
		int32_t iPower = protocol->getValueAsInt32(response);
		printf("EMS BAT power is %i W\n", iPower);
		break;
	}
	case TAG_EMS_POWER_HOME: {    // response for TAG_EMS_REQ_POWER_HOME
		int32_t iPower = protocol->getValueAsInt32(response);
		printf("EMS house power is %i W\n", iPower);
		break;
	}
	case TAG_EMS_POWER_GRID: {    // response for TAG_EMS_REQ_POWER_GRID
		int32_t iPower = protocol->getValueAsInt32(response);
		printf("EMS grid power is %i W\n", iPower);
		break;
	}
	case TAG_EMS_POWER_ADD: {    // response for TAG_EMS_REQ_POWER_ADD
		int32_t iPower = protocol->getValueAsInt32(response);
		printf("EMS add power meter power is %i W\n", iPower);
		break;
	}
	case TAG_DB_HISTORY_DATA_DAY:
	case TAG_DB_HISTORY_DATA_MONTH:
	case TAG_DB_HISTORY_DATA_YEAR: {
		std::vector<SRscpValue> dbData = protocol->getValueAsContainer(response);
		db_history_container(protocol, &dbData);
		break;
	}
	case TAG_BAT_DATA: {        // response for TAG_BAT_REQ_DATA
		uint8_t ucBatteryIndex = 0;
		std::vector<SRscpValue> batteryData = protocol->getValueAsContainer(response);
		for (size_t i = 0; i < batteryData.size(); ++i) {
			if (batteryData[i].dataType == RSCP::eTypeError) {
				// handle error for example access denied errors
				uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
				rError("Tag 0x%08X received error code %u.\n", batteryData[i].tag, uiErrorCode);
				return -1;
			}
			// check each battery sub tag
			switch (batteryData[i].tag) {
			case TAG_BAT_INDEX: {
				ucBatteryIndex = protocol->getValueAsUChar8(&batteryData[i]);
				break;
			}
			case TAG_BAT_RSOC: {              // response for TAG_BAT_REQ_RSOC
				float fSOC = protocol->getValueAsFloat32(&batteryData[i]);
				printf("Battery SOC is %0.1f %%\n", fSOC);
				break;
			}
			case TAG_BAT_MODULE_VOLTAGE: { // response for TAG_BAT_REQ_MODULE_VOLTAGE
				float fVoltage = protocol->getValueAsFloat32(&batteryData[i]);
				printf("Battery total voltage is %0.1f V\n", fVoltage);
				break;
			}
			case TAG_BAT_CURRENT: {    // response for TAG_BAT_REQ_CURRENT
				float fVoltage = protocol->getValueAsFloat32(&batteryData[i]);
				printf("Battery current is %0.1f A\n", fVoltage);
				break;
			}
			case TAG_BAT_STATUS_CODE: {  // response for TAG_BAT_REQ_STATUS_CODE
				uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
				printf("Battery status code is 0x%08X\n", uiErrorCode);
				break;
			}
			case TAG_BAT_ERROR_CODE: {    // response for TAG_BAT_REQ_ERROR_CODE
				uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
				printf("Battery error code is 0x%08X\n", uiErrorCode);
				break;
			}
				// ...
			default:
				// default behaviour
				printf("Unknown battery tag %08X\n", response->tag);
				break;
			}
		}
		protocol->destroyValueData(batteryData);
		break;
	}
		// ...
	default:
		// default behavior
		rWarning("Unknown tag %08X\n", response->tag);
		break;
	}
	return 0;
}

static int processReceiveBuffer(const unsigned char * ucBuffer, int iLength) {
	RscpProtocol protocol;
	SRscpFrame frame;

	int iResult = protocol.parseFrame(ucBuffer, iLength, &frame);
	rDebug("processReceiveBuffer result for parseFrame: %d", iResult);
	if (iResult < 0) {
		// check if frame length error occured
		// in that case the full frame length was not received yet
		// and the receive function must get more data
		if (iResult == RSCP::ERR_INVALID_FRAME_LENGTH) {
			return 0;
		}
		// otherwise a not recoverable error occured and the connection can be closed
		else {
			return iResult;
		}
	}

	int iProcessedBytes = iResult;

	// process each SRscpValue struct seperately
	for (unsigned int i = 0; i < frame.data.size(); i++) {
		handleResponseValue(&protocol, &frame.data[i]);
	}

	// destroy frame data and free memory
	protocol.destroyFrameData(frame);

	// returned processed amount of bytes
	return iProcessedBytes;
}

//
// receiving packages
//
static void receiveLoop(bool & bStopExecution) {
	//--------------------------------------------------------------------------------------------------------------
	// RSCP Receive Frame Block Data
	//--------------------------------------------------------------------------------------------------------------
	// setup a static dynamic buffer which is dynamically expanded (re-allocated) on demand
	// the data inside this buffer is not released when this function is left
	static int iReceivedBytes = 0;
	static std::vector<uint8_t> vecDynamicBuffer;

	// check how many RSCP frames are received, must be at least 1
	// multiple frames can only occur in this example if one or more frames are received with a big time delay
	// this should usually not occur but handling this is shown in this example
	int iReceivedRscpFrames = 0;
	while (!bStopExecution && ((iReceivedBytes > 0) || iReceivedRscpFrames == 0)) {
		// check and expand buffer
		if ((vecDynamicBuffer.size() - iReceivedBytes) < 4096) {
			// check maximum size
			if (vecDynamicBuffer.size() > RSCP_MAX_FRAME_LENGTH) {
				// something went wrong and the size is more than possible by the RSCP protocol
				rError("Maximum buffer size exceeded %i\n", (int ) vecDynamicBuffer.size());
				bStopExecution = true;
				break;
			}
			// increase buffer size by 4096 bytes each time the remaining size is smaller than 4096
			vecDynamicBuffer.resize(vecDynamicBuffer.size() + 4096);
		}
		// receive data
		int iResult = SocketRecvData(iSocket, &vecDynamicBuffer[0] + iReceivedBytes, vecDynamicBuffer.size() - iReceivedBytes);
		if (iResult < 0) {
			// check errno for the error code to detect if this is a timeout or a socket error
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				// receive timed out -> continue with re-sending the initial block
				rError("Response receive timeout (retry)\n");

			}
			// socket error -> check errno for failure code if needed
			rError("Socket receive error. errno %i\n", errno);
			bStopExecution = true;
			break;
		} else if (iResult == 0) {
			// connection was closed regularly by peer
			// if this happens on startup each time the possible reason is
			// wrong AES password or wrong network subnet (adapt hosts.allow file required)
			rError("Connection closed by peer\n");
			bStopExecution = true;
			break;
		}
		rDebug("Received %d bytes", iResult);
		// increment amount of received bytes
		iReceivedBytes += iResult;

		// process all received frames
		while (!bStopExecution) {
			// round down to a multiple of AES_BLOCK_SIZE
			int iLength = ROUNDDOWN(iReceivedBytes, AES_BLOCK_SIZE);
			// if not even 32 bytes were received then the frame is still incomplete
			if (iLength == 0) {
				break;
			}
			// resize temporary decryption buffer
			std::vector<uint8_t> decryptionBuffer;
			decryptionBuffer.resize(iLength);
			// initialize encryption sequence IV value with value of previous block
			aesDecrypter.SetIV(ucDecryptionIV, AES_BLOCK_SIZE);
			// decrypt data from vecDynamicBuffer to temporary decryptionBuffer
			aesDecrypter.Decrypt(&vecDynamicBuffer[0], &decryptionBuffer[0], iLength / AES_BLOCK_SIZE);

			// data was received, check if we received all data
			int iProcessedBytes = processReceiveBuffer(&decryptionBuffer[0], iLength);
			if (iProcessedBytes < 0) {
				// an error occured;
				rError("Error parsing RSCP frame: %i\n", iProcessedBytes);
				// stop execution as the data received is not RSCP data
				bStopExecution = true;
				break;

			} else if (iProcessedBytes > 0) {
				// round up the processed bytes as iProcessedBytes does not include the zero padding bytes
				iProcessedBytes = ROUNDUP(iProcessedBytes, AES_BLOCK_SIZE);
				// store the IV value from encrypted buffer for next block decryption
				memcpy(ucDecryptionIV, &vecDynamicBuffer[0] + iProcessedBytes - AES_BLOCK_SIZE,
				AES_BLOCK_SIZE);
				// move the encrypted data behind the current frame data (if any received) to the front
				memcpy(&vecDynamicBuffer[0], &vecDynamicBuffer[0] + iProcessedBytes, vecDynamicBuffer.size() - iProcessedBytes);
				// decrement the total received bytes by the amount of processed bytes
				iReceivedBytes -= iProcessedBytes;
				// increment a counter that a valid frame was received and
				// continue parsing process in case a 2nd valid frame is in the buffer as well
				iReceivedRscpFrames++;
			} else {
				// iProcessedBytes is 0
				// not enough data of the next frame received, go back to receive mode if iReceivedRscpFrames == 0
				// or transmit mode if iReceivedRscpFrames > 0
				break;
			}
		}
	}
}

//
// create an Rscp request
// either an authentication request
// or a data request
//
int createRequest(SRscpFrameBuffer * frameBuffer) {
	RscpProtocol protocol;
	SRscpValue rootValue;
	// The root container is create with the TAG ID 0 which is not used by any device.
	protocol.createContainerValue(&rootValue, 0);

	//---------------------------------------------------------------------------------------------------------
	// Create a request frame
	//---------------------------------------------------------------------------------------------------------
	if (iAuthenticated == 0) {
		rInfo("Generating request authentication\n");
		// authentication request
		SRscpValue authenContainer;
		protocol.createContainerValue(&authenContainer,
		TAG_RSCP_REQ_AUTHENTICATION);
		protocol.appendValue(&authenContainer, TAG_RSCP_AUTHENTICATION_USER, e3dc_user);
		protocol.appendValue(&authenContainer, TAG_RSCP_AUTHENTICATION_PASSWORD, e3dc_password);
		// append sub-container to root container
		protocol.appendValue(&rootValue, authenContainer);
		// free memory of sub-container as it is now copied to rootValue
		protocol.destroyValueData(authenContainer);
	} else {
		rInfo("Generating request for historical data\n");
		// request power data information
		protocol.appendValue(&rootValue, TAG_EMS_REQ_POWER_PV);
		protocol.appendValue(&rootValue, TAG_EMS_REQ_POWER_BAT);
		protocol.appendValue(&rootValue, TAG_EMS_REQ_POWER_HOME);
		protocol.appendValue(&rootValue, TAG_EMS_REQ_POWER_GRID);
		protocol.appendValue(&rootValue, TAG_EMS_REQ_POWER_ADD);

		// request battery information
//        SRscpValue batteryContainer;
//        protocol.createContainerValue(&batteryContainer, TAG_BAT_REQ_DATA);
//        protocol.appendValue(&batteryContainer, TAG_BAT_INDEX, (uint8_t)0);
//        protocol.appendValue(&batteryContainer, TAG_BAT_REQ_RSOC);
//        protocol.appendValue(&batteryContainer, TAG_BAT_REQ_MODULE_VOLTAGE);
//        protocol.appendValue(&batteryContainer, TAG_BAT_REQ_CURRENT);
//        protocol.appendValue(&batteryContainer, TAG_BAT_REQ_STATUS_CODE);
//        protocol.appendValue(&batteryContainer, TAG_BAT_REQ_ERROR_CODE);
//        // append sub-container to root container
//        protocol.appendValue(&rootValue, batteryContainer);
//        // free memory of sub-container as it is now copied to rootValue
//        protocol.destroyValueData(batteryContainer);
		// request db information
		SRscpValue dbContainer;
		protocol.createContainerValue(&dbContainer, spanTag);

		time_t end = start.seconds + span.seconds;
		time_t s = start.seconds;
		rDebug("Start time: %s", ctime(&s));
		rDebug("interval: %d, Span seconds: %d", (int )interval.seconds, (int ) span.seconds);
		rDebug("End time: %s", ctime(&end));
		protocol.appendValue(&dbContainer, TAG_DB_REQ_HISTORY_TIME_START, start);
		protocol.appendValue(&dbContainer, TAG_DB_REQ_HISTORY_TIME_INTERVAL, interval);
		protocol.appendValue(&dbContainer, TAG_DB_REQ_HISTORY_TIME_SPAN, span);
		protocol.appendValue(&rootValue, dbContainer);
		protocol.destroyValueData(&dbContainer);

	}

	// create buffer frame to send data to the S10
	protocol.createFrameAsBuffer(frameBuffer, rootValue.data, rootValue.length, true); // true to calculate CRC on for transfer
	// the root value object should be destroyed after the data is copied into the frameBuffer and is not needed anymore
	protocol.destroyValueData(rootValue);

	return 0;
}

//
// loop through authentication and request data
//
static void readerLoop(void) {
	RscpProtocol protocol;
	bool bStopExecution = false;
	int loop_count = 0;

	while (!bStopExecution) {
		//--------------------------------------------------------------------------------------------------------------
		// RSCP Transmit Frame Block Data
		//--------------------------------------------------------------------------------------------------------------
		SRscpFrameBuffer frameBuffer;
		memset(&frameBuffer, 0, sizeof(frameBuffer));

		// create an RSCP frame with requests to some example data
		createRequest(&frameBuffer);

		// check that frame data was created
		if (frameBuffer.dataLength > 0) {
			// resize temporary encryption buffer to a multiple of AES_BLOCK_SIZE
			std::vector<uint8_t> encryptionBuffer;
			encryptionBuffer.resize(ROUNDUP(frameBuffer.dataLength, AES_BLOCK_SIZE));
			// zero padding for data above the desired length
			memset(&encryptionBuffer[0] + frameBuffer.dataLength, 0, encryptionBuffer.size() - frameBuffer.dataLength);
			// copy desired data length
			memcpy(&encryptionBuffer[0], frameBuffer.data, frameBuffer.dataLength);
			// set continues encryption IV
			aesEncrypter.SetIV(ucEncryptionIV, AES_BLOCK_SIZE);
			// start encryption from encryptionBuffer to encryptionBuffer, blocks = encryptionBuffer.size() / AES_BLOCK_SIZE
			aesEncrypter.Encrypt(&encryptionBuffer[0], &encryptionBuffer[0], encryptionBuffer.size() / AES_BLOCK_SIZE);
			// save new IV for next encryption block
			memcpy(ucEncryptionIV, &encryptionBuffer[0] + encryptionBuffer.size() - AES_BLOCK_SIZE,
			AES_BLOCK_SIZE);

			// send data on socket
			int iResult = SocketSendData(iSocket, &encryptionBuffer[0], encryptionBuffer.size());
			if (iResult < 0) {
				rError("Socket send error %i. errno %i\n", iResult, errno);
				bStopExecution = true;
			} else {
				// go into receive loop and wait for response
				receiveLoop(bStopExecution);
			}
		}
		// free frame buffer memory
		protocol.destroyFrameData(&frameBuffer);

		//bStopExecution = true;
		loop_count++;
		if (loop_count > 1) {
			bStopExecution = true;
		}

		// main loop sleep / cycle time before next request
		sleep(1);
	}
}

//
// real RSCP reader
//
int RscpReader() {
	// connect to server
	rInfo("Connecting to server %s:%i\n", ip_addr, port_number);
	iSocket = SocketConnect(ip_addr, port_number);
	if (iSocket < 0) {
		rError("Connection failed\n");
		return (1);
	}
	rInfo("Connected successfully\n");

	// reset authentication flag
	iAuthenticated = 0;

	// create AES key and set AES parameters
	{
		// initialize AES encryptor and decryptor IV
		memset(ucDecryptionIV, 0xff, AES_BLOCK_SIZE);
		memset(ucEncryptionIV, 0xff, AES_BLOCK_SIZE);

		// limit password length to AES_KEY_SIZE
		int iPasswordLength = strlen(aes_password);
		if (iPasswordLength > AES_KEY_SIZE)
			iPasswordLength = AES_KEY_SIZE;

		// copy up to 32 bytes of AES key password
		uint8_t ucAesKey[AES_KEY_SIZE];
		memset(ucAesKey, 0xff, AES_KEY_SIZE);
		memcpy(ucAesKey, aes_password, iPasswordLength);

		// set encryptor and decryptor parameters
		aesDecrypter.SetParameters(AES_KEY_SIZE * 8, AES_BLOCK_SIZE * 8);
		aesEncrypter.SetParameters(AES_KEY_SIZE * 8, AES_BLOCK_SIZE * 8);
		aesDecrypter.StartDecryption(ucAesKey);
		aesEncrypter.StartEncryption(ucAesKey);
	}

	readerLoop();
	rDebug("readerLoop ended");

	// close socket connection
	SocketClose(iSocket);
	iSocket = -1;
	return errno;
}

//
// wrapper, setting the time and interval
int RscpReader_Day(const char * user, const char *pw, const char *aes, const char * ip, int port, struct tm *l, bool b) {
	rDebug("RscpReader_Day");
	brief = b;
	e3dc_user = user;
	e3dc_password = pw;
	aes_password = aes;
	ip_addr = ip;
	port_number = port;
	spanTag = TAG_DB_REQ_HISTORY_DATA_DAY;
	start.seconds = mktime(l);
	start.nanoseconds = 0;
	if (brief) {
		interval.seconds = 24 * 3600;
	} else {
		interval.seconds = 15 * 60; // 15 minutes
	}
	interval.nanoseconds = 0;
	span.seconds = 24 * 3600-1;
	span.nanoseconds = 0;
	return RscpReader();
}

int RscpReader_Month(const char * user, const char *pw, const char *aes, const char * ip, int port, struct tm *l, bool b) {
	rDebug("RscpReader_Month");
	brief = b;
	e3dc_user = user;
	e3dc_password = pw;
	aes_password = aes;
	ip_addr = ip;
	port_number = port;
	spanTag = TAG_DB_REQ_HISTORY_DATA_MONTH;
	start.seconds = mktime(l);
	start.nanoseconds = 0;

	interval.nanoseconds = 0;
	if (l->tm_mon == 11) {
		l->tm_mon = 0;
		l->tm_year++;
		span.seconds = mktime(l) - start.seconds - 1;
	} else {
		l->tm_mon++;
		span.seconds = mktime(l) - start.seconds - 1;
	}
	span.nanoseconds = 0;
	if (brief) {
		interval.seconds = start.seconds + span.seconds;
	} else {
		interval.seconds = 24 * 3600; // 1 day
	}
	return RscpReader();
}

int RscpReader_Year(const char * user, const char *pw, const char *aes, const char * ip, int port, struct tm *l, bool b) {
	rDebug("RscpReader_Year");
	brief = b;
	e3dc_user = user;
	e3dc_password = pw;
	aes_password = aes;
	ip_addr = ip;
	port_number = port;
	spanTag = TAG_DB_REQ_HISTORY_DATA_YEAR;
	// only the sum of the year makes sense, month do not have equal length
	start.seconds = mktime(l);
	start.nanoseconds = 0;

	interval.nanoseconds = 0;
	l->tm_year++;
	span.seconds = mktime(l) - start.seconds - 1;
	span.nanoseconds = 0;
	interval.seconds = span.seconds; // does not matter, only sum is valid
	return RscpReader();
}
