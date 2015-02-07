
/****************************************************************/
/*                                                              */
/* Ray Tracing Program for C                                    */
/*                                                              */
/* Original Program by Ryoji Kawamichi                          */
/* Arranged for Chez Scheme by Motohico Nanano                  */
/* Arranged for Objective Caml by Y. Oiwa and E. Sumii          */
/* Added diffuse ray tracer by Y.Ssugawara                      */
/* Arranged for C by K.Watanabe and H. Kobayashi                */
/*                                                              */
/****************************************************************/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

#define SIZE 300 // FIXME

typedef struct {
  float x, y, z;
} vec_t;

typedef struct {
  float x, y, z, w;
} vec4_t;

typedef struct {
  int    tex, shape, surface;
  bool   isrot;
  vec_t  abc, xyz;
  bool   invert;
  vec_t  surfparams, color, rot123;
  vec4_t ctbl;
} obj_t;

typedef struct {
  int  size;
  int* head;
} iarr_t;

typedef struct {
  int    size;
  vec_t* head;
} varr_t;

typedef struct {
  vec_t   rgb;
  varr_t  isect_ps;
  iarr_t  sids;
  iarr_t  cdif;
  varr_t  engy;
  varr_t  r20p;
  int    gid;
  varr_t  nvectors;
} pixel_t;

typedef struct {
  int size;
  pixel_t* head;
} parr_t;

typedef struct {
  vec_t   vec;
  float  *cnst[60];
} dvec_t;

typedef struct {
  int   sid;
  dvec_t  dv;
  float br;
} refl_t;

void copy_obj(obj_t *dst, obj_t *src) {
  memcpy(dst, src, sizeof(obj_t));
}

void copy_vec(vec_t *dst, vec_t *src) {
  memcpy(dst, src, sizeof(vec_t));
}
void copy_vec4(vec4_t *dst, vec4_t *src) {
  memcpy(dst, src, sizeof(vec4_t));
}

void copy_pixel(pixel_t *dst, pixel_t *src) {
  memcpy(dst, src, sizeof(pixel_t));
}

// iarr_t
void iarr_set_nth(iarr_t* arr, int idx, int value) {
  if (arr->size <= idx) {
    arr->head = realloc(arr->head, sizeof(int) * (idx+1));
    arr->size = idx + 1;
  }
  arr->head[idx] = value;
}
int iarr_get_nth(iarr_t* arr, int idx) {
  if (arr->size <= idx) {
    arr->head = realloc(arr->head, sizeof(int) * (idx+1));
    arr->size = idx + 1;
    arr->head[idx] = -1;
  }
  return arr->head[idx];
}


// varr_t
void varr_set_nth(varr_t* arr, int idx, vec_t *value) {
  if (arr->size <= idx) {
    arr->head = realloc(arr->head, sizeof(vec_t) * (idx+1));
    arr->size = idx + 1;
  }
  copy_vec(&arr->head[idx], value);
}

vec_t* varr_get_nth(varr_t* arr, int idx) {
  void vecbzero(vec_t*);
  if (arr->size <= idx) {
    arr->head = realloc(arr->head, sizeof(vec_t) * (idx+1));
    arr->size = idx + 1;
    vecbzero(&arr->head[idx]);
  }
  return &arr->head[idx];
}

void varr_init(varr_t *arr, int sz, float v) {
  arr->size = sz;
  arr->head = (vec_t*)malloc(sizeof(vec_t) * sz);
  memset(arr->head, v, sizeof(vec_t) * sz);
}
void iarr_init(iarr_t *arr, int sz, int v) {
  int i;
  arr->size = sz;
  arr->head = (int*)malloc(sizeof(int) * sz);
  for (i = 0; i < sz; ++i) {
    arr->head[i] = v;
  }
}

// parr_t
void parr_set_nth(parr_t* arr, int idx, pixel_t *value) {
  if (arr->size <= idx) {
    arr->head = realloc(arr->head, sizeof(vec_t) * (idx+1));
    arr->size = idx + 1;
  }
  copy_pixel(&arr->head[idx], value);
}

pixel_t* parr_get_nth(parr_t* arr, int idx) {
  if (arr->size <= idx) {
    arr->head = realloc(arr->head, sizeof(vec_t) * (idx+1));
    arr->size = idx + 1;

  }
  return &arr->head[idx];
}



/*
  global variables
*/

vec_t screen;
vec_t screenx_dir, screeny_dir, screenz_dir;
vec_t viewpoint;
vec_t light;
float beam;


obj_t *objects;
int n_objects;

int *and_net[50];
int **or_net; // or_net is an array of length 1 in min-rt.ml

float solver_dist; // an array of length 1 in min-rt.ml

vec_t  startp;
vec_t  startp_fast;
dvec_t light_dirvec;
vec_t intersection_point;

float tmin;
int intersected_object_id;
int intsec_rectside;

vec_t nvector; // TODO

vec_t texture_color;
vec_t rgb;
refl_t reflections[SIZE];

int n_reflections;
vec_t diffuse_ray;

dvec_t dirvecs[5]; // TODO

int image_size[2];
int image_center[2];
float scan_pitch;
dvec_t ptrace_dirvec;


/******************************************************************************
   ユーティリティー
*****************************************************************************/

/* 符号 */
float sgn (float x) {
  if (fiszero(x)) {
    return 0.0;
  } else if (fispos(x)) {
    return 1.0;
  } else {
    return -1.0;
  }
}

/* 条件付き符号反転 */
float fneg_cond (int cond, float x) {
  return cond ? x : fneg(x);
}

/* (x+y) mod 5 */
int add_mod5 (int x, int y) {
  int sum = x + y;
  return sum >= 5 ? sum - 5 : sum;
}

/******************************************************************************
   ベクトル操作のためのプリミティブ
*****************************************************************************/

/* 値代入 */
void vecset (vec_t *v, float x, float y, float z) {
  v->x = x;
  v->y = y;
  v->z = z;
}

/* 同じ値で埋める */
void vecfill (vec_t *v, float elem) {
  v->x = elem;
  v->y = elem;
  v->z = elem;
}

/* 零初期化 */
void vecbzero (vec_t *v) {
  vecfill(v, 0.0);
}

/* コピー */
void veccpy (vec_t *dest, vec_t *src) {
  dest->x = src->x;
  dest->y = src->y;
  dest->z = src->z;
}

/* 距離の自乗 */
float vecdist2 (vec_t *p, vec_t *q) {
  return fsqr (p->x - q->x) + fsqr (p->y - q->y) + fsqr (p->z - q->z);
}

/* 正規化 ゼロ割りチェック無し */
void vecunit (vec_t *v) {
  float il = 1.0 / sqrt(fsqr(v->x) + fsqr(v->y) + fsqr(v->z));
  v->x = v->x * il;
  v->y = v->y * il;
  v->z = v->z * il;
}

/* 符号付正規化 ゼロ割チェック*/
void vecunit_sgn (vec_t *v, int inv) {
  float l  = sqrt (fsqr(v->x) + fsqr(v->y) + fsqr(v->z));
  float il;
  if (fiszero(l)) {
    il = 1.0;
  } else if (inv) {
    il = -1.0 / l;
  } else {
    il = 1.0 / l;
  }
  v->x = v->x * il;
  v->y = v->y * il;
  v->z = v->z * il;
}

/* 内積 */
float veciprod (vec_t *v, vec_t *w) {
  return v->x * w->x + v->y * w->y + v->z * w->z;
}

/* 内積 */
float veciprod_d (dvec_t *v, vec_t *w) {
  return v->vec.x * w->x + v->vec.y * w->y + v->vec.z * w->z;
}

/* 内積 引数形式が異なる版 */
float veciprod2 (vec_t *v, float w0, float w1, float w2) {
  return v->x * w0 + v->y * w1 + v->z * w2;
}

/* 別なベクトルの定数倍を加算 */
void vecaccum (vec_t *dest, float scale, vec_t *v) {
  dest->x += scale * v->x;
  dest->y += scale * v->y;
  dest->z += scale * v->z;
}

/* ベクトルの和 */
void vecadd (vec_t *dest, vec_t *v) {
  dest->x += v->x;
  dest->y += v->y;
  dest->z += v->z;
}

/* ベクトル要素同士の積 */
void vecmul (vec_t *dest, vec_t *v) {
  dest->x *= v->x;
  dest->y *= v->y;
  dest->z *= v->z;
}

/* ベクトルを定数倍 */
void vecscale (vec_t *dest, float scale) {
  dest->x *= scale;
  dest->y *= scale;
  dest->z *= scale;
}

/* 他の２ベクトルの要素同士の積を計算し加算 */
void vecaccumv (vec_t *dest, vec_t *v,  vec_t *w) {
  dest->x += v->x * w->x;
  dest->y += v->y * w->y;
  dest->z += v->z * w->z;
}

/******************************************************************************
   オブジェクトデータ構造へのアクセス関数
*****************************************************************************/

/* テクスチャ種 0:無し 1:市松模様 2:縞模様 3:同心円模様 4:斑点*/
int o_texturetype (obj_t *m) {
  return m->tex;
}

/* 物体の形状 0:直方体 1:平面 2:二次曲面 3:円錐 */
int o_form (obj_t *m) {
  return m->shape;
}



/* 反射特性 0:拡散反射のみ 1:拡散＋非完全鏡面反射 2:拡散＋完全鏡面反射 */
int o_reflectiontype (obj_t *m) {
  return m->surface;
}

/* 曲面の外側が真かどうかのフラグ true:外側が真 false:内側が真 */
bool o_isinvert (obj_t *m) {
  return m->invert;
}

