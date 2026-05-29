#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int HAL_StatusTypeDef;

typedef struct {
    uint32_t TypeErase;
    uint32_t VoltageRange;
    uint32_t Sector;
    uint32_t NbSectors;
} FLASH_EraseInitTypeDef;

#define HAL_OK 0
#define FLASH_TYPEERASE_SECTORS 0U
#define FLASH_VOLTAGE_RANGE_3 0U
#define FLASH_SECTOR_11 11U
#define FLASH_TYPEPROGRAM_WORD 0U

uint32_t HAL_GetTick(void);
void HAL_FLASH_Unlock(void);
void HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *sector_error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type_program, uintptr_t address, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif