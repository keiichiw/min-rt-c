#ifndef RUNTIME_H
#define RUNTIME_H
int fispos(float);
int fisneg(float);
int fiszero(float);
float fhalf(float);
float fsqr (float);

float mysqrt(float);
float myfloor(float);
float mycos(float);
float mysin(float);
float mytan(float);
float myatan(float);
#define fabs(x) ((x)<0.0?-(x):(x))
#define fneg(x) (-(x))

#define sqrt(x)  (mysqrt(x))
#define floor(x) (myfloor(x))


int   int_of_float (float);
float float_of_int (int);

#define cos(x)   (mycos(x))
#define sin(x)   (mysin(x))
#define tan(x)   (mytan(x))
#define atan(x)  (myatan(x))


float read_float();
int   read_int();

void print_char(char);
void print_int(int);

#endif
