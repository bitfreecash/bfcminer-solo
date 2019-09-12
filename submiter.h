/*
 * submiter.h
 *
 *  Created on: Jun 4, 2019
 *      Author: goose9
 */

#ifndef SUBMITER_H_
#define SUBMITER_H_

extern volatile bool submiter_running;

void *submiter_thread(void *);


#endif /* SUBMITER_H_ */
