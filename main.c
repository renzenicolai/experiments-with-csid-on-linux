#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

#include "libcsid.h"
#include "nanomidi.h"

// ALSA MIDI

snd_rawmidi_t* midiin = NULL;

void alsa_midi_init() {
	int status;
	int mode = SND_RAWMIDI_SYNC;

	const char* portname = "hw:2,0";  // see alsarawportlist.c example program
	if ((status = snd_rawmidi_open(&midiin, NULL, portname, mode)) < 0) {
		printf("Problem opening MIDI input: %s\n", snd_strerror(status));
		exit(1);
	}
	
	snd_rawmidi_nonblock(midiin, 1);
}

float channel_frequency[3] = {0};
int channel_note[3] = {0};
bool channel_active[3] = {false};

float noteToFreq(int note) {
    float a = 440; //frequency of A (coomon value is 440Hz)
    return (a / 32) * pow(2, ((note - 9) / 12.0));
}

void handle_midi_msg(struct midi_msg *msg) {
  switch(msg->type & 0xF0) {
  case MIDI_NOTE_ON:
    printf("Note on #%d note=%d velocity=%d\n", msg->channel, msg->data[0], msg->data[1]);
	/*for (uint8_t i = 0; i < sizeof(channel_frequency); i++) {
		if (channel_note[i] == 0) {
			printf("Channel %u play %u\n", i, msg->data[0]);
			channel_frequency[i] = noteToFreq(msg->data[0]);
			channel_note[i] = msg->data[0];
			break;
		}
	}*/
	
	if (msg->channel < 3) {
		//printf("Channel %u play %u\n", msg->channel, msg->data[0]);
		channel_frequency[msg->channel] = noteToFreq(msg->data[0]);
		channel_note[msg->channel] = msg->data[0];
		channel_active[msg->channel] = true;
	}
    break;
  case MIDI_NOTE_OFF:
    printf("Note off #%d note=%d velocity=%d\n", msg->channel, msg->data[0], msg->data[1]);
	/*for (uint8_t i = 0; i < sizeof(channel_frequency); i++) {
		if (channel_note[i] == msg->data[0]) {
			printf("Channel %u stop %u\n", i, msg->data[0]);
			channel_frequency[i] = 0;
			channel_note[i] = 0;
		}
	}*/
	if (msg->channel < 3) {
		//printf("Channel %u stop %u\n", msg->channel, msg->data[0]);
		//channel_frequency[msg->channel] = 0;
		//channel_note[msg->channel] = 0;
		if (channel_note[msg->channel] == msg->data[0]) {
			channel_active[msg->channel] = false;
		}
	}
    break;
  case MIDI_PROGRAM_CHANGE:
    printf("Program change #%d program=%d\n", msg->channel, msg->data[0]);
    break;
  default:
	  printf("Unhandled %d %d %d", msg->type, msg->channel, msg->data[0]);
  }
}

void alsa_midi_receive() {
	struct midi_buffer midi_buffer; 
	midi_buffer_init(&midi_buffer); 
	midi_buffer.callback = handle_midi_msg;  // Set a callback
	midi_buffer.channel_mask = 0xff;    // Listen to channels 0 to 7

	uint8_t buffer[256];
	int read = snd_rawmidi_read(midiin, buffer, 256);
	if (read < 0) {
		//printf("Problem reading MIDI input: %i, %s\n", read, snd_strerror(read));
		//exit(1);
		return;
	}

	/*for (int i = 0; i < read; i++) {
		printf("%02x ", buffer[i]);
	}
	printf("\n");*/
	
	midi_parse(&midi_buffer, buffer, read);
}

void alsa_midi_stop() {
	snd_rawmidi_close(midiin);
	midiin  = NULL;    // snd_rawmidi_close() does not clear invalid pointer,
}

// ALSA AUDIO OUTPUT
#define PCM_DEVICE "default"

#define SAMPLERATE 22050 * 2

snd_pcm_t *pcm_handle;
snd_pcm_uframes_t max_frames;

