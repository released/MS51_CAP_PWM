
#include "MS51_16K.h"
#include	"project_config.h"

/*_____ D E F I N I T I O N S ______________________________________________*/
#define SYS_CLOCK 								(24000000ul)

#define CAP_CLOCK_DIV_SETTING(void)				{TIMER2_DIV_64;}
#define CAP_CLOCK_VALUE							(SYS_CLOCK >> 6)

#define PWM_CLOCK_DIV_SETTING(void)				{PWM_CLOCK_DIV_64;}
#define PWM_CLOCK_VALUE							(SYS_CLOCK >> 6)

// #define PWM_FREQ 								(1000ul)
#define PWM_FREQ 								(250ul)
// #define PWM_FREQ 								(80ul)

#define DUTY_RESOLUTION							(100ul)
#define DUTY_MAX								(DUTY_RESOLUTION)
#define DUTY_MIN								(1ul)

#define ENABLE_CAP_IRQ
//#define ENABLE_CAP_POLLING

/*_____ D E C L A R A T I O N S ____________________________________________*/
volatile uint8_t u8TH0_Tmp = 0;
volatile uint8_t u8TL0_Tmp = 0;

//UART 0
bit BIT_TMP;
bit BIT_UART;
bit uart0_receive_flag=0;
unsigned char uart0_receive_data;

volatile uint32_t duty_ch0 = 0;
volatile uint16_t freq_ch0 = 0;
volatile uint16_t width0_ch0 = 0;
volatile uint16_t width1_ch0 = 0;
volatile uint16_t widthTotal_ch0 = 0;

/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint16_t counter_tick = 0;

/*_____ M A C R O S ________________________________________________________*/


/*_____ F U N C T I O N S __________________________________________________*/

void tick_counter(void)
{
	counter_tick++;
}

uint16_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void send_UARTString(uint8_t* Data)
{
	#if 1
	uint16_t i = 0;

	while (Data[i] != '\0')
	{
		#if 1
		SBUF = Data[i++];
		#else
		UART_Send_Data(UART0,Data[i++]);		
		#endif
	}

	#endif

	#if 0
	uint16_t i = 0;
	
	for(i = 0;i< (strlen(Data)) ;i++ )
	{
		UART_Send_Data(UART0,Data[i]);
	}
	#endif

	#if 0
    while(*Data)  
    {  
        UART_Send_Data(UART0, (unsigned char) *Data++);  
    } 
	#endif
}

void send_UARTASCII(uint16_t Temp)
{
    uint8_t print_buf[16];
    uint16_t i = 15, j;

    *(print_buf + i) = '\0';
    j = (uint16_t)Temp >> 31;
    if(j)
        (uint16_t) Temp = ~(uint16_t)Temp + 1;
    do
    {
        i--;
        *(print_buf + i) = '0' + (uint16_t)Temp % 10;
        (uint16_t)Temp = (uint16_t)Temp / 10;
    }
    while((uint16_t)Temp != 0);
    if(j)
    {
        i--;
        *(print_buf + i) = '-';
    }
    send_UARTString(print_buf + i);
}

void capture_channel_0(void)
{
	static uint8_t _flag_ch0 = 1;	
	if (_flag_ch0)
	{
		width0_ch0 = MAKEWORD(C0H,C0L);
	}
	else
	{
		width1_ch0 = MAKEWORD(C0H,C0L);
	}

	_flag_ch0 ^= 1;

	widthTotal_ch0 = width0_ch0 + width1_ch0;
	freq_ch0 = (float) (CAP_CLOCK_VALUE)/(widthTotal_ch0);		
	duty_ch0 = ((unsigned int)DUTY_RESOLUTION*((float)width0_ch0 / widthTotal_ch0));
}

void capture_log (void)
{	
	#if (_debug_log_UART_ == 1)	// when width0 = 0 , calculate will fail
	if (is_flag_set(flag_cap0_ready))	//	if ((ABS((timer2_clk/freq)-widthTotal)<5) && ((width0 != 0) && (width1 != 0) ) )
	{
		set_flag(flag_cap0_ready , Disable);			
//		send_UARTString("capture_application :");	

		printf("0)F:%4D" ,freq_ch0);
		printf(",D:%ld" ,duty_ch0);	
		printf(",H:%4D" ,width0_ch0);
		printf(",L:%4D" ,width1_ch0);	
		
 		printf(",T:%4D" ,widthTotal_ch0);		
		printf(",res:%ld" ,CAP_CLOCK_VALUE/freq_ch0);	
		
		printf("\r\n");

	}
	
	#endif		
}

void capture_application (void)
{	

	if((CAPCON0 & SET_BIT0) != 0)
	{
		clr_CAPCON0_CAPF0; 		
		capture_channel_0();

		set_flag(flag_cap0_ready , Enable);		
	}
	
}

#if defined (ENABLE_CAP_IRQ)
void Capture_ISR(void) interrupt 12
{
    _push_(SFRS);

    capture_application();

    _pop_(SFRS);
}
#endif

