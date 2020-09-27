#include "Adafruit_IL0373.h"
#include "Adafruit_EPD.h"

#define BUSY_WAIT 500

const uint8_t il0373_default_init_code[] {
  IL0373_POWER_SETTING, 5, 0x03, 0x00, 0x2b, 0x2b, 0x09,
    IL0373_BOOSTER_SOFT_START, 3, 0x17, 0x17, 0x17,
    IL0373_POWER_ON, 0,
    0xFF, 200,
    IL0373_PANEL_SETTING, 1, 0xCF,
    IL0373_CDI, 1, 0x37,
    IL0373_PLL, 1, 0x29,    
    IL0373_VCM_DC_SETTING, 1, 0x0A,
    0xFF, 20,
    0xFE};

const unsigned char LUTDefault_full[31]{
    0x32, // command
    0x02, // C221 25C Full update waveform
    0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69, 0x69,
    0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00, 0xF8,
    0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

const unsigned char LUTDefault_part[31] = {
    0x32, // command
    0x10, // C221 25C partial update waveform
    0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13,
    0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**************************************************************************/
/*!
    @brief constructor if using external SRAM chip and software SPI
    @param width the width of the display in pixels
    @param height the height of the display in pixels
    @param SID the SID pin to use
    @param SCLK the SCLK pin to use
    @param DC the data/command pin to use
    @param RST the reset pin to use
    @param CS the chip select pin to use
    @param SRCS the SRAM chip select pin to use
    @param MISO the MISO pin to use
    @param BUSY the busy pin to use
*/
/**************************************************************************/
Adafruit_IL0373::Adafruit_IL0373(int width, int height, int8_t SID, int8_t SCLK,
                                 int8_t DC, int8_t RST, int8_t CS, int8_t SRCS,
                                 int8_t MISO, int8_t BUSY)
    : Adafruit_EPD(width, height, SID, SCLK, DC, RST, CS, SRCS, MISO, BUSY) {

  buffer1_size = ((uint32_t)width * (uint32_t)height) / 8;
  buffer2_size = buffer1_size;

  if (SRCS >= 0) {
    use_sram = true;
    buffer1_addr = 0;
    buffer2_addr = buffer1_size;
    buffer1 = buffer2 = NULL;
  } else {
    buffer1 = (uint8_t *)malloc(buffer1_size);
    buffer2 = (uint8_t *)malloc(buffer2_size);
  }
}

// constructor for hardware SPI - we indicate DataCommand, ChipSelect, Reset

/**************************************************************************/
/*!
    @brief constructor if using on-chip RAM and hardware SPI
    @param width the width of the display in pixels
    @param height the height of the display in pixels
    @param DC the data/command pin to use
    @param RST the reset pin to use
    @param CS the chip select pin to use
    @param SRCS the SRAM chip select pin to use
    @param BUSY the busy pin to use
    @param spi the SPI bus to use
*/
/**************************************************************************/
Adafruit_IL0373::Adafruit_IL0373(int width, int height, int8_t DC, int8_t RST,
                                 int8_t CS, int8_t SRCS, int8_t BUSY,
                                 SPIClass *spi)
    : Adafruit_EPD(width, height, DC, RST, CS, SRCS, BUSY, spi) {
  buffer1_size = ((uint32_t)width * (uint32_t)height) / 8;
  buffer2_size = buffer1_size;

  if (SRCS >= 0) {
    use_sram = true;
    buffer1_addr = 0;
    buffer2_addr = buffer1_size;
    buffer1 = buffer2 = NULL;
  } else {
    buffer1 = (uint8_t *)malloc(buffer1_size);
    buffer2 = (uint8_t *)malloc(buffer2_size);
  }
}

/**************************************************************************/
/*!
    @brief wait for busy signal to end
*/
/**************************************************************************/
void Adafruit_IL0373::busy_wait(void) {
  if (_busy_pin >= 0) {
    while (!digitalRead(_busy_pin)) {
      delay(10); // wait for busy high
    }
  } else {
    delay(BUSY_WAIT);
  }
}

/**************************************************************************/
/*!
    @brief begin communication with and set up the display.
    @param reset if true the reset pin will be toggled.
*/
/**************************************************************************/
void Adafruit_IL0373::begin(bool reset) {
  Adafruit_EPD::begin(reset);
  setBlackBuffer(0, true); // black defaults to inverted
  setColorBuffer(1, true); // red defaults to inverted

  powerDown();
}

/**************************************************************************/
/*!
    @brief signal the display to update
*/
/**************************************************************************/
void Adafruit_IL0373::update() {
  EPD_command(IL0373_DISPLAY_REFRESH);

  delay(100);

  busy_wait();
  if (_busy_pin <= -1) {
    delay(15000);
  }
}

/**************************************************************************/
/*!
    @brief start up the display
*/
/**************************************************************************/
void Adafruit_IL0373::powerUp(void) {
  uint8_t buf[5];

  hardwareReset();

  const uint8_t *init_code = il0373_default_init_code;

  if (_epd_init_code != NULL) {
    init_code = _epd_init_code;
  }
  EPD_commandList(init_code);
  
  if (_epd_lut_code) {
    EPD_commandList(_epd_lut_code);
  }

  buf[0] = HEIGHT & 0xFF;
  buf[1] = (WIDTH >> 8) & 0xFF;
  buf[2] = WIDTH & 0xFF;
  EPD_command(IL0373_RESOLUTION, buf, 3);
}

/**************************************************************************/
/*!
    @brief wind down the display
*/
/**************************************************************************/
void Adafruit_IL0373::powerDown() {
  // power off
  uint8_t buf[4];

  buf[0] = 0x17;
  EPD_command(IL0373_CDI, buf, 1);

  buf[0] = 0x00;
  EPD_command(IL0373_VCM_DC_SETTING, buf, 0);

  EPD_command(IL0373_POWER_OFF);
}

/**************************************************************************/
/*!
    @brief Send the specific command to start writing to EPD display RAM
    @param index The index for which buffer to write (0 or 1 or tri-color
   displays) Ignored for monochrome displays.
    @returns The byte that is read from SPI at the same time as sending the
   command
*/
/**************************************************************************/
uint8_t Adafruit_IL0373::writeRAMCommand(uint8_t index) {
  if (index == 0) {
    return EPD_command(EPD_RAM_BW, false);
  }
  if (index == 1) {
    return EPD_command(EPD_RAM_RED, false);
  }
  return 0;
}

/**************************************************************************/
/*!
    @brief Some displays require setting the RAM address pointer
    @param x X address counter value
    @param y Y address counter value
*/
/**************************************************************************/
void Adafruit_IL0373::setRAMAddress(uint16_t x, uint16_t y) {
  // on this chip we do nothing
}
