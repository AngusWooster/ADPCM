#include <stdlib.h> // For malloc/free
#include <string.h> // For memset
#include <stdio.h>	// For test case I/O
#include "adpcm_test.h"

int8_t WAV_PCM_header[44] =
{
	0x52, 0x49, 0x46, 0x46, //ChunkID        : "RIFF" in big endian
	0x24, 0x35, 0x0c, 0x00, //ChunkSize      : 36 + SubChunk2Size
	0x57, 0x41, 0x56, 0x45, //Format         : "WAVE" in big endian
	0x66, 0x6d, 0x74, 0x20, //SubChunk1ID    : "fmt"
	0x10, 0x00, 0x00, 0x00, //SubChunk1Size  : 16 for pcm
	0x01, 0x00,             //AudioFormat    : PCM = 1
	0x01, 0x00,             //Num of channel : Mono = 1, Stereo = 2, etc
	0x80, 0xbb, 0x00, 0x00, //SampleRate     : 48000(default)
	0x88, 0x58, 0x01, 0x00, //ByteRate       : SampleRate * NumChannel * BitsPerSample/8
	0x02, 0x00,             //BlockAlign     : NumChannel * BitsPerSample/8
	0x18, 0x00,             //BitsPerSample  : 16 (default)
	0x64, 0x61, 0x74, 0x61, //SSubChunk2ID   : "data" in big endian
	0x00, 0x35, 0x0c, 0x00, //SubChunk2Size
};

void get_audio_header(FILE *fd_in, audio_info *audio)
{
    int subchunk1size;
    //initialize audio info
    fseek(fd_in, 4, SEEK_SET);
    fread(&WAV_PCM_header[4], 4, 1, fd_in);
//  printf("chunkSize:%x %x %x %x", WAV_PCM_header[4],WAV_PCM_header[5],WAV_PCM_header[6],WAV_PCM_header[7]);
    fseek(fd_in, 16, SEEK_SET);
    fread(&audio->subchunk1size, 4, 1, fd_in);
    fseek(fd_in, 22, SEEK_SET); //NumChannel
    fread(&audio->nb_channel, 2, 1, fd_in);
    fseek(fd_in, 24, SEEK_SET);
    fread(&audio->sample_rate, 4, 1, fd_in);
    fseek(fd_in, 34, SEEK_SET); //BitPerSample
    fread(&audio->bit_depth, 2, 1, fd_in);
    fseek(fd_in, 24 + audio->subchunk1size, SEEK_SET); //num of data byte
    fread(&audio->NumData, 4, 1, fd_in);
    printf("audio.nb_channel:%d\n audio.sample_rate:%d\n audio.bit_depth:%d\n audio.NumData:%d\n subchunk1size%d\n ", audio->nb_channel, audio->sample_rate, audio->bit_depth, audio->NumData, audio->subchunk1size);
    audio->NumSamples = (audio->NumData / audio->nb_channel) / (audio->bit_depth / 8);
//  audio.NumSamples = audio.sample_rate * audio.nb_channel * 10;
}

void get_audio_chunk(FILE *fd_in, audio_info *audio, int32_t *samp_dat)
{
    uint32_t i = 0;

    //read data from file
    fseek(fd_in, 28 + audio->subchunk1size, SEEK_SET); //start of data
    while(i < audio->NumSamples * audio->nb_channel){
        fread(&samp_dat[i], audio->bit_depth / 8, 1, fd_in);
        if(samp_dat[i] & 0x8000 && audio->bit_depth == 16)
            samp_dat[i] |= 0xFFFF0000;
        else if(samp_dat[i] & 0x800000 && audio->bit_depth == 24)
            samp_dat[i] |= 0xFF000000;
        //printf("read%d:%x \r\n",1024 - i, samp_dat[i]);
        i++;
    }
}



#if	1	//ADPCM
typedef struct _adpcm_state
{
    int	valprev[2];	/* Previous output value */
    unsigned char	index[2];	/* Index into stepsize table */
} adpcm_state;