void Timer2_ISR (void) interrupt 5
{
    _push_(SFRS);

    clr_T2CON_TF2;	

    _pop_(SFRS);
}

//P1.2 , IC0  
void CAP_Init(void)
{
    P12_QUASI_MODE;
    P12 = 1;

    TIMER2_CAP0_Capture_Mode;
	
	CAP_CLOCK_DIV_SETTING();//TIMER2_DIV_4;

	IC0_P12_CAP0_BOTHEDGE_CAPTURE;

	#if defined (ENABLE_CAP_IRQ)
	clr_EIPH_PCAPH;
	set_EIP_PCAP;
	
    ENABLE_CAPTURE_INTERRUPT;	//set_EIE_ECAP;
    #endif
	
	clr_T2CON_TF2;
    set_T2CON_TR2;
	ENABLE_TIMER2_INTERRUPT;

    ENABLE_GLOBAL_INTERRUPT;	
}

//(P1.5 , PWM0_CH5) : PWM control output 
void PWM0_CHx_Init(uint16_t uFrequency,uint16_t d)
{
	unsigned long res = 0;

	P15_PUSHPULL_MODE;			//Add this to enhance MOS output capability
    PWM5_P15_OUTPUT_ENABLE;	
 
    PWM_IMDEPENDENT_MODE;
    PWM_CLOCK_DIV_SETTING();	//PWM_CLOCK_DIV_64;

/*
	PWM frequency   = Fpwm/((PWMPH,PWMPL)+1) = (24MHz/2)/(PWMPH,PWMPL)+1) = 20KHz
*/	
	res = PWM_CLOCK_VALUE;	//(SYS_CLOCK>>6);
	res = res/uFrequency;		//value 375 for 16K
	res = res - 1;
	
    PWMPH = HIBYTE(res);
    PWMPL = LOBYTE(res);
	
//	res = d*(MAKEWORD(PWMPH,PWMPL)+1)/DUTY_RESOLUTION;	
	res = d*(res+1)/DUTY_RESOLUTION;

	set_SFRS_SFRPAGE;
    PWM5H = HIBYTE(res);
    PWM5L = LOBYTE(res);
    clr_SFRS_SFRPAGE;
	
    set_PWMCON0_LOAD;
    set_PWMCON0_PWMRUN;	
}

void GPIO_Init(void)
{
	P12_PUSHPULL_MODE;	
}

void task_1000ms(void)
{
	static uint8_t state = 0;
	if (is_flag_set(flag_1000ms))
	{
		set_flag(flag_1000ms,Disable);

		//application
		// P12 ^= 1;
		switch(state)
		{
			case 1:
				PWM0_CHx_Init(PWM_FREQ , 75);
				break;
			case 2:
				PWM0_CHx_Init(PWM_FREQ , 60);
				break;
			case 3:
				PWM0_CHx_Init(PWM_FREQ , 20);
				state = 0;
				break;				
		}

		state++;
	}
}

void task_100ms(void)
{
	if (is_flag_set(flag_100ms))
	{
		set_flag(flag_100ms,Disable);

		//application
	
	}
}

void task_10ms(void)
{	

	if (is_flag_set(flag_10ms))
	{
		set_flag(flag_10ms,Disable);
		
		//application

		#if defined (ENABLE_CAP_POLLING)
		capture_application();	
		#endif

		capture_log();
	}
}

void task_5ms(void)
{
	if (is_flag_set(flag_5ms))
	{
		set_flag(flag_5ms,Disable);

		//application
	
	}
}

void task_1ms(void)
{
	if (is_flag_set(flag_1ms))
	{
		set_flag(flag_1ms,Disable);

		//application
	
	}
}

void task_loop(void)
{
	task_1ms();
	task_5ms();
	task_10ms();
	task_100ms();	
	task_1000ms();	
}

void Timer0_IRQHandler(void)
{

	// const uint16_t div_1ms = 5;	
	const uint16_t div_5ms = 5;	
	const uint16_t div_10ms = 10;
	const uint16_t div_100ms = 100;
	const uint16_t div_1000ms = 1000;

	tick_counter();
	
	set_flag(flag_1ms,Enable);	

	#if 0
	if ((get_tick() % div_1ms) == 0)
	{
		set_flag(flag_1ms,Enable);	
	}
	#endif

	if ((get_tick() % div_5ms) == 0)
	{
		set_flag(flag_5ms,Enable);
	}		

	if ((get_tick() % div_10ms) == 0)
	{
		set_flag(flag_10ms,Enable);
	}		

	if ((get_tick() % div_100ms) == 0)
	{	
		set_flag(flag_100ms,Enable);
	}	

	if ((get_tick() % div_1000ms) == 0)
	{	
		set_flag(flag_1000ms,Enable);
	}
}

