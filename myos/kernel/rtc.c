#include "rtc.h"
#include "ports.h"

#define CMOS_INDEX  0x70
#define CMOS_DATA   0x71

#define REG_SECOND  0x00
#define REG_MINUTE  0x02
#define REG_HOUR    0x04
#define REG_DAY     0x07
#define REG_MONTH   0x08
#define REG_YEAR    0x09
#define REG_STATUS_A 0x0A
#define REG_STATUS_B 0x0B

static unsigned char cmos_read(unsigned char reg)
{
    outb(CMOS_INDEX, reg);
    return inb(CMOS_DATA);
}

static int update_in_progress(void)
{
    return (cmos_read(REG_STATUS_A) & 0x80) != 0;
}

static unsigned char bcd_to_bin(unsigned char value)
{
    return (unsigned char)((value & 0x0F) + ((value >> 4) * 10));
}

static void read_registers(unsigned char *r)
{
    while (update_in_progress()) {}

    r[0] = cmos_read(REG_SECOND);
    r[1] = cmos_read(REG_MINUTE);
    r[2] = cmos_read(REG_HOUR);
    r[3] = cmos_read(REG_DAY);
    r[4] = cmos_read(REG_MONTH);
    r[5] = cmos_read(REG_YEAR);
}

void rtc_read(rtc_time_t *t)
{
    unsigned char a[6];
    unsigned char b[6];
    unsigned char status_b;
    unsigned char pm;
    int same;
    int i;

    read_registers(a);

    do {
        for (i = 0; i < 6; i++)
            b[i] = a[i];

        read_registers(a);

        same = 1;

        for (i = 0; i < 6; i++) {
            if (a[i] != b[i])
                same = 0;
        }
    } while (!same);

    status_b = cmos_read(REG_STATUS_B);

    pm = a[2] & 0x80;
    a[2] &= 0x7F;

    if (!(status_b & 0x04)) {
        for (i = 0; i < 6; i++)
            a[i] = bcd_to_bin(a[i]);
    }

    if (!(status_b & 0x02)) {
        if (a[2] == 12)
            a[2] = 0;

        if (pm)
            a[2] = (unsigned char)(a[2] + 12);
    }

    t->second = a[0];
    t->minute = a[1];
    t->hour = a[2];
    t->day = a[3];
    t->month = a[4];
    t->year = 2000U + a[5];
}

static void put_two(char *dst, unsigned int value)
{
    dst[0] = (char)('0' + (value / 10) % 10);
    dst[1] = (char)('0' + value % 10);
}

void rtc_format(char *buf, const rtc_time_t *t)
{
    buf[0] = (char)('0' + (t->year / 1000) % 10);
    buf[1] = (char)('0' + (t->year / 100) % 10);
    buf[2] = (char)('0' + (t->year / 10) % 10);
    buf[3] = (char)('0' + t->year % 10);
    buf[4] = '-';
    put_two(buf + 5, t->month);
    buf[7] = '-';
    put_two(buf + 8, t->day);
    buf[10] = ' ';
    put_two(buf + 11, t->hour);
    buf[13] = ':';
    put_two(buf + 14, t->minute);
    buf[16] = ':';
    put_two(buf + 17, t->second);
    buf[19] = '\0';
}
