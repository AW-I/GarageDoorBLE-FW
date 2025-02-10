#ifndef __A_WATCHDOG_TASK_H_
#define __A_WATCHDOG_TASK_H_

#define CONNECT_TIMEOUT 300 * 100 //5 Minutes without action is probably idle. 
#define REBOOT_FREQUENCY (24 * 3600 * 100) // Every 24 hours, just case.

void watchdog_task_init();
void watchdog_task_deinit();

#endif