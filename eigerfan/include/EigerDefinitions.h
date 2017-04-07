/*
 * EigerDefinitions.h
 *
 *  Created on: 6 Apr 2017
 *      Author: vtu42223
 */

#ifndef INCLUDE_EIGERDEFINITIONS_H_
#define INCLUDE_EIGERDEFINITIONS_H_

namespace Eiger {

	// Eiger constants
	const std::string STREAM_PORT_NUMBER = "9999";

	const std::string GLOBAL_HEADER_TYPE = "dheader-1.0";
	const std::string IMAGE_HEADER_TYPE = "dimage-1.0";
	const std::string END_HEADER_TYPE = "dseries_end-1.0";

	const std::string HEADER_DETAIL_ALL = "all";
	const std::string HEADER_DETAIL_BASIC = "basic";
	const std::string HEADER_DETAIL_NONE = "none";

	const std::string HEADER_TYPE_KEY = "htype";
	const std::string HEADER_DETAIL_KEY = "header_detail";
	const std::string SERIES_KEY = "series";

	const std::string END_STREAM_MESSAGE = "{\"htype\": \"dseries_end-1.0\", \"series\": 1}";

	// EigerFan related constants
	const int MORE_MESSAGES = 1;

	const std::string CONTROL_KILL = "KILL";
	const std::string CONTROL_STATUS = "STATUS";
	const std::string CONTROL_CLOSE = "CLOSE";

	enum EigerFanState { WAITING_CONSUMERS,WAITING_STREAM,DSTR_HEADER,DSTR_IMAGE,DSTR_END,KILL_REQUESTED};

	static const std::string STATE_WAITING_CONSUMERS = "WAITING_CONSUMERS";
	static const std::string STATE_WAITING_STREAM = "WAITING_STREAM";
	static const std::string STATE_DSTR_HEADER = "DSTR_HEADER";
	static const std::string STATE_DSTR_IMAGE = "DSTR_IMAGE";
	static const std::string STATE_DSTR_END = "DSTR_END";
	static const std::string STATE_KILL_REQUESTED = "KILL_REQUESTED";
}

#endif /* INCLUDE_EIGERDEFINITIONS_H_ */
