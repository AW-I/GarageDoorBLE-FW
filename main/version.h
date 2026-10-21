#ifndef __VERSION_H_
#define __VERSION_H_

/** V1.0.0 - Initial release: NimBLE app control, Forcedoor UART motor link,
 *           Wi-Fi + Home Assistant (MQTT).
 *  V1.0.1 - Wi-Fi TX power capped at 8.5 dBm for better link on ESP32-C3 Super Mini.
 *           Wi-Fi retries every minute instead of exponential back-off; reboots after
 *           1 h without Wi-Fi (when no phone connected and door idle).
 *           Factory reset by holding the BOOT button (GPIO9) for 10 s.
 */
#define VERSION "1.0.1"

#endif
