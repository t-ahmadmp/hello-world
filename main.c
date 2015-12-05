/**
  *****************************************************************************
  * @title   nuc140-LB board CoOS application
  * @author  CooCox
  * @date    Jan 09 2011
  * @brief   A simple example shows how to use CoOS on nuc140-LB board.
  * In this CoOS example all the components are COX components,that can
  * achieve a calendar system, which show in a 128*64 LCD
  * and can set the time and alarm, the keyboard through the matrix to
  * achieve these settings, the first in the digital display key values,
  * the latter three show the value of AD conversion.
  *******************************************************************************
  */

/*---------------------------- Include ---------------------------------------*/
#include <CoOS.h>			                 /*!< CoOS header file            */
#include "DrvSYS.h"
#include "cox_pio.h"

#include "seven_segment.h"
#include "NUC1xx_PIO.h"			// I/O macro

#include "di_uc1601.h"			// LCD 128*64
#include "nuc_spi_master.h"		// SPI for LCD

#include "KeyBoard.h"			// Keypad macro
#include "multitap.h"

/** LCD declaration **/
UC1601_Dev lcd = {
		COX_PIN(3,8),                        /* Chip select: GPIOD8           */
		&pi_pio,                             /* PIO Interface                 */
		&pi_spi3                             /* SPI Interface                 */
};

/** Keypad pinning declaration **/
KeyBoard_PinConfig MatrixConfig =
{
	3,
	{0x02,0x01,0x00},
	3,
	{0x03,0x04,0x05},
	&pi_pio,                                 /* PIO Interface used 		      */
};

/*---------------------------- Symbol Define ---------------------------------*/
#define STACK_SIZE_TASKA 128                 /*!< Define "taskA" task size    */
#define STACK_SIZE_TASKB 128                 /*!< Define "taskB" task size    */
#define TASK_STK_SIZE    128                 /*!< Define "task_init" task size*/


#define SEG_N0   0x82                        /*!< Segment of number 0         */
#define SEG_N1   0xEE                        /*!< Segment of number 1         */
#define SEG_N2   0x07                        /*!< Segment of number 2         */
#define SEG_N3   0x46                        /*!< Segment of number 3         */
#define SEG_N4   0x6A                        /*!< Segment of number 4         */
#define SEG_N5   0x52                        /*!< Segment of number 5         */
#define SEG_N6   0x12                        /*!< Segment of number 6         */
#define SEG_N7   0xE6                        /*!< Segment of number 7         */
#define SEG_N8   0x02                        /*!< Segment of number 8         */
#define SEG_N9   0x62                        /*!< Segment of number 9         */

unsigned char SEG_BUF[10]={SEG_N0, SEG_N1, SEG_N2, SEG_N3, SEG_N4, SEG_N5, SEG_N6, SEG_N7, SEG_N8, SEG_N9};

// LED0 is GPC12
#define LED0		(2<<8) + 12
#define	LED0_init	pi_pio.Init(LED0); pi_pio.Dir(LED0,COX_PIO_OUTPUT)
#define LED0_ON		pi_pio.Out(LED0,COX_PIO_LOW)
#define LED0_OFF	pi_pio.Out(LED0,COX_PIO_HIGH)

// LED1 is GPC13
#define LED1		(2<<8) + 13
#define	LED1_init	pi_pio.Init(LED1); pi_pio.Dir(LED1,COX_PIO_OUTPUT)
#define LED1_ON		pi_pio.Out(LED1,COX_PIO_LOW)
#define LED1_OFF	pi_pio.Out(LED1,COX_PIO_HIGH)

// Backspace is GPB15
#define	BSPACE		(1<<8) + 15
#define	BSPACE_init	pi_pio.Init(BSPACE); pi_pio.Dir(BSPACE,COX_PIO_INPUT)
#define	BSPACE_read	pi_pio.Read(BSPACE)

/*---------------------------- Variable Define -------------------------------*/
OS_STK     taskA_stk[STACK_SIZE_TASKA];	     /*!< Define "taskA" task stack   */
OS_STK     taskB_stk[STACK_SIZE_TASKB];	     /*!< Define "taskB" task stack   */
OS_STK     task_init_Stk[TASK_STK_SIZE];	 /*!< Define "task_init" task stack*/

//OS_FlagID  lcd_change_flag;
uint8_t	lcd_change_flag = 0;

/** 7-segment pinning declaration **/
const S_Segment_Dev nuc_lb_002 = {
	&pi_pio,                                 // Using GPIO Interface
	COX_PIN(4, 7),                           // pin_a
	COX_PIN(4, 2),                           // pin_b
	COX_PIN(4, 3),                           // pin_c
	COX_PIN(4, 4),                           // pin_d
	COX_PIN(4, 6),                           // pin_e
	COX_PIN(4, 5),                           // pin_f
	COX_PIN(4, 0),                           // pin_g
	COX_PIN(4, 1),                           // pin_h
	COX_PIN(2, 4),                           // pin_com0
	COX_PIN(2, 5),                           // pin_com1
	COX_PIN(2, 6),                           // pin_com2
	COX_PIN(2, 7),                           // pin_com3
};

