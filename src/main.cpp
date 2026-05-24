#include <M5Unified.h>
#include <FastLED.h>
#include <WiFi.h>

// 設定
#define PIR_PIN 36          // M5StickC Plus2 HATコネクタ (PIR Hat)
#define LED_PIN 32          // Grove Port A (RGB LED Unit)
#define NUM_LEDS 37         // M5Stack RGB LED Board (Hex)
#define LED_BRIGHTNESS 170  // 明るさ設定 (最大255)

// 定数
const unsigned long KEEP_ALIVE_MS = 10000;    // 点灯時間 (10秒)
const int FADE_DURATION_MS = 500;             // フェードイン・アウトの時間
const unsigned long SCREEN_DURATION_MS = 15000; // 画面表示時間 (15秒)

// グローバルオブジェクト
CRGB leds[NUM_LEDS];
unsigned long lastTriggerTime = 0;
unsigned long screenOnStartTime = 0;
bool screenIsActive = false;
bool ledsCleared = true; // LED消灯済みフラグ

enum ColorMode {
    FIRE,
    SOUL_FIRE
};
ColorMode currentMode = FIRE;

// 前回の炎エフェクト更新時間
unsigned long lastFlameUpdate = 0;

// ダブルバッファ（チラつき防止）用のキャンバス
M5Canvas canvas(&M5.Display);

// コンパイル時刻からRTCを自動設定する関数
void setRtcFromCompileTime() {
    char monthStr[4];
    int day, year, hour, minute, second;
    
    if (sscanf(__DATE__, "%s %d %d", monthStr, &day, &year) == 3 &&
        sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) == 3) {
        
        int month = 1;
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
            if (strcmp(monthStr, months[i]) == 0) {
                month = i + 1;
                break;
            }
        }
        
        auto currentDt = M5.Rtc.getDateTime();
        
        // RTCの時刻がコンパイル時刻よりも古い場合のみ同期する
        bool timeNeedsSync = false;
        if (currentDt.date.year < year) {
            timeNeedsSync = true;
        } else if (currentDt.date.year == year) {
            if (currentDt.date.month < month) {
                timeNeedsSync = true;
            } else if (currentDt.date.month == month) {
                if (currentDt.date.date < day) {
                    timeNeedsSync = true;
                } else if (currentDt.date.date == day) {
                    if (currentDt.time.hours < hour) {
                        timeNeedsSync = true;
                    } else if (currentDt.time.hours == hour) {
                        if (currentDt.time.minutes < minute) {
                            timeNeedsSync = true;
                        }
                    }
                }
            }
        }
        
        if (timeNeedsSync) {
            M5.Rtc.setDateTime({ { year, month, day }, { hour, minute, second } });
        }
    }
}

// 動作時間内 (JST 21:00 〜 翌朝 5:00) か判定する関数
bool isWithinActiveHours(int hour) {
    return (hour >= 21 || hour < 5);
}
bool isWithinActiveHours() {
    auto dt = M5.Rtc.getDateTime();
    return isWithinActiveHours(dt.time.hours);
}

// 炎エフェクトの計算と表示
void drawFlameEffect(uint8_t currentBri) {
    uint32_t t = millis() / 3;
    
    for(int i = 0; i < NUM_LEDS; i++) {
        uint8_t noiseVal = inoise8(i * 30, t); 
        
        // ノイズを明るさに変換 (80〜255)
        uint8_t bri = map(noiseVal, 0, 255, 80, 255);
        
        // ノイズを彩度に変換 (180〜255)
        uint8_t sat = map(noiseVal, 0, 255, 255, 180);

        uint8_t hue;
        if (currentMode == SOUL_FIRE) {
            hue = map(noiseVal, 0, 255, 120, 150); // 青〜水色
        } else {
            hue = map(noiseVal, 0, 255, 10, 30);  // 温かいオレンジ〜黄色
        }

        leds[i] = CHSV(hue, sat, bri);
    }
    FastLED.setBrightness(currentBri);
    FastLED.show();
}

