#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#define GFX_WIDTH 64
#define GFX_HEIGHT 32
#define KEYPAD_SIZE 16

#define WINDOW_SCALE 10
#define WINDOW_WIDTH (GFX_WIDTH * WINDOW_SCALE)
#define WINDOW_HEIGHT (GFX_HEIGHT * WINDOW_SCALE)

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

uint32_t pixels[GFX_WIDTH * GFX_HEIGHT];

typedef unsigned char BYTE;
typedef unsigned short int WORD;

BYTE chip8_fontset[] =
    {
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        0xF0, 0x10, 0x20, 0x40, 0x40,
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        0xF0, 0x80, 0xF0, 0x80, 0x80};

typedef struct ChipContext
{
  BYTE memory[0x1000];
  BYTE registers[16];

  WORD stack[16];

  WORD I;
  WORD PC;
  BYTE SP;

  BYTE delay_reg;
  BYTE sound_reg;

  BYTE gfx[GFX_WIDTH * GFX_HEIGHT];
  BYTE keypad[KEYPAD_SIZE];

  BYTE draw_flag;
  BYTE key_wait_flag;
  BYTE key_press_value;
} ChipContext;

void initialize(ChipContext *chip);
void initSDL();
void closeSDL();
int loadROM(ChipContext *chip, const char *filename);
void emulateLoop(ChipContext *chip);
void drawGraphics(ChipContext *chip);
void handleInput(ChipContext *chip);

void initialize(ChipContext *chip)
{
  chip->PC = 0x200;
  chip->I = 0;
  chip->SP = 0;

  for (int i = 0; i < 0x1000; ++i)
    chip->memory[i] = 0;

  for (int i = 0; i < 16; ++i)
  {
    chip->stack[i] = 0;
    chip->registers[i] = 0;
  }

  for (int i = 0; i < GFX_WIDTH * GFX_HEIGHT; ++i)
    chip->gfx[i] = 0;

  for (int i = 0; i < KEYPAD_SIZE; ++i)
    chip->keypad[i] = 0;

  chip->delay_reg = 0;
  chip->sound_reg = 0;
  chip->draw_flag = 0;
  chip->key_wait_flag = 0;
  chip->key_press_value = 0;

  for (int i = 0; i < 80; ++i)
    chip->memory[i] = chip8_fontset[i];

  srand(time(NULL));
}

void initSDL()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
  {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    exit(1);
  }

  window = SDL_CreateWindow(
      "CHIP-8 Emulator",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!window)
  {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer)
  {
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    exit(1);
  }

  texture = SDL_CreateTexture(renderer,
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              GFX_WIDTH, GFX_HEIGHT);
  if (!texture)
  {
    printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    exit(1);
  }
}

void closeSDL()
{
  if (texture)
    SDL_DestroyTexture(texture);
  if (renderer)
    SDL_DestroyRenderer(renderer);
  if (window)
    SDL_DestroyWindow(window);
  SDL_Quit();
}

int loadROM(ChipContext *chip, const char *filename)
{
  FILE *ROM = fopen(filename, "rb");
  if (!ROM)
  {
    printf("ROM not found! Check filename provided: %s\n", filename);
    return 0;
  }

  fseek(ROM, 0, SEEK_END);
  long rom_size = ftell(ROM);
  rewind(ROM);

  if (rom_size > (0x1000 - 0x200))
  {
    printf("ROM is too large for CHIP-8 memory! Size: %ld bytes\n", rom_size);
    fclose(ROM);
    return 0;
  }

  fread(&chip->memory[0x200], 1, rom_size, ROM);
  fclose(ROM);
  printf("ROM '%s' loaded successfully (%ld bytes).\n", filename, rom_size);
  return 1;
}

void emulateLoop(ChipContext *chip)
{
  if (chip->key_wait_flag)
  {
    return;
  }

  WORD opcode = chip->memory[chip->PC] << 8 | chip->memory[chip->PC + 1];
  chip->PC += 2;

  BYTE X = (opcode & 0x0F00) >> 8;
  BYTE Y = (opcode & 0x00F0) >> 4;
  BYTE N = opcode & 0x000F;
  BYTE NN = opcode & 0x00FF;
  WORD NNN = opcode & 0x0FFF;

  switch (opcode & 0xF000)
  {
  case 0x0000:
    switch (NNN)
    {
    case 0x00E0:
      for (int i = 0; i < GFX_WIDTH * GFX_HEIGHT; ++i)
        chip->gfx[i] = 0;
      chip->draw_flag = 1;
      break;
    case 0x00EE:
      chip->SP--;
      chip->PC = chip->stack[chip->SP];
      break;
    default:
      printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
      break;
    }
    break;

  case 0x1000:
    chip->PC = NNN;
    break;

  case 0x2000:
    chip->stack[chip->SP] = chip->PC;
    chip->SP++;
    chip->PC = NNN;
    break;

  case 0x3000:
    if (chip->registers[X] == NN)
      chip->PC += 2;
    break;

  case 0x4000:
    if (chip->registers[X] != NN)
      chip->PC += 2;
    break;

  case 0x5000:
    if (N == 0)
    {
      if (chip->registers[X] == chip->registers[Y])
        chip->PC += 2;
    }
    else
    {
      printf("Unknown opcode [0x5000]: 0x%X\n", opcode);
    }
    break;

  case 0x6000:
    chip->registers[X] = NN;
    break;

  case 0x7000:
    chip->registers[X] += NN;
    break;

  case 0x8000:
    switch (N)
    {
    case 0x0:
      chip->registers[X] = chip->registers[Y];
      break;
    case 0x1:
      chip->registers[X] |= chip->registers[Y];
      break;
    case 0x2:
      chip->registers[X] &= chip->registers[Y];
      break;
    case 0x3:
      chip->registers[X] ^= chip->registers[Y];
      break;
    case 0x4:
    {
      WORD sum = chip->registers[X] + chip->registers[Y];
      chip->registers[0xF] = (sum > 255);
      chip->registers[X] = (BYTE)sum;
      break;
    }
    case 0x5:
      chip->registers[0xF] = (chip->registers[X] >= chip->registers[Y]);
      chip->registers[X] -= chip->registers[Y];
      break;
    case 0x6:
      chip->registers[0xF] = chip->registers[X] & 0x1;
      chip->registers[X] >>= 1;
      break;
    case 0x7:
      chip->registers[0xF] = (chip->registers[Y] >= chip->registers[X]);
      chip->registers[X] = chip->registers[Y] - chip->registers[X];
      break;
    case 0xE:
      chip->registers[0xF] = (chip->registers[X] & 0x80) >> 7;
      chip->registers[X] <<= 1;
      break;
    default:
      printf("Unknown opcode [0x8000]: 0x%X\n", opcode);
      break;
    }
    break;

  case 0x9000:
    if (N == 0)
    {
      if (chip->registers[X] != chip->registers[Y])
        chip->PC += 2;
    }
    else
    {
      printf("Unknown opcode [0x9000]: 0x%X\n", opcode);
    }
    break;

  case 0xA000:
    chip->I = NNN;
    break;

  case 0xB000:
    chip->PC = NNN + chip->registers[0];
    break;

  case 0xC000:
    chip->registers[X] = (rand() % 256) & NN;
    break;

  case 0xD000:
  {
    BYTE Vx = chip->registers[X];
    BYTE Vy = chip->registers[Y];
    BYTE height = N;
    chip->registers[0xF] = 0;

    for (int yline = 0; yline < height; yline++)
    {
      BYTE pixel_row = chip->memory[chip->I + yline];

      for (int xline = 0; xline < 8; xline++)
      {
        int pixel_x = (Vx + xline) % GFX_WIDTH;
        int pixel_y = (Vy + yline) % GFX_HEIGHT;
        int gfx_index = pixel_y * GFX_WIDTH + pixel_x;

        BYTE sprite_pixel = (pixel_row >> (7 - xline)) & 0x1;

        if (sprite_pixel)
        {
          if (chip->gfx[gfx_index])
            chip->registers[0xF] = 1;

          chip->gfx[gfx_index] ^= 1;
        }
      }
    }
    chip->draw_flag = 1;
    break;
  }

  case 0xE000:
    switch (NN)
    {
    case 0x9E:
      if (chip->keypad[chip->registers[X]] == 1)
        chip->PC += 2;
      break;
    case 0xA1:
      if (chip->keypad[chip->registers[X]] == 0)
        chip->PC += 2;
      break;
    default:
      printf("Unknown opcode [0xE000]: 0x%X\n", opcode);
      break;
    }
    break;

  case 0xF000:
    switch (NN)
    {
    case 0x07:
      chip->registers[X] = chip->delay_reg;
      break;
    case 0x0A:
      chip->key_wait_flag = 1;
      break;
    case 0x15:
      chip->delay_reg = chip->registers[X];
      break;
    case 0x18:
      chip->sound_reg = chip->registers[X];
      break;
    case 0x1E:
      chip->I += chip->registers[X];
      break;
    case 0x29:
      chip->I = chip->registers[X] * 5;
      break;
    case 0x33:
      chip->memory[chip->I] = chip->registers[X] / 100;
      chip->memory[chip->I + 1] = (chip->registers[X] / 10) % 10;
      chip->memory[chip->I + 2] = chip->registers[X] % 10;
      break;
    case 0x55:
      for (int i = 0; i <= X; i++)
      {
        chip->memory[chip->I + i] = chip->registers[i];
      }
      break;
    case 0x65:
      for (int i = 0; i <= X; i++)
      {
        chip->registers[i] = chip->memory[chip->I + i];
      }
      break;
    default:
      printf("Unknown opcode [0xF000]: 0x%X\n", opcode);
      break;
    }
    break;

  default:
    printf("Unknown opcode (major group) 0x%X\n", opcode);
    break;
  }

  if (chip->delay_reg > 0)
    chip->delay_reg--;
  if (chip->sound_reg > 0)
  {
    if (chip->sound_reg == 1)
    {
      printf("BEEP!\n");
    }
    chip->sound_reg--;
  }
}

void drawGraphics(ChipContext *chip)
{
  if (chip->draw_flag)
  {
    for (int i = 0; i < GFX_WIDTH * GFX_HEIGHT; ++i)
    {
      pixels[i] = chip->gfx[i] ? 0xFFFFFFFF : 0xFF000000;
    }

    SDL_UpdateTexture(texture, NULL, pixels, GFX_WIDTH * sizeof(uint32_t));

    SDL_RenderClear(renderer);

    SDL_RenderCopy(renderer, texture, NULL, NULL);

    SDL_RenderPresent(renderer);

    chip->draw_flag = 0;
  }
}

void handleInput(ChipContext *chip)
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT)
    {
      exit(0);
    }

    int pressed = (event.type == SDL_KEYDOWN) ? 1 : 0;

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
    {
      BYTE chip8_key = 0xFF;

      switch (event.key.keysym.sym)
      {
      case SDLK_x:
        chip8_key = 0x0;
        break;
      case SDLK_1:
        chip8_key = 0x1;
        break;
      case SDLK_2:
        chip8_key = 0x2;
        break;
      case SDLK_3:
        chip8_key = 0x3;
        break;
      case SDLK_q:
        chip8_key = 0x4;
        break;
      case SDLK_w:
        chip8_key = 0x5;
        break;
      case SDLK_e:
        chip8_key = 0x6;
        break;
      case SDLK_a:
        chip8_key = 0x7;
        break;
      case SDLK_s:
        chip8_key = 0x8;
        break;
      case SDLK_d:
        chip8_key = 0x9;
        break;
      case SDLK_z:
        chip8_key = 0xA;
        break;
      case SDLK_c:
        chip8_key = 0xB;
        break;
      case SDLK_4:
        chip8_key = 0xC;
        break;
      case SDLK_r:
        chip8_key = 0xD;
        break;
      case SDLK_f:
        chip8_key = 0xE;
        break;
      case SDLK_v:
        chip8_key = 0xF;
        break;
      default:
        break;
      }

      if (chip8_key != 0xFF)
      {
        chip->keypad[chip8_key] = pressed;

        if (chip->key_wait_flag && event.type == SDL_KEYDOWN)
        {
          chip->key_press_value = chip8_key;
          chip->key_wait_flag = 0;
          chip->PC += 2;
        }
      }
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    printf("Usage: %s <ROM file>\n", argv[0]);
    return 1;
  }

  ChipContext ctx;
  initialize(&ctx);
  if (!loadROM(&ctx, argv[1]))
  {
    return 1;
  }

  initSDL();

  Uint64 last_cycle_time = SDL_GetPerformanceCounter();
  Uint64 current_cycle_time;
  double delta_time = 0;
  const double cycles_per_second = 700;
  const double ms_per_cycle = 1000.0 / cycles_per_second;

  Uint64 last_timer_update = SDL_GetTicks();

  while (1)
  {
    current_cycle_time = SDL_GetPerformanceCounter();
    delta_time += (double)((current_cycle_time - last_cycle_time) * 1000 / (double)SDL_GetPerformanceFrequency());
    last_cycle_time = current_cycle_time;

    while (delta_time > ms_per_cycle)
    {
      handleInput(&ctx);
      emulateLoop(&ctx);
      delta_time -= ms_per_cycle;
    }

    if (SDL_GetTicks() - last_timer_update > (1000 / 60))
    {
      if (ctx.delay_reg > 0)
        ctx.delay_reg--;
      if (ctx.sound_reg > 0)
      {
        if (ctx.sound_reg == 1)
        {
          printf("BEEP!\n");
        }
        ctx.sound_reg--;
      }
      last_timer_update = SDL_GetTicks();
    }

    drawGraphics(&ctx);

    SDL_Delay(1);
  }

  closeSDL();
  return 0;
}