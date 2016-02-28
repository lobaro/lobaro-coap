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


//internal function
//QueryStr points to char AFTER ? in a uri query string e.g: 'O' in [...]myuri/bla?Option1=Val1&Option2=Val2
//todo: support percent encoding
static CoAP_Result_t _rom ParseUriQueryFromStringToOption(CoAP_option_t** pUriOptionsListBegin, char* QueryStr)
{
	char* pCurUriPartBegin = QueryStr;
	uint32_t cnt =0;

	while(*QueryStr != 0){ //str end
		if(*QueryStr == '&') {//query delimeter found
			if(cnt == 0){ //part begins with (another) delimiter -> skip it
				pCurUriPartBegin++;
				QueryStr++;
				continue;
			}

			CoAP_AppendOptionToList(pUriOptionsListBegin, OPT_NUM_URI_QUERY, pCurUriPartBegin, cnt); //copy & alloc mem

			pCurUriPartBegin = pCurUriPartBegin+cnt+1;//points to char following delimiter '&'
			cnt=0;
		}
		else cnt++;

		QueryStr++;
	}

	//last uri part which is not a query string
	if(cnt != 0){
		CoAP_AppendOptionToList(pUriOptionsListBegin, OPT_NUM_URI_QUERY, pCurUriPartBegin, cnt); //copy & alloc last uri part
	}

	return COAP_OK;
}



//Appends to list of coap options uri-path and uri-query options from uri-string
CoAP_Result_t _rom CoAP_AppendUriOptionsFromString(CoAP_option_t** pUriOptionsListBegin, char* UriStr)
{
//	Tested against following URI parts:
//	"halloWelt/wiegehts/dir?var1=val1&var2=val2&"
//	"halloWelt/wiegehts/dir"
//	"/halloWelt/wiegehts/dir?bla&bla&bla&bla"
//	"halloWelt/wiegehts/dir/?bla&bla=n&bla&bla"

	if(UriStr == NULL)
		return COAP_ERR_ARGUMENT;

	char* pCurUriPartBegin = UriStr;
	uint32_t cnt =0;

	while(*UriStr != 0){ //str end
		if(*UriStr == '/' || *UriStr == ' ' || *UriStr == '?'){ //uri delimeter found - do not count

			if(cnt == 0){ //part begins with (another) delimiter -> skip it
				if(*UriStr =='?'){
					return ParseUriQueryFromStringToOption(pUriOptionsListBegin,UriStr+1);
				}

				pCurUriPartBegin++;
				UriStr++;
				continue;
			}

			CoAP_AppendOptionToList(pUriOptionsListBegin, OPT_NUM_URI_PATH, pCurUriPartBegin, cnt); //copy & alloc mem

			pCurUriPartBegin = pCurUriPartBegin+cnt+1;//points to char following delimiter '/', ' ' or '?'

			if(*UriStr =='?'){ //case /dir?var1 -> "dir" = path, "var1"=query begin (path component only MAY end with '\')
				return	ParseUriQueryFromStringToOption(pUriOptionsListBegin, UriStr+1);
			}

			cnt=0;
		}
		else cnt++;

		UriStr++;
	}

	//last uri part which is not a query string
	if(cnt != 0){
		CoAP_AppendOptionToList(pUriOptionsListBegin, OPT_NUM_URI_PATH, pCurUriPartBegin, cnt); //copy & alloc last uri part
	}

	return COAP_OK;
}

CoAP_Result_t _rom CoAP_AddUriOptionsToMsgFromString(CoAP_Message_t* msg, char* UriStr)
{
	return CoAP_AppendUriOptionsFromString(&(msg->pOptionsList), UriStr);
}

//filters out any different to uripath options
//uses implicit ordering of uri options!
bool _rom CoAP_UriOptionsAreEqual(CoAP_option_t* OptListA, CoAP_option_t* OptListB){

	CoAP_option_t* CurOptA = OptListA;
	CoAP_option_t* CurOptB = OptListB;

	while(!(CurOptA==NULL && CurOptB==NULL))
	{
		while(CurOptA != NULL) {
			if(CurOptA->Number == OPT_NUM_URI_PATH)break;
			CurOptA = CurOptA->next;
		}

		while(CurOptB != NULL) {
			if(CurOptB->Number == OPT_NUM_URI_PATH)break;
			CurOptB = CurOptB->next;
		}

		if(!CoAP_OptionsAreEqual(CurOptA,CurOptB)) { //returns also true if both NULL! (implicit URI:"/")
			return false;
		}

		if(CurOptB != NULL)CurOptB = CurOptB->next;
		if(CurOptA != NULL)CurOptA = CurOptA->next;
	}
	return true;
}


void _rom CoAP_printUriOptionsList(CoAP_option_t* pOptListBegin)
{
	bool queryPos = false;
	int j;
	while(pOptListBegin != NULL)
	{
		if(pOptListBegin->Number == OPT_NUM_URI_PATH) {
			for(j=0; j< pOptListBegin->Length; j++){
				INFO("%c", pOptListBegin->Value[j]);
			}
			INFO("\\");
		} else if(pOptListBegin->Number == OPT_NUM_URI_QUERY) {
			if(!queryPos) {
				INFO("?");
				queryPos=true;
			}
			else INFO("&");

			for(j=0; j< pOptListBegin->Length; j++){
						INFO("%c", pOptListBegin->Value[j]);
			}
		}

		pOptListBegin = pOptListBegin->next;
	}
	INFO("\r\n");
}



uint8_t* CoAP_GetUriQueryVal(CoAP_option_t* pUriOpt, const char* prefixStr, uint8_t* pValueLen){
	if(pUriOpt == NULL) return NULL;
	if(pUriOpt->Number != OPT_NUM_URI_QUERY) return NULL;
	if(pUriOpt->Length == 0 || pUriOpt->Length > 255) return NULL;

	int prefixLen = coap_strlen(prefixStr);
	if(prefixLen >= pUriOpt->Length) return NULL;

	int i=0;
	for(;i< prefixLen; i++) {
		if(pUriOpt->Value[i] != prefixStr[i]) return NULL;
	}

	//prefix found
	if(pValueLen != NULL)
		*pValueLen = (pUriOpt->Length) - prefixLen;

	return &(pUriOpt->Value[prefixLen]);
}

int8_t CoAP_FindUriQueryVal(CoAP_option_t* pUriOpt, const char* prefixStr, int CmpStrCnt, ...) {
	va_list ap; //compare string pointer
	int i,j;
	char* pStr=NULL;
	bool Match=false;
	uint8_t* pUriQueryVal;
	uint8_t ValLen;

	pUriQueryVal = CoAP_GetUriQueryVal(pUriOpt, prefixStr, &ValLen);
	if(pUriQueryVal == NULL) return 0;//-1; //prefix not found, no uri-query

	va_start (ap, CmpStrCnt);         // Initialize the argument list.
	for(i=1;i<CmpStrCnt+1;i++) { //loop over all string arguments to compare the found uri query against
		pStr = va_arg(ap, char*);

		if(coap_strlen(pStr) != ValLen) continue; //already length does not match -> try next given string

		Match=true;
		for(j=0;j< ValLen; j++) {
			if(pStr[j] != pUriQueryVal[j]){
				Match=false;
				break;
			}
		}
		if(Match==false) continue;
		//found argument string matching to uri-query value
		va_end (ap);
		return i; //return argument number of match
	}

	 va_end (ap);
	 return 0; //not found
}