static const int new_indexTable[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,

	2, 4, 6, 8, 10, 12, 14, 16,
	18, 20, 22, 24, 26, 28, 30, 32,
	34, 36, 38, 40, 42, 44, 46, 48,
	50, 52, 54, 56, 58, 60, 62, 64,
	66, 68, 70, 72, 74, 76, 78, 80,
	82, 84, 86, 88, 90, 92, 94, 96,
	98, 100, 102, 104, 106, 108, 110, 112,
	114, 116, 118, 120, 122, 124, 126, 128,
	
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,

	2, 4, 6, 8, 10, 12, 14, 16,
	18, 20, 22, 24, 26, 28, 30, 32,
	34, 36, 38, 40, 42, 44, 46, 48,
	50, 52, 54, 56, 58, 60, 62, 64,
	66, 68, 70, 72, 74, 76, 78, 80,
	82, 84, 86, 88, 90, 92, 94, 96,
	98, 100, 102, 104, 106, 108, 110, 112,
	114, 116, 118, 120, 122, 124, 126, 128
};

static const unsigned int new_stepsizeTable[147] = {	//89 + 58?
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,


	36670, 40337, 44370, 48807, 53688, 59057, 64963, 71459, 78605, 86465, 
	95112, 104623, 115085, 126594, 139253, 153178, 168496, 185346, 203880, 224268, 
	246695, 271365, 298501, 328351, 361186, 397305, 437035, 480739, 528813, 581694, 
	639864, 703850, 774235, 851658, 936824, 1030507, 1133557, 1246913, 1371604, 1508765, 
	1659641, 1825605, 2008166, 2208982, 2429881, 2672869, 2940156, 3234171, 3557588, 3913347, 
	4304682, 4735150, 5208665, 5729531, 6302485, 6932733, 7626006, 8388607
};

void adpcm_init(adpcm_state *state)
{
	state->valprev[0] = 0;
	state->index[0] = 0;
	state->valprev[1] = 0;
	state->index[1] = 0;
}

int adpcm_coder_2_24bit(int *indata, unsigned char *outdata, int len, adpcm_state *state)
{
	int *inp;         /* Input buffer pointer */
	unsigned char *outp;/* output buffer pointer */
	int val;            /* Current input sample value */
	int sign;           /* Current adpcm sign bit */
	unsigned int delta; /* Current adpcm output value */
	int diff;           /* Difference between val and valprev */
	unsigned int udiff; /* unsigned value of diff */
	unsigned int step;  /* Stepsize */
	int valpred;        /* Predicted output value */
	unsigned int vpdiff;/* Current change to valpred */
	int index;          /* Current step change index */
	unsigned int outputbuffer = 0;/* place to keep previous 4-bit value */
	int count = 0;      /* the number of bytes encoded */

	int i, size;

	for (i = 0; i < 2; i++) {
		outp = outdata + i;
		inp = indata + i;
		valpred = state->valprev[i];
		index = (int)state->index[i];
		step = new_stepsizeTable[index];


		size = (len / 3)/2;
		//printf("== size = %d, index = %x\n",size,index);
		while (size-- > 0) {
			val = *inp;
			val = (val & 0x800000)?(val | 0xFF000000) : (val & 0xFFFFFF);	
			inp += 2;

			/* Step 1 - compute difference with previous value */
			diff = val - valpred;
			if (diff < 0) {
				sign = 0x80;
				diff = (-diff);
			} else {
				sign = 0;
			}
			/* diff will be positive at this point */
			udiff = (unsigned int)diff;
//printf("enc udiff %x, step %x\n",udiff, step);
			/* Step 2 - Divide and clamp */
			/* Note:
			** This code *approximately* computes:
			**    delta = diff*4/step;
			**    vpdiff = (delta+0.5)*step/4;
			** but in shift step bits are dropped. The net result of this is
			** that even if you have fast mul/div hardware you cannot put it to
			** good use since the fixup would be too expensive.
			*/
			delta = 0;
			vpdiff = (step >> 7);
			//printf("1. enc udiff %x, step %x\n",udiff, step);
			if (udiff >= step) {
				delta = 64;
				udiff -= step;
				vpdiff += step;
			}
			//printf("1. delta %x\n",delta);
			step >>= 1;
			//printf("2. enc udiff %x, step %x\n",udiff, step);
			if (udiff >= step) {
				delta |= 32;
				udiff -= step;
				vpdiff += step;
			}
			//printf("2. delta %x\n",delta);
			step >>= 1;
			if (udiff >= step) {
				delta |= 16;
				udiff -= step;
				vpdiff += step;
			}
			step >>= 1;
			if (udiff >= step) {
				delta |= 8;
				udiff -= step;
				vpdiff += step;
			}
			
			step >>= 1;
			if (udiff >= step) {
				delta |= 4;
				udiff -= step;
				vpdiff += step;
			}
			step >>= 1;
			if (udiff >= step) {
				delta |= 2;
				udiff -= step;
				vpdiff += step;
			}
			step >>= 1;
			if (udiff >= step) {
				delta |= 1;
				vpdiff += step;
			}

			/* Phil Frisbie combined steps 3 and 4 */
			/* Step 3 - Update previous value */
			/* Step 4 - Clamp previous value to 16 bits */
			if (sign != 0) {
				valpred -= vpdiff;
				if (valpred < -8388608) valpred = -8388608;
			} else {
				valpred += vpdiff;
				if (valpred > 8388607) valpred = 8388607;
			}

			/* Step 5 - Assemble value, update index and step values */
//printf("#enc delta%x\n",delta);			
			delta |= sign;
//printf("@enc delta%x\n",delta);
			index += new_indexTable[delta];
			if ( index < 0 ) index = 0;
			if ( index > 143 ) index = 143;
			step = new_stepsizeTable[index];
			/* Step 6 - Output value */
			*outp = (char)(delta);
			outp += 2;
			count++;

		}

		//printf("@enc index%x , step%x\n\n\n",index,step);
		state->valprev[i] = (short)valpred;
		state->index[i] = (char)index;
	}

	return count;
}


