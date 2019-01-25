
/* PIC18F4550 USART Header File 
 * http://www.electronicwings.com
 */

#ifndef USART_HEADER_FILE_H
#define	USART_HEADER_FILE_H

#include <pic18f4520.h>             /* Include PIC18F4550 header file */
#define F_CPU 8000000/64            /* Define ferquency */
#define BAUDRATE (((float)(F_CPU)/(float)baud_rate)-1)/* Define Baud value */

void USART_Init(long);              /* USART Initialization function */
void USART_TxChar(char);            /* USART character transmit function */
char USART_RxChar();                /* USART character receive function */
void USART_SendString(const char *);/* USART String transmit function */


#endif	/* USART_HEADER_FILE_H */