/* 回転の有無 true:回転あり false:回転無し 2次曲面と円錐のみ有効 */
bool o_isrot (obj_t *m) {
  return m->isrot;
}

/* 物体形状の aパラメータ */
float o_param_a (obj_t *m) {
  return m->abc.x;
}

/* 物体形状の bパラメータ */
float o_param_b (obj_t *m) {
  return m->abc.y;
}

/* 物体形状の cパラメータ */
float o_param_c (obj_t *m) {
  return m->abc.z;
}

/* 物体形状の abcパラメータ */
vec_t* o_param_abc (obj_t *m) {
  return &m->abc;
}

/* 物体の中心x座標 */
float o_param_x (obj_t *m) {
  return m->xyz.x;
}

/* 物体の中心y座標 */
float o_param_y (obj_t *m) {
  return m->xyz.y;
}

/* 物体の中心z座標 */
float o_param_z (obj_t *m) {
  return m->xyz.z;
}

/* 物体の拡散反射率 0.0 -- 1.0 */
float o_diffuse (obj_t *m) {
  return m->surfparams.x;
}

/* 物体の不完全鏡面反射率 0.0 -- 1.0 */
float o_hilight (obj_t *m) {
  return m->surfparams.y;
}

/* 物体色の R成分 */
float o_color_red (obj_t *m) {
  return m->color.x;
}

/* 物体色の G成分 */
float o_color_green (obj_t *m) {
  return m->color.y;
}

/* 物体色の B成分 */
float o_color_blue (obj_t *m) {
  return m->color.z;
}

/* 物体の曲面方程式の y*z項の係数 2次曲面と円錐で、回転がある場合のみ */
float o_param_r1 (obj_t *m) {
  return m->rot123.x;
}

/* 物体の曲面方程式の x*z項の係数 2次曲面と円錐で、回転がある場合のみ */
float o_param_r2 (obj_t *m) {
  return m->rot123.y;
}

/* 物体の曲面方程式の x*y項の係数 2次曲面と円錐で、回転がある場合のみ */
float o_param_r3 (obj_t *m) {
  return m->rot123.z;
}

/* 光線の発射点をあらかじめ計算した場合の定数テーブル */
/*
  0 -- 2 番目の要素: 物体の固有座標系に平行移動した光線始点
  3番目の要素:
  直方体→無効
  平面→ abcベクトルとの内積
  二次曲面、円錐→二次方程式の定数項
*/
vec4_t *o_param_ctbl (obj_t *m) {
  return &m->ctbl;
}

/******************************************************************************
   Pixelデータのメンバアクセス関数群
*****************************************************************************/

/* 直接光追跡で得られたピクセルのRGB値 */
vec_t *p_rgb (pixel_t *pixel) {
  return pixel->rgb;
}

/* 飛ばした光が物体と衝突した点の配列 */
varr_t *p_intersection_points (pixel_t *pixel) {
  return &pixel->isect_ps;
}

/* 飛ばした光が衝突した物体面番号の配列 */
/* 物体面番号は オブジェクト番号 * 4 + (solverの返り値) */
iarr_t *p_surface_ids (pixel_t *pixel) {
  return &pixel->sids;
}

/* 間接受光を計算するか否かのフラグ */
iarr_t *p_calc_diffuse (pixel_t *pixel) {
  return &pixel->cdif;
}

/* 衝突点の間接受光エネルギーがピクセル輝度に与える寄与の大きさ */
varr_t *p_energy (pixel_t*pixel) {
  return &pixel->engy;
}

/* 衝突点の間接受光エネルギーを光線本数を1/5に間引きして計算した値 */
varr_t *p_received_ray_20percent (pixel_t *pixel) {
  return &pixel->r20p;
}

/* このピクセルのグループ ID */
/*
  スクリーン座標 (x,y)の点のグループIDを (x+2*y) mod 5 と定める
  結果、下図のような分け方になり、各点は上下左右4点と別なグループになる
  0 1 2 3 4 0 1 2 3 4
  2 3 4 0 1 2 3 4 0 1
  4 0 1 2 3 4 0 1 2 3
  1 2 3 4 0 1 2 3 4 0
*/

int p_group_id (pixel_t *pixel) {
  return pixel->gid;
}

/* グループIDをセットするアクセス関数 */
void p_set_group_id (pixel_t *pixel, int id) {
  pixel->gid = id;
}

/* 各衝突点における法線ベクトル */
varr_t* p_nvectors (pixel_t *pixel) {
  return &(pixel->nvectors);
}

/******************************************************************************
   前処理済み方向ベクトルのメンバアクセス関数
*****************************************************************************/

/* ベクトル */
vec_t* d_vec (dvec_t *d) {
  return &(d->vec);
}

/* 各オブジェクトに対して作った solver 高速化用定数テーブル */
float** d_const (dvec_t *d) {
  return d->cnst;
}

/******************************************************************************
   平面鏡面体の反射情報
*****************************************************************************/

/* 面番号 オブジェクト番号*4 + (solverの返り値) */
int r_surface_id (refl_t *r) {
  return r->sid;
}

/* 光源光の反射方向ベクトル(光と逆向き) */
dvec_t* r_dvec (refl_t *r) {
  return &(r->dv);
}

/* 物体の反射率 */
float r_bright (refl_t *r) {
  return r->br;
}

/******************************************************************************
   データ読み込みの関数群
*****************************************************************************/

/* ラジアン */
float rad (float x) {
  return x * 0.017453293;
}

/**** 環境データの読み込み ****/

void read_screen_settings (void) {
  float v1, cos_v1, sin_v1;
  float v2, cos_v2, sin_v2;
  screen.x = read_float();
  screen.y = read_float();
  screen.z = read_float();

  v1 = rad(read_float());
  v2 = rad(read_float());
  cos_v1 = cos(v1);
  sin_v1 = sin(v1);
  cos_v2 = cos(v2);
  sin_v2 = sin(v2);

  screenz_dir.x = cos_v1 * sin_v2 * 200.0;
  screenz_dir.y = sin_v1 * (-200.0);
  screenz_dir.z = cos_v1 * cos_v2 * 200.0;
  screenx_dir.x = cos_v2;
  screenx_dir.y = 0.0;
  screenx_dir.z = - sin_v2;
  screeny_dir.x = - sin_v1 * sin_v2;
  screeny_dir.y = - cos_v1;
  screeny_dir.z = - sin_v1 * cos_v2;
  viewpoint.x = screen.x - screenz_dir.x;
  viewpoint.y = screen.y - screenz_dir.y;
  viewpoint.z = screen.z - screenz_dir.z;
}


void read_light(void) {
  int nl = read_int();
  float l1 = rad(read_float());
  float sl1 = sin(l1);
  float l2 = rad(read_float());
  float cl1 = cos(l1);
  float sl2 = sin(l2);
  float cl2 = cos(l2);
  light.y = - sl1;
  light.x = cl1 * sl2;
  light.z = cl1 * cl2;
  beam = read_float();
}

void rotate_quadratic_matrix(vec_t *abc, vec_t *rot) {
  /* 回転行列の積 R(z)R(y)R(x) を計算する */
  float cos_x = cos(rot->x);
  float sin_x = sin(rot->x);
  float cos_y = cos(rot->y);
  float sin_y = sin(rot->y);
  float cos_z = cos(rot->z);
  float sin_z = sin(rot->z);

  float m00 = cos_y * cos_z;
  float m01 = sin_x * sin_y * cos_z - cos_x * sin_z;
  float m02 = cos_x * sin_y * cos_z + sin_x * sin_z;

  float m10 = cos_y * sin_z;
  float m11 = sin_x * sin_y * sin_z + cos_x * cos_z;
  float m12 = cos_x * sin_y * sin_z - sin_x * cos_z;

  float m20 = - sin_y;
  float m21 = sin_x * cos_y;
  float m22 = cos_x * cos_y;

  /* a, b, cの元の値をバックアップ */
  float ao = abc->x;
  float bo = abc->y;
  float co = abc->z;

  /* R^t * A * R を計算 */

  /* X^2, Y^2, Z^2成分 */
  abc->x = ao * fsqr(m00) + bo * fsqr(m10) + co * fsqr(m20);
  abc->y = ao * fsqr(m01) + bo * fsqr(m11) + co * fsqr(m21);
  abc->z = ao * fsqr(m02) + bo * fsqr(m12) + co * fsqr(m22);

  /* 回転によって生じた XY, YZ, ZX成分 */
  rot->x = 2.0 * (ao * m01 * m02 + bo * m11 * m12 + co * m21 * m22);
  rot->y = 2.0 * (ao * m00 * m02 + bo * m10 * m12 + co * m20 * m22);
  rot->z = 2.0 * (ao * m00 * m01 + bo * m10 * m11 + co * m20 * m21);
}

