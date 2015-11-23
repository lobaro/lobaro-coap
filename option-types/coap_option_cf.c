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
#include "../coap.h"

//add content-format option
CoAP_Result_t _rom AddCfOptionToMsg(CoAP_Message_t* msg, uint16_t cf)
{
	uint8_t wBuf[2];

	//msg->Options[msg->OptionCount].Number = OPT_NUM_CONTENT_FORMAT;
	if(cf==0)
	{
		//msg->Options[msg->OptionCount].Length = 0;
		CoAP_AppendOptionToList(&(msg->pOptionsList), OPT_NUM_CONTENT_FORMAT ,wBuf, 0);
	}
	else if(cf <= 0xff)
	{
		wBuf[0]=(uint8_t)cf;
		//msg->Options[msg->OptionCount].Length = 1;
		CoAP_AppendOptionToList(&(msg->pOptionsList), OPT_NUM_CONTENT_FORMAT ,wBuf, 1);
	}
	else {
		wBuf[0]=(uint8_t)(cf & 0xff);
		wBuf[1]=(uint8_t)(cf>>8);
		//msg->Options[msg->OptionCount].Length = 2;
		CoAP_AppendOptionToList(&(msg->pOptionsList), OPT_NUM_CONTENT_FORMAT ,wBuf, 2);
	}

	return COAP_OK;
}
