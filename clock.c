#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/times.h>
#include "clock.h"

/*
 * Routines for using the cycle counter
 */

/* Detect whether running on Alpha */
#ifdef __alpha
#define IS_ALPHA 1
#else
#define IS_ALPHA 0
#endif

/* Detect whether running on x86 */
#if defined(__i386__) || defined(__x86_64__)
#define IS_x86 1
#else
#define IS_x86 0
#endif

/* Detect whether running on ARM */
#if defined(__arm__) || defined(__aarch64__)
#define IS_ARM 1
#else
#define IS_ARM 0
#endif

#if IS_ALPHA
/* Initialize the cycle counter */
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;

/* Use Alpha cycle timer to compute cycles.  Then use
   measured clock speed to compute seconds
*/

/*
 * counterRoutine is an array of Alpha instructions to access
 * the Alpha's processor cycle counter. It uses the rpcc
 * instruction to access the counter. This 64 bit register is
 * divided into two parts. The lower 32 bits are the cycles
 * used by the current process. The upper 32 bits are wall
 * clock cycles. These instructions read the counter, and
 * convert the lower 32 bits into an unsigned int - this is the
 * user space counter value.
 * NOTE: The counter has a very limited time span. With a
 * 450MhZ clock the counter can time things for about 9
 * seconds. */
static unsigned int counterRoutine[] =
    {
        0x601fc000u,
        0x401f0000u,
        0x6bfa8001u};

/* Cast the above instructions into a function. */
static unsigned int (*counter)(void) = (void *)counterRoutine;

void start_counter()
{
    /* Get cycle counter */
    cyc_hi = 0;
    cyc_lo = counter();
}

double get_counter()
{
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;
    ncyc_lo = counter();
    ncyc_hi = 0;
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;
    hi = ncyc_hi - cyc_hi - borrow;
    result = (double)hi * (1 << 30) * 4 + lo;
    if (result < 0)
    {
        fprintf(stderr, "Error: Cycle counter returning negative value: %.0f\n", result);
    }
    return result;
}
#endif /* Alpha */

#if IS_x86
/* $begin x86cyclecounter */
/* Initialize the cycle counter */
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;

/* Set *hi and *lo to the high and low order bits  of the cycle counter.
   Implementation requires assembly code to use the rdtsc instruction. */
void access_counter(unsigned *hi, unsigned *lo)
{
    asm("rdtsc; movl %%edx,%0; movl %%eax,%1" /* Read cycle counter */
        : "=r"(*hi), "=r"(*lo)                /* and move results to */
        : /* No input */                      /* the two outputs */
        : "%edx", "%eax");
}

/* Record the current value of the cycle counter. */
void start_counter()
{
    access_counter(&cyc_hi, &cyc_lo);
}

/* Return the number of cycles since the last call to start_counter. */
double get_counter()
{
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;

    /* Get cycle counter */
    access_counter(&ncyc_hi, &ncyc_lo);

    /* Do double precision subtraction */
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;
    hi = ncyc_hi - cyc_hi - borrow;
    result = (double)hi * (1 << 30) * 4 + lo;
    if (result < 0)
    {
        fprintf(stderr, "Error: counter returns neg value: %.0f\n", result);
    }
    return result;
}
/* $end x86cyclecounter */
#endif /* x86 */

#if IS_ARM // Make sure to define IS_ARM appropriately in your build environment

static unsigned long long cyc_start = 0;

// Function to read the ARM cycle counter
void access_counter(unsigned *hi, unsigned *lo)
{
    unsigned long long cycle_count;
    asm volatile("mrs %0, cntvct_el0" : "=r"(cycle_count));
    *hi = cycle_count >> 32;
    *lo = cycle_count & 0xFFFFFFFF;
}

// Record the current value of the cycle counter
void start_counter()
{
    unsigned hi, lo;
    access_counter(&hi, &lo);
    cyc_start = ((unsigned long long)hi << 32) | lo;
    printf("cyc_start: %llu\n", cyc_start);
}

// Return the number of cycles since the last call to start_counter
double get_counter()
{
    unsigned hi, lo;
    access_counter(&hi, &lo);
    unsigned long long cyc_end = ((unsigned long long)hi << 32) | lo;
    printf("cyc_end: %llu\n", cyc_end);
    unsigned long long cycles = cyc_end - cyc_start;
    return (double)cycles; // Return the cycle difference as a double
}

#endif /* ARM */

double ovhd()
{
    /* Do it twice to eliminate cache effects */
    int i;
    double result;

    for (i = 0; i < 2; i++)
    {
        start_counter();
        result = get_counter();
    }
    return result;
}

/* $begin mhz */
/* Estimate the clock rate by measuring the cycles that elapse */
/* while sleeping for sleeptime seconds */
double mhz_full(int verbose, int sleeptime)
{
    double rate;

    start_counter();
    sleep(sleeptime);
    rate = get_counter() / (1e6 * sleeptime);
    if (verbose)
        printf("Processor clock rate ~= %.1f MHz\n", rate);
    return rate;
}
/* $end mhz */

/* Version using a default sleeptime */
double mhz(int verbose)
{
    return mhz_full(verbose, 2);
}

/** Special counters that compensate for timer interrupt overhead */

static double cyc_per_tick = 0.0;
static int cpt_set = 0;

#define NEVENT 100
#define THRESHOLD 1000
#define RECORDTHRESH 3000

/* Attempt to see how much time is used by timer interrupt */
static void callibrate(int verbose)
{
    double oldt;
    struct tms t;
    clock_t oldc;
    int e = 0;

    times(&t);
    oldc = t.tms_utime;
    start_counter();
    oldt = get_counter();
    while (e < NEVENT)
    {
        double newt = get_counter();

        if (newt - oldt >= THRESHOLD)
        {
            clock_t newc;
            times(&t);
            newc = t.tms_utime;
            if (newc > oldc)
            {
                double cpt = (newt - oldt) / (newc - oldc);
                if ((cyc_per_tick == 0.0 || cyc_per_tick > cpt) && cpt > RECORDTHRESH)
                    cyc_per_tick = cpt;
                /*
                  if (verbose)
                  printf("Saw event lasting %.0f cycles and %d ticks.  Ratio = %f\n",
                  newt-oldt, (int) (newc-oldc), cpt);
                */
                e++;
                oldc = newc;
            }
            oldt = newt;
        }
    }
    /* ifdef added by Sanjit - 10/2001 */
#ifdef DEBUG
    if (verbose)
        printf("Setting cyc_per_tick to %f\n", cyc_per_tick);
#endif
}

static clock_t start_tick = 0;

void start_comp_counter()
{
    struct tms t;

    if (!cpt_set)
    {
        callibrate(1);
        cpt_set = 1;
    }
    times(&t);
    start_tick = t.tms_utime;
    start_counter();
}

double get_comp_counter()
{
    double time = get_counter();
    double ctime;
    struct tms t;
    clock_t ticks;

    times(&t);
    ticks = t.tms_utime - start_tick;
    ctime = time - ticks * cyc_per_tick;
    /*
      printf("Measured %.0f cycles.  Ticks = %d.  Corrected %.0f cycles\n",
      time, (int) ticks, ctime);
    */
    return ctime;
}
