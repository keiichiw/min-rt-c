#include <stdio.h>
#include <math.h>

int fispos(float x) {
  return x > 0.0;
}

int fisneg(float x) {
  return x < 0.0;
}

int fiszero(float x) {
  return x == 0.0;
}

float fhalf(float x) {
  return x * 0.5;
}

float fsqr(float x) {
  return x * x;
}

float mysqrt(float f) {
  return sqrt(f);
}

float myfloor(float f) {
  return floor(f);
}

float mycos(float f) {
  return cos(f);
}

float mysin(float f) {
  return sin(f);
}

float mytan(float f) {
  return tan(f);
}

float myatan(float f) {
  return atan(f);
}

int int_of_float(float f) {
  return (int)f;
}

float float_of_int(int i) {
  return (float) i;
}

int read_int() {
  unsigned n = 0;
  n += getchar();
  n += getchar() << 8;
  n += getchar() << 16;
  n += getchar() << 24;
  return n;
}

float read_float() {
  union {unsigned i; float f;} u;
  u.i = read_int();
  return u.f;
}

void print_char(char c) {
  printf("%c", c);
}

void print_int(int i) {
  printf("%d", i);
}