/**** オブジェクト1つのデータの読み込み ****/
bool read_nth_object(int n) {

  int texture = read_int();
  if (texture != -1) {
    int form = read_int ();
    int refltype = read_int ();
    int isrot_p = read_int () ;
    vec_t abc;
    vec_t xyz;
    int m_invert;
    vec_t reflparam;
    vec_t color;
    vec_t rotation;

    bool m_invert2;
    vec4_t ctbl;

    abc.x = read_float ();
    abc.y = read_float ();
    abc.z = read_float ();


    xyz.x = read_float ();
    xyz.y = read_float ();
    xyz.z = read_float ();

    m_invert = fisneg (read_float ());

    reflparam.x = read_float (); /* diffuse */
    reflparam.y = read_float (); /* hilight */

    color.x = read_float ();
    color.y = read_float ();
    color.z = read_float (); /* 15 */

    if (isrot_p != 0) {
      rotation.x = rad (read_float ());
      rotation.y = rad (read_float ());
      rotation.z = rad (read_float ());
    }

    /* パラメータの正規化 */

    /* 注: 下記正規化 (form = 2) 参照 */
    m_invert2 = form == 2 ? true : m_invert;

    {

      /* ここからあとは abc と rotation しか操作しない。*/
      objects[n].tex     = texture;
      objects[n].shape   = form;
      objects[n].surface = refltype;
      objects[n].isrot   = isrot_p;

      copy_vec(&objects[n].abc,    &abc);
      copy_vec(&objects[n].xyz,    &xyz);

      objects[n].invert  = m_invert2;

      copy_vec(&objects[n].surfparams, &reflparam); /* reflection paramater */
      copy_vec(&objects[n].color,  &color);
      copy_vec(&objects[n].rot123, &rotation);

      copy_vec4(&objects[n].ctbl,  &ctbl);
    }

    if (form == 3) {
      /* 2次曲面: X,Y,Z サイズから2次形式行列の対角成分へ */

      float a = abc.x;
      float b = abc.y;
      float c = abc.z;
      abc.x = a == 0.0 ? 0.0 : sgn(a) / fsqr(a); /* X^2 成分 */
      abc.y = b == 0.0 ? 0.0 : sgn(b) / fsqr(b); /* Y^2 成分 */
      abc.z = c == 0.0 ? 0.0 : sgn(c) / fsqr(c); /* Z^2 成分 */
    } else if (form == 2) {
      /* 平面: 法線ベクトルを正規化, 極性を負に統一 */
      vecunit_sgn(&abc, !m_invert);
    }

    /* 2次形式行列に回転変換を施す */
    if (isrot_p != 0) {
      rotate_quadratic_matrix(&abc, &rotation);
    }

    return true;
  } else {
    return false; /* データの終了 */
  }
}

/**** 物体データ全体の読み込み ****/
void read_object(int n) {
  if (n < 60) {
    if (read_nth_object(n)) {
      read_object (n + 1);
    }else {
      n_objects = n;
    }
  }
}

void read_all_object() {
  read_object(0);
}

/**** AND, OR ネットワークの読み込み ****/

/* ネットワーク1つを読み込みベクトルにして返す */
int *read_net_item(int length) {
  int item = read_int ();
  if (item == -1) {
    int *ary = (int *) malloc(sizeof(int) * (length + 1));
    int i;
    for (i = 0; i < length + 1; ++i) {
      ary[i] = -1;
    }
    return ary;
  }else {
    int *v = read_net_item (length + 1);
    v[length] = item;
    return v;
  }
}

int **read_or_network(int length) {
  int *net = read_net_item(0);
  if (net[0] == -1) {
    int **ary = (int **) malloc(sizeof(int*) * (length + 1));
    int i;
    for (i = 0; i < length + 1; ++i) {
      ary[i] = net;
    }
    return ary;
  } else {
    int **v = read_or_network (length + 1);
    v[length] = net;
    return v;
  }
}


void read_and_network (int n) {
  int *net = read_net_item(0);
  if (net[0] != -1) {
    and_net[n] = net;
    read_and_network (n + 1);
  }
}

void read_parameter() {
  read_screen_settings();
  read_light();
  read_all_object ();
  read_and_network(0);
  or_net = read_or_network(0);
}

/******************************************************************************
   直線とオブジェクトの交点を求める関数群
*****************************************************************************/

/* solver :
   オブジェクト (の index) と、ベクトル L, P を受けとり、
   直線 Lt + P と、オブジェクトとの交点を求める。
   交点がない場合は 0 を、交点がある場合はそれ以外を返す。
   この返り値は nvector で交点の法線ベクトルを求める際に必要。
   (直方体の場合)

   交点の座標は t の値として solver_dist に格納される。
*/

/* 直方体の指定された面に衝突するかどうか判定する */
/* i0 : 面に垂直な軸のindex X:0, Y:1, Z:2         i2,i3は他の2軸のindex */
bool solver_rect_surface(obj_t *m, dvec_t *dirvec, float b0, float b1, float b2, int i0, int i1, int i2) {
  float *dirvec_arr = (float *) dirvec;
  if (dirvec_arr[i0] == 0.0) {
    return false;
  } else {
    vec_t *abc = o_param_abc(m);
    float *abc_arr = (float *) abc;
    float d = fneg_cond(o_isinvert(m) ^ fisneg(dirvec_arr[i0]), abc_arr[i0]);

    float d2 = (d - b0) / dirvec_arr[i0];
    if ((fabs(d2 * dirvec_arr[i1] + b1)) < abc_arr[i1]) {
      if ((fabs(d2 * dirvec_arr[i2] + b2)) < abc_arr[i2]) {
        solver_dist = d2;
        return true;
      }else {
        return false;
      }
    }
    else {
      return false;
    }
  }
}



/***** 直方体オブジェクトの場合 ****/
int solver_rect (obj_t *m, dvec_t *dirvec, float b0, float b1, float b2) {
  if (solver_rect_surface(m, dirvec, b0, b1, b2, 0, 1, 2)) {
    return 1;   /* YZ 平面 */
  } else if (solver_rect_surface(m, dirvec, b1, b2, b0, 1, 2, 0)) {
    return 2;   /* ZX 平面 */
  } else if (solver_rect_surface(m, dirvec, b2, b0, b1, 2, 0, 1)) {
    return 3;   /* XY 平面 */
  } else {
    return 0;
  }
}


/* 平面オブジェクトの場合 */
int solver_surface(obj_t *m, dvec_t *dirvec, float b0, float b1, float b2) {
  /* 点と平面の符号つき距離 */
  /* 平面は極性が負に統一されている */
  vec_t *abc = o_param_abc(m);
  float d = veciprod_d(dirvec, abc);
  if (d > 0.0) {
    solver_dist = fneg(veciprod2(abc, b0, b1, b2)) / d;
    return 1;
  } else {
    return 0;
  }
}

/* 3変数2次形式 v^t A v を計算 */
/* 回転が無い場合は対角部分のみ計算すれば良い */
float quadratic(obj_t *m, float v0, float v1, float v2) {
  float diag_part =
    fsqr(v0) * o_param_a(m) + fsqr(v1) * o_param_b(m) + fsqr(v2) * o_param_c(m);
  if (o_isrot(m) == 0) {
    return diag_part;
  } else {
    return diag_part
      + v1 * v2 * o_param_r1(m)
      + v2 * v0 * o_param_r2(m)
      + v0 * v1 * o_param_r3(m);
  }
}

/* 3変数双1次形式 v^t A w を計算 */
/* 回転が無い場合は A の対角部分のみ計算すれば良い */
float bilinear(obj_t *m, float v0, float v1, float v2, float w0, float w1, float w2) {
  float diag_part =
    v0 * w0 * o_param_a(m)
    + v1 * w1 * o_param_b(m)
    + v2 * w2 * o_param_c(m);
  if (o_isrot(m) == 0) {
    return diag_part;
  } else {
    diag_part + fhalf
      ((v2 * w1 + v1 * w2) * o_param_r1(m)
       + (v0 * w2 + v2 * w0) * o_param_r2(m)
       + (v0 * w1 + v1 * w0) * o_param_r3(m));
  }
}

/* 2次曲面または円錐の場合 */
/* 2次形式で表現された曲面 x^t A x - (0 か 1) = 0 と 直線 base + dirvec*t の
   交点を求める。曲線の方程式に x = base + dirvec*t を代入してtを求める。
   つまり (base + dirvec*t)^t A (base + dirvec*t) - (0 か 1) = 0、
   展開すると (dirvec^t A dirvec)*t^2 + 2*(dirvec^t A base)*t  +
   (base^t A base) - (0か1) = 0 、よってtに関する2次方程式を解けば良い。*/

int solver_second(obj_t *m, dvec_t *dirvec, float b0, float b1, float b2) {
  /* 解の公式 (-b' ± sqrt(b'^2 - a*c)) / a  を使用(b' = b/2) */
  /* a = dirvec^t A dirvec */
  float aa = quadratic(m, dirvec->vec.x, dirvec->vec.y, dirvec->vec.z);

  if (aa == 0.0) {
    return 0; /* 正確にはこの場合も1次方程式の解があるが、無視しても通常は大丈夫 */
  } else {

    /* b' = b/2 = dirvec^t A base   */
    float bb = bilinear(m, dirvec->vec.x, dirvec->vec.y, dirvec->vec.z, b0, b1, b2);
    /* c = base^t A base  - (0か1)  */
    float cc0 = quadratic(m, b0, b1, b2);
    float cc = o_form(m) == 3 ? cc0 - 1.0 : cc0;
    /* 判別式 */
    float d = bb * bb - aa * cc;

    if (d > 0.0) {
      float sd = sqrt(d);
      float t1 = o_isinvert(m) ? sd : - sd;
      solver_dist = (t1 - bb) /  aa;
      return 1;
    } else {
      return 0;
    }
  }
}

/**** solver のメインルーチン ****/
int solver(int index, dvec_t *dirvec, vec_t *org) {
  obj_t *m = &objects[index];
  /* 直線の始点を物体の基準位置に合わせて平行移動 */
  float b0 =  org->x - o_param_x(m);
  float b1 =  org->y - o_param_y(m);
  float b2 =  org->z - o_param_z(m);
  int m_shape = o_form(m);
  /* 物体の種類に応じた補助関数を呼ぶ */
  int ret;
  if (m_shape == 1) {
    ret = solver_rect(m, dirvec, b0, b1, b2);    /* 直方体 */
  } else if (m_shape == 2) {
    ret = solver_surface(m, dirvec, b0, b1, b2); /* 平面 */
  } else {
    ret = solver_second(m, dirvec, b0, b1, b2);  /* 2次曲面/円錐 */
  }
  return ret;
}

