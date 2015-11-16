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

			append_OptionToList(pUriOptionsListBegin, OPT_NUM_URI_QUERY, pCurUriPartBegin, cnt); //copy & alloc mem

			pCurUriPartBegin = pCurUriPartBegin+cnt+1;//points to char following delimiter '&'
			cnt=0;
		}
		else cnt++;

		QueryStr++;
	}

	//last uri part which is not a query string
	if(cnt != 0){
		append_OptionToList(pUriOptionsListBegin, OPT_NUM_URI_QUERY, pCurUriPartBegin, cnt); //copy & alloc last uri part
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

			append_OptionToList(pUriOptionsListBegin, OPT_NUM_URI_PATH, pCurUriPartBegin, cnt); //copy & alloc mem

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
		append_OptionToList(pUriOptionsListBegin, OPT_NUM_URI_PATH, pCurUriPartBegin, cnt); //copy & alloc last uri part
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

uint8_t* CoAP_UriQuery_strstr(CoAP_option_t* pUriOpt, const char* str) {
	if(pUriOpt == NULL) return NULL;
	if(pUriOpt->Number != OPT_NUM_URI_QUERY) return NULL;
	if(pUriOpt->Length == 0 || pUriOpt->Length > 255) return NULL;

	//problem pUriOpt data is not a C-String (terminating zero is missing)
	//todo: implement strstr without memcpy and big local mem
	char pUriQuery[255]; //max len of query opt

	//Copy into pUriQuery
	coap_memcpy(pUriQuery, pUriOpt->Value, pUriOpt->Length);
	pUriQuery[pUriOpt->Length]=0; //terminating char

	char* pLoc = (char*)coap_strstr(pUriQuery,str);
	if(pLoc == NULL) return NULL;

	return &(pUriOpt->Value[(pLoc-pUriQuery)]); //return pointer to the first occurrence of str in Opt.val, (pLoc-pUriQuery) = actual offset
}

uint8_t* CoAP_UriQuery_GetValueAfterPrefix(CoAP_option_t* pUriOpt, const char* prefixStr, uint8_t* pValueLen){
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

bool CoAP_UriQuery_KeyCorrect(CoAP_option_t* pUriOpt, const char* Key){

	uint8_t KeyLen;
	uint8_t* pVal = CoAP_UriQuery_GetValueAfterPrefix(pUriOpt, "key=", &KeyLen);


	if(pVal == NULL || KeyLen != coap_strlen(Key)) return false;

	int i=0;
	for(;i<KeyLen;i++) {
		if(Key[i]!=pVal[i]) {
			return false;
		}
	}
	return true;
}

//
//CoAp_Result_t dbgUriPath(CoAP_uri_t* uri)
//{
//	INFO("\r\n>>Uri Path\r\n");
//	INFO("- Path=/");
//
//	for(int i=0; i<uri->UriPathLevels;i++)
//	{
//		INFO("%s/",uri->UriPath[i]);
//	}
//	INFO("\r\n");
//
//	return COAP_OK;
//}
//
//
//CoAp_Result_t TransformUriToString(CoAP_uri_t* uri, uint8_t* pStr)
//{
//	pStr[0] = 0; //string termination
//	for(int i=0; i<uri->UriPathLevels;i++)
//	{
//		strcat(pStr,"/");
//		strcat(pStr,uri->UriPath[i]);
//	}
//	return

