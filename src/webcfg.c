
#include <lwip_netconf.h>
#include "tcpip.h"
#include <dhcp/dhcps.h>
#if CONFIG_WLAN
#include <wlan/wlan_test_inc.h>
#include <wifi/wifi_conf.h>
#include <wifi/wifi_util.h>
#endif
#include "at_cmd/atcmd_wifi.h"

extern rtw_mode_t wifi_mode;
extern struct netif xnetif[NET_IF_NUM]; 
extern rtw_wifi_setting_t wifi_setting;

int run_config_mode(){
  wifi_mode==RTW_MODE_AP;

  rtw_ap_info_t ap = {0};
  u8 *mac = LwIP_GetMAC(&xnetif[0]);
  sprintf((char *)ap.ssid.val, "cfg_%02x%02x%02x%02x%02x%02x",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  ap.ssid.len = strlen((char*)ap.ssid.val);
  ap.security_type = RTW_SECURITY_OPEN;
	ap.password = NULL;
	ap.password_len = 0;
  memset(wifi_setting.password, 0, sizeof(wifi_setting.password));
  wifi_setting.security_type = ap.security_type;
  printf("Ssid %s\n",ap.ssid.val);

  /*
  ap.password = "defaultpw";
  ap.password_len = strlen((char*)ap.password);
  ap.security_type = RTW_SECURITY_WPA2_AES_PSK;
  */

  ap.channel = 1;

  /*
  memset(wifi_setting.ssid, 0, sizeof(wifi_setting.ssid));;
  memcpy(wifi_setting.ssid, ap.ssid.val, strlen((char*)ap.ssid.val));
  wifi_setting.ssid[ap.ssid.len] = '\0';
  wifi_setting.security_type = ap.security_type;
  if(ap.security_type !=0) wifi_setting.security_type = 1;
  else wifi_setting.security_type = 0;
  if (ap.password)
    memcpy(wifi_setting.password, ap.password, strlen((char*)ap.password));
  else
    memset(wifi_setting.password, 0, sizeof(wifi_setting.password));
  wifi_setting.channel = ap.channel;


  printf("Ssid %s\n",wifi_setting.ssid);
  printf("%s:%d\n",__FUNCTION__,__LINE__);
  */
	dhcps_deinit();
	wifi_off();
	vTaskDelay(20);
  if (wifi_on(wifi_mode) < 0){
    printf("\r\n[ATPA] ERROR : Wifi on failed");
    return -1;
  }

  if(wifi_start_ap((char*)ap.ssid.val, ap.security_type, (char*)ap.password, ap.ssid.len, ap.password_len, ap.channel) < 0) {
    printf("\r\n[ATPA] ERROR : Start AP failed");
    return -1;
  }

  int timeout = 20;
  while(1) {
    char essid[33];
    if(wext_get_ssid( WLAN0_NAME , (unsigned char *) essid) > 0) {
      if(strcmp((const char *) essid, (const char *)ap.ssid.val) == 0) {
        break;
      }
    }

    if(timeout == 0) {
      printf("\r\n[ATPA] ERROR : Start AP timeout");
      return -1;
    }
    vTaskDelay(1 * configTICK_RATE_HZ);
    timeout --;
  }

  LwIP_UseStaticIP(&xnetif[0]);
  dhcps_init(&xnetif[0]);
  //init_wifi_struct();
  start_web_server();
};

int run_prod_mode(){
  stop_web_server();
  wifi_mode=RTW_MODE_STA;
  LoadWifiConfig();
  dhcps_deinit();
  wifi_off();
  vTaskDelay(20);
  if (wifi_on(RTW_MODE_STA) < 0){
    printf("\r\n[ATPN] ERROR: Wifi on failed");
    return -1;
  }
}
