/*
 * rpc.h
 *
 *  Created on: Jun 4, 2019
 *      Author: goose9
 */

#ifndef RPC_H_
#define RPC_H_


void *rpc_thread(void *arg);

extern volatile bool rpc_running;


#endif /* RPC_H_ */