// 日本語ステータス確認画面の描画 (M5Canvas を使用してチラつきを完全に排除)
void drawDashboard(const m5::rtc_datetime_t& dt, bool activeHours) {
    // オフスクリーンバッファ（メモリ内）をクリア
    canvas.fillSprite(BLACK);
    
    // 日付 (左上)
    canvas.setFont(&fonts::lgfxJapanGothic_12);
    canvas.setTextColor(LIGHTGREY, BLACK);
    canvas.printf("%04d/%02d/%02d", dt.date.year, dt.date.month, dt.date.date);
    
    // 電源ステータス (右上)
    char powerStr[32];
    if (M5.Power.isCharging() || M5.Power.getBatteryVoltage() > 4.5) {
        sprintf(powerStr, "USB給電中");
    } else {
        int batLevel = M5.Power.getBatteryLevel();
        sprintf(powerStr, "電池:%d%%", batLevel);
    }
    int powerWidth = canvas.textWidth(powerStr);
    canvas.setTextColor(WHITE, BLACK);
    canvas.drawString(powerStr, 230 - powerWidth, 2);
    
    // 区切り線
    canvas.drawFastHLine(0, 18, 240, DARKGREY);
    
    // 時刻表示 (中央に大きく)
    char timeStr[16];
    sprintf(timeStr, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
    canvas.setFont(&fonts::lgfxJapanGothic_32);
    int timeWidth = canvas.textWidth(timeStr);
    canvas.setTextColor(WHITE, BLACK);
    canvas.drawString(timeStr, (240 - timeWidth) / 2, 28);
    
    // 区切り線
    canvas.drawFastHLine(0, 72, 240, DARKGREY);
    
    // 現在のモード表示
    canvas.setFont(&fonts::lgfxJapanGothic_16);
    if (currentMode == SOUL_FIRE) {
        canvas.setTextColor(CYAN, BLACK);
        canvas.drawString("モード: 青い炎 (魂)", 10, 78);
    } else {
        canvas.setTextColor(ORANGE, BLACK);
        canvas.drawString("モード: 通常の炎 (暖色)", 10, 78);
    }
    
    // 動作設定時間
    canvas.setFont(&fonts::lgfxJapanGothic_12);
    canvas.setTextColor(LIGHTGREY, BLACK);
    canvas.drawString("動作設定: JST 21:00 - 05:00", 10, 102);
    
    // 判定ステータス
    canvas.setFont(&fonts::lgfxJapanGothic_16);
    if (activeHours) {
        canvas.setTextColor(GREEN, BLACK);
        canvas.drawString("判定: 時間内 (点灯ON)", 10, 116);
    } else {
        canvas.setTextColor(RED, BLACK);
        canvas.drawString("判定: 時間外 (スリープ)", 10, 116);
    }
    
    // 完成した描画を一瞬で画面に出力（フリッカーが完全に無くなります）
    canvas.pushSprite(0, 0);
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false; // スピーカー無効
    cfg.serial_baudrate = 0;  // シリアル通信無効
    M5.begin(cfg);

    // Wi-Fi / Bluetooth を無効化して省電力化
    WiFi.mode(WIFI_OFF);
    btStop();

    // Grove / HAT への 5V 給電を有効化
    M5.Power.setExtOutput(true);

    // 初期状態は画面消灯
    M5.Display.setBrightness(0);
    M5.Display.setRotation(1); // 横向き表示
    
    // チラつき防止用キャンバスの初期化 (240x135画素)
    canvas.createSprite(240, 135);

    // ピンモード設定
    pinMode(PIR_PIN, INPUT);

    // FastLED 初期化
    FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(0);
    FastLED.clear();
    FastLED.show();
    
    // コンパイル時刻からRTCを自動補正
    setRtcFromCompileTime();
    
    // 起動要因のチェック
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        M5.update();
        
        // ボタンで起きた場合は、画面表示をアクティブにする
        if (digitalRead(37) == LOW || digitalRead(39) == LOW) {
            screenOnStartTime = millis();
            screenIsActive = true;
            M5.Display.setBrightness(80);
        }
        
        // 動作時間内で、かつPIRがHIGHならLED起動
        if (digitalRead(PIR_PIN) == HIGH && isWithinActiveHours(dt.time.hours)) {
            lastTriggerTime = millis();
        }
    } else {
        // 通常電源オン時は、確認用に15秒画面を表示し、動作時間内ならLEDも点ける
        screenOnStartTime = millis();
        screenIsActive = true;
        M5.Display.setBrightness(80);
        
        if (isWithinActiveHours()) {
            lastTriggerTime = millis();
        }
    }
}

