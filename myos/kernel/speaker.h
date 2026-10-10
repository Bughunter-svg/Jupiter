#ifndef SPEAKER_H
#define SPEAKER_H

void speaker_on(unsigned int frequency);
void speaker_off(void);
void speaker_beep(unsigned int frequency, unsigned int duration_ms);
void speaker_melody(void);

#endif
