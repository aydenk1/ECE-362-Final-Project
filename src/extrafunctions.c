#include "stm32f0xx.h"
#include <stdio.h>      // for sprintf()
#include <stdint.h>     // for uint8_t
#include <string.h>     // for strlen() and strcmp()
#include "lcd.h"

//Globals
char display[8];
char offset;
char history[16];
char queue[2];
int  qin;
int  qout;
int percentTable[16];

const char font[] = {
        [' '] = 0x00,
        ['0'] = 0x3f,
        ['1'] = 0x06,
        ['2'] = 0x5b,
        ['3'] = 0x4f,
        ['4'] = 0x66,
        ['5'] = 0x6d,
        ['6'] = 0x7d,
        ['7'] = 0x07,
        ['8'] = 0x7f,
        ['9'] = 0x67,
        ['A'] = 0x77,
        ['B'] = 0x7c,
        ['C'] = 0x39,
        ['D'] = 0x5e,
        ['*'] = 0x49,
        ['#'] = 0x76,
        ['.'] = 0x80,
        ['?'] = 0x53,
        ['b'] = 0x7c,
        ['r'] = 0x50,
        ['g'] = 0x6f,
        ['i'] = 0x10,
        ['n'] = 0x54,
        ['u'] = 0x1c,
};

//Functions Declarations
//Display Functions
//=============================================================================
void setup_spi2();
void spi_cmd(int x);
void spi_data(int x);
void spi_init_oled();

//I2C / Ammo LED Functions
//=============================================================================
void setup_i2c(void);
void i2c_start(uint32_t devaddr, uint8_t size, uint8_t dir);
void i2c_stop(void);
void i2c_waitidle(void);
int8_t i2c_senddata(uint8_t devaddr, void *pdata, uint8_t size);
int8_t i2c_receivedata(uint8_t SlaveAddress, uint8_t *pData, uint8_t Size);
void i2c_set_iodir(uint8_t x);
void i2c_set_gpio(uint8_t x);
uint8_t i2c_get_gpio(void);

//Keypad and Seven-Segment Functions
//=============================================================================
void set_row();
int get_cols();
void insert_queue(int n);
void update_hist(int cols);
void setup_tim7();
int getkey();
int checkkey();
void shift(char c);

