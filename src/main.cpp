#include <M5Unified.h>
#include <FastLED.h>
#include <WiFi.h>

// 設定
#define PIR_PIN 36          // M5StickC Plus2 HATコネクタ (PIR Hat)
#define LED_PIN 32          // Grove Port A (RGB LED Unit)
#define NUM_LEDS 37         // M5Stack RGB LED Board (Hex)
#define LED_BRIGHTNESS 170  // 明るさ設定 (最大255)

// 定数
const unsigned long KEEP_ALIVE_MS = 10000; // 点灯時間 (10秒)

// グローバルオブジェクト
CRGB leds[NUM_LEDS];
unsigned long lastTriggerTime = 0;

void setup() {
    auto cfg = M5.config();
    // 省電力のため不要なデバイスを無効化
    cfg.internal_spk = false; // スピーカー無効
    cfg.serial_baudrate = 0;  // シリアル通信無効
    M5.begin(cfg);

    // Wi-Fi / Bluetooth を明示的に無効化
    WiFi.mode(WIFI_OFF);
    btStop();

    // Grove/HATへの5V出力(ExtOutput)を有効化
    M5.Power.setExtOutput(true);

    // 画面の輝度を0にして消費電力を削減
    M5.Display.setBrightness(0);

    // ピンモード設定
    pinMode(PIR_PIN, INPUT);

    // FastLEDの初期化
    FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    
    // 起動要因のチェック
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        // PIRによる起動
        lastTriggerTime = millis();
    } else {
        // 電源ONによる起動
        // 初回起動時も少し待機してから動作開始
        lastTriggerTime = millis(); 
    }
}

void loop() {
    M5.update();

    // PIRセンサーの状態確認
    if (digitalRead(PIR_PIN) == HIGH) {
        lastTriggerTime = millis(); // 動きがあればタイマーリセット
    }

    // タイムアウト判定
    if (millis() - lastTriggerTime < KEEP_ALIVE_MS) {
        // 動作中: 炎のようなエフェクトを表示

        uint32_t t = millis() / 3; // 時間ベースの変動値
        
        for(int i = 0; i < NUM_LEDS; i++) {
            // パーリンノイズを使って滑らかなゆらぎを作る
            // i を掛けることでピクセルごとに個体差を出す
            uint8_t noiseVal = inoise8(i * 30, t); 
            
            // ノイズを明るさに変換 (80〜255) - 真っ暗にならないように
            uint8_t bri = map(noiseVal, 0, 255, 80, 255);
            
            // ノイズを色相に変換 (赤〜オレンジ〜黄色)
            uint8_t hue = map(noiseVal, 0, 255, 0, 35);

            leds[i] = CHSV(hue, 255, bri);
        }
        FastLED.show();
        delay(20); // スムーズなアニメーションのための更新頻度
    } else {
        // タイムアウト: Light Sleep (省電力モード) へ移行

        // スリープ前にLEDを消灯
        FastLED.clear();
        FastLED.show();

        delay(100); 

        // Grove/Hat(センサー)への電源供給は維持する
        M5.Power.setExtOutput(true); 

        // GPIOによるWakeup設定
        // 1. GPIOウェイクアップ機能を有効化
        esp_sleep_enable_gpio_wakeup();
        
        // 2. ピン指定とトリガー(HIGH)設定
        gpio_wakeup_enable((gpio_num_t)PIR_PIN, GPIO_INTR_HIGH_LEVEL);

        // Light Sleep へ移行
        // メモリ内容や周辺機器への給電を維持したままCPUを停止
        M5.Power.lightSleep(); 
        
        // --- 起床後 ---
        // ここから処理が再開される
        
        // 誤検知ループを防ぐためWakeupソースを解除
        gpio_wakeup_disable((gpio_num_t)PIR_PIN);

        M5.Display.wakeup(); // 必要に応じて復帰
        lastTriggerTime = millis();
    }
}
