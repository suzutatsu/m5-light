# M5StickC Plus2 PIR & RGB LED Controller

M5StickC Plus2 に接続した PIR（人感）センサーが動きを検知すると、RGB LED が「炎（キャンドル）」のようなエフェクトで点灯するプロジェクトです。
バッテリー駆動での長時間運用を想定し、非検知時は省電力モード（Light Sleep）で動作します。

## 主な機能
- **自動点灯**: PIRセンサー検知で即座に起動し、LEDを点灯。
- **炎エフェクト**: 単調な点滅ではなく、パーリンノイズを使用した自然なゆらぎ表現（赤〜オレンジ〜黄色）。
- **タイマー制御**: 動き検知後、30秒間点灯を維持。動きがあれば延長。
- **省電力動作**: 非動作時は消費電力を抑える Light Sleep モードへ自動移行（センサー給電は維持）。

## ハードウェア構成

| 部品名 | 接続 / 役割 | 備考 |
| -- | -- | -- |
| **M5StickC Plus2** | メインコントローラ | バッテリー(200mAh)内蔵 |
| **M5StickC PIR Hat** | HATコネクタ (GPIO 36) | AS312センサー搭載 |
| **RGB LED Board** | Grove Port A (GPIO 32, 33) | SK6812 / WS2812 (37灯を想定) |

※ ピン配置やLED個数は `src/main.cpp` で変更可能です。

## 使用方法

### 1. 準備
- [VS Code](https://code.visualstudio.com/) と [PlatformIO](https://platformio.org/) 拡張機能をインストールしてください。
- Macの場合は `brew install platformio` でCLIツールを入れると便利です。

### 2. 書き込み
デバイスをUSB接続し、以下のコマンドで書き込みます。

```bash
# ビルドとアップロード
pio run -t upload
```

### 3. LED設定の調整
`src/main.cpp` の以下の定数を変更することで挙動を調整できます。

```cpp
#define NUM_LEDS 37         // LEDの個数 (Hexボードは37個)
#define LED_BRIGHTNESS 150  // 明るさ (0〜255)
const unsigned long KEEP_ALIVE_MS = 30000; // 点灯時間 (ミリ秒)
```

## 省電力運用について
本プログラムは **Light Sleep** を使用しています。
これは Deep Sleep よりも若干消費電力は高いですが、PIR Hat への 3.3V 給電能力を維持し、センサー検知による「ウェイクアップ」を確実にするためです。

- **動作時**: 炎エフェクト表示 (高負荷)
- **待機時**: 画面OFF + CPU停止 + センサー監視のみ (中負荷)

## 謝辞
このプロジェクトの開発は、Google Antigravityの支援を受けて行われました。
