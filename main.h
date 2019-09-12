#ifndef __MAIN_H__
#define __MAIN_H__

#include <pthread.h>
#include <unistd.h>

#define MAX_MINER_THREADS 16
struct miner_cb {
	int minerid;
	int start;
	int stride;
	bool next;
};

extern struct miner_cb g_mcb[MAX_MINER_THREADS];
extern int cardsNum;

extern bool sol_found;

extern bool show_verbose;
extern bool show_info;

extern char node_host[256];
extern char node_port[256];
extern char rpc_user[256];
extern char rpc_password[256];
extern char miner_address[256];

#endif
