#include <stdio.h> 

#include <avr/io.h>

#include <avr/interrupt.h>
#include <avr/signal.h>
#include <string.h>


#define SKIP_ROM   0xCC 

#define F_CPU 20000000UL
#include <util/delay.h> 


#define CONVERT_T   0x44      // DS18S20 commands 
#define READ      0xBE 
#define WRITE      0x4E 
#define EE_WRITE   0x48 
#define READ_PS      0xB4
#define UART_BUF_SIZE 16

#define MIN(X, Y)				((X) > (Y) ? (Y) : (X))
#define MAX(X, Y)				((X) > (Y) ? (X) : (Y))


typedef	unsigned char		UINT8;
typedef	unsigned int		UINT16;
typedef	unsigned long		UINT32;

#define	INVALID8	((UINT8) -1)
#define	INVALID16	((UINT16) -1)
#define	INVALID32	((UINT32) -1)

volatile int length = 0;
volatile char buffer[UART_BUF_SIZE] = {0};






unsigned short w1_init(unsigned short); 
unsigned short w1_read(unsigned short); 
void w1_write( unsigned short, unsigned short); 
void w1_writebyte(unsigned short, unsigned short); 
unsigned short w1_readbyte(unsigned short);
void PRINTF(char *data, ...);
void Init_System(void);
void on_off(int port);
void putch(char TX);
void take_over(char recived_data);
SIGNAL(SIG_USART_RECV);






UINT8 reset (UINT16 port) 
{
    DDRC |= (1 << port);            //Ausgang
	PORTC &= ~(1 << port);
    _delay_us(480);
	DDRC &= ~(1 << port);
    _delay_us(80);
	if (!(PINC & (1 << port))) {     //Pr?fe Slave-Antwort
		_delay_us(450);
        return 1;
	}
    else
        return 0;
}

UINT8 read_bit (UINT16 port) 
{
    DDRC |= (1 << port);            //Ausgang
	PORTC &= ~(1 << port);
    _delay_us(1);
	DDRC &= ~(1 << port);
    _delay_us(12);
    if (!(PINC & (1 << port)))       //Abtastung innerhalb von 15탎
        return 0;
    else
        return 1;
}

void write_bit (unsigned char bitval, UINT16 port) {    //kann 0 oder 1 sein
    DDRC |= (1 << port);            //Ausgang
	PORTC &= ~(1 << port);
    if (bitval)
        PORTC |= (1 << port);      //H-Pegel
    _delay_us (110);        
    DDRC &= ~(1 << port);
    PORTC &= ~(1 << port);
}

UINT8 read_byte (UINT16 port)
{
    unsigned char byte = 0;
	int i;
    for (i=0 ; i < 8 ; i++) {
        if (read_bit(port))
            byte |= (1 << i);
        _delay_us(120);
    }
    return byte;
}

void write_byte (unsigned char byte, UINT16 port)
{
	int i;

    for (i=0 ; i < 8 ; i++) {
        if (byte & (1 << i))
          write_bit(1, port);
        else
            write_bit(0, port);
	}
    _delay_us(120);
}  

UINT16 read_scratchpad (UINT16 port)
{
	int scratchpad[9];
	int i;
	if (reset(port))
	{
		write_byte(0xCC, port);
		write_byte(0x44, port);
		wait_ready(port);
		if (reset(port))
		{
			write_byte(0xCC, port);
			write_byte(0xBE, port);
			for (i=0; i<9; i++)
			{
				scratchpad [i] = read_byte(port);
			}
			
			return (scratchpad[0] | scratchpad[1] << 8);
		}
	}
	return 0;
}

void wait_ready (UINT16 port) {
	while (!(read_bit(port)));
}






void PRINTF(char *data, ...)
{

	va_list args;
	char astr[80];
	char *p_str;

	va_start(args, data);
	(void)vsnprintf(astr, sizeof(astr), data, args);
	va_end(args);

	p_str = astr;

	while(*p_str)
	{
	  putch(*p_str);
	  p_str++;
	}
}

