/*********************************************************
 * config.h - Configuration data for the driver.c program.
 *********************************************************/
#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
 * CPEs for the baseline (naive) version of the pinwheel function that
 * was handed out to the students. Rd is the measured CPE for a dxd
 * image. Run the driver.c program on your system to get these
 * numbers.
 */
#define R64 7.2
#define R128 7.1
#define R256 6.8
#define R512 8.4
#define R1024 8.8

/*
 * CPEs for the baseline (naive) version of the glow function that
 * was handed out to the students. Sd is the measure CPE for a dxd
 * image. Run the driver.c program on your system to get these
 * numbers.
 */
#define S32 97.9
#define S64 99.9
#define S128 100.9
#define S256 101.6
#define S512 102.0

#endif /* _CONFIG_H_ */
