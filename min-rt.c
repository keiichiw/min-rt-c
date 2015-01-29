
/****************************************************************/
/*                                                              */
/* Ray Tracing Program for C                                    */
/*                                                              */
/* Original Program by Ryoji Kawamichi                          */
/* Arranged for Chez Scheme by Motohico Nanano                  */
/* Arranged for Objective Caml by Y. Oiwa and E. Sumii          */
/* Added diffuse ray tracer by Y.Ssugawara                      */
/* Arranged for C by K.Watanabe                                 */
/*                                                              */
/****************************************************************/

#include <stdlib.h>
#include "runtime.h"

typedef enum {
  True  = 1,
  False = 0
} bool;

typedef struct l {
  int car;
  struct l *cdr;
} list;

typedef struct {
  float x, y, z;
} vec;

typedef struct {
  float x, y, z, w;
} vec4;

typedef struct {
  int  tex, shape, surface;
  bool isrot;
  vec  abc, xyz;
  bool invert;
  vec  surfparams, color, rot123;
  vec4 ctbl;
} obj;

typedef struct {
  int   rgb;
  list  isect_ps;
  list  sids;
  bool  cdif;
  float engy;
  float r20p;
  int   gid;
  vec   nvectors;
} pixel;

typedef struct {
  vec   vec;
  int   cnst;
} dvec;

typedef struct {
  int   sid;
  dvec  dv;
  float br;
} refl;


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
void vecset (vec *v, float x, float y, float z) {
  v->x = x;
  v->y = y;
  v->z = z;
}

/* 同じ値で埋める */
void vecfill (vec *v, float elem) {
  v->x = elem;
  v->y = elem;
  v->z = elem;
}

/* 零初期化 */
void vecbzero (vec *v) {
  vecfill(v, 0.0);
}

/* コピー */
void veccpy (vec *dest, vec *src) {
  dest->x = src->x;
  dest->y = src->y;
  dest->z = src->z;
}

/* 距離の自乗 */
float vecdist2 (vec *p, vec *q) {
  return fsqr (p->x - q->x) + fsqr (p->y - q->y) + fsqr (p->z - q->z);
}

/* 正規化 ゼロ割りチェック無し */
void vecunit (vec *v) {
  float il = 1.0 / sqrt(fsqr(v->x) + fsqr(v->y) + fsqr(v->z));
  v->x = v->x * il;
  v->y = v->y * il;
  v->z = v->z * il;
}