void alsa_init() {
    unsigned int pcm;
    unsigned int tmp, dir;
	int rate = SAMPLERATE;
    int channels = 1;

	snd_pcm_hw_params_t *params;
	char *buff;
	int buff_size, loops;

	/* Open the PCM device in playback mode */
	if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE,
					SND_PCM_STREAM_PLAYBACK, 0) < 0) 
		printf("ERROR: Can't open \"%s\" PCM device. %s\n",
					PCM_DEVICE, snd_strerror(pcm));

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
					SND_PCM_ACCESS_RW_INTERLEAVED) < 0) 
		printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
						SND_PCM_FORMAT_S16_LE) < 0) 
		printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0) 
		printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0) 
		printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

	/* Write parameters */
	if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
		printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

	/* Resume information */
	printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

	printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

	snd_pcm_hw_params_get_channels(params, &tmp);
	printf("channels: %i ", tmp);

	if (tmp == 1)
		printf("(mono)\n");
	else if (tmp == 2)
		printf("(stereo)\n");

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	printf("rate: %d bps\n", tmp);

	/* Allocate buffer to hold single period */
	/*snd_pcm_hw_params_get_period_size(params, &max_frames, 0);

	buff_size = max_frames * channels * 2 /* 2 -> sample size;*/
	
	snd_pcm_hw_params_get_buffer_size(params, &max_frames);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
}

void alsa_stop() {
	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
}

void alsa_play(char* buff, snd_pcm_uframes_t frames) {
    unsigned int pcm = 0;
    if (pcm = snd_pcm_writei(pcm_handle, buff, frames) == -EPIPE) {
        printf("XRUN.\n");
        snd_pcm_prepare(pcm_handle);
    } else if (pcm < 0) {
        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
    }
}

// CSID

#define NUM_SAMPLES SAMPLERATE / 100
unsigned short mono_samples_data[2 * NUM_SAMPLES];
unsigned short samples_data[2 * 2 * NUM_SAMPLES];

static void render_audio() {
    libcsid_render(mono_samples_data, NUM_SAMPLES);
    alsa_play(mono_samples_data, NUM_SAMPLES);

    // Duplicate mono samples to create stereo buffer
    /*for(unsigned int i = 0; i < NUM_SAMPLES; i ++) {
        unsigned int sample_val = mono_samples_data[i];
        samples_data[i * 2 + 0] = (unsigned short) (((short)sample_val) * 0.7);
        samples_data[i * 2 + 1] = (unsigned short) (((short)sample_val) * 0.7);
    }

    int pos = 0;
    int left = 2 * 2 * NUM_SAMPLES;
    unsigned char *ptr = (unsigned char *)samples_data;

    while (left > 0) {
        size_t written = 0;
        //i2s_write(I2S_NUM, (const char *)ptr, left, &written, 100 / portTICK_RATE_MS);
        alsa_play(ptr, left); written += left;
        pos += written;
        ptr += written;
        left -= written;
    }*/
}

static void render_audio2(int samples) {
	static int i;
	uint8_t* stream = (uint8_t*) mono_samples_data;
	for(i = 0; i < samples * 2; i+=2) {
		step(&stream[i]);
	}
    alsa_play(mono_samples_data, samples);
}

// Application

#include "phantom.inc"

snd_pcm_uframes_t get_available_frames() {
	snd_pcm_sframes_t empty_frames = snd_pcm_avail(pcm_handle);
	if (empty_frames < 0) {
		empty_frames = max_frames;
	}
	snd_pcm_uframes_t available_frames = max_frames - empty_frames;
	//printf("A: %i\n", available_frames);
	return available_frames;
}

#define SID_FREQLO1 0
#define SID_FREQHI1 1
#define SID_PWLO1   2
#define SID_PWHI1   3
#define SID_CR1     4
#define SID_AD1     5
#define SID_SR1     6
#define SID_FREQLO2 7
#define SID_FREQHI2 8
#define SID_PWLO2   9
#define SID_PWHI2   10
#define SID_CR2     11
#define SID_AD2     12
#define SID_SR2     13
#define SID_FREQLO3 14
#define SID_FREQHI3 15
#define SID_PWLO3   16
#define SID_PWHI3   17
#define SID_CR3     18
#define SID_AD3     19
#define SID_SR3     20
#define SID_FCLO    21
#define SID_FCHI    22
#define SID_RESFILT 23
#define SID_MODEVOL 24

