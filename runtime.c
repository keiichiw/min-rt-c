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
  return (int) f;
}
float float_of_int(int i) {
  return (float) i;
}

float read_float() {
  float f;
  scanf("%f", &f);
  return f;
}

int read_int() {
  int i;
  scanf("%d", &i);
  return i;
}

void print_char(char c) {
  printf("%c\n", c);
}

void print_int(int i) {
  printf("%d\n", i);
}
