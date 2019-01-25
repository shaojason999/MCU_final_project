#include <pic18f4520.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "Configuration_header_file.h"
#include "LCD_16x2_Header_File.h"   /* Include LCD header file */
#include "USART_Header_File.h"      /* Include USART header file */

/* Define Required XBee Frame Type and Responses */
#define START_DELIMITER                     0x7E
#define AT_COMMAND_FRAME                    0x08
#define TRANSMIT_REQUEST_FRAME              0x10
#define REMOTE_AT_COMMAND_FRAME             0x17
#define IO_DATA_SAMPLE_FRAME                0x92
#define AT_COMMAND_RESPONSE_FRAME           0x88
#define REMOTE_AT_COMMAND_RESPONSE_FRAME    0x97
#define RECEIVE_PACKET_FRAME                0x90
#define TRANSMIT_STATUS_FRAME               0x8B

#define FRAME_ID                            0x01
#define REMOTE_AT_COMMAND_OPT               0x02
#define TRANSMIT_REQUEST_OPT                0x00
#define TRANSMIT_REQUEST_BROADCAST_RADIUS   0x00

#define Read                                0
#define Write                               1


#define BUFFER_SIZE							100
#define DIGITAL_BUFFER_SIZE					16
#define ANALOG_BUFFER_SIZE					8

uint8_t		ReceiveBuffer[BUFFER_SIZE];
int8_t		DigitalData[DIGITAL_BUFFER_SIZE];
int16_t		AnalogData[ANALOG_BUFFER_SIZE];
uint16_t	BufferPointer	= 0,
			LastByteOfFrame	= 0; 
uint32_t	Command_Value	= 0;

bool _IsContainsDigital = false,
	 _IsContainsAnalog	= false;	

PWM1_Init(long setDuty)
{
    PR2 = setDuty;
    //(155+1)*4*(1/500k)=20ms
}

PWM1_Duty(unsigned int duty)
{
    //set duty to CCPR1L , CCP1X(CCP1CON:5) and CCP1Y(CCP1CON:4)
    CCP1CONbits.CCP1X = 1;
    CCP1CONbits.CCP1Y = 0;
    CCPR1L = duty >> 2;
}

PWM1_Start()
{
    //set CCP1CON
    CCP1CONbits.CCP1M0 = 0;
    CCP1CONbits.CCP1M1 = 0;
    CCP1CONbits.CCP1M2 = 1;
    CCP1CONbits.CCP1M3 = 1;
    //set TIMER2 on
    T2CONbits.TMR2ON = 1;
    //set FOSC = 500kHz
    //OSCCONbits.IRCF0 = 1;
   // OSCCONbits.IRCF1 = 1;
    //OSCCONbits.IRCF2 = 0;    
    //TRISC: set RC2 as output
    TRISCbits.RC2 = 0;
    //set TIMER2 prescaler
//    if (TMR2PRESCALE == 1)
//    {
//        T2CONbits.T2CKPS0 = 0;
//        T2CONbits.T2CKPS1 = 0;
//    }
//    else if(TMR2PRESCALE == 4)
//    {  
//        T2CONbits.T2CKPS0 = 1;
//        T2CONbits.T2CKPS1 = 0;
//    }
//    else if (TMR2PRESCALE == 16)
//    {
        T2CONbits.T2CKPS0 = 0;
        T2CONbits.T2CKPS1 = 1;
//    }
}


bool Is_Data_Received()				/* Check weather data received or not  */
{
	for (uint8_t i = 0; i < BUFFER_SIZE; i++)
	{
		if (ReceiveBuffer[i] != 0 && i > 0)
			return true;
	}
	return false;
}

bool Is_Checksum_Correct()			/* Check weather received data is correct or not */
{
	uint16_t checksum = 0;
	for(uint8_t i = 3; i < LastByteOfFrame; i++)
		checksum = checksum + ReceiveBuffer[i];
	checksum = 0xFF - checksum;
	if(ReceiveBuffer[LastByteOfFrame] == (uint8_t)checksum && LastByteOfFrame > 0)
		return true;
	else
		return false;
}

