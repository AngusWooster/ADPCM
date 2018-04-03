#ifndef _ADPCM_TEST_H_
#define _ADPCM_TEST_H_

typedef struct
{
	uint16_t nb_channel;	//number of channel. 1:mono, 2:stereo
	uint32_t sample_rate;	//number of samples per second
	uint8_t bit_depth;		//Bits Per Sample
	uint32_t subchunk1size;       //number of byte of audio data
	uint32_t NumData;       //number of byte of audio data
	uint32_t NumSamples;	//number of samples
	int8_t  filter_type;
} audio_info;

#endif