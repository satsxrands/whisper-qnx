/* Count VAD utterances in a 16 kHz mono 16-bit WAV (44-byte header): cc -I voicecmd/src voicecmd/tests/vad_segments.c -lm && ./a.out x.wav */
#include "vad.h"
#include <stdio.h>
int main(int c,char**v){FILE*f=fopen(v[1],"rb");fseek(f,44,0);int16_t fr[VAD_FRAME];vad_t d;vad_init(&d);long i=0;int n=0;
while(fread(fr,2,VAD_FRAME,f)==VAD_FRAME){int e=vad_push(&d,fr);if(e==VAD_START)printf("start %.2f ",i*.02);if(e==VAD_END){printf("end %.2f\n",i*.02);n++;}i++;}printf("utterances %d floor %.1f\n",n,d.floor_db);}