//Game Specific Functions
//=============================================================================
void nano_wait(unsigned int n) {
    asm( "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

void setupAmmoLEDs()
{
    setup_i2c();            //Turns on GPIO Extender
    i2c_set_iodir(0xf0);    //Sets LEDS to output
}

void updateAmmoLEDs(int ammo)
{
    switch(ammo) {
            case 3:
                i2c_set_gpio(0xF);
                break;
            case 2:
                i2c_set_gpio(0xC);
                break;
            case 1:
                i2c_set_gpio(0x8);
                break;
            case 0:
                i2c_set_gpio(0x0);
                break;
            default:
                i2c_set_gpio(0xF);
                break;
    }
}

void setup_tim17()
{
    // Set this to invoke the ISR 100 times per second.
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;

    TIM17->PSC = 48000-1;
    TIM17->ARR = 10-1;

    TIM17->DIER |= TIM_DIER_UIE;        //Enables the interrupt in TIM7
    NVIC->ISER[0] |= 1 << TIM17_IRQn;   //Enables the interrupt
    NVIC_SetPriority(TIM17_IRQn,0xff);
    TIM17->CR1 |= TIM_CR1_CEN;          //Enables the timer
}

char check_key(const char left, const char right, const char fire)
{
    // If the '*' key is pressed, return '*'
    // If the 'D' key is pressed, return 'D'
    // Otherwise, return 0

    char key = checkkey();
    if (key == left || key == right || key == fire)
    {
        return key;
    }

    return (0);
}

void updateScore(int score)
{
    int i = 0;
    char str[9];
    snprintf(str, 9, "%d", score);

    display[0] = font['0'];
    display[1] = font['0'];
    display[2] = font['0'];
    display[3] = font['0'];
    display[4] = font['0'];
    display[5] = font['0'];
    display[6] = font['0'];
    display[7] = font['0'];

    while ((str[i] !='\0') && (i < 8))
    {
        shift(str[i]);
        ++i;
    }
}

void setupOLED()
{
    setup_spi2();
    spi_init_oled();
}

void updateOLED(int fireRdy)
{
    int topAddr;
    int botAddr;
    int i;

    if(fireRdy == 1) {
        spi_cmd(0x01);                  //Clear display
        spi_cmd(0x02);                  //Return home
        return;
    }

    for(i = 0; i < 16; ++i) {
        if(fireRdy < percentTable[i]) {
            break;
        }
        else if(fireRdy == percentTable[i]) {
            topAddr = i | 0x80;
            botAddr = topAddr | 0x40;
            spi_cmd(topAddr);           //Move cursor to first row
            spi_data(0xFF);             //Send bar
            spi_cmd(botAddr);           //Move cursor to bottom row
            spi_data(0xFF);             //Send bar
            return;
        }
    }
}

void setPercentTable(const int fireDly)
{
    float div = (float)fireDly / 16.0;

    for(int i = 0; i < 16; ++i)
    {
        percentTable[i] = div * ((float)i + 1.0);
    }
}

void initOLED()
{
    int topAddr;
    int botAddr;
    int i;

    for(i = 0; i < 16; ++i) {
        topAddr = i | 0x80;
        botAddr = topAddr | 0x40;
        spi_cmd(topAddr);           //Move cursor to first row
        spi_data(0xFF);             //Send bar
        spi_cmd(botAddr);           //Move cursor to bottom row
        spi_data(0xFF);             //Send bar
    }
}

//Display Functions
//=============================================================================
void setup_spi1_displayTFT(){
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3 | GPIO_MODER_MODER4 | GPIO_MODER_MODER5 | GPIO_MODER_MODER7);
    GPIOA->MODER |= GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1 | GPIO_MODER_MODER7_1;    //Alternate Functions
    GPIOA->MODER |= GPIO_MODER_MODER2_0 | GPIO_MODER_MODER3_0;                          //Outputs

    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR1 &= ~SPI_CR1_BR;
    SPI1->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
    SPI1->CR2 = SPI_CR2_SSOE | SPI_CR2_NSSP;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void setup_spi2(){
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= 1<<14;                  //Enables SPI2
    GPIOB->MODER &= ~(GPIO_MODER_MODER12 | GPIO_MODER_MODER13 | GPIO_MODER_MODER15);
    GPIOB->MODER |= GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1 | GPIO_MODER_MODER15_1;
    SPI2->CR1 |= SPI_CR1_MSTR | SPI_CR1_BR;
    SPI2->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
    SPI2->CR2 = SPI_CR2_SSOE | SPI_CR2_NSSP | SPI_CR2_DS_3 | SPI_CR2_DS_0;
    SPI2->CR1 |= SPI_CR1_SPE;
}

void spi_cmd(int x){
    while((((SPI2->SR) >> 1) & 1) != 1);    //Waits for the TXE bit to be set
    SPI2->DR = x;                           //Writes to the DE
}

void spi_data(int x){
    while((((SPI2->SR) >> 1) & 1) != 1);    //Waits for the TXE bit to be set
    SPI2->DR = x | 0x200;                   //Writes data to the display
}

void spi_init_oled(){
    nano_wait(1000000);
    spi_cmd(0x38);                           //set for 8-bit operation
    spi_cmd(0x08);                           //turn display off
    spi_cmd(0x01);                           //clear display
    nano_wait(2000000);
    spi_cmd(0x06);                           //set the display to scroll
    spi_cmd(0x02);                           //move the cursor to the home position
    spi_cmd(0x0c);
}

//I2C / Ammo LED Functions
//=============================================================================
void setup_i2c(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    GPIOB->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9);
    GPIOB->MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1;
    GPIOB->AFR[1] |= 1 | 1 << 4;

    I2C1->CR1 &= ~I2C_CR1_PE;               //Disable to reset
    I2C1->CR1 &= ~I2C_CR1_ANFOFF;           //0 makes analog noise filter on
    I2C1->CR1 &= ~I2C_CR1_ERRIE;            //Error interrupt disable
    I2C1->CR1 &= ~I2C_CR1_NOSTRETCH;        //Allow clock stretching

    I2C1->TIMINGR &= ~I2C_TIMINGR_PRESC;    //Clear prescaler
    I2C1->TIMINGR |= 0 << 28;               //Set prescaler to 0
    I2C1->TIMINGR |= 3 << 20;               //SCLDEL
    I2C1->TIMINGR |= 1 << 16;               //SDADEL
    I2C1->TIMINGR |= 3 << 8;                //SCLH
    I2C1->TIMINGR |= 9 << 0;                //SCLL

    //I2C "Own address" 1 register (I2C_OAR1)
    I2C1->OAR1 &= ~I2C_OAR1_OA1EN;          // Disable own address 1
    I2C1->OAR2 &= ~I2C_OAR2_OA2EN;          // Disable own address 2

    I2C1->CR2 &= ~I2C_CR2_ADD10;            //0 = 7-bit mode; 1 = 10-bit
    I2C1->CR2 |= I2C_CR2_AUTOEND;           //Enable the auto end

    I2C1->CR1 |= I2C_CR1_PE;                //Enable I2C1
}

void i2c_start(uint32_t devaddr, uint8_t size, uint8_t dir) {
    //
    // dir: 0 = master requests a write transfer
    // dir: 1 = master requests a read transfer
    uint32_t tmpreg = I2C1->CR2;
    tmpreg &= ~(I2C_CR2_SADD | I2C_CR2_NBYTES |
    I2C_CR2_RELOAD | I2C_CR2_AUTOEND |
    I2C_CR2_RD_WRN | I2C_CR2_START | I2C_CR2_STOP);
    if (dir == 1)
        tmpreg |= I2C_CR2_RD_WRN; // Read from slave
    else
        tmpreg &= ~I2C_CR2_RD_WRN; // Write to slave
    tmpreg |= ((devaddr << 1) & I2C_CR2_SADD) | ((size << 16) & I2C_CR2_NBYTES);
    tmpreg |= I2C_CR2_START;
    I2C1->CR2 = tmpreg;
}

void i2c_stop(void) {
    if (I2C1->ISR & I2C_ISR_STOPF)
        return;
    // Master: Generate STOP bit after current byte has been transferred.
    I2C1->CR2 |= I2C_CR2_STOP;
    // Wait until STOPF flag is reset
    while ((I2C1->ISR & I2C_ISR_STOPF) == 0);
    I2C1->ICR |= I2C_ICR_STOPCF; // Write to clear STOPF flag
}

void i2c_waitidle(void) {
    while ((I2C1->ISR & I2C_ISR_BUSY) == I2C_ISR_BUSY); // while busy, wait.
}

int8_t i2c_senddata(uint8_t devaddr, void *pdata, uint8_t size) {
    int i;
    if (size <= 0 || pdata == 0)
        return -1;
    uint8_t *udata = (uint8_t*) pdata;
    i2c_waitidle();
    // Last argument is dir: 0 = sending data to the slave.
    i2c_start(devaddr, size, 0);
    for (i = 0; i < size; i++) {
        // TXIS bit is set by hardware when the TXDR register is empty and the
        // data to be transmitted must be written in the TXDR register. It is
        // cleared when the next data to be sent is written in the TXDR reg.
        // The TXIS flag is not set when a NACK is received.
        int count = 0;
        while ((I2C1->ISR & I2C_ISR_TXIS) == 0) {
            count += 1;
            if (count > 1000000)
                return -1;
            if ((I2C1->ISR & I2C_ISR_NACKF)) {
                (I2C1->ISR &= ~I2C_ISR_NACKF);
                i2c_stop();
                return -1;
            }

        }
        // TXIS is cleared by writing to the TXDR register.
        I2C1->TXDR = udata[i] & I2C_TXDR_TXDATA;

    }
    // Wait until TC flag is set or the NACK flag is set.
    while ((I2C1->ISR & I2C_ISR_TC) == 0 && (I2C1->ISR & I2C_ISR_NACKF) == 0);
    if ((I2C1->ISR & I2C_ISR_NACKF) != 0)
        return -1;
    i2c_stop();
    return 0;
}

int8_t i2c_receivedata(uint8_t SlaveAddress, uint8_t *pData, uint8_t Size){
    int i;

    if (Size <= 0 || pData == NULL) return -1;

    i2c_waitidle();

    i2c_start(SlaveAddress, Size, 1);

    for (i = 0; i < Size; ++i){
        while((I2C1->ISR & I2C_ISR_RXNE) == 0);
        pData[i] = I2C1->RXDR & I2C_RXDR_RXDATA;
    }

    while((I2C1->ISR & I2C_ISR_TC) == 0);

    i2c_stop();

    return 0;
}

void i2c_set_iodir(uint8_t x) {
    uint8_t devaddr = 0x27;             //Address for MCP23008 when A2,A1, and A0 are high
    uint8_t size = 2;
    uint8_t pdata[size];

    pdata[0] = 0;                       //IODIR Address
    pdata[1] = x;                       //Pins to set IODIR, 1:input, 0:output

    i2c_senddata(devaddr, &pdata, size);
}

void i2c_set_gpio(uint8_t x) {
    uint8_t devaddr = 0x27;             //Address for MCP23008 when A2,A1, and A0 are high
    uint8_t size = 2;
    uint8_t pdata[size];

    pdata[0] = 0x0A;                    //OLAT Address, Supposed to be used to modify the GPIO
    pdata[1] = x;                       //Pins to set OLAT, 1:Pin High, 0:Pin Low

   i2c_senddata(devaddr, &pdata, size);
}

uint8_t i2c_get_gpio(void) {
    uint8_t devaddr = 0x27;             //Address for MCP23008 when A2,A1, and A0 are high
    uint8_t size = 1;
    uint8_t pdata[size];
    int8_t rtVal = 0;
    uint8_t rdata = 0;

    pdata[0] = 0x09;                    //GPIO Address, Supposed to be used to read the GPIO

    rtVal = i2c_senddata(devaddr, &pdata, size);
    if (rtVal == -1){
        asm("wfi");
    }
    else{
        i2c_receivedata(devaddr, &rdata, size);
    }

    return rdata;
}

//Keypad and Seven-Segment Functions
//=============================================================================
// enable_keypad()
// Configure GPIO Port B and calls setup TIM7 to generate interrupts.
// Parameters: none
//============================================================================
void enable_keypad_display()
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;  //Enables port B
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;  //Enables port C

    //GPIOB Config
    GPIOB->MODER &= ~(0xFFFF);          //Clears pins 0 - 7
    GPIOB->MODER |= 0x55;               //Sets pins 0 - 3 to outputs

    GPIOB->PUPDR &= ~(0xFFFF);
    GPIOB->PUPDR |= 0xAA00;             //Sets pins 4 - 7 to pulldown

    //GPIOC Config
    GPIOC->MODER &= ~(0x3FFFFF);        //Clears pins 0 - 10
    GPIOC->MODER |= 0x155555;           //Sets pins 0 - 10 to outputs

    //TIM7 is used to generate an interrupt to check if a key is pressed.
    setup_tim7();
}
//============================================================================
// show_digit()    (Autotest #4)
// Output a single digit on the seven-segment LED array.
// Parameters: none
//============================================================================
void show_digit()
{
    if (offset > 7)
    {
        return;
    }

    int off = offset & 7;
    GPIOC->ODR = (off << 8) | display[off];
}
//============================================================================
// set_row()    (Autotest #5)
// Set the row active on the keypad matrix.
// Parameters: none
//============================================================================
void set_row()
{
    GPIOB->BSRR = 0xf0000 | (1 << (offset & 3));
}
//============================================================================
// get_cols()    (Autotest #6)
// Read the column pins of the keypad matrix.
// Parameters: none
// Return value: The 4-bit value read from PB[7:4].
//============================================================================
int get_cols()
{
    int value = 0;

    value = (GPIOB->IDR) >> 4;
    value = value & 0xf;

    return(value);
}
//============================================================================
// insert_queue()    (Autotest #7)
// Insert the key index number into the two-entry queue.
// Parameters: n: the key index number
//============================================================================
void insert_queue(int n)
{
    n |= 0x80;
    queue[qin] = n;
    qin = qin ^ 1;
}
//============================================================================
// update_hist()    (Autotest #8)
// Check the columns for a row of the keypad and update history values.
// If a history entry is updated to 0x01, insert it into the queue.
// Parameters: none
//============================================================================
void update_hist(int cols)
{
    int row = offset & 3;
    char *hrow = &history[4*row];

    for(int i=0; i < 4; i++) {
        hrow[i] = hrow[i] << 1;

        if (((cols >> i) & 1) != 0) {
            hrow[i] += 1;
        }

        //Added or condition to check if the button is held down
        if ((hrow[i] == 0x1) || (hrow[i] == 0xFF))
        {
            insert_queue((4 * row) + i);
        }
    }
}
//============================================================================
// Timer 7 ISR()    (Autotest #9)
// The Timer 7 ISR
// Parameters: none
// (Write the entire subroutine below.)
//============================================================================
void TIM7_IRQHandler(void)
{
    TIM7->SR = 0;

    show_digit();
    int value = get_cols();
    update_hist(value);

    offset = (offset + 1) & 7;

    set_row();
}
//============================================================================
// setup_tim7()    (Autotest #10)
// Configure timer 7.
// Parameters: none
//============================================================================
void setup_tim7()
{
    RCC->APB1ENR |= (1<<5);         //Enables clock to TIM7

    TIM7->PSC = 4800 - 1;
    TIM7->ARR = 10 - 1;             //Sets interrupt to occur at 1kHz

    TIM7->DIER |= TIM_DIER_UIE;     //Enables the interrupt in TIM7
    TIM7->CR1 |= TIM_CR1_CEN;       //Enables the timer

    NVIC->ISER[0] |= 1 << TIM7_IRQn;//Enables the interrupt
    NVIC_SetPriority(TIM7_IRQn,0);

}
//============================================================================
// getkey()    (Autotest #11)
// Wait for an entry in the queue.  Translate it to ASCII.  Return it.
// Parameters: none
// Return value: The ASCII value of the button pressed.
//============================================================================
int getkey()
{
    do {
        asm volatile("wfi");
    } while(queue[qout] == 0);

    int tmp = queue[qout];
    queue[qout] = 0;
    qout = qout ^ 1;
    tmp = tmp & 0x7f;

    char rVal;
    switch(tmp) {

        case 0:
            rVal = '1';
            break;
        case 1:
            rVal = '2';
            break;
        case 2:
            rVal = '3';
            break;
        case 3:
            rVal = 'A';
            break;
        case 4:
            rVal = '4';
            break;
        case 5:
            rVal = '5';
            break;
        case 6:
            rVal = '6';
            break;
        case 7:
            rVal = 'B';
            break;
        case 8:
            rVal = '7';
            break;
        case 9:
            rVal = '8';
            break;
        case 10:
            rVal = '9';
            break;
        case 11:
            rVal = 'C';
            break;
        case 12:
            rVal = '*';
            break;
        case 13:
            rVal = '0';
            break;
        case 14:
            rVal = '#';
            break;
        case 15:
            rVal = 'D';
            break;
    }
    return (rVal);
}

//Same functionality as getkey(), but it doesn't wait for an input
int checkkey()
{
    //Checks to see if a key was pressed
    if(queue[qout] == 0)
    {
        return (0);
    }

    int tmp = queue[qout];
    queue[qout] = 0;
    qout = qout ^ 1;
    tmp = tmp & 0x7f;

    char rVal;
    switch(tmp) {

        case 0:
            rVal = '1';
            break;
        case 1:
            rVal = '2';
            break;
        case 2:
            rVal = '3';
            break;
        case 3:
            rVal = 'A';
            break;
        case 4:
            rVal = '4';
            break;
        case 5:
            rVal = '5';
            break;
        case 6:
            rVal = '6';
            break;
        case 7:
            rVal = 'B';
            break;
        case 8:
            rVal = '7';
            break;
        case 9:
            rVal = '8';
            break;
        case 10:
            rVal = '9';
            break;
        case 11:
            rVal = 'C';
            break;
        case 12:
            rVal = '*';
            break;
        case 13:
            rVal = '0';
            break;
        case 14:
            rVal = '#';
            break;
        case 15:
            rVal = 'D';
            break;
    }
    return (rVal);
}

void shift(char c)
{
    memcpy(display, &display[1], 7);
    display[7] = font[c];
}
