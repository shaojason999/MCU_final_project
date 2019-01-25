/* PIC18F4550 USART Source File 
 * http://www.electronicwings.com
 */

#include "USART_Header_File.h"      /* Include USART header file */

/*****************************USART Initialization*******************************/
void USART_Init(long baud_rate)
{
    TRISC6 = 0;                     /* Make Tx pin as output */
    TRISC7 = 1;                     /* Make Rx pin as input */
    SPBRG = (int)BAUDRATE;          /* Baud rate, SPBRG = (F_CPU /(64*baud_rate)-1) */
    TXSTA = 0x20;                   /* Transmit Enable(TX) enable */ 
    RCSTA = 0x90;                   /* Receive Enable(RX) enable and serial port enable */
}
/******************TRANSMIT FUNCTION*****************************************/ 
void USART_TxChar(char data)
{        
    while(!TXIF);                   /* Wait for buffer empty */
    TXREG = data;                   /* Copy data to be transmit in Tx register */
}
/*******************RECEIVE FUNCTION*****************************************/
char USART_RxChar()
{
    while(!RCIF);                   /* Wait for receive interrupt flag*/
    return(RCREG);                  /* Return received data */
}

/*******************SEND STRING FUNCTION*****************************************/
void USART_SendString(const char *str)
{
   while(*str!='\0')                /* Transmit data until null */
   {            
        USART_TxChar(*str);
        str++;
   }
}
