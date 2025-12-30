#include <M5Unified.h>
#include <FastLED.h>

// 設定
#define PIR_PIN 36          // M5StickC Plus2 HATコネクタ (PIR Hat)
#define LED_PIN 32          // Grove Port A (RGB LED Unit)
#define NUM_LEDS 37         // M5Stack RGB LED Board (Hex)
#define LED_BRIGHTNESS 150  // 明るさ設定 (最大255)

// 定数
const unsigned long KEEP_ALIVE_MS = 30000; // 点灯時間 (30秒)

// グローバルオブジェクト
CRGB leds[NUM_LEDS];
unsigned long lastTriggerTime = 0;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    // Grove/HATへの5V出力(ExtOutput)を有効化
    M5.Power.setExtOutput(true);

    // ピンモード設定
    pinMode(PIR_PIN, INPUT);

    // FastLEDの初期化
    FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    M5.Display.setTextSize(2);
    
    // 起動要因のチェック
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        // PIRによる起動
        M5.Display.println("Wakeup: PIR");
        lastTriggerTime = millis();
    } else {
        // 電源ONによる起動
        M5.Display.println("Power ON");
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
        M5.Display.setCursor(0, 40);
        M5.Display.print("Active...");

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

        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 40);
        M5.Display.println("Light Sleep...");
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

        M5.Display.wakeup();
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.println("Wakeup!");
        lastTriggerTime = millis();
    }
}
