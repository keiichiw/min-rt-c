#ifndef RUNTIME_H
#define RUNTIME_H
int fispos(float);
int fisneg(float);
int fiszero(float);
float fhalf(float);
float fsqr (float);

#define fabs(x) ((x)<0.0?-(x):(x))
#define fneg(x) (-(x))

float sqrt  (float);
float floor (float);

int   int_of_float (float);
float float_of_int (int);

float cos (float);
float sin (float);
float atan(float);

float read_float();
int   read_int();

#endif