void Init_System(void)
{
	DDRD = 0xFE;
	//PORTD |= 0xC0;

	//보-레이트 115.2kbps,비동기식,패리티 없음,스탑 1bit,문자열 사이즈 8bit,상승 엣지 검출
	//보-레이트 115.2kbps,비동기식,패리티 없음,스탑 1bit,문자열 사이즈 8bit,상승 엣지 검출
	UBRR0H = 0;                        
	UBRR0L = 10;                       
	UCSR0A = 0x00;                     
	UCSR0B = 0x98;                     
	UCSR0C = 0x86;         


	sei(); //Global interrupt enable

}


void on_off(int port)
{

		if(((PORTC >> port) & 0x1))
	{
		PORTD = PORTD & ~((1 << port) & 0xF); 
	}
	else
	{
		PORTD = PORTD | ((1 << port) & 0xF);
	}
}

void putch(char TX)
{

	while(!(UCSR0A & 0x20));

	UDR0 = TX;
}



void take_over(char recived_data)
{
	buffer[length++]= recived_data;

	char *pStr;
	int i;
	
	if(recived_data != 0x08)
	{
		putch(recived_data);
	}

	if(buffer[length - 1] == 0x0D)
	{

		if(buffer[length - 2] == '?')
		{
			PRINTF("\r\nHELP ");
			PRINTF("\r\n1. ");
			PRINTF("\r\n2. ");
			PRINTF("\r\n3. ");
			PRINTF("\r\n4. ");
			PRINTF("\r\n5. ");
			PRINTF("\r\n6. ");			
		}
		else if(strstr(buffer, "t0") != 0)
		{
			PRINTF("temperature = %d\r\n", read_scratchpad(0)/2);
		}
		else if(strstr(buffer, "t1") != 0)
		{
			PRINTF("temperature = %d\r\n", read_scratchpad(1)/2);
		}
		else if(strstr(buffer, "portd status") != 0)
		{
			PRINTF("\r\n");
			PRINTF("PORTD= 0x%x\r\n", PORTD);
		}		
		else if(length == 1)
		{
			//do notthing
		}
		else
		{
			PRINTF("\r\nERROR ");
		}

		PRINTF("\r\nUART> ");


		for(i = 0 ; i < length ; i ++)
		{		
			buffer[i] = 0;		
		}
		length = 0;		
	}
	else if(length >= 256)
	{
		PRINTF("\r\nERROR ");
		PRINTF("\r\nUART> ");
		length = 0;
	}
}

SIGNAL(SIG_USART_RECV)
{
	while(!(UCSR0A & 0x80));

	take_over(UDR0);
}



int main(void)
{
	UINT16 temperature_raw_0;
	UINT16 temperature_raw_1;
	UINT16 temp_gap;


	Init_System();
	while(1)
	{

		
		temperature_raw_0 = read_scratchpad(0);
		temperature_raw_1 = read_scratchpad(1);

		PRINTF("Temp0 : %u / Temp1 : %u\r\n", temperature_raw_0, temperature_raw_1);

		//NG
		//- temperature should be very big number
		temp_gap = (MAX(temperature_raw_0, temperature_raw_1) - MIN(temperature_raw_0, temperature_raw_1));

		switch(temp_gap)			
		{
			case 0:
				//do nothing
			break;

			case 1:
				//do nothing
			break;

			case 2:
				//PWM 15%
			break;

			case 3:
				//PWM 25%
			break;
			
			case 4:
				//PWM 35%
			break;

			case 5:
				//PWM 45%
			break;
			
			case 6:
				//PWM 55%
			break;
			
			case 7:
				//PWM 65%
			break;

			case 8:
				//PWM 70%
			break;


			default:
				//PWM 75%
			break;
			
		}
		
			



	}
}

