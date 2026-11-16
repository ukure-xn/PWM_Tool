#include "flash.h" 

uint16_t SPI_FLASH_TYPE=W25Q64;//默认就是25Q64
extern SPI_HandleTypeDef hspi2;
#define W25Qx_TIMEOUT_VALUE 100


uint8_t SPI2_ReadByte(void)
{			
	uint8_t RxData;		
	HAL_SPI_Receive(&hspi2, &RxData, 1, W25Qx_TIMEOUT_VALUE);		 
	return RxData; //返回通过SPIx最近接收的数据					    
}
 
void SPI2_WriteByte(uint8_t TxData)
{					  	
	HAL_SPI_Transmit(&hspi2, &TxData, 1, W25Qx_TIMEOUT_VALUE);  //通过外设SPIx发送一个数据						   			    
}

/**
* @brief    读取W25QXX的状态寄存器，W25Q64一共有2个状态寄存器
* @param     reg  —— 状态寄存器编号(1~2)
* @retval    状态寄存器的值
*/
uint8_t SPI_Flash_ReadSR(void )
{
 	uint8_t byte=0;   
 	FLASH_CS_ENABLE();                            //使能器件   
 	SPI2_WriteByte(W25X_ReadStatusReg);    //发送读取状态寄存器命令    
 	byte = SPI2_ReadByte();             //读取一个字节  
 	FLASH_CS_DISABLE();                            //取消片选     
 	return byte;   

}
void SPI_Flash_Wait_Busy(void)   
{   
while ((SPI_Flash_ReadSR()&0x01)==0x01);   // 等待BUSY位清空
}  



 
//SPI_FLASH写使能	
 //将WEL置位   
 void SPI_FLASH_Write_Enable(void)   
 {
 	FLASH_CS_ENABLE();                           //使能器件   
 	SPI2_WriteByte(W25X_WriteEnable);      //发送写使能  
 	FLASH_CS_DISABLE();                            //取消片选     	      
 } 
 
 //SPI_FLASH写禁止	
 //将WEL清零  
 void SPI_FLASH_Write_Disable(void)   
 {  
 	FLASH_CS_ENABLE();                           //使能器件   
 	SPI2_WriteByte(W25X_WriteDisable);     //发送写禁止指令    
 	FLASH_CS_DISABLE();                            //取消片选     	      
 } 

 
//uint8_t W25QXX[4096];
//uint16_t SPI_Flash_ReadID(void) //读ID
//{
//    MX_FLASH_Init();		    
//		HAL_SPI_TransmitReceive(&hspi2,cmd,W25QXX,4,500);
//	
//		//SPI_Flash_Erase_Chip(); //全片擦除
//		//写入10个字节  然后再读取10个字节  
//	
//	  SPI_Flash_Write_Page(0,4,wwwwww);
//	  SPI_Flash_Read(0,4,SSSSSS);
//		MX_OLED_Init();
//		OLED_ShowNum(0,0,SSSSSS[0],4,12);
//		OLED_ShowNum(0,1,SSSSSS[1],4,12);
//		OLED_ShowNum(0,2,SSSSSS[2],4,12);
//		OLED_ShowNum(0,3,SSSSSS[3],4,12);
//		OLED_Refresh_Gram();
//		return 0;
//}   
uint16_t SPI_Flash_ReadID(void)
{
		MX_FLASH_Init();
		uint16_t Temp = 0;	  
		FLASH_CS_ENABLE();				    
		SPI2_WriteByte(0x90);//发送读取ID命令	    
		SPI2_WriteByte(0x00); 	    
		SPI2_WriteByte(0x00); 	    
		SPI2_WriteByte(0x00); 	 

		Temp|=SPI2_ReadByte()<<8;  
		Temp|=SPI2_ReadByte();	 
		FLASH_CS_DISABLE();		

		return Temp;
} 

 //擦除整个芯片
 //整片擦除时间:
 //W25X16:25s 
 //W25X32:40s 
 //W25X64:40s 
 //等待时间超长...
 void SPI_Flash_Erase_Chip(void)   
 {                                             
 	SPI_FLASH_Write_Enable();                  //SET WEL 
 	SPI_Flash_Wait_Busy();   
 	FLASH_CS_ENABLE();                            //使能器件   
 	SPI2_WriteByte(W25X_ChipErase);        //发送片擦除命令  
 	FLASH_CS_DISABLE();                            //取消片选     	      
 	SPI_Flash_Wait_Busy();   				   //等待芯片擦除结束
 }  
 
 //读取SPI FLASH  
 //在指定地址开始读取指定长度的数据
 //pBuffer:数据存储区
 //ReadAddr:开始读取的地址(24bit)
 //NumByteToRead:要读取的字节数(最大65535)
 uint8_t cnd_data[4];
 void SPI_Flash_Read(uint32_t ReadAddr,uint16_t NumByteToRead,uint8_t* pBuffer)   
 { 
		MX_FLASH_Init(); 												    
		cnd_data[0]=W25X_ReadData;
		cnd_data[1]=(uint8_t)((ReadAddr)>>16);
		cnd_data[2]=(uint8_t)((ReadAddr)>>8);
		cnd_data[3]=(uint8_t)ReadAddr;
		HAL_SPI_Transmit(&hspi2, cnd_data, 4, W25Qx_TIMEOUT_VALUE); 
		HAL_SPI_Receive(&hspi2,pBuffer,NumByteToRead,W25Qx_TIMEOUT_VALUE);
		FLASH_CS_DISABLE();//取消片选
 }
 
 //SPI在一页(0~65535)内写入少于256个字节的数据
 //在指定地址开始写入最大256字节的数据
 //pBuffer:数据存储区
 //WriteAddr:开始写入的地址(24bit)
 //NumByteToWrite:要写入的字节数(最大256),该数不应该超过该页的剩余字节数!!!
 void SPI_Flash_Write_Page(uint32_t WriteAddr,uint16_t NumByteToWrite,uint8_t* pBuffer)
 {
	MX_FLASH_Init();
 	SPI_FLASH_Write_Enable();                  //SET WEL
 	FLASH_CS_ENABLE();                            //使能器件 
	cnd_data[0]=W25X_PageProgram;
	cnd_data[1]=(uint8_t)((WriteAddr)>>16);
	cnd_data[2]=(uint8_t)((WriteAddr)>>8);
	cnd_data[3]=(uint8_t)WriteAddr;
	HAL_SPI_Transmit(&hspi2, cnd_data, 4, W25Qx_TIMEOUT_VALUE); 
	HAL_SPI_Transmit(&hspi2, pBuffer, NumByteToWrite, W25Qx_TIMEOUT_VALUE); 
 //	for(i=0;i<NumByteToWrite;i++) SPI2_WriteByte(pBuffer[i]);//循环写数
 	FLASH_CS_DISABLE();                            //取消片选 
 	SPI_Flash_Wait_Busy();					   //等待写入结束
 } 