/******************************************************************************
   solverのテーブル使用高速版
*****************************************************************************/
/*
  通常版solver と同様、直線 start + t * dirvec と物体の交点を t の値として返す
  t の値は solver_distに格納

  solver_fast は、直線の方向ベクトル dirvec について作ったテーブルを使用
  内部的に solver_rect_fast, solver_surface_fast, solver_second_fastを呼ぶ

  solver_fast2 は、dirvecと直線の始点 start それぞれに作ったテーブルを使用
  直方体についてはstartのテーブルによる高速化はできないので、solver_fastと
  同じく solver_rect_fastを内部的に呼ぶ。それ以外の物体については
  solver_surface_fast2またはsolver_second_fast2を内部的に呼ぶ

  変数dconstは方向ベクトル、sconstは始点に関するテーブル
*/

/***** solver_rectのdirvecテーブル使用高速版 ******/
bool solver_rect_fast(obj_t *m, vec_t *v, float *dconst, float b0, float b1, float b2) {
  float d0 = (dconst[0] - b0) * dconst[1];
  bool tmp0;
  float d1 = (dconst[2] - b1) * dconst[3];
  bool tmp_zx;
  float d2 = (dconst[4] - b2) * dconst[5];
  bool tmp_xy;

  /* YZ平面との衝突判定 */
  if (fabs (d0 * v->y + b1) < o_param_b(m)) {
    if (fabs (d0 * v->z + b2) < o_param_c(m)) {
      tmp0 = dconst[1] != 0.0;
    }
    else {
      tmp0 = false;
    }
  }
  else tmp0 = false;
  if (tmp0 != false) {
    solver_dist = d0;
    return 1;
  }

  /* ZX平面との衝突判定 */
  if (fabs (d1 * v->x + b0) < o_param_a(m)) {
    if (fabs (d1 * v->z + b2) < o_param_c(m)) {
      tmp_zx = dconst[3] != 0.0;
    } else {
      tmp_zx = false;
    }
  }
  else tmp_zx = false;
  if (tmp_zx != false) {
    solver_dist = d1;
    return 2;
  }

  /* XY平面との衝突判定 */
  if (fabs (d2 * v->x + b0) < o_param_a(m)) {
    if (fabs (d2 * v->y + b1) < o_param_b(m)) {
      tmp_xy = dconst[5] != 0.0;
    }
    else tmp_xy = false;
  }
  else tmp_xy = false;
  if (tmp_xy != false) {
    solver_dist = d2;
    return 3;
  }
  return 0;
}


/**** solver_surfaceのdirvecテーブル使用高速版 ******/
int solver_surface_fast(obj_t *m, float *dconst, float b0, float b1, float b2) {
  if (fisneg(dconst[0])) {
    solver_dist = dconst[1] * b0 + dconst[2] * b1 + dconst[3] * b2;
    return 1;
  } else {
    return 0;
  }
}


/**** solver_second のdirvecテーブル使用高速版 ******/
int solver_second_fast(obj_t *m, float *dconst, float b0, float b1, float b2) {
  float aa = dconst[0];
  if (fiszero(aa)) {
    return 0;
  } else {
    float neg_bb = dconst[1] * b0 + dconst[1] * b1 + dconst[3] * b2;
    float cc0 = quadratic(m, b0, b1, b2);
    float cc = o_form(m) == 3 ? cc0 - 1.0 : cc0;
    float d = fsqr(neg_bb) - aa * cc;
    if (fispos(d)) {
      if (o_isinvert(m)) {
        solver_dist = (neg_bb + sqrt(d)) * dconst[4];
      } else {
        solver_dist = (neg_bb - sqrt(d)) * dconst[4];
      }
    } else {
      return 0;
    }
  }
}

/**** solver のdirvecテーブル使用高速版 *******/
int solver_fast(int index, dvec_t *dirvec, vec_t *org) {
  obj_t *m = &objects[index];
  float b0 = org->x - o_param_x(m);
  float b1 = org->y - o_param_y(m);
  float b2 = org->z - o_param_z(m);
  float **dconsts = d_const(dirvec);
  float  *dconst  = dconsts[index];
  int m_shape = o_form(m);
  int ret;
  if (m_shape == 1) {
    ret = solver_rect_fast(m, d_vec(dirvec), dconst, b0, b1, b2);
  } else if (m_shape == 2) {
    ret = solver_surface_fast(m, dconst, b0, b1, b2);
  } else {
    ret = solver_second_fast(m, dconst, b0, b1, b2);
  }
  return ret;
}


/* solver_surfaceのdirvec+startテーブル使用高速版 */
int solver_surface_fast2(obj_t *m, float *dconst, vec4_t *sconst, float b0, float b1, float b2) {
  if (fisneg(dconst[0])) {
    solver_dist = dconst[0] * sconst->w;
    return 1;
  } else {
    return 0;
  }
}

/* solver_secondのdirvec+startテーブル使用高速版 */
int solver_second_fast2(obj_t *m, float *dconst, vec4_t *sconst, float b0, float b1, float b2) {
  float aa = dconst[0];
  if (fiszero(aa)) {
    return 0;
  } else {
    float neg_bb = dconst[1] * b0 + dconst[2] * b1 + dconst[3] * b2;
    float cc = sconst->w;
    float d = fsqr(neg_bb) - aa * cc;
    if (fispos(d)) {
      if (o_isinvert(m)) {
        solver_dist = (neg_bb + sqrt(d)) * dconst[4];
      } else {
        solver_dist = (neg_bb - sqrt(d)) * dconst[4];
      }
      return 1;
    } else {
      return 0;
    }
  }
}

/* solverの、dirvec+startテーブル使用高速版 */
int solver_fast2(int index, dvec_t *dirvec) {
  obj_t *m = &objects[index];
  vec4_t *sconst = o_param_ctbl(m);
  float b0 = sconst->x;
  float b1 = sconst->y;
  float b2 = sconst->z;
  float **dconsts = d_const(dirvec);
  float  *dconst  = dconsts[index];
  int m_shape = o_form(m);
  if (m_shape == 1) {
    return solver_rect_fast(m, d_vec(dirvec), dconst, b0, b1, b2);
  } else if (m_shape == 2) {
    return solver_surface_fast2(m, dconst, sconst, b0, b1, b2);
  } else {
    return solver_second_fast2(m, dconst, sconst, b0, b1, b2);
  }
}

/******************************************************************************
   方向ベクトルの定数テーブルを計算する関数群
*****************************************************************************/

/* 直方体オブジェクトに対する前処理 */
float* setup_rect_table(vec_t *vec, obj_t *m) {
  float *consts = (float*)malloc(6 * sizeof(float));

  if (fiszero(vec->x)) { /* YZ平面 */
    consts[1] = 0.0;
  } else {
    /* 面の X 座標 */
    consts[0] = fneg_cond(o_isinvert(m)^fisneg(vec->x), o_param_a(m));
    /* 方向ベクトルを何倍すればX方向に1進むか */
    consts[1] = 1.0 / vec->x;
  }
  if (fiszero(vec->y)) { /* ZX平面 : YZ平面と同様*/
    consts[3] = 0.0;
  } else {
    consts[2] = fneg_cond(o_isinvert(m)^fisneg(vec->y), o_param_b(m));
    consts[3] = 1.0 / vec->y;
  }
  if (fiszero(vec->z)) { /* XY平面 : YZ平面と同様*/
    consts[5] = 0.0;
  } else {
    consts[4] = fneg_cond(o_isinvert(m)^fisneg(vec->z), o_param_c(m));
    consts[5] = 1.0 / vec->z;
  }
  return consts;
}

/* 平面オブジェクトに対する前処理 */
float* setup_surface_table(vec_t *vec, obj_t *m) {
  float *consts = (float*)malloc(4 * sizeof(float));
  float d = vec->x * o_param_a(m) + vec->y * o_param_b(m) + vec->z * o_param_c(m);
  if (fispos(d)) {
    /* 方向ベクトルを何倍すれば平面の垂直方向に 1 進むか */
    consts[0] = -1.0 / d;
    /* ある点の平面からの距離が方向ベクトル何個分かを導く3次一形式の係数 */
    consts[1] = fneg(o_param_a(m) / d);
    consts[2] = fneg(o_param_b(m) / d);
    consts[3] = fneg(o_param_c(m) / d);
  } else {
    consts[0] = 0;
    consts[1] = 0;
    consts[2] = 0;
    consts[3] = 0;
  }
  return consts;
}


/* 2次曲面に対する前処理 */
float* setup_second_table(vec_t *v, obj_t *m) {
  float *consts = malloc(5 * sizeof(float));
  float aa = quadratic(m, v->x, v->y, v->z);
  float c1 = fneg(v->x * o_param_a(m));
  float c2 = fneg(v->y * o_param_b(m));
  float c3 = fneg(v->z * o_param_c(m));

  consts[0] = aa;  /* 2次方程式の a 係数 */

  /* b' = dirvec^t A start だが、(dirvec^t A)の部分を計算しconst.(1:3)に格納。
     b' を求めるにはこのベクトルとstartの内積を取れば良い。符号は逆にする */
  if (o_isrot(m) != 0) {
    consts[1] = c1 - fhalf(v->z * o_param_r2(m) + v->y * o_param_r3(m));
    consts[2] = c2 - fhalf(v->z * o_param_r1(m) + v->x * o_param_r3(m));
    consts[3] = c3 - fhalf(v->y * o_param_r1(m) + v->x * o_param_r2(m));
  } else {
    consts[1] = c1;
    consts[2] = c2;
    consts[3] = c3;
  }

  if (!fiszero(aa)) {
    consts[4] = 1.0 / aa; /* a係数の逆数を求め、解の公式での割り算を消去 */
  } else {
    consts[4] = 0.0;
  }

  return consts;
}


