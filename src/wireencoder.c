#include "wireencoder.h"

int encode_bin(uint8_t *bin, uint16_t binlen, uint8_t* buf, uint16_t buflen){
  *buf=0xf0; buf++; buflen--;
  int res=2;
  while(binlen>0){
    if((*bin == 0xf0) || (*bin == 0xf1) || (*bin == 0xf2)){
      if(buflen<3) return -1;
      *buf=0xf2; buf++; buflen--; res++;
      *buf=*bin ^ 0xf2;
    }else{
      if(buflen<2) return -1;
      *buf=*bin;
    }
    buf++; buflen--; res++;
    bin++; binlen--;
  }
  *buf=0xf1;
  return res;
}

int decode_bin(uint8_t *bin, uint16_t binlen, uint8_t* buf, uint16_t buflen){
  if(buf==(uint8_t *)0){
    buf=bin;
    buflen=binlen;
  }
  int res=0;
  if(bin[0]!=0xf0) return -1;
  bin++;binlen--;
  while(binlen>0){
    if(*bin == 0xf1) return res;
    if(buflen<1) return -1;
    if(*bin == 0xf2){
      bin++;binlen--;
      *buf=*bin ^ 0xf2;
    }else{
      *buf=*bin;
    }
    buf++; buflen--; res++;
    bin++; binlen--;
  }
  return -2;
}

