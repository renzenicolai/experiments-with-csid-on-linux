/*
  nanomidi 0.1 - minimal MIDI parser for C

  2006-12-21

  Ole Martin Bjorndalen
  ombdalen@gmail.com
  http://nerdly.info/ole/
 */

#include "nanomidi.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>


void midi_buffer_init(struct midi_buffer *buf)
{
  memset(buf, 0, sizeof(struct midi_buffer));
  buf->channel_mask = 0xffff;
}

unsigned char previous_opcode = 0;
unsigned char previous_readlen = 0;

int midi_parse(struct midi_buffer *buf, unsigned char *bytes, int numbytes)
{
  int i;
  int num_callbacks = 0;

  for(i = 0; i < numbytes; i++) {
    unsigned char c = bytes[i];

    if(c & 0x80) {
      // Opcode

      if((c & 0xf0) != 0xf0) {
        // Channel message

        buf->channel = c & 0x0f;

        if((1 << buf->channel) & buf->channel_mask) {
          buf->opcode = c;
          previous_opcode = c;
          buf->type = buf->opcode & 0xf0;
          buf->pos = 0;

          if(buf->type == MIDI_CHANNEL_PRESSURE || buf->type == MIDI_PROGRAM_CHANGE) {
            buf->readlen = 1;
          } else {
            buf->readlen = 2;
          }
          previous_readlen = buf->readlen;
        }
      }
      //printf("OC %02x\n", c);
    } else {
      //printf("DT %02x (opcode %02x readlen %u)\n", c, buf->opcode, buf->readlen);
      if (buf->opcode == 0 && buf->readlen == 0) {
        //printf("USE PREVIOUS\n");
        buf->opcode = previous_opcode;
        buf->readlen = previous_readlen;
      }
      buf->data[buf->pos++] = c;
    }

    if(buf->pos >= buf->readlen) {
      buf->pos = 0;

      if(buf->opcode) {
        if(buf->callback) {
          buf->callback((struct midi_msg *)buf);
          num_callbacks++;
        }
    
        buf->opcode = 0;
        buf->pos = 0;
        buf->readlen = 0;
      }
    }
  }
  
  //printf("\n");

  return num_callbacks;
}

#define READ_BUFSIZE 1024

int midi_read(struct midi_buffer *buf, int fd)
{
  unsigned char bytes[READ_BUFSIZE];
  int size;

  size = read(fd, bytes, READ_BUFSIZE);
  if(size > 0) {
    return midi_parse(buf, bytes, size);
  } else {
    return size;
  }
}
