
/**
--------------------------------------------------------------------------------
--                                                                            --
--                      UNIVERSIDADE FEDERAL DE SANTA MARIA		              --
--							Centro de Tecnologia			                  --
--						Curso de Engenharia de Computação	                  --
--                                                                            --
--                      Santa Maria - Rio Grande do Sul/BR                    --
--                                                                            --
--------------------------------------------------------------------------------
--                                                                            --
-- Design      : Real Time Home  v2.0                                         --
-- File		   : csc_app.c													  --
-- Author      : Luis Felipe de Deus and Nathanael Jorge Luchetta             --
--                                                                            --
--------------------------------------------------------------------------------
--                                                                            --
-- Created     : 10 Jun 2019                                                  --
-- Update      : 23 Jun 2019                                                  --
--------------------------------------------------------------------------------
--                              Overview                                      --
--                                                                            --
--  Board: SAMD21							                                  --
--  Peripherals: Bluetooth - ATBTLC1000-MR | Temperature: I/O1		          --
--  Features: FreeRTOS v10.0								                  --
--            Tasks: Bluetooth connection, receive and flow control			  --
--            Temperatura SP e estado do lampada salvos em serial flash       --
--------------------------------------------------------------------------------
*/


/*- Includes -----------------------------------------------------------------------*/
#include <asf.h>
#include "demotasks.h"

int main(void )
{
	///Init System
	system_init();
	///Init Board
	board_init();
	///Create Tasks execution flow
	demotasks_init();	
	
	///FreeRTOS run tasks!
	vTaskStartScheduler();
	
	while(true){}
		
	return 0;
}



