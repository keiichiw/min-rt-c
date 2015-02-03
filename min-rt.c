
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

typedef struct l {
  int car;
  struct l *cdr;
} list_t;

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
  int    rgb;
  list_t isect_ps;
  list_t sids;
  bool   cdif;
  float  engy;
  float  r20p;
  int    gid;
  vec_t  nvectors;
} pixel_t;

typedef struct {
  vec_t   vec;
  int   cnst;
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


/*
  global variables
*/

vec_t screen;
vec_t screenx_dir, screeny_dir, screenz_dir;
vec_t viewpoint;
vec_t light, beam;

obj_t *objects;
int n_objects;

int *and_net[50];
int **or_net; // or_net is an array of length 1 in min-rt.ml

float solver_dist; // an array of length 1 in min-rt.ml

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
float o_coler_red (obj_t *m) {
  return m->color.x;
}

/* 物体色の G成分 */
float o_coler_green (obj_t *m) {
  return m->color.y;
}

/* 物体色の B成分 */
float o_coler_blue (obj_t *m) {
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
int p_rgb (pixel_t *pixel) {
  return pixel->rgb;
}

/* 飛ばした光が物体と衝突した点の配列 */
list_t *p_intersection_points (pixel_t *pixel) {
  return &pixel->isect_ps;
}

/* 飛ばした光が衝突した物体面番号の配列 */
/* 物体面番号は オブジェクト番号 * 4 + (solverの返り値) */
list_t *p_surface_ids (pixel_t *pixel) {
  return &pixel->sids;
}

/* 間接受光を計算するか否かのフラグ */
int p_calc_diffuse (pixel_t *pixel) {
  return pixel->cdif;
}

/* 衝突点の間接受光エネルギーがピクセル輝度に与える寄与の大きさ */
float p_energy (pixel_t*pixel) {
  return pixel->engy;
}

/* 衝突点の間接受光エネルギーを光線本数を1/5に間引きして計算した値 */
float p_received_ray_20percent (pixel_t *pixel) {
  return pixel->r20p;
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
vec_t* p_nvectors (pixel_t *pixel) {
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
int d_const (dvec_t *d) {
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
  beam.x = read_float();
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
bool solver_rect_surface(obj_t *m, vec_t *dirvec, float b0, float b1, float b2, int i0, int i1, int i2) {
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
int solver_rect (obj_t *m, vec_t *dirvec, float b0, float b1, float b2) {
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
int solver_surface(obj_t *m, vec_t *dirvec, float b0, float b1, float b2) {
  /* 点と平面の符号つき距離 */
  /* 平面は極性が負に統一されている */
  vec_t *abc = o_param_abc(m);
  float d = veciprod(dirvec, abc);
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

int solver_second(obj_t *m, vec_t *dirvec, float b0, float b1, float b2) {
  /* 解の公式 (-b' ± sqrt(b'^2 - a*c)) / a  を使用(b' = b/2) */
  /* a = dirvec^t A dirvec */
  float aa = quadratic(m, dirvec->x, dirvec->y, dirvec->z);

  if (aa == 0.0) {
    return 0; /* 正確にはこの場合も1次方程式の解があるが、無視しても通常は大丈夫 */
  } else {

    /* b' = b/2 = dirvec^t A base   */
    float bb = bilinear(m, dirvec->x, dirvec->y, dirvec->z, b0, b1, b2);
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
int solver(int index, vec_t *dirvec, vec_t *org) {
  obj_t *m = &objects[index];
  /* 直線の始点を物体の基準位置に合わせて平行移動 */
  float b0 =  org->x - o_param_x(m);
  float b1 =  org->y - o_param_y(m);
  float b2 =  org->z - o_param_z(m);
  int m_shape = o_form(m);
  /* 物体の種類に応じた補助関数を呼ぶ */
  if (m_shape == 1) {
    solver_rect(m, dirvec, b0, b1, b2);    /* 直方体 */
  } else if (m_shape == 2) {
    solver_surface(m, dirvec, b0, b1, b2); /* 平面 */
  } else {
    solver_second(m, dirvec, b0, b1, b2);  /* 2次曲面/円錐 */
  }
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
