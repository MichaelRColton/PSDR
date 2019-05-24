[#ftl]
/**
  ******************************************************************************
  * File Name          : dma.h
  * Description        : This file contains all the function prototypes for
  *                      the dma.c file
  ******************************************************************************
[@common.optinclude name=mxTmpFolder+"/license.tmp"/][#--include License text --]
  ******************************************************************************
  */
[#assign ipName = "DMA"]
[#if dmas?size > 0]
  [#list dmas as dma]
    [#assign ipName = dma]
  [/#list]
[/#if]
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __${ipName?lower_case}_H
#define __${ipName?lower_case}_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
[#assign includesList = ""]
[#list includes as include]
[#if !includesList?contains(include)]
#include "${include}"
[#assign includesList = includesList+" "+include]
[/#if]
[/#list]
#n
/* DMA memory to memory transfer handles -------------------------------------*/
[#if variables?? && variables?size > 0]
[#list variables as variable]
extern ${variable.value} ${variable.name};
[/#list]
[/#if]
extern void _Error_Handler(char*, int);
#n
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */
#n
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */
#n
void MX_${ipName}_Init(void);
[#compress]
#n/* USER CODE BEGIN Prototypes */
#n     
/* USER CODE END Prototypes */
#n
[/#compress]
#ifdef __cplusplus
}
#endif

#endif /* __${ipName?lower_case}_H */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
