#include "main.h"
#include "mlx90614.h"

extern I2C_HandleTypeDef hi2c2;

#define MLX90614_ADDR (0x5A << 1)

float MLX90614_ReadObjectTemp(void)
{
    uint8_t data[3];
    uint16_t raw;

    HAL_I2C_Mem_Read(&hi2c2,
                     MLX90614_ADDR,
                     0x07,
                     I2C_MEMADD_SIZE_8BIT,
                     data,
                     3,
                     HAL_MAX_DELAY);

    raw = ((uint16_t)data[1] << 8) | data[0];

    return (raw * 0.02f) - 273.15f;
}

float MLX90614_ReadAmbientTemp(void)
{
    uint8_t data[3];
    uint16_t raw;

    HAL_I2C_Mem_Read(&hi2c2,
                     MLX90614_ADDR,
                     0x06,
                     I2C_MEMADD_SIZE_8BIT,
                     data,
                     3,
                     HAL_MAX_DELAY);

    raw = ((uint16_t)data[1] << 8) | data[0];

    return (raw * 0.02f) - 273.15f;
}
