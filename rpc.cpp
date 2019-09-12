/*
 * rpc.cpp
 *
 *  Created on: Jun 4, 2019
 *      Author: goose9
 */

#include "rpc.h"
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>
#include <cstring>
#include <cassert>
#include "main.h"
#include "compat.h"

volatile bool rpc_running = false;

extern Json::Value next_mine_block;
extern pthread_mutex_t mine_block_mutex;

extern pthread_mutex_t rpc_mutex;
CURL *curl = NULL;

char g_block_buffer[0x2000000] = { 0 };
size_t g_block_buffer_length = 0;
char g_block_buffer_prv[0x2000000] = { 0 };
size_t g_block_buffer_length_prv = 0;
char error_buffer[2048] = { 0 };

curl_off_t total_length;
int64_t current_length;

static size_t writeCallback(void *input, size_t uSize, size_t uCount, void *arg)
{
	size_t length = uCount * uSize;

	if(show_verbose) printf("writeCallback: %s\n", (char *)input);

	if(current_length < total_length) {
		memcpy(g_block_buffer + current_length, input, length);
		current_length += length;
	}

	if(current_length < total_length) {
		return length;
	}

	g_block_buffer_length = total_length;

	//If same with previous buffer, don't process (ignore tail as curtime)
	if(g_block_buffer_length == g_block_buffer_length_prv && strncmp(g_block_buffer, g_block_buffer_prv, g_block_buffer_length - 187) == 0) {
		return length;
	}

	Json::CharReaderBuilder builder;
	builder["collectComments"] = false;

	Json::CharReader *reader = builder.newCharReader();

	Json::Value root;
	JSONCPP_STRING errs;

	bool ok = reader->parse((char *)g_block_buffer, (char *)g_block_buffer + g_block_buffer_length - 1, &root, &errs);
	if(!ok) {
		std::cout << "Json RPC parse err: " << errs << std::endl;
		goto out_parse;
	}

	if(!root.isMember("result") || root["result"].isNull()) {
		std::cout << "Cannot find result in jsonrpc" << std::endl;
		goto out_parse;
	}

	pthread_mutex_lock(&mine_block_mutex);
	next_mine_block = root["result"];
	for(int i = 0; i < cardsNum; i ++) {
		g_mcb[i].next = true;
	}
	pthread_mutex_unlock(&mine_block_mutex);

out_parse:
	return length;
}

void *rpc_thread(void *arg)
{
	const char *pat = "{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\": \"getblocktemplate\", \"params\": [ { \"rules\" : [\"segwit\"], \"mineraddress\" : \"%s\" } ] }";
	char buffer[512] = { 0 };
	sprintf(buffer, pat, miner_address);

	pthread_mutex_lock(&rpc_mutex);
	curl = curl_easy_init();
	if(!curl) {
		std::cout << "Failed to curl_easy_init()" << std::endl;
		return 0;
	}

    char urlbuf[256] = { 0 };
    sprintf(urlbuf, "http://%s:%s", node_host, node_port);
	CURLcode res;
	res = curl_easy_setopt(curl, CURLOPT_URL, urlbuf);
	if(res != CURLE_OK) {
		std::cout << "Failed to CURLOPT_URL: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
	if(res != CURLE_OK) {
		std::cout << "Failed to set CURLOPT_ERRORBUFFER: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
	if(res != CURLE_OK) {
		std::cout << "Failed to set CURLOPT_TIMEOUT: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_POST, 1);
	if(res != CURLE_OK) {
		std::cout << "Failed to set CURLOPT_POST: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_USERNAME, rpc_user);
	if(res != CURLE_OK) {
		std::cout << "Failed to set user: " << res << std::endl;
		goto error_out;
	}

	res = curl_easy_setopt(curl, CURLOPT_PASSWORD, rpc_password);
	if(res != CURLE_OK) {
		std::cout << "Failed to set password: " << res << std::endl;
		goto error_out;
	}
	pthread_mutex_unlock(&rpc_mutex);

	rpc_running = true;

	while(rpc_running) {
		pthread_mutex_lock(&rpc_mutex);

		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCallback);
		if(res != CURLE_OK) {
			std::cout << "Failed to set write callback: " << res << std::endl;
			goto error_out;
		}

		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
		if(res != CURLE_OK) {
			std::cout << "Failed to set CURLOPT_POSTFIELDS: " << res << std::endl;
			goto error_out;
		}

		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(buffer));
		if(res != CURLE_OK) {
			std::cout << "Failed to set CURLOPT_POSTFIELDSIZE: " << res << std::endl;
			break;
		}

		memcpy(g_block_buffer_prv, g_block_buffer, sizeof(g_block_buffer));
		g_block_buffer_length_prv = g_block_buffer_length;

		memset(g_block_buffer, 0, sizeof(g_block_buffer));
		current_length = 0;

		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			std::cout << "Failed to curl_easy_perform: " << res << " : " << error_buffer << std::endl;
			goto sleep_continue;
		}

		long httpCode;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		if(res != CURLE_OK) {
			std::cout << "Failed to curl_get_info CURLINFO_RESPONSE_CODE, err " << res << std::endl;
			goto sleep_continue;
		}

		if(httpCode != 200) {
			std::cout << "Http Connection Failed with status: " << httpCode << std::endl;
			goto sleep_continue;
		}

#ifdef _WIN32
		double len;
		res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &len);
		if(res != CURLE_OK) {
			std::cout << "Failed to curl_get_info CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, err " << res << std::endl;
		}
		total_length = (int64_t)len;
#else
		res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total_length);
		if (res != CURLE_OK) {
			std::cout << "Failed to curl_get_info CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, err " << res << std::endl;
		}
#endif

sleep_continue:
		pthread_mutex_unlock(&rpc_mutex);
		sleep(10);
	}
	rpc_running = false;

error_out:
	curl_easy_cleanup(curl);
	curl = NULL;
	pthread_mutex_unlock(&rpc_mutex);

	return NULL;
}
