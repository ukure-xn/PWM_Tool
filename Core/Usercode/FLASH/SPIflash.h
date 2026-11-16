#ifndef __SPIFLASH_H
#define __SPIFLASH_H			    
#include "oled.h" 
#include "stm32f1xx_hal.h"
//W25X系列/Q系列芯片列表	   
//W25Q80 ID  0XEF13
//W25Q16 ID  0XEF14
//W25Q32 ID  0XEF15
//W25Q64 ID  0XEF16	
#define W25Q80 	0XEF13 	
#define W25Q16 	0XEF14
#define W25Q32 	0XEF15
#define W25Q64 	0XEF16
#define W25Q128 	0XEF17

#define     FLASH_CS_GPIO_Port   GPIOB
#define     FLASH_CS_Pin  GPIO_PIN_10

#define     FLASH_CS_DISABLE()      HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
#define     FLASH_CS_ENABLE()       HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);


extern uint16_t SPI_FLASH_TYPE;		//定义我们使用的flash芯片型号		   
				 
////////////////////////////////////////////////////////////////////////////
 
//指令表
#define W25X_WriteEnable		  0x06
#define W25X_WriteDisable		  0x04
#define W25X_ReadStatusReg		0x05
#define W25X_WriteStatusReg		0x01
#define W25X_ReadData			    0x03
#define W25X_FastReadData		  0x0B
#define W25X_FastReadDual		  0x3B
#define W25X_PageProgram		  0x02
#define W25X_BlockErase			  0xD8
#define W25X_SectorErase		  0x20
#define W25X_ChipErase			  0xC7
#define W25X_PowerDown			  0xB9
#define W25X_ReleasePowerDown	0xAB
#define W25X_DeviceID			    0xAB
#define W25X_ManufactDeviceID	0x90
#define W25X_JedecDeviceID		0x9F

void SPI_Flash_Read(uint32_t ReadAddr,uint16_t NumByteToRead,uint8_t* pBuffer);
void SPI_Flash_Write_Page(uint32_t WriteAddr,uint16_t NumByteToWrite,uint8_t* pBuffer);
uint16_t SPI_Flash_ReadID(void);
void W25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite);
void W25QXX_Erase_Sector(uint32_t Dst_Addr)   ;
void W25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite);
#endif
















