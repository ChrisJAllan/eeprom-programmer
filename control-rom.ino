/**
 * Programs an EEPROM for instruction decoding.
 * 
 * The EEPROM's input pins are:
 *     0-5: Ring counter signal (T0, T1, ... inverted)
 *     6-10: Instruction
 * 
 * This is for either of two EEPROMs used to output the full control word.
 */

#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13

/*
 * Output the address bits and outputEnable signal using shift registers.
 */
void setAddress(int address, bool outputEnable) {
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}


/*
 * Read a byte from the EEPROM at the specified address.
 */
byte readEEPROM(int address) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    pinMode(pin, INPUT);
  }
  setAddress(address, /*outputEnable*/ true);

  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin -= 1) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}


/*
 * Write a byte to the EEPROM at the specified address.
 */
void writeEEPROM(int address, byte data) {
  setAddress(address, /*outputEnable*/ false);
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    pinMode(pin, OUTPUT);
  }

  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, HIGH);
  delay(10);
}


/*
 * Read the contents of the EEPROM and print them to the serial monitor.
 */
void printContents() {
  for (int base = 0; base <= 255; base += 16) {
    byte data[16];
    for (int offset = 0; offset <= 15; offset += 1) {
      data[offset] = readEEPROM(base + offset);
    }

    char buf[80];
    sprintf(buf, "%03x:  %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);
  }
}

void writeInstruction(byte instruction, int t_start, const byte ops[3])
{
  for (int t = 0; t < 3; ++t) {
    int address = (0x80 >> (t_start + t));
    address     = ~address; // Invert T
    address     &= 0x3F0;   // Mask T
    instruction &= 0x00F;   // Mask instruction

    address |= instruction; // Combine to get address
    
    writeEEPROM(address, ops[t]);
  }
}


// Program ROM 1 or 2
#define ROM_SELECT 1

const bool ROM_1_SELECT = (ROM_SELECT == 1);
const bool ROM_2_SELECT = (ROM_SELECT == 2);

// Signals
// J-AO on rom 1
const byte J  = 1   * ROM_1_SELECT;
const byte CO = 2   * ROM_1_SELECT;
const byte CE = 4   * ROM_1_SELECT;
const byte OI = 8   * ROM_1_SELECT;
const byte BI = 16  * ROM_1_SELECT;
const byte SU = 32  * ROM_1_SELECT;
const byte EO = 64  * ROM_1_SELECT;
const byte AO = 128 * ROM_1_SELECT;

// AI-HLT on rom 2
const byte AI  = 1  * ROM_2_SELECT;
const byte II  = 2  * ROM_2_SELECT;
const byte IO  = 4  * ROM_2_SELECT;
const byte RO  = 8  * ROM_2_SELECT;
const byte RI  = 16 * ROM_2_SELECT;
const byte MI  = 32 * ROM_2_SELECT;
const byte HLT = 64 * ROM_2_SELECT;

const byte NOOP = 0;

// Operations
const byte FETCH[] = {CO | MI, RO | II, CE};
const byte LDA[]   = {IO | MI, RO | AI, NOOP};
const byte ADD[]   = {IO | MI, RO | BI, EO | AI};
const byte SUB[]   = {IO | MI, RO | BI, EO | AI | SU};
const byte OUT[]   = {AO | OI, NOOP, NOOP};
const byte HALT[]  = {HLT, NOOP, NOOP};

void setup() {
  // put your setup code here, to run once:
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);
  Serial.begin(57600);

  // Erase entire EEPROM
  Serial.print("Erasing EEPROM");
  for (int address = 0; address <= 2047; address += 1) {
    writeEEPROM(address, 0xff);

    if (address % 64 == 0) {
      Serial.print(".");
    }
  }
  Serial.println(" done");

  // Program fetch cycle
  Serial.print("Programming fetch cycle");
  for (byte i = 0; i < 0x10; ++i) {
    writeInstruction(i, 0, FETCH);
  }

  Serial.print("Programming LDA");
  writeInstruction(0x0, 3, LDA);

  Serial.print("Programming ADD");
  writeInstruction(0x1, 3, ADD);

  Serial.print("Programming SUB");
  writeInstruction(0x2, 3, SUB);

  Serial.print("Programming OUT");
  writeInstruction(0xE, 3, OUT);

  Serial.print("Programming HALT");
  writeInstruction(0xF, 3, HALT);

  // Read and print out the contents of the EERPROM
  Serial.println("Reading EEPROM");
  printContents();
}


void loop() {
  // put your main code here, to run repeatedly:

}