int adpcm_decoder_2_24bit(unsigned char *indata, char *outdata, int len, adpcm_state *state)
{
	unsigned char *inp; /* Input buffer pointer */
	char *outp;        /* output buffer pointer */
	unsigned int sign;  /* Current adpcm sign bit */
	unsigned int delta; /* Current adpcm output value */
	unsigned int step;  /* Stepsize */
	int valpred;        /* Predicted value */
	unsigned int vpdiff;/* Current change to valpred */
	int index;          /* Current step change index */
	unsigned int inputbuffer = 0;/* place to keep next 4-bit value */
	int count = 0;

	int i, size;

	for (i = 0; i < 2; i++) {

		outp = (char*)(outdata);
		if (i==0) {
			outp += 0;
		} else {
			outp += 3;
		}

		
		inp = indata + i;

		valpred = state->valprev[i];
		index = (int)state->index[i];
		step = new_stepsizeTable[index];

		size = len;

		while (size-- > 0) {

			/* Step 1 - get the delta value */
			delta = (unsigned int)*inp;
			//printf("dec delta = %x\n",delta);
			inp += 2;

			/* Step 2 - Find new index value (for later) */
			index += new_indexTable[delta];
			if (index < 0) index = 0;
			if (index > 143) index = 143;

			/* Step 3 - Separate sign and magnitude */
			sign = delta & 0x80;
			delta = delta & 0x7f;

			/* Phil Frisbie combined steps 4 and 5 */
			/* Step 4 - Compute difference and new predicted value */
			/* Step 5 - clamp output value */
			/*
			** Computes 'vpdiff = (delta+0.5)*step/4', but see comment
			** in adpcm_coder.
			*/
			vpdiff = step >> 7;
			if ((delta & 64) != 0) vpdiff += step;
			if ((delta & 32) != 0) vpdiff += step>>1;
			if ((delta & 16) != 0) vpdiff += step>>2;
			if ((delta & 8) != 0) vpdiff += step>>3;
			if ((delta & 4) != 0) vpdiff += step>>4;
			if ((delta & 2) != 0) vpdiff += step>>5;
			if ((delta & 1) != 0) vpdiff += step>>6;

			if ( sign != 0 ) {
				valpred -= vpdiff;
				if (valpred < -8388608) valpred = -8388608;
			} else {
				valpred += vpdiff;
				if (valpred > 8388607) valpred = 8388607;
			}

			/* Step 6 - Update step value */
			step = new_stepsizeTable[index];

			/* Step 7 - Output value */
			*outp = (char)(valpred & 0xff);
			*(outp+1) = (char)((valpred >> 8) & 0xff);
			*(outp+2)  = (char)((valpred >> 16) & 0xff);
			outp += 6;
			
			count++;
		}

		state->valprev[i] = (short)valpred;
		state->index[i] = (char)index;
	}

	return count;
}
#endif