// iteration
/* 各オブジェクトについて補助関数を呼んでテーブルを作る */
void iter_setup_dirvec_constants (dvec_t *dirvec, int index) {
  if (index >= 0) {
    obj_t *m = &objects[index];
    float **dconst = d_const(dirvec);
    vec_t *v = d_vec(dirvec);
    int m_shape = o_form(m);

    if (m_shape == 1) { /* rect */
      dconst[index] = setup_rect_table(v, m);
    } else if (m_shape == 2) { /* surface */
      dconst[index] = setup_surface_table(v, m);
    } else { /* second */
      dconst[index] = setup_second_table(v, m);
    }

    iter_setup_dirvec_constants(dirvec, index-1);
  }
}

void setup_dirvec_constants(dvec_t *dirvec) {
  iter_setup_dirvec_constants(dirvec, n_objects - 1);
}

/******************************************************************************
   直線の始点に関するテーブルを各オブジェクトに対して計算する関数群
*****************************************************************************/

void setup_startp_constants(vec_t *p, int index) {
  if (index >= 0) {
    obj_t *obj = &objects[index];
    vec4_t *sconst = o_param_ctbl(obj);
    int m_shape = o_form(obj);

    sconst->x = p->x - o_param_x(obj);
    sconst->y = p->y - o_param_y(obj);
    sconst->z = p->z - o_param_z(obj);

    if (m_shape == 2) { /* surface */
      sconst->w = veciprod2(o_param_abc(obj), sconst->x, sconst->y, sconst->z);
    } else if (m_shape > 2) { /* second */
      float cc0 = quadratic(obj, sconst->x, sconst->y, sconst->z);
      sconst->w = (m_shape == 3 ? cc0 - 1.0 : cc0);
    }

    setup_startp_constants(p, index - 1);
  }
}

void setup_startp(vec_t *p) {
  veccpy(&startp_fast, p);
  setup_startp_constants(p, n_objects - 1);
}

/******************************************************************************
   与えられた点がオブジェクトに含まれるかどうかを判定する関数群
*****************************************************************************/

/**** 点 q がオブジェクト m の外部かどうかを判定する ****/

/* 直方体 */
bool is_rect_outside(obj_t *m, float p0, float p1, float p2) {
  bool b0 = fabs(p0) < o_param_a(m);
  bool b1 = fabs(p1) < o_param_b(m);
  bool b2 = fabs(p2) < o_param_c(m);
  if(b0 && b1 && b2){
    return o_isinvert(m);
  } else {
    return !o_isinvert(m);
  }
}

/* 平面 */
bool is_plane_outside(obj_t *m, float p0, float p1, float p2) {
  float w = veciprod2(o_param_abc(m), p0, p1, p2);
  return !(o_isinvert(m) ^ fisneg(w));
}

/* 2次曲面 */
bool is_second_outside(obj_t *m, float p0, float p1, float p2) {
  float w  = quadratic(m, p0, p1, p2);
  float w2 = (o_form(m) == 3 ? w - 1.0 : w);
  return !(o_isinvert(m) ^ fisneg(w2));
}

/* 物体の中心座標に平行移動した上で、適切な補助関数を呼ぶ */
bool is_outside(obj_t *m, float q0, float q1, float q2) {
  float p0 = q0 - o_param_x(m);
  float p1 = q1 - o_param_y(m);
  float p2 = q2 - o_param_z(m);
  int m_shape = o_form(m);
  if (m_shape == 1) {
    return is_rect_outside(m, p0, p1, p2);
  } else if (m_shape == 2) {
    return is_plane_outside(m, p0, p1, p2);
  } else {
    return is_second_outside(m, p0, p1, p2);
  }
}


// iteration
bool check_all_inside(int ofs, int *iand, float q0, float q1, float q2) {
  int head = iand[ofs];
  if (head == -1) {
    return true;
  } else {
    if (is_outside(&objects[head], q0, q1, q2)) {
      return false;
    } else {
      return check_all_inside(ofs+1, iand, q0, q1, q2);
    }
  }
}


/******************************************************************************
   衝突点が他の物体の影に入っているか否かを判定する関数群
*****************************************************************************/

/* 点 intersection_point から、光線ベクトルの方向に辿り、   */
/* 物体にぶつかる (=影にはいっている) か否かを判定する。*/

/**** AND ネットワーク iand の影内かどうかの判定 ****/
// iteration
bool shadow_check_and_group(int iand_ofs, int *and_group) {
  if (and_group[iand_ofs] == -1) {
    return false;
  } else {
    int obj   = and_group[iand_ofs];
    float t0  = solver_fast(obj, &light_dirvec, &intersection_point);
    float t0p = solver_dist;
    if (t0 != 0 && t0p < -0.2) {
      /* Q: 交点の候補。実際にすべてのオブジェクトに */
      /* 入っているかどうかを調べる。*/
      float t  = t0p + 0.01;
      float q0 = light.x * t + intersection_point.x;
      float q1 = light.y * t + intersection_point.y;
      float q2 = light.z * t + intersection_point.z;
      if (check_all_inside(0, and_group, q0, q1, q2)) {
        return true;
      } else {
        /* 次のオブジェクトから候補点を探す */
        return shadow_check_and_group(iand_ofs + 1, and_group);
      }
    } else {
      /* 交点がない場合: 極性が正(内側が真)の場合、    */
      /* AND ネットの共通部分はその内部に含まれるため、*/
      /* 交点はないことは自明。探索を打ち切る。        */
      if (o_isinvert(&objects[obj])) {
        return shadow_check_and_group((iand_ofs + 1), and_group);
      } else {
        return false;
      }
    }
  }
}

/**** OR グループ or_group の影かどうかの判定 ****/
// iteration
bool shadow_check_one_or_group(int ofs, int *or_group) {
  int head = or_group[ofs];
  if (head == -1) {
    return false;
  } else {
    int *and_group = and_net[head];
    bool shadow_p = shadow_check_and_group(0, and_group);
    if (shadow_p) {
      return true;
    } else {
      return shadow_check_one_or_group(ofs + 1, or_group);
    }
  }
}

/**** OR グループの列のどれかの影に入っているかどうかの判定 ****/
// iteration
bool shadow_check_one_or_matrix(int ofs, int **or_matrix) {
  int *head = or_matrix[ofs];
  int range_primitive = head[0];
  if (range_primitive == -1) { /* OR行列の終了マーク */
    return false;
  } else {
    /* range primitive が無いか、またはrange_primitiveと交わる事を確認 */
    bool no_rp = range_primitive == 99;  /* range primitive が無い */
    int t = solver_fast(range_primitive, &light_dirvec, &intersection_point);
    /* range primitive とぶつからなければ */
    /* or group との交点はない            */
    bool cross = (t != 0) && solver_dist < -0.1 && shadow_check_one_or_group(1, head);

    if (no_rp || cross) {
      if (shadow_check_one_or_group(1, head)) {
        return true; /* 交点があるので、影に入る事が判明。探索終了 */
      } else {
        return shadow_check_one_or_matrix(ofs + 1, or_matrix); /* 次の要素を試す */
      }
    } else {
      return shadow_check_one_or_matrix(ofs + 1, or_matrix); /* 次の要素を試す */
    }
  }
}


/******************************************************************************
   光線と物体の交差判定
*****************************************************************************/

/**** あるANDネットワークが、レイトレースの方向に対し、****/
/**** 交点があるかどうかを調べる。                    ****/
// iteration
void solve_each_element(int iand_ofs, int *and_group, dvec_t *dirvec) {
  int iobj = and_group[iand_ofs];
  if (iobj == -1) {
    return;
  } else {
    int t0 = solver(iobj, dirvec, &startp);
    if (t0 != 0) {
      /* 交点がある時は、その交点が他の要素の中に含まれるかどうか調べる。*/
      /* 今までの中で最小の t の値と比べる。*/
      float t0p = solver_dist;
      if (0.0 < t0p) {
        if (t0p < tmin) {
          float t = t0p + 0.01;
          vec_t *v = &dirvec->vec;
          float q0 = v->x * t + startp.x;
          float q1 = v->x * t + startp.y;
          float q2 = v->x * t + startp.z;
          if (check_all_inside(0, and_group, q0, q1, q2)) {
            tmin = t;
            vecset(&intersection_point, q0, q1, q2);
            intersected_object_id = iobj;
            intsec_rectside = t0;
          }
        }
      }
      solve_each_element(iand_ofs + 1, and_group, dirvec);
    } else {
      /* 交点がなく、しかもその物体は内側が真ならこれ以上交点はない */
      if (o_isinvert(&objects[iobj])) {
        solve_each_element(iand_ofs + 1, and_group, dirvec);
      }
    }
  }
}


/**** 1つの OR-group について交点を調べる ****/
// iteraion
void solve_one_or_network(int ofs, int *or_group, dvec_t *dirvec) {
  int head = or_group[ofs];
  if (head != -1) {
    int *and_group = and_net[head];
    solve_each_element(0, and_group, dirvec);
    solve_one_or_network(ofs + 1, or_group, dirvec);
  }
}