#define SID_NOISE (1 << 7)
#define SID_PULSE (1 << 6)
#define SID_SAW   (1 << 5)
#define SID_TRI   (1 << 4)
#define SID_TEST  (1 << 3)
#define SID_RING  (1 << 2)
#define SID_SYNC  (1 << 1)
#define SID_GATE  (1 << 0)

int main(int argc, char **argv) { 
    libcsid_init(SAMPLERATE, SIDMODEL_6581);
    libcsid_load((unsigned char *)&phantom_of_the_opera_sid, phantom_of_the_opera_sid_len, 0);
    printf("SID Title: %s\n", libcsid_gettitle());
    printf("SID Author: %s\n", libcsid_getauthor());
    printf("SID Info: %s\n", libcsid_getinfo());
    
    alsa_init();
	alsa_midi_init();
	
	struct timespec delay_amount = {
		.tv_sec = 0,
		.tv_nsec = 1000000000 / 100
	};
    
    /*for (int i = 0; i < 10; i++) {
        render_audio();
    }*/

    printf("---\n");
	
	sidset(SID_FREQLO1, 0);
	sidset(SID_FREQHI1, 0);
	
	uint16_t pulsewidth = 2048;//3070;
	sidset(SID_PWLO1, pulsewidth & 0xFF);
	sidset(SID_PWHI1, (pulsewidth >> 8) & 0x0F);
	
	sidset(SID_CR1, SID_TRI);// + SID_GATE);//SID_SAW + SID_GATE);
	
	uint8_t attack = 8;
	uint8_t decay = 8;
	uint8_t sustain = 15;//8;
	uint8_t release = 8;//12;
	
	sidset(SID_AD1, ((attack & 0xF) << 4) + (decay & 0xF));
	sidset(SID_SR1, ((sustain & 0xF) << 4) + (release & 0xF));

	printf("Pulsewidth: %u\n", (sidget(SID_PWHI1) << 8) + sidget(SID_PWLO1));
	printf("CR:         %02x\n", sidget(SID_CR1));
	printf("Attack:     %u\n", sidget(SID_AD1) >> 4);
	printf("Decay:      %u\n", sidget(SID_AD1) & 0xF);
	printf("Sustain:    %u\n", sidget(SID_SR1) >> 4);
	printf("Release:    %u\n", sidget(SID_SR1) & 0xF);

	for (uint8_t reg = 0; reg < 7; reg++) { // Copy voice 1 settings to voice 2 and 3
		sidset(reg + SID_FREQLO2, sidget(reg));
		sidset(reg + SID_FREQLO3, sidget(reg));
	}
	
	sidset(SID_RESFILT, 0);
	
	//sidset(SID_CR2, SID_TRI + SID_GATE);
	//sidset(SID_CR3, SID_PULSE + SID_GATE);
	
    
	uint16_t FREQ_CONSTANT = pow(256, 3) / 985248;

	printf("MAX: %u\n", max_frames);
	
	printf("\x1bc"); // Reset
	printf("\x1b[0;37m"); // White
	
    while (1) {
		alsa_midi_receive();
		
		uint16_t regvalue_channel1_frequency = FREQ_CONSTANT * channel_frequency[0];
		sidset(0, regvalue_channel1_frequency & 0xFF);
		sidset(1, regvalue_channel1_frequency >> 8);
		uint16_t regvalue_channel2_frequency = FREQ_CONSTANT * channel_frequency[1];
		sidset(7, regvalue_channel2_frequency & 0xFF);
		sidset(8, regvalue_channel2_frequency >> 8);
		uint16_t regvalue_channel3_frequency = FREQ_CONSTANT * channel_frequency[2];
		sidset(14, regvalue_channel3_frequency & 0xFF);
		sidset(15, regvalue_channel3_frequency >> 8);
		
		sidset(SID_CR1, SID_PULSE + (channel_active[0] ? SID_GATE : 0));
		sidset(SID_CR2, SID_SAW + (channel_active[1] ? SID_GATE : 0));
		sidset(SID_CR3, SID_TRI + (channel_active[2] ? SID_GATE : 0));

		while (get_available_frames() < 4410 / 2) {
			render_audio2(100);
		}
		
		nanosleep(&delay_amount, NULL);
	}
    
    alsa_stop();
    
    return 0;
}