/* 符号付正規化 ゼロ割チェック*/
void vecunit_sgn (vec *v, int inv) {
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
float veciprod (vec *v, vec *w) {
  return v->x * w->x + v->y * w->y + v->z * w->z;
}

/* 内積 引数形式が異なる版 */
float veciprod2 (vec *v, float w0, float w1, float w2) {
    return v->x * w0 + v->y * w1 + v->z * w2;
}

/* 別なベクトルの定数倍を加算 */
void vecaccum (vec *dest, float scale, vec *v) {
  dest->x += scale * v->x;
  dest->y += scale * v->y;
  dest->z += scale * v->z;
}

/* ベクトルの和 */
void vecadd (vec *dest, vec *v) {
  dest->x += v->x;
  dest->y += v->y;
  dest->z += v->z;
}

/* ベクトル要素同士の積 */
void vecmul (vec *dest, vec *v) {
  dest->x *= v->x;
  dest->y *= v->y;
  dest->z *= v->z;
}

/* ベクトルを定数倍 */
void vecscale (vec *dest, float scale) {
  dest->x *= scale;
  dest->y *= scale;
  dest->z *= scale;
}

/* 他の２ベクトルの要素同士の積を計算し加算 */
void vecaccumv (vec *dest, vec *v,  vec *w) {
  dest->x += v->x * w->x;
  dest->y += v->y * w->y;
  dest->z += v->z * w->z;
}

/******************************************************************************
   オブジェクトデータ構造へのアクセス関数
*****************************************************************************/

/* テクスチャ種 0:無し 1:市松模様 2:縞模様 3:同心円模様 4:斑点*/
int o_texturetype (obj *m) {
  return m->tex;
}

/* 物体の形状 0:直方体 1:平面 2:二次曲面 3:円錐 */
int o_form (obj *m) {
  return m->shape;
}



/* 反射特性 0:拡散反射のみ 1:拡散＋非完全鏡面反射 2:拡散＋完全鏡面反射 */
int o_reflectiontype (obj *m) {
  return m->surface;
}

/* 曲面の外側が真かどうかのフラグ true:外側が真 false:内側が真 */
bool o_isinvert (obj *m) {
  return m->invert;
}

/* 回転の有無 true:回転あり false:回転無し 2次曲面と円錐のみ有効 */
bool o_isrot (obj *m) {
  return m->isrot;
}

/* 物体形状の aパラメータ */
float o_param_a (obj *m) {
  return m->abc.x;
}

/* 物体形状の bパラメータ */
float o_param_b (obj *m) {
  return m->abc.y;
}

/* 物体形状の cパラメータ */
float o_param_c (obj *m) {
  return m->abc.z;
}

/* 物体形状の abcパラメータ */
vec* o_param_abc (obj *m) {
  return &m->abc;
}

/* 物体の中心x座標 */
float o_param_x (obj *m) {
  return m->xyz.x;
}

/* 物体の中心y座標 */
float o_param_y (obj *m) {
  return m->xyz.y;
}

/* 物体の中心z座標 */
float o_param_z (obj *m) {
  return m->xyz.z;
}

/* 物体の拡散反射率 0.0 -- 1.0 */
float o_diffuse (obj *m) {
  return m->surfparams.x;
}

/* 物体の不完全鏡面反射率 0.0 -- 1.0 */
float o_hilight (obj *m) {
  return m->surfparams.y;
}

/* 物体色の R成分 */
float o_coler_red (obj *m) {
  return m->color.x;
}

/* 物体色の G成分 */
float o_coler_green (obj *m) {
  return m->color.y;
}

/* 物体色の B成分 */
float o_coler_blue (obj *m) {
  return m->color.z;
}

/* 物体の曲面方程式の y*z項の係数 2次曲面と円錐で、回転がある場合のみ */
float o_param_r1 (obj *m) {
  return m->rot123.x;
}

/* 物体の曲面方程式の x*z項の係数 2次曲面と円錐で、回転がある場合のみ */
float o_param_r2 (obj *m) {
  return m->rot123.y;
}

/* 物体の曲面方程式の x*y項の係数 2次曲面と円錐で、回転がある場合のみ */
float o_param_r3 (obj *m) {
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
vec4* o_param_ctbl (obj *m) {
  return &m->ctbl;
}

/******************************************************************************
   Pixelデータのメンバアクセス関数群
*****************************************************************************/

/* 直接光追跡で得られたピクセルのRGB値 */
int p_rgb (pixel *pixel) {
  return pixel->rgb;
}

/* 飛ばした光が物体と衝突した点の配列 */
list p_intersection_points (pixel *pixel) {
  return pixel->isect_ps;
}

/* 飛ばした光が衝突した物体面番号の配列 */
/* 物体面番号は オブジェクト番号 * 4 + (solverの返り値) */
list p_surface_ids (pixel *pixel) {
  return pixel->sids;
}

/* 間接受光を計算するか否かのフラグ */
int p_calc_diffuse (pixel *pixel) {
  return pixel->cdif;
}

/* 衝突点の間接受光エネルギーがピクセル輝度に与える寄与の大きさ */
float p_energy (pixel *pixel) {
  return pixel->engy;
}

/* 衝突点の間接受光エネルギーを光線本数を1/5に間引きして計算した値 */
float p_received_ray_20percent (pixel *pixel) {
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

int p_group_id (pixel *pixel) {
  return pixel->gid;
}

/* グループIDをセットするアクセス関数 */
void p_set_group_id (pixel *pixel, int id) {
  pixel->gid = id;
}

/* 各衝突点における法線ベクトル */
vec* p_nvectors (pixel *pixel) {
  return &(pixel->nvectors);
}

/******************************************************************************
   前処理済み方向ベクトルのメンバアクセス関数
*****************************************************************************/

/* ベクトル */
vec* d_vec (dvec *d) {
  return &(d->vec);
}

/* 各オブジェクトに対して作った solver 高速化用定数テーブル */
int d_const (dvec *d) {
  return d->cnst;
}

/******************************************************************************
   平面鏡面体の反射情報
*****************************************************************************/

/* 面番号 オブジェクト番号*4 + (solverの返り値) */
int r_surface_id (refl *r) {
  return r->sid;
}

/* 光源光の反射方向ベクトル(光と逆向き) */
dvec* r_dvec (refl *r) {
  return &(r->dv);
}

/* 物体の反射率 */
float r_bright (refl *r) {
  return r->br;
}

/******************************************************************************
   データ読み込みの関数群
*****************************************************************************/

/* ラジアン */
float rad (float x) {
  return x * 0.017453293;
}
