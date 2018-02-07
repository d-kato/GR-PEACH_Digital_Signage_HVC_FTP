# GR-PEACH_Digital_Signage_HVC_FTP
[GR-PEACH_Digital_Signage](https://os.mbed.com/users/dkato/code/GR-PEACH_Digital_Signage/) に、オムロン社製 HVC-P2 を接続できるようにしたサンプルプログラムです。HVC-P2を接続することにより、顔検出、年齢推定、性別推定、表情推定が行えるようになります。HVC-P2単体のサンプルプログラムは[こちら](https://os.mbed.com/teams/Renesas/code/GR-PEACH_HVC-P2_sample/)を参照ください。  

## 開発環境
GCC環境でのみ動作します。開発環境については、[GR-PEACHオフライン開発環境の手順](https://os.mbed.com/users/1050186/notebook/offline-development-langja/)を参照ください。

## 概要
micro SDカード、または、USBメモリに書き込まれたJPEGファイルをGR-PEACHのハードウエアでデコードしています。表示画面はタッチパネルやUSBマウスで操作することができます。  
一定時間表示(時間の変更可)すると次のJPEGファイルを表示します。JPEGファイルはディスプレイに出力する画像解像度にあわせて拡大/縮小されます。そのため、JPEGファイルはディスプレイ出力に近い解像度のほうが鮮明に表示されます。  

## 構成
GR-PEACH、micro SDカード または USBメモリ、USBマウス(マウス操作を行わない場合は不要)、USB HUB(USBメモリとUSBマウスを両方使用する場合)、LCD Shield、HVC-P2(顔検出を行わない場合は不要)。
USBを使用する際はJP3をショートする必要があります。Jumper位置については[こちら](
https://os.mbed.com/teams/Renesas/wiki/Jumper-settings-of-GR-PEACH)を参照ください。  

### 表示可能なJPEGファイル

|            |                                                       |
|:-----------|:------------------------------------------------------|
|ファイル位置|フォルダの深さはルートフォルダを含め8階層まで。        |
|ファイル名  |半角英数字 (全角には対応していません)                  |
|拡張子      |".jpg" , ".JPG"                                        |
|解像度制限  |上限1280 x 800 ピクセル。MCU単位のサイズ。             |
|サイズ上限  |450Kbyte                                               |
|フォーマット|JPEGベースラインに準拠 (最適化、プログレッシブは非対応)|

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

### HVC-P2による顔検出
HVC-P2 (Human Vision Components B5T-007001)は、人を認識する画像センシングコンポです。OKAO Vision の10種類の画像センシング機能とカメラモジュールをコンパクトに一体化した組み込み型のモジュールです。  
詳しくは下記リンクを参照ください。

SENSING EGG PROJECT > HVC-P2  
https://plus-sensing.omron.co.jp/egg-project/product/hvc-p2/  
HVC-P2 (Human Vision Components B5T-007001)  
http://www.omron.co.jp/ecb/products/mobile/hvc_p2/  
OKAO Vision  
http://plus-sensing.omron.co.jp/technology/  
本サンプルの HVCApi フォルダ以下は、下記リンク先 Sample Code「SampleCode_rev.2.0.2」のコードを使用しています。(ページ中程「製品関連情報」->「サンプルコード」からダウンロード出来ます)  
http://www.omron.co.jp/ecb/products/mobile/hvc_p2/  

本サンプルプログラムでは、使用例として顔を検出すると特定のコンテンツを表示します。(詳細は「設定ファイル」参照)  また、LCD画面上に顔検出結果を表示することができます。

### 設定ファイル
micro SDカード、または、USBメモリに``setting.txt`` を置くことで、サンプルプログラムの様々な設定を変更することができます。  
``setting.txt``の書き方についてはプロジェクト内「\docs\About the setting file.xlsx」を参照ください。

## オプション機能
### NTPの設定
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

### FTPの設定
``mbed_app.json``ファイルの``ftp-connect``を"1"にすることでFTPサーバからコンテンツデータをアップデートできるようになります。  
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

### LCDの設定
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