/* Sample function for parsing received buffer as per frame type */
void sample()
{
	uint8_t Frame_Type, Is_Analog;
	uint16_t Length, Is_Digital, Digital_Value;
	
	_IsContainsAnalog = false;
	_IsContainsDigital = false;
	
	Length = ((int)ReceiveBuffer[1]<<8) + ReceiveBuffer[2];/* 2 byte Frame length is at 1st and 2nd position of frame */
	Frame_Type = ReceiveBuffer[3];					/* 1 byte Frame type is at 3rd position of frame */

	switch (Frame_Type)
	{
		case (IO_DATA_SAMPLE_FRAME):	/* Parse received I/O data sample frame */
			if(Is_Data_Received() == false || Is_Checksum_Correct() == false) break;
			Is_Digital = ((int)ReceiveBuffer[16]<<8) + ReceiveBuffer[17];
			Is_Analog = ReceiveBuffer[18];
            
			Digital_Value = ((int)ReceiveBuffer[19]<<8) + ReceiveBuffer[20];    /*digital is always first*/
			
            /*there may more than one sample input in one packet*/
			if(Is_Analog != 0)
				_IsContainsAnalog = true;
			if(Is_Digital != 0)
				_IsContainsDigital = true;
/****** Check For whether sample contains Analog/Digital Sample *********/		

			for (uint8_t i = 0; i < DIGITAL_BUFFER_SIZE; i++)
			{
				if(((Is_Digital >> i) & 0x01) == 1 && ((Digital_Value>>i) & 0x01) != 0){     /*check the mask*/
                 //   LATCbits.LATC3=1;
					DigitalData[i] = 1;
                }
				else if(((Is_Digital >> i) & 0x01) == 1 && ((Digital_Value>>i) & 0x01) == 0){
                //    LATCbits.LATC3=0;
					DigitalData[i] = 0;
                }
				else
					DigitalData[i] = -1;
			}
			
			for (uint8_t i = 0, j = 0; i < ANALOG_BUFFER_SIZE; i++)
			{
				if(((Is_Analog >> i) & 0x01) == 1)
				{
					if(Is_Digital != 0) /*if the input has analog and digital, the analog will be 21~22+j*2*/
						AnalogData[i] = 256 * ReceiveBuffer[21+(j*2)] + ReceiveBuffer[22+(j*2)];    /*y is in [0], x is in [2]*/
					else    /*if there is only analog, it will be 19 and 20*/
						AnalogData[i] = 256 * ReceiveBuffer[19+(j*2)] + ReceiveBuffer[20+(j*2)];
					j++;    /*how many analog*/
				}
				else
				{
					AnalogData[i] = -1;
				}
			}
		default:
			break;
	}
}

bool Get_Sample()								/* Get sample function */
{
	MSdelay(200);								/* Wait for response */
	for (uint16_t count = 0; Is_Data_Received() == false; count++)/* Check for if data available or not */
	{
		if(count>15000)							/* Print error message if buffer still empty */
		{
			return false;
		}
	}
    INTCONbits.GIE=0;                           /* Disable global interrupt to parse received data */
	sample();									/* Parse data in sample function */
	memset(ReceiveBuffer,0,BUFFER_SIZE);		/* Clear ReceiveBuffer */
    INTCONbits.GIE=1;                           /* Enable Global Interrupt */
	return true;								/* Return success value */
}

void Remote_AT_Command(uint32_t Long_Address_MSB, uint32_t Long_Address_LSB, uint16_t Short_Address, const char* ATCommand, bool action)
{
    uint16_t Length,Checksum;
	if (action == Write)
	{
		if(Command_Value > 0x00FFFFFF) Length = 19;/* Define parameter value depend frame length */
		else if(Command_Value > 0x00FFFF) Length = 18;
		else if(Command_Value > 0x00FF) Length = 17;
		else Length = 16;
	}
	else
		Length = 15;

	Checksum = REMOTE_AT_COMMAND_FRAME + FRAME_ID;/* Calculate Checksum */
	for (int8_t i = 24; i >= 0; i = i-8) 
		Checksum = Checksum + (Long_Address_MSB >> i);
	for (int8_t i = 24 ; i >= 0; i = i-8) 
		Checksum = Checksum + (Long_Address_LSB >> i);
	
	Checksum = Checksum + (Short_Address >> 8) + Short_Address + REMOTE_AT_COMMAND_OPT + ATCommand[0] + ATCommand[1];
	if (action == Write)
	Checksum = Checksum + (Command_Value >> 24) + (Command_Value >> 16) + (Command_Value >> 8) + Command_Value ;
	Checksum = 0xFF - Checksum;					/* Subtract checksum lower byte from 0xFF to get 1 byte checksum */

	USART_TxChar(START_DELIMITER);				/* Send frame byte by byte serially, start with 1 byte Delimiter */
	USART_TxChar(Length >> 8);					/* Send 2 byte length */
	USART_TxChar(Length);
	USART_TxChar(REMOTE_AT_COMMAND_FRAME);		/* Send 1 byte frame type */
	USART_TxChar(FRAME_ID);						/* Send 1 byte frame ID */
	for (int8_t i = 24 ; i >= 0 ; i = i-8)		/* Send 32-bit long destination address MSB */
		USART_TxChar(Long_Address_MSB >> i);
	for (int8_t i = 24 ; i >= 0 ; i = i-8)		/* Send 32-bit long destination address LSB */
		USART_TxChar(Long_Address_LSB >> i);
	USART_TxChar(Short_Address >> 8);			/* Send 16-bit long destination address */
	USART_TxChar(Short_Address);
	USART_TxChar(REMOTE_AT_COMMAND_OPT);		/* Send Option */
	USART_SendString(ATCommand);				/* Send AT command */
    int i=i+1;
	if(action == Write)
	{
		if(Length == 19)						/* Send value */
		{
			USART_TxChar(Command_Value >> 24);
			USART_TxChar(Command_Value >> 16);
			USART_TxChar(Command_Value >> 8);
		}
		if(Length == 18)
		{
			USART_TxChar(Command_Value >> 16);
			USART_TxChar(Command_Value >> 8);
		}
		if(Length == 17) 
			USART_TxChar(Command_Value >> 8);
		USART_TxChar(Command_Value);
	}
	USART_TxChar(Checksum);						/* Send Checksum */
    //char *a=0x7E001217010013A2004189A9C2FFFE024952313030D2;
}

