## [JP]:
- https://github.com/tsnest/MetroDeveloper を Metro Exodus Enhanced Edition でも動作するように改修したものです。
- EEで利用する場合は MetroDeveloper.ini の ```[other]``` セクションに下記を設定してください。
  - ```enhanced_edition = yes``` を追加
- VisualStudio2026でビルド
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [RU]
- Это модифицированная версия https://github.com/tsnest/MetroDeveloper для работы с Metro Exodus Enhanced Edition.
- При использовании в EE необходимо внести следующие настройки в раздел ```[other]``` файла MetroDeveloper.ini.
  - Добавьте строку ```enhanced_edition = yes```
- Собрано с помощью Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [EN]
- This is a modified version of https://github.com/tsnest/MetroDeveloper to work with Metro Exodus Enhanced Edition.
- When using the Enhanced Edition, please configure the following in the ```[other]``` section of MetroDeveloper.ini:
  - Add ```enhanced_edition = yes```
- Built with Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [更新履歴]
- MetroDeveloperの20260404リリース版をベースにMetroExodusEEでも開発者コンソール及びFlyモードが動作するように改修



# カスタム機能

- カスタム機能はMetro Exodus Enhanced Editionでしか動作しない

## 雑多な機能追加/変更

- ゲーム時間速度変更/pauseがflyモードではなくとも動作するように変更
  - テンキーの「+」でスピードUP、「-」でスピードDOWN

- テンキーの「*」でゲームスピードを1(通常状態)に設定する機能を追加

- キー設定の追加
  - 開発者コンソール：F11でも起動可能に
    - デフォルトのキー「`」はOSの言語設定を英語にしたうえで半角/全角キーをクリックする必要があり面倒だったため
  - pauseコマンド：テンキーの「/」でも実行可能に
    - デフォルトのキーが開発者のキーボードでは存在しなかったため

- `civ_off` コマンドを追加。Civモード(武器非表示モード)を解除する。この状態で`civ_on`実行で再度Civモードになる。
  - `civ_on`は、`civ_off` 実行状態の解除以外では利用不可



## 武器付与コマンド wpn_give

### 概要

- `wpn_give <武器エンティティ名>` で 対象武器を取得する
  - 例えば`wpn_give ak_74_0001` でak_74_0001という名前が与えられたカラシニコフを取得する
  - 改造パーツが付いている場合は、付いたまま取得できる
- 武器スロットが満杯の場合は、現在手に持っている武器と交換になる
  - ティハールとヘルブレスは専用スロットに入る(どちらか1つのみ)
  - DLC「サムの物語」ではサミーが専用スロット武器

### 武器エンティティ名のリスト表示

- `wpn_give list`を実行すると、現在いるマップの武器エンティティの一覧が`wpn_give_list.log`が出力される

  - logファイルはゲームフォルダ(MetroExodus.exe があるフォルダ)に出力される
  - 武器エンティティの一覧はレベル(マップ)毎に固定のため注意

    - つまり、「モスクワ」で出力した武器エンティティ名は「ヴォルガ川」では利用できないため再度`wpn_give list`を実行すること
  - `wpn_give list <プレフィックス文字列>`で先頭一致で一覧を出力することもできる

- 「モスクワ」でのリスト出力例。[ ]内はエンティティID。※記述を省略しているが、実際はもっと多い

  ```
  === weapons\ak_74 (87) ===
    [10812] ak_74_0001
    [5642] ak_74_0064
  === weapons\ashot (10) ===
    [32072] ashot_0000
    [33407] ashot_0069
  === weapons\flamethrower (1) ===
    [42249] flamethrower_base
  === weapons\gatling (2) ===
    [12727] gatling_armory
    [47624] gatling_base
  === weapons\helsing (1) ===
    [47623] helsing_base
  === weapons\macheta (1) ===
    [258] wpn_macheta_0002
  === weapons\revolver (17) ===
    [30273] revolver_0000
  === weapons\tihar (1) ===
    [521] tihar_base
  === weapons\ubludok (2) ===
    [16134] ubludok_armory
    [10497] ubludok_base
  === weapons\uboynicheg (13) ===
    [33538] damir_heavy_uboynicheg_0002
    [2642] uboynicheg_0001
  === weapons\ventil (2) ===
    [54281] ventil_armory
    [23555] ventil_base
  === weapons\vyhlop (3) ===
    [15366] vyhlop_armory
    [31754] vyhlop_base
  
  140 weapons in 12 types
  
  ```



### Base武器の付与

- `wpn_give_<武器名>`でBase武器(全マップ共通で用意されている、改造パーツがついていない素の状態の武器)が付与される

  - DLC「2人の大佐」ではデフォルト武器がマップに配置されていないため動作しない。「サムの物語」でが一部対応。

  | コマンド              | 取得できる武器 | サムの物語 | ショートカットキー |
  | --------------------- | -------------- | ---------- | ------------------ |
  | wpn_give_ak_74        | カラシニコフ   | 〇         | テンキー 2         |
  | wpn_give_ashot        | ASショット     | 〇         | テンキー 7         |
  | wpn_give_flamethrower | 火炎放射器     |            | テンキー 9         |
  | wpn_give_gatling      | ガトリング     |            | テンキー 6         |
  | wpn_give_helsing      | ヘルシング     |            | テンキー .         |
  | wpn_give_revolver     | リボルバー     | 〇         | テンキー 4         |
  | wpn_give_tihar        | ティハール     |            | テンキー 0         |
  | wpn_give_ubludok      | バスタード     | 〇         | テンキー 1         |
  | wpn_give_uboynicheg   | シャンブラー   | 〇         | テンキー 8         |
  | wpn_give_ventil       | バルブ         | 〇         | テンキー 5         |
  | wpn_give_vyhlop       | ブルドッグ     | 〇         | テンキー 3         |



### 投擲アイテムのリスト表示

- `wpn_give throw_list`現在いるマップの投擲アイテム(ダイナマイトなど)を一覧表示する。
- `wpn_give <投擲アイテムエンティティ名>`で取得する



### 注意事項

- 通常のゲームプレイでは起こりえない状況を作りだしているため、クラッシュや進行不能になる可能性がある。
  - エンティティ名が「scene_ashot」などイベント上のシーンで利用しそうなものは取得を避けておくとよい

- 武器を拾う処理を、レベル内の武器エンティティに対して実行しているため、他キャラクターや敵が所持している武器を奪う形になっている。例えばアンナの持っている武器を取得すると、アンナは武器が無い状態になる。この時、アンナが発砲のアクションをするとプレイヤーキャラが所有している銃が勝手に発砲する。
- 火炎放射器など、一部武器はマップに弾薬が落ちていないが、`g_unlimitedammo on`で弾薬無制限を有効化することで弾薬不要になる
- アンダースコア「_」は、開発者コンソールでは「Shift + -(ハイフン)」で入力する。日本語キーボードの標準とは異なるため注意。