/**** ORマトリクス全体について交点を調べる。****/
// iteration
void trace_or_matrix(int ofs, int **or_network, dvec_t *dirvec) {
  int *head = or_network[ofs];
  int range_primitive = head[0];
  if (range_primitive == -1) { /* 全オブジェクト終了 */
    return;
  } else {
    /* range primitive の衝突しなければ交点はない */
    float t = solver(range_primitive, dirvec, &startp);
    if (t != 0) {
      float tp = solver_dist;
      if (tp < tmin) {
        solve_one_or_network(1, head, dirvec);
      }
    }
    trace_or_matrix(ofs + 1, or_network, dirvec);
  }
}

/**** トレース本体 ****/
/* トレース開始点 ViewPoint と、その点からのスキャン方向ベクトル */
/* Vscan から、交点 crashed_point と衝突したオブジェクト        */
/* crashed_object を返す。関数自体の返り値は交点の有無の真偽値。 */
bool judge_intersection(dvec_t *dirvec) {
  float tmin = 1000000000.0;
  float t;
  trace_or_matrix(0, or_net, dirvec);
  t = tmin;
  if (-0.1 < t) {
    return t < 100000000.0;
  }
  return false;
}

/******************************************************************************
   光線と物体の交差判定 高速版
*****************************************************************************/

// iteration
void solve_each_element_fast(int iand_ofs, int *and_group, dvec_t *dirvec) {
  vec_t *vec = d_vec(dirvec);
  int iobj = and_group[iand_ofs];
  if (iobj == -1) {
    return;
  } else {
    float t0 = solver_fast2(iobj, dirvec);
    if (t0 != 0) {
      /* 交点がある時は、その交点が他の要素の中に含まれるかどうか調べる。*/
      /* 今までの中で最小の t の値と比べる。*/
      float t0p = solver_dist;
      if (0.0 < t0p) {
        if (t0p < tmin) {
          float t  = t0p + 0.01;
          float q0 = vec->x * t + startp_fast.x;
          float q1 = vec->y * t + startp_fast.y;
          float q2 = vec->z * t + startp_fast.z;
          if (check_all_inside(0, and_group, q0, q1, q2)) {
            tmin = t;
            vecset(&intersection_point, q0, q1, q2);
            intersected_object_id = iobj;
            intsec_rectside = t0;
          }
        }
      }
      solve_each_element_fast(iand_ofs + 1, and_group, dirvec);
    } else {
      /* 交点がなく、しかもその物体は内側が真ならこれ以上交点はない */
      if (o_isinvert(&objects[iobj])) {
        solve_each_element_fast(iand_ofs + 1, and_group, dirvec);
      }
    }
  }
}

/**** 1つの OR-group について交点を調べる ****/
// iteration
void solve_one_or_network_fast(int ofs, int *or_group, dvec_t *dirvec) {
  int head = or_group[ofs];
  if (head != -1) {
    int *and_group = and_net[head];
    solve_each_element_fast(0, and_group, dirvec);
    solve_one_or_network_fast(ofs+1, or_group, dirvec);
  }
}

/**** ORマトリクス全体について交点を調べる。****/
void trace_or_matrix_fast(int ofs, int **or_network, dvec_t *dirvec) {
  int *head = or_network[ofs];
  int range_primitive = head[0];
  if (range_primitive == -1) { /* 全オブジェクト終了 */
    return;
  } else {
    if (range_primitive == 99) { /* range primitive なし */
      solve_one_or_network_fast(1, head, dirvec);
    } else {
      /* range primitive の衝突しなければ交点はない */
      float t = solver_fast2(range_primitive, dirvec);
      if (t != 0) {
        float tp = solver_dist;
        if (tp < tmin) {
          solve_one_or_network_fast(1, head, dirvec);
        }
      }
    }
    trace_or_matrix_fast(ofs + 1, or_network, dirvec);
  }
}

/**** トレース本体 ****/
bool judge_intersection_fast(dvec_t *dirvec) {
  float tmin = 1000000000.0;
  float t;
  trace_or_matrix_fast(0, or_net, dirvec);
  t = tmin;
  if (-0.1 < t) {
    return t < 100000000.0;
  } else {
    return false;
  }
}


/******************************************************************************
   物体と光の交差点の法線ベクトルを求める関数
*****************************************************************************/

/**** 交点から法線ベクトルを計算する ****/
/* 衝突したオブジェクトを求めた際の solver の返り値を */
/* 変数 intsec_rectside 経由で渡してやる必要がある。 */
/* nvector もグローバル。 */
void get_nvector_rect(dvec_t *dirvec) {
  int rectside = intsec_rectside;
  /* solver の返り値はぶつかった面の方向を示す */
  vecbzero(&nvector);
  switch(rectside-1) {
  case 1:
    nvector.x = fneg(sgn(dirvec->vec.x));
    break;
  case 2:
    nvector.x = fneg(sgn(dirvec->vec.x));
    break;
  case 3:
    nvector.x = fneg(sgn(dirvec->vec.x));
    break;
  default:
    abort(); /* Error */
  }

}


/* 平面 */
void get_nvector_plane(obj_t *m) {
  /* m_invert は常に true のはず */
  nvector.x = fneg(o_param_a(m));
  nvector.y = fneg(o_param_b(m));
  nvector.z = fneg(o_param_c(m));
}

/* 2次曲面 :  grad x^t A x = 2 A x を正規化する */
void get_nvector_second(obj_t *m) {
  float p0 = intersection_point.x - o_param_x(m);
  float p1 = intersection_point.y - o_param_y(m);
  float p2 = intersection_point.z - o_param_z(m);

  float d0 = p0 * o_param_a(m);
  float d1 = p1 * o_param_b(m);
  float d2 = p2 * o_param_c(m);

  if (o_isrot(m) == 0) {
    nvector.x = d0;
    nvector.y = d1;
    nvector.z = d2;
  } else {
    nvector.x = d0 + fhalf(p1 * o_param_r3(m) + p2 * o_param_r2(m));
    nvector.y = d0 + fhalf(p0 * o_param_r3(m) + p2 * o_param_r1(m));
    nvector.z = d0 + fhalf(p0 * o_param_r2(m) + p1 * o_param_r1(m));
  }
  vecunit_sgn(&nvector, o_isinvert(m));
}

void get_nvector(obj_t *m, dvec_t *dirvec) {
  int m_shape = o_form(m);
  if (m_shape == 1) {
    get_nvector_rect(dirvec);
  } else if (m_shape == 2) {
    get_nvector_plane(m);
  } else { /* 2次曲面 or 錐体 */
    get_nvector_second(m);
  }
}


/******************************************************************************
   物体表面の色(色付き拡散反射率)を求める
*****************************************************************************/


/**** 交点上のテクスチャの色を計算する ****/
void utexture(obj_t *m, vec_t *p) {
  int m_tex = o_texturetype(m);
  /* 基本はオブジェクトの色 */
  texture_color.x = o_color_red(m);
  texture_color.y = o_color_green(m);
  texture_color.z = o_color_blue(m);
  if (m_tex == 1) {
    /* zx方向のチェッカー模様 (G) */
    float w1 = p->x - o_param_x(m);
    float d1 = (floor(w1 * 0.05)) * 20.0;
    float w3 = p->z - o_param_z(m);
    float d2 = (floor(w3 * 0.05)) * 20.0;
    int flag1 = (w1-d1 < 10.0);
    int flag2 = (w3-d2 < 10.0);
    if (flag1 ^ flag2) {
      texture_color.y = 0.0;
    } else {
      texture_color.y = 255.0;
    }
  } else if (m_tex == 2) {
    /* y軸方向のストライプ (R-G) */
    float w2 = fsqr(sin(p->y * 0.25));
    texture_color.x = 255.0 * w2;
    texture_color.y = 255.0 * (1.0 - w2);
  } else if (m_tex == 3) {
    /* ZX面方向の同心円 (G-B) */
    float w1 = p->x - o_param_x(m);
    float w3 = p->z - o_param_z(m);
    float w2 = sqrt (fsqr(w1) + fsqr(w3)) / 10.0;
    float w4 = (w2 - floor(w2)) * 3.1415927;
    float cws= fsqr(cos(w4));
    texture_color.y = cws * 255.0;
    texture_color.z = (1.0 - cws) * 255.0;
  } else if (m_tex == 4) {
    /* 球面上の斑点 (B) */
    float w1 = (p->x - o_param_x(m)) * (sqrt(o_param_a(m)));
    float w2 = (p->z - o_param_y(m)) * (sqrt(o_param_b(m)));
    float w3 = (p->z - o_param_z(m)) * (sqrt(o_param_c(m)));
    float w4 = fsqr(w1) + fsqr(w3);
    float w5 = fabs(w3 / w1);
    float w6 = fabs(w2 / w4);
    float w7, w8, w9, w10, w11, w12;
    if (fabs(w1) < 1.0e-4) {
      w7 = 15.0; /* atan +infty = pi/2 */
    } else {
      w7 = (atan(w5) * 30.0) / 3.1415927;
    }
    if (fabs(w4) < 1.0e-4) {
      w8 = 15.0; /* atan +infty = pi/2 */
    } else {
      w8 = (atan(w6) * 30.0) / 3.1415927;
    }
    w9  = w7 - floor(w7);
    w10 = w8 - floor(w8);
    w11 = 0.15 - fsqr(0.5 - w9) - fsqr(0.5 - w10);
    w12 = (fisneg(w11)) ? 0.0 : w11;
    texture_color.z = (255.0 * w12) / 0.3;
  }
}

/******************************************************************************
   衝突点に当たる光源の直接光と反射光を計算する関数群
*****************************************************************************/

