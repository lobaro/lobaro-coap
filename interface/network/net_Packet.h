/*******************************************************************************
 * Copyright (c)  2015  Dipl.-Ing. Tobias Rohde, http://www.lobaro.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************/
#ifndef COM_NET_PACKET_H
#define COM_NET_PACKET_H

typedef enum
{
	META_INFO_NONE,
	META_INFO_RF_PATH
}MetaInfoType_t;

typedef struct
{
	uint8_t HopCount;
	int32_t RSSI;
}MetaInfo_RfPath_t;

typedef union
{
	MetaInfo_RfPath_t RfPath;
}MetaInfoUnion_t;

typedef struct {
	MetaInfoType_t Type;
	MetaInfoUnion_t Dat;
}MetaInfo_t;

//################################
// general network packet
// received in callbacks and sendout
// in network send routines
//################################
typedef struct
{
	uint8_t* pData;
	uint16_t size;
	NetEp_t Sender;
	NetEp_t Receiver;
	MetaInfo_t MetaInfo;
}NetPacket_t;
//################################

void PrintRawPacket(NetPacket_t* pckt);

#endif