void adpcm_process(int argc, char *argv[])
{
	char file_name[30];
	FILE *fd_in, *fd_out;
	audio_info audio_inf;
	int32_t *audio_data, cnt, audio_tmp_buf;
	unsigned char adpcm_val[4];
	char decode_val[6];
	
	adpcm_state encode_state, decode_state;
	
	adpcm_init(&encode_state);
	adpcm_init(&decode_state);

	
	sprintf(file_name, "%s", argv[1]);
	printf("wave name = %s\n",file_name);
	fd_in = fopen(file_name, "r");
	if(!fd_in) {
		printf("file open fail! \n");
	} else {
		get_audio_header(fd_in, &audio_inf);
		audio_data = (int32_t*)malloc(audio_inf.NumSamples * audio_inf.nb_channel * sizeof(int32_t));
		get_audio_chunk(fd_in, &audio_inf, audio_data);
		printf("%x %x %x\n",*(audio_data),*(audio_data+1),*(audio_data+2));




	//wav pcm header
	uint32_t ChunkSize, ByteRate;
	uint16_t BlockAlign;

	memcpy(&WAV_PCM_header[40], &audio_inf.NumData, 4);
	ChunkSize = 36 + audio_inf.NumData;
	memcpy(&WAV_PCM_header[4], &ChunkSize, 4);
	memcpy(&WAV_PCM_header[22], &audio_inf.nb_channel, 2);
	memcpy(&WAV_PCM_header[24], &audio_inf.sample_rate, 4);


	ByteRate = audio_inf.sample_rate*audio_inf.nb_channel*(audio_inf.bit_depth/8);
	memcpy(&WAV_PCM_header[28], &ByteRate, 4);

	BlockAlign = audio_inf.nb_channel*(audio_inf.bit_depth/8);
	memcpy(&WAV_PCM_header[32], &BlockAlign, 2);
	memcpy(&WAV_PCM_header[34], &audio_inf.bit_depth, 2);

	fd_out = fopen("adpcm_encode.wav", "w");
	for(cnt = 0; cnt < 44; cnt++) {
	    fwrite(&WAV_PCM_header[cnt], sizeof(int8_t), 1, fd_out);
	}
	

#if	0
			for (cnt=0; cnt < 3; cnt++) {
				if ((cnt % 2) == 0) {
					adpcm_coder_2_24bit((audio_data+cnt), adpcm_val, 6, &encode_state);
					printf("audio data %x %x, adpcm %x %x\n",*(audio_data+cnt),*(audio_data+cnt+1),adpcm_val[0],adpcm_val[1]);
					
					adpcm_decoder_2_24bit(adpcm_val, decode_val, 1, &decode_state);
					printf("adpcm %x %x, decode %x %x\n",adpcm_val[0],adpcm_val[1],decode_val[0],decode_val[1]);
					fwrite(decode_val, sizeof(int8_t), 6, fd_out);
				}
			}
#else

		for (cnt=0; cnt < audio_inf.NumSamples*2; cnt++) {
			
			if ((cnt % 2) == 0) {
				adpcm_coder_2_24bit((audio_data+cnt), adpcm_val, 6, &encode_state);
				adpcm_decoder_2_24bit(adpcm_val, decode_val, 1, &decode_state);
				fwrite(decode_val, sizeof(int8_t), 6, fd_out);
			}
			//fwrite((audio_data+cnt), sizeof(int8_t), 3, fd_out);
		}
		
#endif



	
		fclose(fd_in);
		fclose(fd_out);
		free(audio_data);
	}
}


int main(int argc, char *argv[])
{
	
	adpcm_process(argc, argv);

    return 0;
}