/** Buzzer macro functions **/
// Buzzer is GPB11; Port 1 Pin 11
#define Buzzer_PIO		(1<<8)+11
#define Buzzer_init()	pi_pio.Init(Buzzer_PIO); pi_pio.Dir(Buzzer_PIO,COX_PIO_OUTPUT)
#define Buzzer_start()	pi_pio.Out(Buzzer_PIO,COX_PIO_LOW)
#define Buzzer_stop()	pi_pio.Out(Buzzer_PIO,COX_PIO_HIGH)

/*******************************************************************************
 * @brief       "taskA" task routine
 * @param[in]   None
 * @param[out]  None
 * @retval      None
 * @par Description
 * @details		Complete keyboard scan and add it to string buffer.
 * 				Also scans for all inputs and rings the buzzer.
 *******************************************************************************/
void taskA (void* pdata)
{
	char char_in, flag, led = 0, bspace = 0;
	uint32_t temp_key=0;


	KW1_561ASB.Init(&nuc_lb_002);
	LED0_init;
	BSPACE_init;

	KW1_561ASB.Clear(&nuc_lb_002);

	while(1)
	{
		if(!led)
		{
			led = 1;
			LED0_ON;
		}
		else
		{
			led = 0;
			LED0_OFF;
		}

		if((temp_key = KeyB.GetKey(&MatrixConfig)) || (bspace = !(BSPACE_read)))
		{
			KW1_561ASB.Clear(&nuc_lb_002);
			KW1_561ASB.Show(&nuc_lb_002,COX_PIN(2, 4+0),(SEG_BUF[temp_key]));

			if(bspace)
				temp_key = 'A';

			char_in = keypad_multitap(temp_key,&flag);
			buffer(char_in,flag);
			//CoSetFlag(lcd_change_flag);
			lcd_change_flag = 1;

			CoTimeDelay(0,0,0,50);
			while(KeyB.Scan(&MatrixConfig) == COX_SUCCESS || !(BSPACE_read))
				CoTimeDelay(0,0,0,50);
		}
	}
}

/*******************************************************************************
 * @brief       "taskB" task code
 * @param[in]   None
 * @param[out]  None
 * @retval      None
 * @par Description
 * @details    	Prints characters on string buffer to LCD
 *******************************************************************************
 */
void taskB (void* pdata)
{
	int i = 0, led = 0;

	di_uc1601.Init(&lcd);
	di_uc1601.Clear(&lcd);
	LED1_init;

	for (;;)
	{
		if(!led)
		{
			led = 1;
			LED1_ON;
		}
		else
		{
			led = 0;
			LED1_OFF;
		}

		//CoWaitForSingleFlag(lcd_change_flag,0);
		while(!lcd_change_flag)
			CoTimeDelay(0,0,0,100);

		di_uc1601.Clear(&lcd);

		for(i = 0; i<char_vert; i++)
		{
			if(text[i*char_horz])
				di_uc1601.Print(&lcd,i,&text[i*char_horz]);
			else
				break;
		}

		//CoClearFlag(lcd_change_flag);
		lcd_change_flag = 0;

		//CoTimeDelay(0,0,0,100);
	}
}

/*******************************************************************************
 * @brief       Initialization all the task in this application.
 * @param[in]   None
 * @param[out]  None
 * @retval      None
 * @par Description
 * @details    Initialize all the hardware ,Create all the flag and mutex in this application.
 *******************************************************************************
 */
void task_init (void* pdata)
{
	//lcd_change_flag = CoCreateFlag(0,0);

	CoCreateTask (taskB,0,0,&taskB_stk[STACK_SIZE_TASKB-1],STACK_SIZE_TASKB);
	CoCreateTask (taskA,0,0,&taskA_stk[STACK_SIZE_TASKA-1],STACK_SIZE_TASKA);

	CoExitTask();	                         /*!< Delete 'task_init' task.    */
}

/**
 *******************************************************************************
 * @brief		main function
 * @param[in] 	None
 * @param[out] 	None
 * @retval		None
 *******************************************************************************
 */
int main ()
{
	UNLOCKREG();
	SYSCLK->PWRCON.XTL12M_EN = 1;

	CoInitOS ();                             /*!< Initial CooCox CoOS         */
	/*!< Create init tasks	*/
	CoCreateTask(task_init, (void *)0, 10,&task_init_Stk[TASK_STK_SIZE-1], TASK_STK_SIZE);

	CoStartOS ();			                 /*!< Start multitask	          */

	while (1);                               /*!< The code don''t reach here  */
}


