#ifndef __A_WATCHDOG_TASK_H_
#define __A_WATCHDOG_TASK_H_

#define CONNECT_TIMEOUT 60 * 120 // 60 Seconds
#define REBOOT_FREQUENCY (12 * 3600 * 100)

void watchdog_task_init();

#endif