void W25QXX_Erase_Sector(uint32_t Dst_Addr)   
{  
	//监视falsh擦除情况,测试用   
 	//printf("fe:%x\r\n",Dst_Addr);	  
		Dst_Addr*=4096;
			SPI_FLASH_Write_Enable();                     //SET WEL 	 
		SPI_Flash_Wait_Busy();   
		FLASH_CS_ENABLE();                            //使能器件   
//		SPI2_WriteByte(W25X_SectorErase);      //发送扇区擦除指令 
//		SPI2_WriteByte((uint8_t)((Dst_Addr)>>16));  //发送24bit地址    
//		SPI2_WriteByte((uint8_t)((Dst_Addr)>>8));   
//		SPI2_WriteByte((uint8_t)Dst_Addr); 
		cnd_data[0]=W25X_SectorErase;
		cnd_data[1]=(uint8_t)((Dst_Addr)>>16);
		cnd_data[2]=(uint8_t)((Dst_Addr)>>8);
		cnd_data[3]=(uint8_t)Dst_Addr;
	  HAL_SPI_Transmit(&hspi2, cnd_data, 4, W25Qx_TIMEOUT_VALUE); 
 	FLASH_CS_DISABLE();                            //取消片选 
 	SPI_Flash_Wait_Busy();					   //等待写入结束
}  



void W25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)   
{ 			 		 
	uint16_t pageremain;	   
	pageremain=256-WriteAddr%256; //单页剩余的字节数		 	    
	if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//不大于256个字节
	while(1)
	{	   
		SPI_Flash_Write_Page(WriteAddr,pageremain,pBuffer);
		if(NumByteToWrite==pageremain)break;//写入结束了
	 	else //NumByteToWrite>pageremain
		{
			pBuffer+=pageremain;
			WriteAddr+=pageremain;	

			NumByteToWrite-=pageremain;			  //减去已经写入了的字节数
			if(NumByteToWrite>256)pageremain=256; //一次可以写入256个字节
			else pageremain=NumByteToWrite; 	  //不够256个字节了
		}
	};	    
} 

uint8_t W25QXX_BUFFER[4096];		 
void W25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)   
{ 
	MX_FLASH_Init();
	uint32_t secpos;
	uint16_t secoff;
	uint16_t secremain;	   
 	uint16_t i;    
	uint8_t * W25QXX_BUF;	  
   	W25QXX_BUF=W25QXX_BUFFER;	     
 	secpos=WriteAddr/4096;//扇区地址  
	secoff=WriteAddr%4096;//在扇区内的偏移
	secremain=4096-secoff;//扇区剩余空间大小   
 	//printf("ad:%X,nb:%X\r\n",WriteAddr,NumByteToWrite);//测试用
 	if(NumByteToWrite<=secremain)secremain=NumByteToWrite;//不大于4096个字节
	while(1) 
	{	
		SPI_Flash_Read(secpos*4096,4096,W25QXX_BUF);//读出整个扇区的内容
		for(i=0;i<secremain;i++)//校验数据
		{
			if(W25QXX_BUF[secoff+i]!=0XFF)break;//需要擦除  	  
		}
		if(i<secremain)//需要擦除
		{
			W25QXX_Erase_Sector(secpos);//擦除这个扇区
			for(i=0;i<secremain;i++)	   //复制
			{
				W25QXX_BUF[i+secoff]=pBuffer[i];	  
			}
			W25QXX_Write_NoCheck(W25QXX_BUF,secpos*4096,4096);//写入整个扇区  

		}else W25QXX_Write_NoCheck(pBuffer,WriteAddr,secremain);//写已经擦除了的,直接写入扇区剩余区间. 				   
		if(NumByteToWrite==secremain)break;//写入结束了
		else//写入未结束
		{
			secpos++;//扇区地址增1
			secoff=0;//偏移位置为0 	 

		   	pBuffer+=secremain;  //指针偏移
			WriteAddr+=secremain;//写地址偏移	   
		   	NumByteToWrite-=secremain;				//字节数递减
			if(NumByteToWrite>4096)secremain=4096;	//下一个扇区还是写不完
			else secremain=NumByteToWrite;			//下一个扇区可以写完了
		}	 
	};	 
}





