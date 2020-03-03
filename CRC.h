#ifndef _CRC16_
#define _CRC16_

void CRC16_Update(unsigned char *CRC16_High, unsigned char *CRC16_Low, unsigned char val,unsigned char * crctable);
void CRC16_Init  (unsigned char *CRC16_High, unsigned char *CRC16_Low, unsigned char * crctable,unsigned short polynome,unsigned short initvalue);

#endif