/* 当たった光による拡散光と不完全鏡面反射光による寄与をRGB値に加算 */
void add_light(float bright, float hilight, float hilight_scale) {

  /* 拡散光 */
  if (fispos(bright)) {
    vecaccum(&rgb, bright, &texture_color);
  }

  /* 不完全鏡面反射 cos ^4 モデル */
  if (fispos(hilight)) {
    float ihl = fsqr(fsqr(hilight)) * hilight_scale;
    rgb.x += ihl;
    rgb.y += ihl;
    rgb.z += ihl;
  }

}


/* 各物体による光源の反射光を計算する関数(直方体と平面のみ) */
// iteration
void trace_reflections(int index, float diffuse, float hilight_scale, dvec_t *dirvec) {
  if (index >= 0) {
    refl_t *rinfo = &reflections[index]; /* 鏡平面の反射情報 */
    dvec_t *dvec  = r_dvec(rinfo);       /* 反射光の方向ベクトル(光と逆向き */

    /*反射光を逆にたどり、実際にその鏡面に当たれば、反射光が届く可能性有り */
    if (judge_intersection_fast(dvec)) {
      int surface_id = intersected_object_id * 4 + intsec_rectside;
      if (surface_id == r_surface_id(rinfo)) {
        /* 鏡面との衝突点が光源の影になっていなければ反射光は届く */
        if (!shadow_check_one_or_matrix(0, or_net)) {
          /* 届いた反射光による RGB成分への寄与を加算 */
          float p = veciprod_d(dvec, &nvector);
          float scale = r_bright(rinfo);
          float bright = scale  * diffuse * p;
          float hilight = scale * veciprod_d(dirvec, d_vec(dvec));
          add_light(bright, hilight, hilight_scale);
        }
      }
    }
    trace_reflections(index - 1, diffuse, hilight_scale, dirvec);
  }
}

/******************************************************************************
   直接光を追跡する
*****************************************************************************/
// iteration
void trace_ray(int nref, float energy, dvec_t *dirvec, pixel_t *pixel, float dist) {
  if (nref <= 4) {
    iarr_t *surface_ids = p_surface_ids(pixel);
    if (judge_intersection(dirvec)) {
      /* オブジェクトにぶつかった場合 */
      int obj_id = intersected_object_id;
      obj_t *obj = &objects[obj_id];
      int m_surface = o_reflectiontype(obj);
      float diffuse = o_diffuse(obj) * energy;
      varr_t *intersection_points;
      iarr_t *calc_diffuse;
      float w, hilight_scale;
      get_nvector(obj, dirvec); /* 法線ベクトルを get */
      veccpy(&startp, &intersection_point);  /* 交差点を新たな光の発射点とする */
      utexture(obj, &intersection_point); /*テクスチャを計算 */

      /* pixel tupleに情報を格納する */
      iarr_set_nth(surface_ids, nref, obj_id * 4 + intsec_rectside);
      intersection_points = p_intersection_points(pixel);
      veccpy(varr_get_nth(intersection_points, nref),
             &intersection_point);
      /* 拡散反射率が0.5以上の場合のみ間接光のサンプリングを行う */

      calc_diffuse = p_calc_diffuse(pixel);
      if (o_diffuse(obj) < 0.5) {
        iarr_set_nth(calc_diffuse, nref, false);
      } else {
        varr_t *energya  = p_energy(pixel);
        varr_t *nvectors = p_nvectors(pixel);
        iarr_set_nth(calc_diffuse, nref, true);
        varr_set_nth(energya, nref, &texture_color);
        vecscale(varr_get_nth(energya, nref),
                 (1.0 / 256.0) * diffuse);
        varr_set_nth(nvectors, nref, &nvector);
      }

      w = (-2.0) * veciprod_d(dirvec, &nvector);
      vecaccum(d_vec(dirvec), w, &nvector);

      hilight_scale = energy * o_hilight(obj);
      /* 光源光が直接届く場合、RGB成分にこれを加味する */
      if (!(shadow_check_one_or_matrix(0, or_net))) {
        float bright = fneg(veciprod(&nvector, &light)) * diffuse;
        float hilight = fneg(veciprod_d(dirvec, &light));
        add_light(bright, hilight, hilight_scale);
      }

      /* 光源光の反射光が無いか探す */
      setup_startp(&intersection_point);
      trace_reflections(n_reflections-1, diffuse, hilight_scale, dirvec);

      /* 重みが 0.1より多く残っていたら、鏡面反射元を追跡する */
      if (0.1 < energy) {
        if (nref < 4) {
          iarr_set_nth(surface_ids, nref+1, -1);
        }
      }

      if (m_surface == 2) {
        float energy2 = energy * (1.0 - o_diffuse(obj));
        trace_ray(nref+1, energy2, dirvec, pixel, dist + tmin);
      }
    } else {
      /* どの物体にも当たらなかった場合。光源からの光を加味 */
      iarr_set_nth(surface_ids, nref+1, -1);
      if (nref != 0) {
        float hl = fneg(veciprod_d(dirvec, &light));
        /* 90°を超える場合は0 (光なし) */
        if (fispos(hl)) {
          /* ハイライト強度は角度の cos^3 に比例 */
          float ihl = fsqr(hl) * hl * energy * beam;
          rgb.x += ihl;
          rgb.y += ihl;
          rgb.z += ihl;
        }

      }

    }
  }
}


/******************************************************************************
   間接光を追跡する
*****************************************************************************/

/* ある点が特定の方向から受ける間接光の強さを計算する */
/* 間接光の方向ベクトル dirvecに関しては定数テーブルが作られており、衝突判定
   が高速に行われる。物体に当たったら、その後の反射は追跡しない */
void trace_diffuse_ray(dvec_t *dirvec, float energy) {
  /* どれかの物体に当たるか調べる */
  if (judge_intersection_fast(dirvec)) {
    obj_t *obj = &objects[intersected_object_id];
    get_nvector(obj, dirvec);
    utexture(obj, &intersection_point);

    /* その物体が放射する光の強さを求める。直接光源光のみを計算 */
    if (!shadow_check_one_or_matrix(0, or_net)) {
      float br = fneg(veciprod(&nvector, &light));
      float bright = (fispos(br) ? br : 0.0);
      vecaccum(&diffuse_ray,
               energy * bright * o_diffuse(obj),
               &texture_color);
    }
  }

}

/* あらかじめ決められた方向ベクトルの配列に対し、各ベクトルの方角から来る
   間接光の強さをサンプリングして加算する */
// iteration
void iter_trace_diffuse_rays(dvec_t *dirvec_group, vec_t *nvector, vec_t *org, int index) {
  if (index >= 0) {
    float p = veciprod(d_vec(&dirvec_group[index]), nvector);

    /* 配列の 2n 番目と 2n+1 番目には互いに逆向の方向ベクトルが入っている
       法線ベクトルと同じ向きの物を選んで使う */
    if (fisneg(p)) {
      trace_diffuse_ray(&dirvec_group[index+1], p / -150.0);
    } else {
      trace_diffuse_ray(&dirvec_group[index],   p / -150.0);
    }
    iter_trace_diffuse_rays(dirvec_group, nvector, org, (index - 2));
  }
}

/* 与えられた方向ベクトルの集合に対し、その方向の間接光をサンプリングする */
void trace_diffuse_rays(dvec_t *dirvec_group, vec_t *nvector, vec_t *org) {
  setup_startp(org);

  /* 配列の 2n 番目と 2n+1 番目には互いに逆向の方向ベクトルが入っていて、
     法線ベクトルと同じ向きの物のみサンプリングに使われる */
  /* 全部で 120 / 2 = 60本のベクトルを追跡 */
  iter_trace_diffuse_rays(dirvec_group, nvector, org, 118);
}

/* 半球方向の全部で300本のベクトルのうち、まだ追跡していない残りの240本の
   ベクトルについて間接光追跡する。60本のベクトル追跡を4セット行う */
void trace_diffuse_ray_80percent(int group_id, vec_t *nvector, vec_t *org) {

  int i;

  for (i = 0; i <= 4; ++i) {
    if (group_id != i) {
      trace_diffuse_rays(&dirvecs[i], nvector, org);
    }
  }

}

/* 上下左右4点の間接光追跡結果を使わず、300本全部のベクトルを追跡して間接光を
   計算する。20%(60本)は追跡済なので、残り80%(240本)を追跡する */
void calc_diffuse_using_1point(pixel_t *pixel, int nref) {
  varr_t *ray20p = p_received_ray_20percent(pixel);
  varr_t  *nvectors = p_nvectors(pixel);
  varr_t  *intersection_points = p_intersection_points(pixel);
  varr_t *energya = p_energy(pixel);
  veccpy(&diffuse_ray, varr_get_nth(ray20p, nref));
  trace_diffuse_ray_80percent(p_group_id(pixel),
                              varr_get_nth(nvectors, nref),
                              varr_get_nth(intersection_points, nref));
  vecaccumv(&rgb, varr_get_nth(energya, nref), &diffuse_ray);
}

/* 自分と上下左右4点の追跡結果を加算して間接光を求める。本来は 300 本の光を
   追跡する必要があるが、5点加算するので1点あたり60本(20%)追跡するだけで済む */