void loop() {
    M5.update();

    auto dt = M5.Rtc.getDateTime();
    bool activeHours = isWithinActiveHours(dt.time.hours);

    // ボタン押下検出
    bool btnAPressed = M5.BtnA.wasPressed() || M5.BtnA.isPressed();
    bool btnBPressed = M5.BtnB.wasPressed() || M5.BtnB.isPressed();

    if (btnAPressed || btnBPressed) {
        screenOnStartTime = millis();
        if (!screenIsActive) {
            screenIsActive = true;
            M5.Display.setBrightness(80);
        }

        // BtnAでモード切替 (動作時間内なら点灯延長)
        if (M5.BtnA.wasPressed()) {
            currentMode = (currentMode == FIRE) ? SOUL_FIRE : FIRE;
            if (activeHours) {
                lastTriggerTime = millis();
            }
        }
    }

    // PIRセンサーの状態確認 (動作時間内のみタイマーリセット)
    if (digitalRead(PIR_PIN) == HIGH) {
        if (activeHours) {
            lastTriggerTime = millis();
        }
    }

    // LED点灯判定 (動作時間内で、タイマー時間内の場合)
    bool ledActive = (millis() - lastTriggerTime < KEEP_ALIVE_MS) && activeHours;
    
    // 画面点灯判定
    bool screenActive = screenIsActive && (millis() - screenOnStartTime < SCREEN_DURATION_MS);

    // 1. LED制御
    if (ledActive) {
        ledsCleared = false; // アクティブなので消灯フラグを下げる
        unsigned long elapsed = millis() - lastTriggerTime;
        uint8_t currentBri = LED_BRIGHTNESS;

        // フェードイン・フェードアウトの処理
        if (elapsed < FADE_DURATION_MS) {
            // フェードイン (0 -> LED_BRIGHTNESS)
            currentBri = map(elapsed, 0, FADE_DURATION_MS, 0, LED_BRIGHTNESS);
        } else if (KEEP_ALIVE_MS - elapsed < FADE_DURATION_MS) {
            // フェードアウト (LED_BRIGHTNESS -> 0)
            currentBri = map(KEEP_ALIVE_MS - elapsed, 0, FADE_DURATION_MS, 0, LED_BRIGHTNESS);
        }

        // 非ブロッキングでの炎エフェクト描画 (20msごと)
        if (millis() - lastFlameUpdate >= 20) {
            lastFlameUpdate = millis();
            drawFlameEffect(currentBri);
        }
    } else {
        // 消灯処理 (フェードアウトし終わっていない場合、ゆっくり消灯)
        if (!ledsCleared) {
            int currentBri = FastLED.getBrightness();
            if (currentBri > 0) {
                currentBri = max(0, currentBri - 15);
                FastLED.setBrightness(currentBri);
                FastLED.show();
                delay(15);
            } else {
                FastLED.clear();
                FastLED.show();
                ledsCleared = true;
            }
        }
    }

    // 2. 画面制御
    if (screenActive) {
        drawDashboard(dt, activeHours);
        delay(50); // 描画更新間隔
    } else {
        if (screenIsActive) {
            M5.Display.setBrightness(0);
            M5.Display.clear();
            screenIsActive = false;
        }
    }

    // 3. スリープ制御 (LEDも画面も非アクティブならスリープへ)
    if (!ledActive && !screenActive) {
        // LEDと画面を確実にクリア
        FastLED.clear();
        FastLED.show();
        M5.Display.setBrightness(0);
        M5.Display.clear();
        screenIsActive = false;
        
        delay(100);

        // センサーへの5V給電は維持
        M5.Power.setExtOutput(true);

        // Wakeup設定
        esp_sleep_enable_gpio_wakeup();
        gpio_wakeup_enable((gpio_num_t)PIR_PIN, GPIO_INTR_HIGH_LEVEL);
        gpio_wakeup_enable((gpio_num_t)37, GPIO_INTR_LOW_LEVEL); // BtnA
        gpio_wakeup_enable((gpio_num_t)39, GPIO_INTR_LOW_LEVEL); // BtnB

        // スリープ実行
        M5.Power.lightSleep();

        // --- 起床後 ---
        gpio_wakeup_disable((gpio_num_t)PIR_PIN);
        gpio_wakeup_disable((gpio_num_t)37);
        gpio_wakeup_disable((gpio_num_t)39);

        M5.update();

        // 起床要因のチェックと処理
        if (digitalRead(37) == LOW || digitalRead(39) == LOW) {
            // ボタン押下で起きた場合
            screenOnStartTime = millis();
            screenIsActive = true;
            M5.Display.wakeup();
            M5.Display.setBrightness(80);
        }

        auto wakeDt = M5.Rtc.getDateTime();
        bool wakeActiveHours = isWithinActiveHours(wakeDt.time.hours);
        if (digitalRead(PIR_PIN) == HIGH && wakeActiveHours) {
            // 動作時間内に動き検知で起きた場合
            lastTriggerTime = millis();
        }
    }
}
