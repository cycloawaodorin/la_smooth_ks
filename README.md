# 局所領域選択・平均化フィルタ

局所領域の選択と平均化を行う AviUtl のフィルタプラグインです．

# インストール
AviUtl のルートまたは plugins フォルダに la_smooth_ks.auf をコピーします．

# 設定項目
## 注目点中心2次モーメントで領域を選択
分散の代わりに注目画素からの2次モーメントが小さい領域を選択します．
これにより，なだらかなエッジがいずれかの方向に吸収されるのを防ぎ，
細い線が消えにくくなります．

## 色差のみ平均化
色差のみを平均化し，輝度はそのままにするので，見た目の変化がほとんどなくなります．

# 現状
昔書いたものをちょっとだけ直したもので，ナイーブ実装なので無駄計算が多く，重いです．
計算量の削減とか，平均化範囲の変更とか入れたいけど，やるかは不明．
とりあえずフィルタプラグインのサンプルということで……．
