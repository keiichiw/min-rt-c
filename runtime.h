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

float fabs  (float);
float fneg  (float);
float sqrt  (float);
float floor (float);

int   int_of_float (float);
float float_of_int (int);

float cos (float);
float sin (float);
float atan(float);

float read_float();
