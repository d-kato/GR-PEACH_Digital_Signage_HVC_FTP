# GR-PEACH_Digital_Signage_HVC_FTP
micro SDカード、または、USBメモリに書き込まれたJPEGファイルをGR-PEACHのハードウエアでデコードし、ディスプレイに表示します。表示中のコンテンツはタッチパネルやUSBマウスで操作することができます。一定時間表示(時間の変更可)すると次のJPEGファイルを表示します。  

[GR-PEACH_Digital_Signage](https://os.mbed.com/users/dkato/code/GR-PEACH_Digital_Signage/) に下記の機能を追加しました。  

1. コンテンツ表示順をインデックスファイルで指定
2. テロップ表示
3. オムロン社製 HVC-P2 による顔検出、年齢推定、性別推定、表情推定
4. 時間帯毎に表示コンテンツを変更
5. FTPサーバからコンテンツデータをダウンロード

## 開発環境
GCC環境でのみ動作します。開発環境については、[GR-PEACHオフライン開発環境の手順](https://os.mbed.com/users/1050186/notebook/offline-development-langja/)を参照ください。

## 構成
・GR-PEACH (ディスプレイにLCD Shieldを使用する際は [こちら](http://www.core.co.jp/product/m2m/gr-peach/gr-lcd.html#lcd-stack1))を参照ください。  
・USBA-microUSB変換ケーブル (GR-PEACH電源用)  
・micro SDカード または USBメモリ  
・ディスプレイ (``mbed_app.json``ファイルの``lcd-type``で指定したもの)  
・USBマウス (マウス操作を行わない場合は不要)  
・HVC-P2 (顔検出を行わない場合は不要)  
・USBA-microUSB変換アダプタ (USBメモリ、USBマウス、HVC-P2のいずれかを使用する場合)  
・USB HUB (USBメモリ、USBマウス、HVC-P2を組み合わせて使用する場合)  
・イーサーネットケーブル (NTP接続、FTP接続を行わない場合は不要)  

USBを使用する際はJP3をショートする必要があります。Jumper位置については[こちら](
https://os.mbed.com/teams/Renesas/wiki/Jumper-settings-of-GR-PEACH)を参照ください。  

## 機能
### JPEG画像表示
micro SDカード、または、USBメモリに書き込まれたJPEGファイルを表示します。画像は出力するディスプレイの画像解像度にあわせて拡大/縮小されます。表示可能なJPEGファイルは以下の通りです。  

|            |                                                       |
|:-----------|:------------------------------------------------------|
|ファイル位置|フォルダの深さはルートフォルダを含め8階層まで。        |
|ファイル名  |半角英数字 (全角には対応していません)                  |
|拡張子      |".jpg" , ".JPG"                                        |
|解像度制限  |上限1280 x 720 ピクセル。MCU単位のサイズ。             |
|サイズ上限  |450Kbyte                                               |
|フォーマット|JPEGベースラインに準拠 (最適化、プログレッシブは非対応)|

JPEGファイルの表示順はインデックスファイル ``index.txt`` にて指定します。表示したい順にファイル名を記載し、micro SDカード、または、USBメモリのルートディレクトリに置いてください。  

```txt
image001.jpg
image002.jpg
image003.jpg
```
#### PowerPointを使ったJPEGファイル作成
PowerPointで作成した資料を「名前を付けて保存」を選び、「ファイルの種類」に「JPEGファイル交換形式」を指定すると本サンプルで使用可能なJPEGファイルとして保存することができます。  

#### JPEGベースラインに準拠した形式に変換する方法
このサンプルでは、JPEGベースラインに準拠したJPEGファイルのみ表示可能です。  
表示できないJPEGファイルがある場合は以下の手順をお試しください。  
1. 画像編集ツールGIMPをインストールする  
2. GIMPで画像ファイルを開く  
  画像ファイル上で右クリック  
  Edit with GIMPを選択  
3. 画像サイズを変更する  
  「画像」→「画像の拡大・縮小」を選んで、画像の拡大・縮小ダイアログをから変更する  
4. JPEG形式で保存  
  「ファイル」→「名前をつけてエクスポート」で保存ダイアログを表示  
  保存先フォルダを選択 (ファイルの拡張子は.jpg)  
  「エクスポート」をクリックし、エクスポートダイアログを表示  
  「＋詳細設定」の「＋」部分をクリックして詳細設定画面を表示  
  「最適化」と「プログレッシブ」のチェックを外す  
  エクスポートボタンを押す (注意：出力サイズが450Kbyteを超える場合は、品質値を調整して下さい。)  

### テロップ表示
プロジェクト内「\docs\create_telop_data.zip」を使用することでテロップ用データを作成することができます。以下にテロップデータの作成手順を示します。  

1. テロップとして表示したい内容をPNGファイルで作成。
2. テロップ画像は時計回りに90°回転させた状態で保存。ファイル名は「telop.png」。  
![テロップ作成例](docs/img/telop_example.png)  
3. 「create_telop_data.zip」を展開し、「create_telop_data\Images」ディレクトリに作成した「telop.png」を置く。  
4. 「BinaryImageMakeRGAH.bat」を実行すると「create_telop_data\Images」ディレクトリに「telop.bin」が作成される。  
5. 「telop.bin」をmicro SDカード、または、USBメモリのルートディレクトリに保存する。

### タッチパネル操作
LCD上のタッチパネルにて、以下の操作が可能です。(lcd-type = GR_PEACH_DISPLAY_SHIELDの場合は不可)  

|操作        |動作                                                       |
|:-----------|:----------------------------------------------------------|
|左フリック  |次のJPEGファイルを表示します。                             |
|右フリック  |一つ前のJPEGファイルを表示します。                         |
|ピンチアウト|画面を拡大します。                                         |
|ピンチイン  |拡大した画面を縮小します。                                 |
|ダブルタップ|拡大中に、素早く画面を2回タップすると元のサイズに戻します。|

### マウス操作
USBマウスを接続することで、以下の操作が可能です。  

|操作            |動作                                                   |
|:---------------|:------------------------------------------------------|
|左クリック      |次のJPEGファイルを表示します。                         |
|右クリック      |一つ前のJPEGファイルを表示します。                     |
|センタークリック|マウスポインタ表示。マウスポインタ表示中は時間経過による次ファイル遷移は行いません。             |
|センターホイール|マウスポインタ表示中：画面の拡大/縮小。マウスポインタ非表示中：JPEGファイルの表示する時間を変更。|

### 顔検出、年齢推定、性別推定、表情推定
オムロン社製 HVC-P2を接続することで、顔検出、年齢推定、性別推定、表情推定が行えます。  
HVC-P2 (Human Vision Components B5T-007001)は、人を認識する画像センシングコンポです。OKAO Vision の10種類の画像センシング機能とカメラモジュールをコンパクトに一体化した組み込み型のモジュールです。詳しくは下記リンクを参照ください。

・[SENSING EGG PROJECT > HVC-P2](https://plus-sensing.omron.co.jp/egg-project/product/hvc-p2/)  
・[HVC-P2 (Human Vision Components B5T-007001)](http://www.omron.co.jp/ecb/products/mobile/hvc_p2/)  
・[OKAO Vision](http://plus-sensing.omron.co.jp/technology/)  

本サンプルの HVCApi フォルダ以下は、下記リンク先 Sample Code「SampleCode_rev.2.0.2」のコードを使用しています。(ページ中程「製品関連情報」->「サンプルコード」からダウンロード出来ます)  
http://www.omron.co.jp/ecb/products/mobile/hvc_p2/  

Mbedサイトにて、HVC-P2単体のサンプルプログラムも公開しています。接続方法についてはこちらを参照ください。  
・[GR-PEACH_HVC-P2_sample](https://os.mbed.com/teams/Renesas/code/GR-PEACH_HVC-P2_sample/)  

本サンプルプログラムでは、使用例として顔を検出すると、特定のコンテンツを表示します。  
また、``setting.txt`` に `DISP_IMAGE=1` を設定するとディスプレイ上に顔検出結果を表示することができます。詳細は「設定ファイル」参照ください。  

### 設定ファイル
micro SDカード、または、USBメモリのルートディレクトリに``setting.txt`` を置くことで、サンプルプログラムの様々な設定を変更することができます。  
``setting.txt``の書き方についてはプロジェクト内「\docs\About the setting file.xlsx」を参照ください。

### NTP設定
``mbed_app.json``ファイルの``ntp-connect``を"1"にすることでNTPサーバから時間を取得できるようになります。時間を取得すると、コンテンツ表示のタイムテーブルが使用できるようになります。  
```json
        "ntp-connect":{
            "help": "0:disable 1:enable",
            "value": "0"
        },
        "ntp-address": {
            "help": "NTP SERVER ADDRESS (Required only for ntp-connect=1)",
            "value": "\"2.pool.ntp.org\""
        },
        "ntp-port": {
            "help": "NTP SERVER PORT (Required only for ntp-connect=1)",
            "value": "123"
        },
```

タイムテーブルは``timetable.txt``で設定します。``timetable.txt``は、時間とインデックスファイル(ファイル名は任意)の組み合わせで記載します。下記の例では、12:00から13:00まではindex_1.txtにて指定されたコンテンツを表示します。13:00から翌日12:00まではindex_2.txtで指定されたコンテンツを表示します。(10:00にGR-PEACHを起動した場合はindex_2.txtのコンテンツを表示します)  

```txt
12:00 index_1.txt
13:00 index_2.txt
```

### FTP設定
``mbed_app.json``ファイルの``ftp-connect``を"1"にすることでFTPサーバからコンテンツデータをアップデートできるようになります。コンテンツのアップデートはGR-PEACHのリセットスタート時に行われます。  
```json
        "ftp-connect":{
            "help": "0:disable 1:enable",
            "value": "0"
        },
        "ftp-ip-address": {
            "help": "FTP IP ADDRESS (Required only for ftp-connect=1)",
            "value": "\"192.168.0.1\""
        },
        "ftp-port": {
            "help": "FTP PORT (Required only for ftp-connect=1)",
            "value": "21"
        },
        "ftp-user": {
            "help": "FTP User Name (Required only for ftp-connect=1)",
            "value": "\"User\""
        },
        "ftp-password": {
            "help": "FTP Password (Required only for ftp-connect=1)",
            "value": "\"Password\""
        },
        "ftp-directory": {
            "help": "FTP Directory (Required only for ftp-connect=1)",
            "value": "\"signage/image\""
        },
```

### ディスプレイ設定
``mbed_app.json``ファイルの``lcd-type``にて、使用するLCDパネルを選択することができます。
```json
      "lcd-type":{
          "help": "Options are GR_PEACH_4_3INCH_SHIELD, GR_PEACH_7_1INCH_SHIELD, GR_PEACH_RSK_TFT, GR_PEACH_DISPLAY_SHIELD, GR_LYCHEE_LCD",
          "value": "GR_PEACH_DISPLAY_SHIELD"
      },
```

| lcd-type "value"        | 説明                               |
|:------------------------|:-----------------------------------|
| GR_PEACH_4_3INCH_SHIELD | GR-PEACH 4.3インチLCDシールド      |
| GR_PEACH_7_1INCH_SHIELD | GR-PEACH 7.1インチLCDシールド      |
| GR_PEACH_RSK_TFT        | GR-PEACH RSKボード用LCD            |
| GR_PEACH_DISPLAY_SHIELD | GR-PEACH Display Shield            |
