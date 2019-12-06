#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>

extern void console_init(void);
extern void tcpclient_start(void);
extern int uart_atcmd_module_init(void);
typedef int (*init_done_ptr)(void);
extern init_done_ptr p_wlan_init_done_callback;

int other_threads_init(void){
  uart_atcmd_module_init();
  tcpclient_start();
}

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
	/* Initialize log uart and at command service */
	console_init();	

	/* pre-processor of application example */
	pre_example_entry();

	/* wlan intialization */
	wlan_network();

	p_wlan_init_done_callback = other_threads_init;

	/* Execute application example */
	example_entry();

//  tcpclient_start();
    	/*Enable Schedule, Start Kernel*/
	vTaskStartScheduler();
}


