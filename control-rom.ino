/**
 * Programs an EEPROM for instruction decoding.
 * 
 * The EEPROM's input pins are:
 *     11:   0
 *     10-6: Instruction
 *     5-0:  Ring counter signal (T0, T1, ... inverted)
 * 
 * This is for either of two EEPROMs used to output the full control word.
 * 
 * Instructions taken from the SAP-1 spec:
 *   0 LDA
 *   1 ADD
 *   2 SUB
 *   E OUT
 *   F HLT
 * 
 * Other instructions TBD:
 *   LDI, STA, INC, DEC, JMP, NOP
 */

// This is defined somewhere
#undef DEC

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
  int i_address = instruction << 6;
  
  for (int t = 0; t < 3; ++t) {
    int address;
    int t_address = (0x20 >> (t + t_start));
    t_address     = ~t_address; // Invert T
    t_address     &= 0x3F;    // Mask T
    
    address = i_address | t_address; // Combine to get address
    
    writeEEPROM(address, ops[t]);
  }
}


// Program ROM 1 or 2
#define ROM_SELECT 1

const bool ROM_1_SELECT = (ROM_SELECT == 1);
const bool ROM_2_SELECT = (ROM_SELECT == 2);

// Signals
// HLT-AO on ROM 1
const byte HLT = 1   * ROM_2_SELECT;
const byte MI  = 2   * ROM_2_SELECT;
const byte RI  = 4   * ROM_2_SELECT;
const byte RO  = 8   * ROM_2_SELECT;
const byte IO  = 16  * ROM_2_SELECT;
const byte II  = 32  * ROM_2_SELECT;
const byte AI  = 64  * ROM_2_SELECT;
const byte AO  = 128 * ROM_1_SELECT;

// EO-J on ROM 2
const byte EO = 1   * ROM_1_SELECT;
const byte SU = 2   * ROM_1_SELECT;
const byte BI = 4   * ROM_1_SELECT;
const byte OI = 8   * ROM_1_SELECT;
const byte CE = 16  * ROM_1_SELECT;
const byte CO = 32  * ROM_1_SELECT;
const byte J  = 64  * ROM_1_SELECT;

const byte NOOP = 0;

// Operations
const byte FETCH[] = {CO | MI,  RO | II,  CE};

const byte LDA[]   = {IO | MI,  RO | AI,       NOOP};         // A = RAM[X]
const byte ADD[]   = {IO | MI,  RO | BI,       EO | AI};      // A = A + RAM[X]
const byte SUB[]   = {IO | MI,  RO | BI,       EO | AI | SU}; // A = A - RAM[X]

const byte LDI[]   = {IO | AI,  NOOP,          NOOP};         // A = X
const byte STA[]   = {IO | MI,  AO | RI,       NOOP};         // RAM[X] = A
const byte INC[]   = {IO | BI,  EO | AI,       NOOP};         // A = A + X
const byte DEC[]   = {IO | BI,  EO | AI | SU,  NOOP};         // A = A - X
const byte JMP[]   = {IO | J,   NOOP,          NOOP};         // GOTO X
const byte NOP[]   = {NOOP,     NOOP,          NOOP};

const byte OUT[]   = {AO | OI,  NOOP,          NOOP};         // OUT = A
const byte HALT[]  = {HLT,      NOOP,          NOOP};

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
