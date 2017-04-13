/*
 * LATRDDefinitions.h
 *
 *  Created on: 23 Feb 2017
 *      Author: gnx91527
 */

#ifndef INCLUDE_LATRDDEFINITIONS_H_
#define INCLUDE_LATRDDEFINITIONS_H_

namespace Eiger
{

	typedef struct
	{
		uint32_t frame_number;

		uint32_t shapeSizeX;
		uint32_t shapeSizeY;
		uint32_t shapeSizeZ;

		uint32_t blob_size;
	} FrameHeader;

	static const size_t raw_buffer_size    = 17942760 + sizeof(FrameHeader); // 4,485,690 pixels at 32 bit pixel depth
	static const size_t frame_size         = 17942760 + sizeof(FrameHeader); // 4,485,690 pixels at 32 bit pixel depth

	static const int image_data_imaged_part = 2; // Image dimensions are on the 2nd image part
	static const int image_data_blob_part = 3; // Image data is on the 3rd image part

	const std::string GLOBAL_HEADER_TYPE = "dheader-1.0";
	const std::string IMAGE_HEADER_TYPE = "dimage-1.0";
	const std::string END_HEADER_TYPE = "dseries_end-1.0";

	const std::string HEADER_TYPE_KEY = "htype";
	const std::string FRAME_KEY = "frame";

	enum EigerMessageType { GLOBAL_HEADER, IMAGE_DATA, END_OF_STREAM};
}



#endif /* INCLUDE_LATRDDEFINITIONS_H_ */
