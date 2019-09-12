/*
 * submiter.cpp
 *
 *  Created on: Jun 4, 2019
 *      Author: goose9
 */

#include "submiter.h"
#include <unistd.h>
#include <iostream>
#include <curl/curl.h>
#include "block.h"
#include <pthread.h>
#include "streams.h"
#include "version.h"
#include "utilstrencodings.h"
#include "main.h"

volatile bool submiter_running = false;
extern pthread_cond_t submit_sem;
extern pthread_mutex_t submit_mutex;
extern pthread_mutex_t rpc_mutex;

extern CBlock submit_block;
extern pthread_mutex_t submit_block_mutex;

extern CURL *curl;

char rpc_submit_buffer[0x2000000] = { 0 };
const char *pattern = "{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\": \"submitblock\", \"params\": [\"%s\"] }";

extern char error_buffer[2048];

static size_t submitCallback(void *input, size_t uSize, size_t uCount, void *arg)
{
	if(show_info) printf("submit callback: %s\n", (char *)input);
	size_t length = uCount * uSize;

	return length;
}

static void send_submit_rpc()
{
	pthread_mutex_lock(&rpc_mutex);
	CURLcode res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &submitCallback);
	if(res != CURLE_OK) {
		std::cout << "Failed to set write callback: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, rpc_submit_buffer);
	if(res != CURLE_OK) {
		std::cout << "Failed to set CURLOPT_POSTFIELDS: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(rpc_submit_buffer));
	if(res != CURLE_OK) {
		std::cout << "Failed to set CURLOPT_POSTFIELDSIZE: " << res << std::endl;
		goto error_out;
	}

	if(show_info) printf("submit: %s\n", rpc_submit_buffer);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		std::cout << "Failed to curl_easy_perform: " << res << ":" << error_buffer << std::endl;
		goto error_out;
	}

	long httpCode;
	res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
	if(res != CURLE_OK) {
		std::cout << "Failed to curl_get_info CURLINFO_RESPONSE_CODE, err " << res << std::endl;
		goto error_out;
	}

	if(httpCode != 200) {
		std::cout << "Http Connection Failed with status: " << httpCode << std::endl;
		goto error_out;
	}

error_out:
	pthread_mutex_unlock(&rpc_mutex);
}

void *submiter_thread(void *arg)
{
	submiter_running = true;

	while(true) {
		pthread_mutex_lock(&submit_mutex);
		pthread_cond_wait(&submit_sem, &submit_mutex);
		pthread_mutex_unlock(&submit_mutex);

		if(!submiter_running) {
			break;
		}
		if(show_info) std::cout << "submiter_thread running ..." << std::endl;

		// generate rpc message
	    CDataStream ssData(SER_NETWORK, PROTOCOL_VERSION);

	    pthread_mutex_lock(&submit_block_mutex);
	    ssData << submit_block;
		pthread_mutex_unlock(&submit_block_mutex);

		memset(rpc_submit_buffer, 0, sizeof(rpc_submit_buffer));
		sprintf(rpc_submit_buffer, pattern, HexStr(ssData.str()).c_str());

		// submit rpc
		send_submit_rpc();
	}

	return NULL;
}