void AT_Command(const char* ATCommand, bool action)
{
	uint16_t Length,Checksum;
	if (action == Write)
	{
        /*Length=basic 4 bytes + x bytes*/
        /*Frame Length = Length of (Frame Type + Frame ID + AT Command) in Bytes*/
		if(Command_Value > 0x00FFFFFF) Length = 8;/* Define parameter value depend frame length */
		else if(Command_Value > 0x00FFFF) Length = 7;
		else if(Command_Value > 0x00FF) Length = 6;
		else Length = 5;
	}
	else
		Length = 4;
    
    /*reference http://www.electronicwings.com/sensors-modules/xbee-module*/
    /*Checksunm=0x08+0x01+data(add every byte)*/    /*see line 17*/
    /*FRAME_ID: set to non-zero. You can disable the response by setting the frame ID to 0 in the request.*/
	Checksum = AT_COMMAND_FRAME + FRAME_ID + ATCommand[0] + ATCommand[1];
	if (action == Write)
	Checksum = Checksum + (Command_Value >> 24) + (Command_Value >> 16) + (Command_Value >> 8) + Command_Value ;    /*Command_Value is a global variable*/
	Checksum = 0xFF - Checksum;

    /* Send frame byte by byte serially */
	USART_TxChar(START_DELIMITER);				/*send char(byte)*/
	USART_TxChar(Length >> 8);  /*length is always 2 bytes*/
	USART_TxChar(Length);
	USART_TxChar(AT_COMMAND_FRAME);
	USART_TxChar(FRAME_ID);
	USART_SendString(ATCommand);    /*send string (call USART_TxChar several times)*/

    /*I think it may send a 0x00000000(basic 4 bytes) in the beginning*/
	if(action == Write)
	{
		if(Length == 8)						/* Send value */    /*8 bytes*/
		{
			USART_TxChar(Command_Value >> 24);
			USART_TxChar(Command_Value >> 16);
			USART_TxChar(Command_Value >> 8);
		}
		if(Length == 7) /*7 bytes*/
		{
			USART_TxChar(Command_Value >> 16);
			USART_TxChar(Command_Value >> 8);
		}
		if(Length == 6) 
			USART_TxChar(Command_Value >> 8);
        
		USART_TxChar(Command_Value);
	}
	USART_TxChar(Checksum);
}

void Write_AT_Command(char* ATCommand, uint32_t _CommandValue)
{
	Command_Value = _CommandValue;
	AT_Command(ATCommand, Write);
}

void Read_AT_Command(char* ATCommand)
{
	AT_Command(ATCommand, Read);
}

void Write_Remote_AT_Command(uint32_t Long_Address_MSB, uint32_t Long_Address_LSB, uint16_t Short_Address, char* ATCommand, uint32_t _CommandValue)
{
	Command_Value = _CommandValue;
	Remote_AT_Command(Long_Address_MSB, Long_Address_LSB, Short_Address, ATCommand, Write);
}

