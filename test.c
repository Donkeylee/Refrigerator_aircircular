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

volatile int length = 0;
volatile char buffer[64] = {0};


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






unsigned char reset (void) {
    DDRC |= (1<<0);            //Ausgang
	PORTC &= ~(1<<0);
    _delay_us (480);
	DDRC &= ~(1<<0);
    _delay_us (80);
	if (!(PINC & (1<<0))) {     //Pr?fe Slave-Antwort
		_delay_us (450);
        return 1;
	}
    else
        return 0;
}

unsigned char read_bit (void) {
    DDRC |= (1<<0);            //Ausgang
	PORTC &= ~(1<<0);
    _delay_us (1);
	DDRC &= ~(1<<0);
    _delay_us (12);
    if (!(PINC & (1<<0)))       //Abtastung innerhalb von 15�s
        return 0;
    else
        return 1;
}

void write_bit (unsigned char bitval) {    //kann 0 oder 1 sein
    DDRC |= (1<<0);            //Ausgang
	PORTC &= ~(1<<0);
    if (bitval)
        PORTC |= (1<<0);      //H-Pegel
    _delay_us (110);        
    DDRC &= ~(1<<0);
    PORTC &= ~(1<<0);
}

unsigned char read_byte (void) {
    unsigned char byte = 0;
	int i;
    for (i=0; i<8; i++) {
        if (read_bit ())
            byte |= (1<<i);
        _delay_us (120);
    }
    return byte;
}

void write_byte (unsigned char byte) {
	int i;

    for (i=0; i<8; i++) {
        if (byte & (1<<i))
          write_bit (1);
        else
            write_bit (0);
	}
    _delay_us (120);
}  

unsigned int read_scratchpad (void) {
	int scratchpad[9];
	int i;
	if (reset ()) {
		write_byte (0xCC);
		write_byte (0x44);
		wait_ready ();
		if (reset ()) {
			write_byte (0xCC);
			write_byte (0xBE);
			for (i=0; i<9; i++)
				scratchpad [i] = read_byte ();

			//for (i=0; i<9; i++)
				//PRINTF("scratchpad [%d] = \r\n", scratchpad[i]);

			return (scratchpad[0] | scratchpad[1] << 8);
		}
	}
	return 0;
}

void wait_ready (void) {
	while (!(read_bit ()));
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

	//��-����Ʈ 115.2kbps,�񵿱��,�и�Ƽ ����,��ž 1bit,���ڿ� ������ 8bit,��� ���� ����
	//��-����Ʈ 115.2kbps,�񵿱��,�и�Ƽ ����,��ž 1bit,���ڿ� ������ 8bit,��� ���� ����
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
		else if(strstr(buffer, "t") != 0)		
		{
			PRINTF("temperature = %d\r\n", read_scratchpad()/2);
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


	Init_System();
	while(1)
	{
		int temperature_raw;
		float temperature;
		int target_temp = 43;

		

		
		temperature_raw = read_scratchpad();

		if(temperature_raw % 2 == 0)
		{
			PRINTF("current temp / target temp = %4d.0 / %4d.0 (heater status = %s)\r\n", temperature_raw/2, target_temp, (PORTD >> 2) & 1 ? "ON" : "OFF");
		}
		else
		{
			PRINTF("current temp / target temp = %4d.5 / %4d.5 (heater status = %s)\r\n", temperature_raw/2, target_temp, (PORTD >> 2) & 1 ? "ON" : "OFF");
		}

		if(temperature_raw > (target_temp * 2))
		{
			PORTD = PORTD & ~((1 << 2) & 0xF);
			PORTD = PORTD & ~((1 << 3) & 0xF);
		}
		else if(temperature_raw < (target_temp * 2))
		{
			PORTD = PORTD | ((1 << 2) & 0xF);
			PORTD = PORTD | ((1 << 3) & 0xF);
		}
	}
}