void calc_diffuse_using_5points(int x, parr_t *prev, parr_t *cur, parr_t *next, int nref) {
  varr_t *r_up     = p_received_ray_20percent(parr_get_nth(prev, x));
  varr_t *r_left   = p_received_ray_20percent(parr_get_nth(cur, x-1));
  varr_t *r_center = p_received_ray_20percent(parr_get_nth(cur, x));
  varr_t *r_right  = p_received_ray_20percent(parr_get_nth(cur, x+1));
  varr_t *r_down   = p_received_ray_20percent(parr_get_nth(next, x));

  varr_t *energya  = p_energy(parr_get_nth(cur, x));

  veccpy(&diffuse_ray, varr_get_nth(r_up,     nref));

  vecadd(&diffuse_ray, varr_get_nth(r_left,   nref));
  vecadd(&diffuse_ray, varr_get_nth(r_center, nref));
  vecadd(&diffuse_ray, varr_get_nth(r_right,  nref));
  vecadd(&diffuse_ray, varr_get_nth(r_down,   nref));

  vecaccumv(&rgb, varr_get_nth(energya, nref), &diffuse_ray);

}

/* 上下左右4点を使わずに直接光の各衝突点における間接受光を計算する */
// iteration
void do_without_neighbors(pixel_t *pixel, int nref) {
  if (nref <= 4) {
    /* 衝突面番号が有効(非負)かチェック */
    iarr_t *surface_ids = p_surface_ids(pixel);
    if (iarr_get_nth(surface_ids, nref) >= 0) {
      iarr_t *calc_diffuse = p_calc_diffuse(pixel);
      if (iarr_get_nth(calc_diffuse, nref)) {
        calc_diffuse_using_1point(pixel, nref);
      }
      do_without_neighbors(pixel, nref+1);
    }
  }
}

/* 画像上で上下左右に点があるか(要するに、画像の端で無い事)を確認 */
bool neighbors_exist(int x, int y, pixel_t *next) {
  if (0 < y && y+1 < image_size[1]) {
    if (0 < x && x+1 < image_size[0]) {
      return true;
    }
  }
  return false;
}

int get_surface_id(pixel_t *pixel, int index) {
  iarr_t *surface_ids = p_surface_ids(pixel);
  return iarr_get_nth(surface_ids, index);
}

/* 上下左右4点の直接光追跡の結果、自分と同じ面に衝突しているかをチェック
   もし同じ面に衝突していれば、これら4点の結果を使うことで計算を省略出来る */
bool neighbors_are_available(int x, parr_t *prev, parr_t *cur, parr_t *next, int nref) {
  int sid_center = get_surface_id(parr_get_nth(cur, x), nref);
  if (get_surface_id(parr_get_nth(prev, x),  nref) == sid_center) {
    if (get_surface_id(parr_get_nth(next, x),  nref) == sid_center) {
      if (get_surface_id(parr_get_nth(cur, x-1), nref) == sid_center) {
        if (get_surface_id(parr_get_nth(cur, x+1), nref) == sid_center) {
          return true;
        }
      }
    }
  }
  return false;
}

/* 直接光の各衝突点における間接受光の強さを、上下左右4点の結果を使用して計算
   する。もし上下左右4点の計算結果を使えない場合は、その時点で
   do_without_neighborsに切り替える */
// iteration
void try_exploit_neighbors(int x, int y, parr_t *prev, parr_t *cur, parr_t *next, int nref) {
  pixel_t *pixel = parr_get_nth(cur, x);
  if (nref <= 4) {
    /* 衝突面番号が有効(非負)か */
    if (get_surface_id(pixel, nref) >= 0) {
      /* 周囲4点を補完に使えるか */
      if (neighbors_are_available(x, prev, cur, next, nref)) {

        /* 間接受光を計算するフラグが立っていれば実際に計算する */
        iarr_t *calc_diffuse = p_calc_diffuse(pixel);
        if (iarr_get_nth(calc_diffuse, nref)) {
          calc_diffuse_using_5points(x, prev, cur, next, nref);
        }
        /* 次の反射衝突点へ */
        try_exploit_neighbors(x, y, prev, cur, next, nref + 1);
      } else {
        /* 周囲4点を補完に使えないので、これらを使わない方法に切り替える */
        do_without_neighbors(parr_get_nth(cur, x), nref);
      }
    }

  }
}


/******************************************************************************
   PPMファイルの書き込み関数
*****************************************************************************/
void write_ppm_header() {
  print_char(80); /* 'P' */
  print_char(48 + 3); /* +6 if binary */ /* 48 = '0' */
  print_char(10);
  print_int(image_size[0]);
  print_char(32);
  print_int(image_size[1]);
  print_char(32);
  print_int(255);
  print_char(10);
}

void write_rgb_element(int x) {
  int ix = int_of_float(x);
  int elem = ix;
  if (ix > 255) {
    elem = 255;
  } else if (ix < 0) {
    elem = 0;
  }
  print_int(elem);
}


void write_rgb () {
  write_rgb_element(rgb.x); /* Red */
  print_char(32);
  write_rgb_element(rgb.y); /* Green */
  print_char(32);
  write_rgb_element(rgb.z); /* Blue */
  print_char(10);
}

/******************************************************************************
   あるラインの計算に必要な情報を集めるため次のラインの追跡を行っておく関数群
*****************************************************************************/

/* 間接光のサンプリングでは上下左右4点の結果を使うので、次のラインの計算を
   行わないと最終的なピクセルの値を計算できない */

/* 間接光を 60本(20%)だけ計算しておく関数 */
// iteration
void pretrace_diffuse_rays(pixel_t *pixel, int nref) {
  if (nref <= 4) {
    /* 面番号が有効か */
    int sid = get_surface_id(pixel, nref);
    if (sid >= 0) {
      /* 間接光を計算するフラグが立っているか */
      iarr_t *calc_diffuse = p_calc_diffuse(pixel);
      if (iarr_get_nth(calc_diffuse, nref)) {
        int group_id = p_group_id(pixel);
        varr_t *nvectors;
        varr_t *intersection_points;
        varr_t *ray20p;
        vecbzero(&diffuse_ray);

        /* 5つの方向ベクトル集合(各60本)から自分のグループIDに対応する物を
           一つ選んで追跡 */
        nvectors = p_nvectors(pixel);
        intersection_points = p_intersection_points(pixel);
        trace_diffuse_rays(&dirvecs[group_id],
                           varr_get_nth(nvectors, nref),
                           varr_get_nth(intersection_points, nref));
        ray20p = p_received_ray_20percent(pixel);
        veccpy(varr_get_nth(ray20p, nref),
               &diffuse_ray);
      }
      pretrace_diffuse_rays(pixel, nref+1);
    }

  }
}


/* 各ピクセルに対して直接光追跡と間接受光の20%分の計算を行う */
// iteration
void pretrace_pixels(parr_t *line, int x, int group_id, float lc0, float lc1, float lc2) {
  if (x >= 0) {
    float xdisp = scan_pitch * float_of_int(x - image_center[0]);
    ptrace_dirvec.vec.x = xdisp * screenx_dir.x + lc0;
    ptrace_dirvec.vec.y = xdisp * screenx_dir.y + lc1;
    ptrace_dirvec.vec.z = xdisp * screenx_dir.z + lc2;
    vecunit_sgn(&ptrace_dirvec.vec, false);
    vecbzero(&rgb);
    veccpy(&startp, &viewpoint);

    /* 直接光追跡 */
    trace_ray(0, 1.0, &ptrace_dirvec, parr_get_nth(line, x), 0.0);
    veccpy(p_rgb(parr_get_nth(line, x)), &rgb);
    p_set_group_id(parr_get_nth(line, x), group_id);

    /* 間接光の20%を追跡 */
    pretrace_diffuse_rays(parr_get_nth(line, x), 0);

    pretrace_pixels(line, x-1, add_mod5(group_id, 1), lc0, lc1, lc2);

  }
}


/* あるラインの各ピクセルに対し直接光追跡と間接受光20%分の計算をする */
void pretrace_line(parr_t *line, int y, int group_id) {
  float ydisp = scan_pitch * float_of_int(y - image_center[1]);
  /* ラインの中心に向かうベクトルを計算 */
  float lc0 = ydisp * screeny_dir.x + screenz_dir.x;
  float lc1 = ydisp * screeny_dir.y + screenz_dir.y;
  float lc2 = ydisp * screeny_dir.z + screenz_dir.z;
  pretrace_pixels(line, image_size[0] - 1, group_id, lc0, lc1, lc2);
}

/******************************************************************************
   ピクセルの情報を格納するデータ構造の割り当て関数群
 *****************************************************************************/
/* ピクセルを表すtupleを割り当て */
pixel_t *create_pixel() {
  pixel_t *pixel = malloc(sizeof(pixel_t));
  vecbzero(&pixel->rgb);
  varr_init(&pixel->isect_ps, 5, 0.0);
  iarr_init(&pixel->sids, 5, 0);
  iarr_init(&pixel->cdif, 5, false);
  varr_init(&pixel->engy, 5, 0.0);
  varr_init(&pixel->r20p, 5, 0.0);
  pixel->gid = 0;
  varr_init(&pixel->nvectors, 5, 0.0);
  return pixel;
}


/* 横方向1ライン中の各ピクセル要素を割り当てる */
// iteration
parr_t *init_line_elements(parr_t *line, int n) {
  if (n >= 0) {
    pixel_t *pixel = create_pixel();
    parr_set_nth(line, n, pixel);
    return init_line_elements(line, n-1);
  } else {
    return line;
  }
}

/* 横方向1ライン分のピクセル配列を作る */
parr_t *create_pixelline() {
  int i;
  int x = image_size[0];
  parr_t *line = (parr_t *)malloc(sizeof(parr_t));

  for (i = 0; i < x; ++i) {
    parr_set_nth(line, i, create_pixel());
  }
  return init_line_elements(line, image_size[0]-2);
}
