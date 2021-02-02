################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/background.c \
../src/ball.c \
../src/bounce.c \
../src/enemyship.c \
../src/explosion.c \
../src/extrafunctions.c \
../src/laser.c \
../src/lcd.c \
../src/paddle.c \
../src/railgun.c \
../src/syscalls.c \
../src/system_stm32f0xx.c 

OBJS += \
./src/background.o \
./src/ball.o \
./src/bounce.o \
./src/enemyship.o \
./src/explosion.o \
./src/extrafunctions.o \
./src/laser.o \
./src/lcd.o \
./src/paddle.o \
./src/railgun.o \
./src/syscalls.o \
./src/system_stm32f0xx.o 

C_DEPS += \
./src/background.d \
./src/ball.d \
./src/bounce.d \
./src/enemyship.d \
./src/explosion.d \
./src/extrafunctions.d \
./src/laser.d \
./src/lcd.d \
./src/paddle.d \
./src/railgun.d \
./src/syscalls.d \
./src/system_stm32f0xx.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -mfloat-abi=soft -DSTM32 -DSTM32F0 -DSTM32F091RCTx -DDEBUG -DSTM32F091 -DUSE_STDPERIPH_DRIVER -I"/home/ayden/STM32Workspace/Final Project/StdPeriph_Driver/inc" -I"/home/ayden/STM32Workspace/Final Project/inc" -I"/home/ayden/STM32Workspace/Final Project/CMSIS/device" -I"/home/ayden/STM32Workspace/Final Project/CMSIS/core" -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