void Timer0_ISR(void) interrupt 1        // Vector @  0x0B
{
    _push_(SFRS);	
	
    TH0 = u8TH0_Tmp;
    TL0 = u8TL0_Tmp;
    clr_TCON_TF0;
	
	Timer0_IRQHandler();

    _pop_(SFRS);	
}

void TIMER0_Init(void)
{
	uint16_t res = 0;

	/*
		formula : 16bit 
		(0xFFFF+1 - target)  / (24MHz/psc) = time base 

	*/	
	// const uint16_t TIMER_DIVx_VALUE_200us_FOSC_240000 = 65536-400;

	ENABLE_TIMER0_MODE1;	// mode 0 : 13 bit , mode 1 : 16 bit
    TIMER0_FSYS_DIV12;
	
	u8TH0_Tmp = HIBYTE(TIMER_DIV12_VALUE_1ms);
	u8TL0_Tmp = LOBYTE(TIMER_DIV12_VALUE_1ms); 

    TH0 = u8TH0_Tmp;
    TL0 = u8TL0_Tmp;

    ENABLE_TIMER0_INTERRUPT;                       //enable Timer0 interrupt
    ENABLE_GLOBAL_INTERRUPT;                       //enable interrupts
  
    set_TCON_TR0;                                  //Timer0 run
}


void Serial_ISR (void) interrupt 4 
{
    _push_(SFRS);

    if (RI)
    {   
      uart0_receive_flag = 1;
      uart0_receive_data = SBUF;
      clr_SCON_RI;                                         // Clear RI (Receive Interrupt).
    }
    if  (TI)
    {
      if(!BIT_UART)
      {
          TI = 0;
      }
    }

    _pop_(SFRS);	
}

void UART0_Init(void)
{
	#if 1
	const unsigned long u32Baudrate = 115200;
	P06_QUASI_MODE;    //Setting UART pin as Quasi mode for transmit
	
	SCON = 0x50;          //UART0 Mode1,REN=1,TI=1
	set_PCON_SMOD;        //UART0 Double Rate Enable
	T3CON &= 0xF8;        //T3PS2=0,T3PS1=0,T3PS0=0(Prescale=1)
	set_T3CON_BRCK;        //UART0 baud rate clock source = Timer3

	RH3    = HIBYTE(65536 - (SYS_CLOCK/16/u32Baudrate));  
	RL3    = LOBYTE(65536 - (SYS_CLOCK/16/u32Baudrate));  
	
	set_T3CON_TR3;         //Trigger Timer3
	set_IE_ES;

	ENABLE_GLOBAL_INTERRUPT;

	set_SCON_TI;
	BIT_UART=1;
	#else	
    UART_Open(SYS_CLOCK,UART0_Timer3,115200);
    ENABLE_UART0_PRINTF; 
	#endif
}


void MODIFY_HIRC_24(void)
{
	unsigned char u8HIRCSEL = HIRC_24;
    unsigned char data hircmap0,hircmap1;
//    unsigned int trimvalue16bit;
    /* Check if power on reset, modify HIRC */
    SFRS = 0 ;
	#if 1
    IAPAL = 0x38;
	#else
    switch (u8HIRCSEL)
    {
      case HIRC_24:
        IAPAL = 0x38;
      break;
      case HIRC_16:
        IAPAL = 0x30;
      break;
      case HIRC_166:
        IAPAL = 0x30;
      break;
    }
	#endif
	
    set_CHPCON_IAPEN;
    IAPAH = 0x00;
    IAPCN = READ_UID;
    set_IAPTRG_IAPGO;
    hircmap0 = IAPFD;
    IAPAL++;
    set_IAPTRG_IAPGO;
    hircmap1 = IAPFD;
    clr_CHPCON_IAPEN;

	#if 0
    switch (u8HIRCSEL)
    {
		case HIRC_166:
		trimvalue16bit = ((hircmap0 << 1) + (hircmap1 & 0x01));
		trimvalue16bit = trimvalue16bit - 15;
		hircmap1 = trimvalue16bit & 0x01;
		hircmap0 = trimvalue16bit >> 1;

		break;
		default: break;
    }
	#endif
	
    TA = 0xAA;
    TA = 0x55;
    RCTRIM0 = hircmap0;
    TA = 0xAA;
    TA = 0x55;
    RCTRIM1 = hircmap1;
    clr_CHPCON_IAPEN;
    PCON &= CLR_BIT4;
}


void SYS_Init(void)
{
    MODIFY_HIRC_24();

    ALL_GPIO_QUASI_MODE;
    ENABLE_GLOBAL_INTERRUPT;                // global enable bit	
}

void main (void) 
{
    SYS_Init();

    UART0_Init();
	GPIO_Init();					

	TIMER0_Init();

//	PWM0_CHx_Init(PWM_FREQ , 850);		//8.5 %
	PWM0_CHx_Init(PWM_FREQ , 75);		//75 %	, solution : 100

	//(P1.2 , PWM0_CH0 , IC0) , input capture 
	CAP_Init();
		
    while(1)
    {
		task_loop();
		
    }
}



