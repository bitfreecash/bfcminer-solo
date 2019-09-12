/*
 * miner.cpp
 *
 *  Created on: Jun 4, 2019
 *      Author: goose9
 */

#include "miner.h"
#include <unistd.h>
#include <iostream>
#include "block.h"
#include <pthread.h>
#include "streams.h"
#include "utilstrencodings.h"
#include "version.h"
#include "base58.h"
#include "standard.h"
#include <jsoncpp/json/json.h>
#include "blake2b.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "util.h"
#include "main.h"
#include "compat.h"

volatile bool miner_running = false;

extern Json::Value next_mine_block;
extern pthread_mutex_t mine_block_mutex;

extern pthread_cond_t submit_sem;
extern pthread_mutex_t submit_mutex;
extern CBlock submit_block;
extern pthread_mutex_t submit_block_mutex;

int cuckoo_mean(unsigned char *base64, int length, const uint256 &ptarget, int device, int &good_sols, CBlock &block);
uint64_t timestamp();

#define PROOFSIZE 42
char headhash[32] = { 0 };

void *miner_thread(void *arg)
{
	struct miner_cb *cb = (struct miner_cb *)arg;
	miner_running = true;

	printf("miner thread %d started\n", cb->minerid);

	CBlock block;
	unsigned char base64[100] = { 0 };

	uint256 nTarget;

	uint64_t total_tm_duration = 0, recent_tm_duration = 0; // ms
	unsigned int total_sols = 0, recent_sols = 0;

	while(miner_running) {
again:
		if(!miner_running) break;

		pthread_mutex_lock(&mine_block_mutex);
		if(cb->next) {
			block.SetNull();
			block.nVersion = next_mine_block["version"].asInt();
			block.hashPrevBlock.SetHex(next_mine_block["previousblockhash"].asCString());
			block.hashMerkleRoot.SetHex(next_mine_block["merkleroot"].asCString());
			block.nTime = next_mine_block["curtime"].asInt();
			block.nBits = std::stoi(next_mine_block["bits"].asString(), 0, 16);

			if(show_info) printf("miner thread %d start mining new block\n", cb->minerid);

			if(show_info) printf("nTarget = %s\n", next_mine_block["target"].asString().c_str());
			nTarget.SetHex(next_mine_block["target"].asString());

			int nHeight = next_mine_block["height"].asInt();

			block.vtx.clear();

			CDataStream stream(ParseHex(next_mine_block["coinbasetxn"]["data"].asCString()), SER_NETWORK, PROTOCOL_VERSION);
            CTransaction coinbase(deserialize, stream);
			block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));

			Json::Value trans = next_mine_block["transactions"];
			for(Json::Value::ArrayIndex i = 0; i < trans.size(); i ++) {
				CDataStream stream(ParseHex(trans[i]["data"].asCString()), SER_NETWORK, PROTOCOL_VERSION);
            	CTransaction tx(deserialize, stream);
			    block.vtx.push_back(MakeTransactionRef(std::move(tx)));
			}

			if(show_info) std::cout << "Block: " << block.ToString() << std::endl;

			blake2b(headhash, sizeof(headhash), (const void *)BEGIN(block.nVersion), END(block.nBits) - BEGIN(block.nVersion), 0, 0);

			cb->next = false;
			sol_found = false;

			pthread_mutex_unlock(&mine_block_mutex);
		} else {
			pthread_mutex_unlock(&mine_block_mutex);
			sleep(2);
			goto again;
		}

		memcpy(base64 + 0, EncodeBase64((const unsigned char *)headhash, 32).c_str(), 44);

		block.nNonce.SetNull();
		arith_uint256 nonce = UintToArith256(block.nNonce);
		nonce += cb->start;

		arith_uint256 maxNonce;
		maxNonce.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

		uint64_t tm_start = timestamp();
		int last_good_sols = 0;

		while(!cb->next && !sol_found) {
            block.nNonce = ArithToUint256(nonce);
			memcpy(base64 + 44, EncodeBase64((const unsigned char *)block.nNonce.begin(), 32).c_str(), 44);

			int good_sols = 0;
			int ret = cuckoo_mean(base64, sizeof(base64), nTarget, cb->minerid, good_sols, block);

			total_sols += good_sols;
			recent_sols += good_sols;
			last_good_sols += good_sols;

            if(ret < 0) {
                printf("miner error, thread exit\n");
                return NULL;
            } else if(ret > 0) {
				sol_found = true;

            	pthread_mutex_lock(&submit_block_mutex);
            	submit_block = block;
				submit_block.nSolutions.clear();
				for(int i = 0; i < PROOFSIZE; i ++) {
					submit_block.nSolutions.push_back(block.nSolutions[i]);
				}

				if(show_info) {
					printf("miner thread %d Found Solution: \n", cb->minerid);
					for(int i = 0; i < PROOFSIZE; i ++)
						printf("%u, ", block.nSolutions[i]);
					printf("\n");
					printf("Solution block: %s\n", submit_block.ToString().c_str());
				}

            	pthread_mutex_unlock(&submit_block_mutex);

				pthread_mutex_lock(&submit_mutex);
				pthread_cond_signal(&submit_sem);
				pthread_mutex_unlock(&submit_mutex);
            }

mine_next_nonce:
			nonce += cb->stride;

            if (nonce >= maxNonce)
                break;
		}

		uint64_t tm_end = timestamp();
		uint64_t tm_duration = (tm_end - tm_start) / 1000000;

		total_tm_duration += tm_duration;
		recent_tm_duration += tm_duration;

		if(show_info) {
			printf("GPU %d used time %lums, %d good solutions, hashrate %.2f\n",
				cb->minerid,
				tm_duration,
				last_good_sols,
				1000 * last_good_sols / (tm_duration / 1000.f));
		}

		if(recent_tm_duration >= 300000) {
			printf("GPU %d total hashrate %.2f H/s, recent hashrate %.2f H/s\n",
					cb->minerid,
					1000 * total_sols / (total_tm_duration / 1000.f),
					1000 * recent_sols / (recent_tm_duration / 1000.f));
			recent_tm_duration = 0;
			recent_sols = 0;
		}
	}

	return NULL;
}
