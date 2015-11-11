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
#include "../../../lobaro.h"

//blockwise transfers options

CoAP_Result_t dbgBlkOption(CoAP_blockwise_option_t* blkOption)
{
	if(blkOption->Type == BLOCK_1)
	{
		INFO("\r\n>>Block1 (#%d) Option found\r\n", blkOption->Type);
	}

	if(blkOption->Type == BLOCK_2)
	{
		INFO("\r\n>>Block2 (#%d) Option found\r\n", blkOption->Type);
	}

	INFO("- BlockSize: %d\r\n", blkOption->BlockSize);
	INFO("- BlockNum: %d\r\n", blkOption->BlockNum);
	INFO("- MoreFlag: %d\r\n", blkOption->MoreFlag);

	return COAP_OK;
}

CoAP_Result_t AddBlkOptionToMsg(CoAP_Message_t* msg, CoAP_blockwise_option_t* blkOption)
{
	if(blkOption->Type != BLOCK_1 && blkOption->Type != BLOCK_2) return COAP_ERR_WRONG_OPTION;

	if(blkOption->BlockNum == 0 && blkOption->BlockSize == BLOCK_SIZE_16 && blkOption->MoreFlag == false ) //=> NUM, |M| and SZX are all coded to zero -> send zero-byte integer
	{
		return append_OptionToList(&(msg->pOptionsList), (uint16_t) blkOption->Type, NULL, 0);
	}

	uint32_t OptionValue = 0;

	//Block Size
	uint8_t szxCalc = (uint8_t)((blkOption->BlockSize)>>4); // divide by 16
	for(int i=6; i>=0; i--) //find highest bit, e.g. calc log2, i=7 forbidden
	{
		if(szxCalc & 1<<i)
		{
			OptionValue |= i;
			break;
		}
	}

	//More Flag
	if(blkOption->MoreFlag)OptionValue |= 1<<3;

	//Num Value
	OptionValue |= (blkOption->BlockNum) << 4;

	uint8_t wBuf[3];

	if(blkOption->BlockNum < 16)
	{
		//msg->Options[msg->OptionCount].Length = 1;
		//wBuf[0]=msg->Options[msg->OptionCount].Value[0] = (uint8_t)OptionValue;
		wBuf[0]= (uint8_t)OptionValue;
		return append_OptionToList(&(msg->pOptionsList), (uint16_t) blkOption->Type, wBuf, 1);
	}
	else if(blkOption->BlockNum < 4096)
	{
		//msg->Options[msg->OptionCount].Length = 2;
		wBuf[0]= (uint8_t)(OptionValue>>8);
		wBuf[1]=(uint8_t)(OptionValue & 0xff);
		return append_OptionToList(&(msg->pOptionsList), (uint16_t) blkOption->Type, wBuf, 2);
	}
	else
	{
		//msg->Options[msg->OptionCount].Length = 3;
		wBuf[0]=(uint8_t)(OptionValue>>16);
		wBuf[1]=(uint8_t)(OptionValue>>8);
		wBuf[2]=(uint8_t)(OptionValue & 0xff);
		return append_OptionToList(&(msg->pOptionsList), (uint16_t) blkOption->Type, wBuf, 3);
	}
}


CoAP_Result_t GetBlockOptionFromMsg(CoAP_Message_t* msg, CoAP_blockwise_option_type_t Type, CoAP_blockwise_option_t* BlkOption)
{
   CoAP_option_t* pOption = msg->pOptionsList;

   if(pOption == NULL) return COAP_ERR_NOT_FOUND;

   do
   {
	   if(pOption->Number == (uint16_t)Type)
	   	 {
	   		  uint16_t ValLength = pOption->Length;
	   		  uint32_t OptionValue = 0;
	   		  uint8_t* pVals = pOption->Value;

	   		  BlkOption->Type = Type;

	   		  if(pOption->Length > 3) return COAP_PARSE_MESSAGE_FORMAT_ERROR;

	   			if(ValLength == 0)
	   			{
	   				BlkOption->BlockNum = 0;
	   				BlkOption->BlockSize = BLOCK_SIZE_16;
	   				BlkOption->MoreFlag = false;
	   				return COAP_OK;
	   			}
	   			else if(ValLength == 1)
	   			{
	   				OptionValue = (uint32_t) pVals[0];
	   			}
	   			else if(ValLength == 2)
	   			{
	   				OptionValue = (((uint32_t) pVals[0]) << 8) | ((uint32_t) pVals[1]);
	   			}
	   			else if(ValLength == 3)
	   			{
	   				OptionValue = (((uint32_t) pVals[0]) << 16) | (((uint32_t) pVals[1]) << 8) | ((uint32_t) pVals[2]);
	   			}

	   			uint8_t SZX = OptionValue & 7;
	   			if(SZX == 7) return COAP_PARSE_MESSAGE_FORMAT_ERROR; //must lead to 4.00 Bad Request!
	   			BlkOption->BlockSize = 1<<(SZX+4);

	   			if(OptionValue & 8) BlkOption->MoreFlag = true;
	   			else BlkOption->MoreFlag = false;

	   			BlkOption->BlockNum = OptionValue >> 4;

	   			return COAP_OK;
	   	   }

	  if(pOption->next == NULL) return COAP_ERR_NOT_FOUND;
	  pOption = pOption->next;
   }while(1);

   return COAP_ERR_NOT_FOUND;
}

