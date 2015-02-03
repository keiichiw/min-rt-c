#include <stdio.h>

int fispos(float x) {
  return x > 0.0;
}

int fisneg (float x) {
  return x < 0.0;
}

int fiszero (float x) {
  return x == 0.0;
}

float fhalf (float x) {
  return x * 0.5;
}

float fsqr (float x) {
  return x * x;
}

float fabs  (float x) {
  return x<0 ? -x : x;
}

float fneg  (float x) {
  return -x;
}
