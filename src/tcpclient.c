#include <platform_opts.h>
#include "main.h"
#include "tcpclient.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/tcp.h"
#include "wireencoder.h"
#include "gpio_api.h"   // mbed
#include "at_cmd/atcmd_wifi.h"

extern struct netif xnetif[NET_IF_NUM]; 

struct tcpclient_conf conf;

void init_tcpthread(void *param);
void tcpclient_start() {
  printf("\n\rDBG: %s(%d)", __FUNCTION__, __LINE__);	
  if(xTaskCreate(init_tcpthread, ((const char*)"tcpclient"), 8*1024, NULL, 
        tskIDLE_PRIORITY + 1, NULL) != pdPASS)
    printf("\n\r%s xTaskCreate(tcpclient) failed", __FUNCTION__);
  else
    printf("\n\r%s xTaskCreate(tcpclient) success", __FUNCTION__);
}

#if 0
#define ATSTRING_LEN 	(LOG_SERVICE_BUFLEN)
extern char at_string[ATSTRING_LEN];
#define at_printf(fmt, args...)  do{\
			/*uart_at_lock();*/\
			snprintf(at_string, ATSTRING_LEN, fmt, ##args); \
			uart_at_send_string(at_string);\
			/*uart_at_unlock();*/\
	}while(0)
#endif

void init_tcpthread(void *param) {
  printf("\n\rUART2: %s(%d), Available heap 0x%x\r\n", 
      __FUNCTION__, __LINE__, xPortGetFreeHeapSize());	
  int delay=2000/portTICK_PERIOD_MS;
  read_tcpclient_setting();
  vTaskDelay(delay);
  printf("\n\rUART2: %s(%d)\r\n", __FUNCTION__, __LINE__);	

  //struct tcp_pcb * fd;
  //struct in_addr dst;
  /*
  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = inet_addr("192.168.34.20");
  dst.sin_port = htons(15666);
  uint8_t cam=0;
  */
  conf.enable=1;
  memset(&conf.server, 0, sizeof(conf.server));
  conf.server.sin_family = AF_INET;
  conf.server.sin_addr.s_addr = inet_addr("192.168.34.20");
  conf.server.sin_port = htons(15666);
  conf.cam=1;
  //write_tcpclient_setting();

  int c_sockfd=-1;
  int sock_state=-1;
  int i;
  fd_set readfds;
  fd_set exceptfds;
  u8 *mac = LwIP_GetMAC(&xnetif[0]);
  u8 buf[32];
  u8 buf0[]={1,conf.cam,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]};
  
  printf("\n\rUART2: %s(%d)\r\n", __FUNCTION__, __LINE__);	

  gpio_t gpio_ledr;
  gpio_t gpio_ledg;

  gpio_init(&gpio_ledr, PC_0);
  gpio_dir(&gpio_ledr, PIN_OUTPUT);    // Direction: Output
  gpio_mode(&gpio_ledr, PullNone);     // No pull

  gpio_init(&gpio_ledg, PA_5);
  gpio_dir(&gpio_ledg, PIN_OUTPUT);    // Direction: Output
  gpio_mode(&gpio_ledg, PullNone);     // No pull
  gpio_write(&gpio_ledr, 1);
  gpio_write(&gpio_ledg, 1);

  while (1) {
    if(sock_state==-1){
      if(wifi_is_connected_to_ap()==0){
        c_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        printf("New sockfd %d!\r\n",c_sockfd);
        int res=connect(c_sockfd, (struct sockaddr *)&conf.server,  sizeof(conf.server));
        if(res == 0){
          printf("Connect %d to Server successful!\r\n",c_sockfd);
          printf("source %d bytes\r\n",7);
          for(i=0;i<8;i++){
            printf("%02x ",buf0[i]);
          }
          printf("\r\n");

          int encoded=encode_bin(buf0,7,buf,sizeof(buf));
          printf("encoded %d bytes\r\n",encoded);
          for(i=0;i<encoded;i++){
            printf("%02x ",buf[i]);
          }
          printf("\r\n");

          if(encoded>0)
            send(c_sockfd,buf,encoded,0);
          sock_state=1;
        }else{
          printf("Connect %d error %d\r\n",c_sockfd,res);
          printf("Connect error %s\r\n",strerror(errno));
          close(c_sockfd);
          vTaskDelay(delay);
        }
      }
    }else{
      FD_SET(c_sockfd, &readfds);
      FD_SET(c_sockfd, &exceptfds);	
      struct timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      int ret = select(1, &readfds, NULL, &exceptfds, &tv);
      if (ret>0){
        //connected
        //FD_ISSET(c_sockfd, &readfds)
        int bytes=read(c_sockfd, buf, 32);
        if (bytes==0) {
          printf("Connection closed\r\n");
          close(c_sockfd);
          sock_state=-1;
        }else if(bytes==-1){
          printf("Connection error %s\r\n",strerror(errno));
          close(c_sockfd);
          sock_state=-1;
        }else{
          int res=decode_bin(buf,bytes,NULL,0);
          if(res>0){
            printf(">+ cam %d LED %02x\r\n",conf.cam,buf[conf.cam]);
            gpio_write(&gpio_ledr, buf[conf.cam]&1?0:1);
            gpio_write(&gpio_ledg, buf[conf.cam]&2?0:1);
          }
          /*
          printf("got %d bytes ",bytes);
          printf("decoded %d bytes: ",res);
          for(i=0;i<res;i++){
            printf("%02x ",buf[i]);
          }
          printf("\r\n");
          */
        }
      }
    }
  }
}
int read_tcpclient_setting() {
  bool load_default = _TRUE;
  atcmd_update_partition_info(AT_PARTITION_TALLY, AT_PARTITION_READ, (u8 *)&conf, sizeof(conf));	
  printf("tcp client config en %d dst %s:%d cam %d\r\n",
      conf.enable,
      inet_ntoa(conf.server.sin_addr),
      htons(conf.server.sin_port),
      conf.cam
      );
  return 0;
}

int write_tcpclient_setting() {
	atcmd_update_partition_info(AT_PARTITION_TALLY, AT_PARTITION_WRITE, (u8 *)&conf, sizeof(conf));	
}

