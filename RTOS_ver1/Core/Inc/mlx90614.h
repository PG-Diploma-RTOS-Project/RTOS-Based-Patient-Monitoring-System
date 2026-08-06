/*
 * mlx90614.h
 *
 *  Created on: 13-Jun-2026
 *      Author: sunbeam
 */

#ifndef INC_MLX90614_H_
#define INC_MLX90614_H_


#include "main.h"

#define MLX90614_ADDR (0x5A << 1)

float MLX90614_ReadObjectTemp(void);
float MLX90614_ReadAmbientTemp(void);

#endif /* INC_MLX90614_H_ */