void __interrupt(low_priority) LO_ISR(void)
{
    char received_char;
    if(RCIF==1){    /*receive interrupt flag*/
        received_char = RCREG;
        if(RCSTAbits.OERR)                      /* check if any overrun occur due to continuous reception */
        {           
            CREN = 0;
            NOP();
            CREN=1;
        }
        if (received_char == START_DELIMITER)	/* If received byte is start delimiter then copy LastByteOfFrame & reset counter */
        {
            LastByteOfFrame = BufferPointer;
            BufferPointer = 0;
            ReceiveBuffer[BufferPointer] = received_char;
        }
        else
        {
            BufferPointer++;
            ReceiveBuffer[BufferPointer] = received_char;	/* Else copy data to buffer & increment counter */
        }
    }
}

void SetTo_Broadcast()
{
	MSdelay(500);
	Write_AT_Command("DH", 0x00000000);
	MSdelay(500);
	Write_AT_Command("DL", 0x0000FFFF);
	MSdelay(500);
}

int main(void)
{
	char _buffer[25];
    double Temperature;
	uint32_t Remote_Address_DH = 0x0013A200;
    uint32_t Remote_Address_DL = 0x4189A9C2;

    OSCCON=0x72;                                /* set internal clock to 8MHz */
	USART_Init(9600);							/* Initiate USART with 9600 baud rate */
	LCD_Init();									/* Initialize LCD */
	LCD_String_xy(1, 0, "X-Bee Network ");  	/* Print initial test message */
	LCD_String_xy(2, 0, "Demo..!!");
	MSdelay(1000);                             /* XBee Initialize time */
	LCD_Clear();
    INTCONbits.GIE=1;                           /* enable Global Interrupt */
    INTCONbits.PEIE=1;                          /* enable Peripheral Interrupt */
    PIE1bits.RCIE=1;                            /* enable Receive Interrupt */	

	LCD_String_xy(1, 0, "Setting X-Bee to");
	LCD_String_xy(2, 0, "Broadcast mode  ");
    
    TRISCbits.RC3=0;    /*direction control 1*/
    TRISDbits.RD1=0;    /*direction control 2*/
    TRISCbits.RC0=1;    /*infrared ray detection*/
    TRISCbits.RC1=1;    /*laser detection*/
    TRISCbits.RC2=0;    /*light*/
    LATCbits.LATC3=0;
    LATCbits.LATC2=0;
   
	SetTo_Broadcast();  						/* Set XBee coordinator to broadcast mode */
	LCD_Clear();
	
	LCD_String_xy(1, 0, "Request Samples ");
	/* Request Samples from remote X-Bee device at 100ms Sample rate */
	Write_Remote_AT_Command(Remote_Address_DH, Remote_Address_DL, 0xFFFE, "IR", 100);   /*0x64*/
	MSdelay(1000);
    
	LCD_Clear();
    RCIF=0;
    
 //   PWM1_Init(255);
 //   PWM1_Start();
 //   int degree=200;

	while (1)
	{
		Get_Sample();
		if (_IsContainsDigital)
		{
            if(DigitalData[4] > 0)/* Switch status on DIO4 pin */
            {
                sprintf(_buffer, "Switch = %d   ", DigitalData[2]);
                //LATCbits.LATC1=1;
                //LATCbits.LATC2=1;
                LCD_String_xy(1, 0, _buffer);/* print on 1st row */
				memset(_buffer, 0, 25);		/* Clear Buffer */
            }
            else{
                ;
                //LATCbits.LATC1=0;
                //LATCbits.LATC2=0;
            }
		}
		if (_IsContainsAnalog)
		{
            /*y*/
			//if(AnalogData[0] <=1523 && AnalogData[0]>=900){/*y not move*/
			if(AnalogData[0] <=1523 && AnalogData[0]>=900 && AnalogData[2] <=1523 && AnalogData[2]>=900){/*x,y not move*/
                LATDbits.LATD1=0;
                LATCbits.LATC3=0;
            }
			else if(AnalogData[0] <=650 && AnalogData[0]>=350){/*y down*/
                LATDbits.LATD1=1;
                LATCbits.LATC3=0;
            }
			else if(AnalogData[0] <=200 && AnalogData[0]>=0 && RC0!=0){/*y up*/
                LATCbits.LATC3=1;
                LATDbits.LATD1=0;
            }
            
            /*x*/
            if(AnalogData[2] <=1523 && AnalogData[2]>=900){/*x not move*/
                ;
            }
			else if(AnalogData[2] <=650 && AnalogData[2]>=350){/*x right*/
                ;
            }
			else if(AnalogData[2] <=200 && AnalogData[2]>=0){/*x left*/
                LATDbits.LATD1=1;
                LATCbits.LATC3=0;
            }
		}
        if(RC1)
            LATCbits.LATC2=1;
        else
            LATCbits.LATC2=0;
	}
}