#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define BOOL int
#define TRUE (1)
#define FALSE (0)

/* PPM header information */
typedef struct {
  int version; /* must be 3 or 6 */
  int width;
  int height;
  int max;     /* defined but not used */
} ppm_info;

/* for SLD tata stream */
typedef union {
  int i;
  float f;
} fi_union;


/* the binary image of the input SLD data */
#define MAX_N_WORDS 4096
static fi_union sld_words[MAX_N_WORDS];
static unsigned sld_n_words = 0;


static int error_flag = 0;
void error(char *fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  error_flag = 1;
}


/*****************************************************************************
 * SLD Reader : convert the input SLD text file into a binary format
 ****************************************************************************/

/*-----------------------------------------------------------------------------
 * read a float in the SLD file and append it to the array sld_words.
 * fp : input SLD file stream
 * RETURN value : the float read from the file
 */
static float read_float(FILE* fp)
{
  float f;

  if(sld_n_words >= MAX_N_WORDS){
    error("read_float : too many sld words.\n");
    exit(1);
  }

  if(fscanf(fp, "%f", &f) != 1){
    error("failed to read a float\n");
    exit(1);
  }

  return (sld_words[sld_n_words++].f = f);
}

/*-----------------------------------------------------------------------------
 * read an integer in the SLD file and append it to the array sld_words.
 * fp : input SLD file stream
 * RETURN value : the integer read from the file
 */
static int read_int(FILE* fp)
{
  int i;
  if(sld_n_words >= MAX_N_WORDS){
    error("read_int : too many sld words.\n");
    exit(1);
  }

  if(fscanf(fp, "%d", &i) != 1){
    error("failed to read an int\n");
    exit(1);
  }

  return (sld_words[sld_n_words++].i = i);
}

/*-----------------------------------------------------------------------------
 * read a 3D float vector and append it to the array sld_words.
 * fp : input SLD file stream
 */
static void read_vec3(FILE* fp)
{
  read_float(fp);
  read_float(fp);
  read_float(fp);
}

/*-----------------------------------------------------------------------------
 * read the scene environments
 * fp : input SLD file stream
 */
static void read_sld_env(FILE* fp)
{
  /* screen pos */
  read_vec3(fp);
  /* screen rotation */
  read_float(fp);  read_float(fp);
  /* n_lights : Actually, it should be an int value ! */
  read_float(fp);
  /* light rotation */
  read_float(fp);  read_float(fp);
  /* beam  */
  read_float(fp);
}

/*-----------------------------------------------------------------------------
 * read all the objects
 * fp : input SLD file stream
 */
static void read_objects(FILE* fp)
{

  while (read_int(fp) != -1) {  /* texture : -1 -> end */
    /* form */
    read_int(fp);
    /* refltype */
    read_int(fp);
    /* isrot_p*/
    int is_rot = read_int(fp);
    /* abc */
    read_vec3(fp);
    /* xyz */
    read_vec3(fp);
    /* is_invert */
    read_float(fp);
    /* refl_param */
    read_float(fp); read_float(fp);
    /* color */
    read_vec3(fp);
    /* rot */
    if(is_rot){
      read_vec3(fp);
    }
  }
}

/*-----------------------------------------------------------------------------
 * read the AND-network
 * fp : input SLD file stream
 */
static void read_and_network(FILE* fp)
{
  while(read_int(fp) != -1){
    while(read_int(fp) != -1);
  }
}

/*-----------------------------------------------------------------------------
 * read the OR-network
 * fp : input SLD file stream
 */
static void read_or_network(FILE* fp)
{
  while(read_int(fp) != -1){
    while(read_int(fp) != -1);
  }
}

/*-----------------------------------------------------------------------------
 * read the SLD file
 * fp : input SLD file stream
 */
static void read_sld(FILE* fp, BOOL conv_to_big_endian)
{
  read_sld_env(fp);
  read_objects(fp);
  read_and_network(fp);
  read_or_network(fp);
  if(conv_to_big_endian){
    unsigned u;
    for(u = 0; u < sld_n_words; u++){
      int i = sld_words[u].i;
      sld_words[u].i =
        ((i & 0xff) << 24) | ((i & 0xff00) << 8) |
          ((i >> 8) & 0xff00) | ((i >> 24) & 0xff);
    }
  }
}

/*****************************************************************************
 * Each step of the server
 *****************************************************************************/

/*-----------------------------------------------------------------------------
 * read the SLD file and convert into a binary format.
 * sld_file_name : name of the SLD data file
 */
static void load_sld_file(const char* sld_file_name, BOOL conv_to_big_endian)
{
  FILE* fp = sld_file_name ? fopen(sld_file_name, "rb") : stdin;
  if(fp == NULL) {
    error("cannot open SLD file %s\n", sld_file_name);
    exit(1);
  }
  read_sld(fp, conv_to_big_endian);

  fclose(fp);
}

/******************************************************************************
 *  main part
 ******************************************************************************/

int main()
{
  /* load SLD data */
  load_sld_file(NULL, FALSE);

  unsigned i;
  for (i = 0; i < sld_n_words * 4; ++i)
    putchar(((char*)sld_words)[i]);

  return 0;
}
