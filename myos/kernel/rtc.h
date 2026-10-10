#ifndef RTC_H
#define RTC_H

typedef struct {
    unsigned int year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
} rtc_time_t;

void rtc_read(rtc_time_t *t);
void rtc_format(char *buf, const rtc_time_t *t);

#endif
