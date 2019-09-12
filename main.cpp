#include <iostream>
#include "rpc.h"
#include "miner.h"
#include "submiter.h"
#include <jsoncpp/json/json.h>
#include <pthread.h>
#include "block.h"
#include <unistd.h>
#include "main.h"
#include <cuda_runtime.h>
#include "compat.h"

#if _MSC_VER>=1900
_ACRTIMP_ALT FILE* __cdecl __acrt_iob_func(unsigned);
#ifdef __cplusplus 
extern "C"
#endif
FILE* __cdecl __iob_func(unsigned i) {
	return __acrt_iob_func(i);
}
#endif

pthread_t threads[MAX_MINER_THREADS + 2];

pthread_mutex_t rpc_mutex = PTHREAD_MUTEX_INITIALIZER;

Json::Value next_mine_block;
pthread_mutex_t mine_block_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t submit_sem = PTHREAD_COND_INITIALIZER;
pthread_mutex_t submit_mutex = PTHREAD_MUTEX_INITIALIZER;
CBlock submit_block;
pthread_mutex_t submit_block_mutex = PTHREAD_MUTEX_INITIALIZER;

struct miner_cb g_mcb[MAX_MINER_THREADS];
int cardsNum = 1;

bool sol_found = false;

bool show_verbose = false;
bool show_info = false;

/**
 * -h bitfree node host
 * -p bitfree node port
 * -u rpcuser
 * -P rpcpasswd
 * -m miner address
 * -i basic info
 * -v verbose log
 **/
static char const short_options[] = "h:p:u:P:m:iv";

const char* default_node_host = "127.0.0.1";
const char* default_node_port = "28332";
const char* default_rpc_user = "miner";
const char* default_rpc_password = "miner123";
const char* default_miner_address = "FJhruFy3X3N8dHfGyRS5qbYXL9Wg7CKco2";

char node_host[256] = { 0 };
char node_port[256] = { 0 };
char rpc_user[256] = { 0 };
char rpc_password[256] = { 0 };
char miner_address[256] = { 0 };
int n_miner_threads = 1;

void parse_arg(int key, char *arg)
{
    switch(key) {
    case 'h':
        memset(node_host, 0, sizeof(node_host));
        memcpy(node_host, arg, strlen(arg));
        break;
    case 'p':
        memset(node_port, 0, sizeof(node_port));
        memcpy(node_port, arg, strlen(arg));
        break;
    case 'u':
        memset(rpc_user, 0, sizeof(rpc_user));
        memcpy(rpc_user, arg, strlen(arg));
        break;
    case 'P':
        memset(rpc_password, 0, sizeof(rpc_password));
        memcpy(rpc_password, arg, strlen(arg));
        break;
    case 'm':
        memset(miner_address, 0, sizeof(miner_address));
        memcpy(miner_address, arg, strlen(arg));
        break;
    case 'i':
        show_info = true;
        break;
    case 'v':
        show_info = true;
        show_verbose = true;
        break;
	case 't':
		n_miner_threads = atoi(arg);
		break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
	char c = 'c';

#ifdef _WIN32
	setvbuf(stdout, NULL, _IONBF, 0);
#else
	setvbuf(stdout, NULL, _IOLBF, 0);
#endif

	printf("Bitfree solo miner 1.0\n");

    memcpy(node_host, default_node_host, strlen(default_node_host));
    memcpy(node_port, default_node_port, strlen(default_node_port));
    memcpy(rpc_user, default_rpc_user, strlen(default_rpc_user));
    memcpy(rpc_password, default_rpc_password, strlen(default_rpc_password));
    memcpy(miner_address, default_miner_address, strlen(default_miner_address));

	cudaGetDeviceCount(&cardsNum);
	printf("%d GPU cards found\n", cardsNum);
	if(cardsNum == 0) {
		fprintf(stderr, "There is no device.\n");
		return 0;
	}

	int device;
	for (device = 0; device < cardsNum; ++device) {
		cudaDeviceProp deviceProp;
		cudaGetDeviceProperties(&deviceProp, device);
		printf("Device %d: %s, memory %ldG, shared %ldK, register %d, has compute capability %d.%d.\n", device, deviceProp.name, deviceProp.totalGlobalMem >> 30, deviceProp.sharedMemPerBlock >> 10, deviceProp.regsPerBlock, deviceProp.major, deviceProp.minor);
	}

	n_miner_threads = cardsNum;

    int key;
    while(true) {
        key = getopt(argc, argv, short_options);
        if(key < 0)
            break;

        parse_arg(key, optarg);
    }

	int ret = pthread_create(&threads[0], NULL, rpc_thread, NULL);
	if(ret) {
		std::cout << "Failed to create rpc thread: " << ret << std::endl;
		goto out;
	}

	for(int i = 0; i < n_miner_threads; i ++) {
		g_mcb[i].minerid = i;
		g_mcb[i].start = i;
		g_mcb[i].stride = n_miner_threads;

		ret = pthread_create(&threads[2 + i], NULL, miner_thread, (void *)&g_mcb[i]);
		if(ret) {
			std::cout << "Failed to create miner thread: " << ret << std::endl;
			goto out;
		}
	}

	ret = pthread_create(&threads[1], NULL, submiter_thread, NULL);
	if(ret) {
		std::cout << "Failed to create submiter thread: " << ret << std::endl;
		goto out;
	}

	while(c != 'q') {
		/*std::cout << "input q to exit" << std::endl;
		std::cin >> c;*/
		sleep(10);
	}

out:
	if(rpc_running) {
		rpc_running = false;
		pthread_cancel(threads[0]);
	}
	if(miner_running) {
		miner_running = false;
		for(int i = 0; i < cardsNum; i ++) {
			pthread_cancel(threads[2 + i]);
		}
	}
	if(submiter_running) {
		submiter_running = false;
		pthread_mutex_lock(&submit_mutex);
		pthread_cond_broadcast(&submit_sem);
		pthread_mutex_unlock(&submit_mutex);
		pthread_cancel(threads[2]);
	}

	void *status;

	std::cout << "kill rpc thread if needed" << std::endl;
	pthread_join(threads[0], &status);

	std::cout << "kill submit thread if needed" << std::endl;
	pthread_join(threads[1], &status);

	std::cout << "kill miner threads if needed" << std::endl;
	for(int i = 0; i < cardsNum; i ++) {
		pthread_join(threads[2 + i], &status);
	}

	return 0;
}
