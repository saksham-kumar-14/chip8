#include<stdio.h>

#define GFX_WIDTH 64
#define GFX_HEIGHT 32
#define KEYPAD_SIZE 16

/* Hardware Emulation*/ 

// To emulate chpi8 we need variable of sizes - 1 byte (8 bits) and 2 bytes (16bits)
typedef unsigned char BYTE;
typedef unsigned short int WORD;

typedef struct ChipContext{
  BYTE memory[0xFFF]; // 4096 bytes of memory
  BYTE registers[16]; // 16 8-bit registers

  WORD stack[16];

  // Special registers
  WORD I; // like an address pointer
  WORD PC; // program counter
  BYTE SP; // stack pointer
  BYTE delay_reg; // delay timer
  BYTE sound_reg; //sound timer


  // Output
  BYTE gfx[GFX_WIDTH * GFX_HEIGHT];
  BYTE keypad[KEYPAD_SIZE];  


} ChipContext;

void initialize(ChipContext *chip){
  chip->PC = 0x200;
  chip->I = 0;
  chip->SP = 0;

  // clearing memory
  for(int i = 0; i < 0XFFF; ++i)
    chip->memory[i] = 0;

  for(int i = 0; i < 16; ++i){
    chip->stack[i] = 0;
    chip->registers[i] = 0;
  }

  for(int i = 0; i < GFX_WIDTH * GFX_HEIGHT; ++i) 
    chip->gfx[i] = 0;
  for(int i = 0; i < KEYPAD_SIZE; ++i)
    chip->keypad[i] = 0;

  chip->delay_reg = 0;
  chip->sound_reg = 0;
}


int loadROM(ChipContext *chip, const char *filname){
  FILE* ROM = fopen(filename, "rb");
  if(!ROM){
    printf("ROM not found! Check filename provided");
    return 0;
  }

  fread(&chip->memory[0x200], 1, 0xFFF - 0x200, ROM);
  fclose(ROM);
  return 1;
}


void emulateLoop(ChipContext *chip){

}

