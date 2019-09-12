/*
 * miner.h
 *
 *  Created on: Jun 4, 2019
 *      Author: goose9
 */

#ifndef MINER_H_
#define MINER_H_

extern volatile bool miner_running;

void *miner_thread(void *);


#endif /* MINER_H_ */
