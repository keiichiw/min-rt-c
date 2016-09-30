# min-rt-c
min-rtのANSI-Cへの移植
* test.shを実行することで、/test/ 以下に、入力ファイルとppm形式の画像が生成される
* x86上でmin-rt.mlとの出力の一致を確認(contest.sld)

## MinCaml内のraytrace.cとの比較

MinCamlレポジトリ内にもmin-rtのC実装である[raytrace.c](https://github.com/esumii/min-caml/blob/master/min-rt/raytrace.c)が存在するが、以下の点が異なる
* min-rt-cはANSI-Cの範囲内で実装されている
  - min-rt-cは `-pedantic-errors` オプションをつけてもコンパイルできるが、raytrace.cはそのままではできない
* min-rt-cは[ML版min-rt](https://github.com/kw-udon/min-rt-c/blob/master/origin/min-rt.ml)に忠実な実装を目指した
  - 関数名、構造体名などはML版と同じものを使用
  - ML版で`create_array`でヒープ領域の確保をしている部分では`calloc`を使用
  - contest.sldについて、出力の一致を確認済