CoAP_Result_t GetBlock1OptionFromMsg(CoAP_Message_t* msg, CoAP_blockwise_option_t* BlkOption)
{
	return GetBlockOptionFromMsg(msg, BLOCK_1, BlkOption);
}

CoAP_Result_t GetBlock2OptionFromMsg(CoAP_Message_t* msg, CoAP_blockwise_option_t* BlkOption)
{
	return GetBlockOptionFromMsg(msg, BLOCK_2, BlkOption);
}

CoAP_Result_t CoAP_DefineGetRespPayload(CoAP_Message_t* pMsgReq, CoAP_Message_t* pMsgResp, uint8_t* pPayload, uint16_t payloadTotalSize, bool copyPl)
{
	CoAP_blockwise_option_t B2opt = {.Type = BLOCK_2};
	int32_t BytesToSend = 0;

	//is block2 option included in request (control usage)?
	if(GetBlock2OptionFromMsg(pMsgReq, &B2opt) == COAP_OK) //=found
	{
		//clients blocksize wish too big for us?
		if(B2opt.BlockSize > MAX_PAYLOAD_SIZE)
			B2opt.BlockSize = MAX_PAYLOAD_SIZE; //choose a smaller value (our maximum)

		int32_t TotalBytesLeft = ((int32_t)(payloadTotalSize)) - (int32_t)((B2opt.BlockSize) * (B2opt.BlockNum));
		if(TotalBytesLeft <= 0)
		{
			addTxtPayloadToMessage(pMsgResp, "block not existing");
			pMsgResp->Code=RESP_BAD_OPTION_4_02;
			return COAP_ERR_WRONG_OPTION;
		}

		if(TotalBytesLeft > B2opt.BlockSize)
		{
			BytesToSend = B2opt.BlockSize;
			B2opt.MoreFlag=true;
		}
		else BytesToSend = TotalBytesLeft;

		AddBlkOptionToMsg(pMsgResp, &B2opt);

		//set payload to calculated position of given payload buf
		CoAP_free_MsgPayload(&pMsgReq);
		if(copyPl) {
			pMsgResp->Payload = (uint8_t*)com_mem_get(BytesToSend); //alloc new buffer to copy data to send to
			memcpy(pMsgResp->Payload, &(pPayload[(B2opt.BlockSize) * (B2opt.BlockNum)]),BytesToSend);
			pMsgResp->PayloadBufSize = BytesToSend;
		} else {
			pMsgResp->Payload = &(pPayload[(B2opt.BlockSize) * (B2opt.BlockNum)]); //use external set buffer (will not be freed, must be static!)
			pMsgResp->PayloadBufSize = 0; //protect "external to msg" buffer
		}

	} else { //no block2 in request

		if(payloadTotalSize > MAX_PAYLOAD_SIZE){ //must use blockwise transfer?

			B2opt.BlockSize = MAX_PAYLOAD_SIZE;
			B2opt.BlockNum = 0;
			B2opt.MoreFlag = true;
			AddBlkOptionToMsg(pMsgResp, &B2opt);
			BytesToSend = MAX_PAYLOAD_SIZE;
		}else {
			BytesToSend = payloadTotalSize;
		}

		//set payload to beginning of given payload buf
		CoAP_free_MsgPayload(&pMsgReq);
		if(copyPl) {
			pMsgResp->Payload = (uint8_t*)com_mem_get(BytesToSend); //alloc new buffer to copy data to send to
			memcpy(pMsgResp->Payload, &(pPayload[0]), BytesToSend);
			pMsgResp->PayloadBufSize = BytesToSend;
		} else {
			pMsgResp->Payload = &(pPayload[0]); //use external set buffer (will not be freed, must be static!)
			pMsgResp->PayloadBufSize = 0; //protect "external to msg" buffer
		}
	}

	pMsgResp->PayloadLength = BytesToSend;
	return COAP_OK;
}


//useful if implementing blk2 raw without knowing in advance size of payload

//CoAP_blockwise_option_t blk2Opt = {.Type = BLOCK_2} ;
//
//if(GetBlock2OptionFromMsg(pReq, &blk2Opt) == COAP_OK) {
//	int32_t TotalBytesLeft = payloadSize - blk2Opt.BlockNum * blk2Opt.BlockSize;
//
//	if(TotalBytesLeft <= 0) {
//		addTxtPayloadToMessage(pResp, "block not existing");
//		pResp->Code=RESP_BAD_OPTION_4_02;
//		return COAP_OK;
//	}
//
//	if(TotalBytesLeft > blk2Opt.BlockSize) {
//		blk2Opt.MoreFlag=true;
//		pResp->PayloadLength = blk2Opt.BlockSize;
//	}
//	else {
//		blk2Opt.MoreFlag=false;
//		pResp->PayloadLength = blk2Opt.BlockSize - TotalBytesLeft;
//	}
//
//	AddBlkOptionToMsg(pResp, &blk2Opt);
//	pResp->Payload = (&(CoapInfoStringInFlash[0])) + blk2Opt.BlockNum * blk2Opt.BlockSize;
//
//
//}else {
//	blk2Opt.BlockSize = pResp->PayloadBufSize;
//	blk2Opt.MoreFlag = true;
//	pResp->Payload = (&(CoapInfoStringInFlash[0]));
//	pResp->PayloadLength = pResp->PayloadBufSize;
//}
//
//
//
//pResp->Code = RESP_SUCCESS_CONTENT_2_05;


