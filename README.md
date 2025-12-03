# Xiao ESP32S3 Sense Camera Firmware

Xiao ESP32S3 Sense用のカメラファームウェア。WiFiストリーミング、マイク、SDカード対応。

## なぜCameraWebServerのExampleが動かないのか

ArduinoのCameraWebServerサンプルは複雑で、Xiao ESP32S3 Senseでは動かないことが多い。主な原因：

1. **ピン設定が違う** - ESP32-CAMやM5Stack用の設定になってる
2. **PSRAMを前提にしてる** - Xiaoは設定によってはPSRAM無効
3. **AsyncWebServerが複雑** - 依存関係でトラブりやすい

## このファームウェアの特徴

- **シンプルなWebServer.h使用** - 依存関係少ない
- **PSRAM不要** - `CAMERA_FB_IN_DRAM`でDRAMのみ使用
- **Xiao専用ピン設定** - コピペで動く
- **複数エンドポイント** - カメラ+マイク+SD統合

## エンドポイント

| パス | 機能 |
|------|------|
| `/` | ステータス画面（カメラプレビュー付き） |
| `/stream` | MJPEGストリーム |
| `/capture` | 静止画撮影（JPEG） |
| `/audio` | マイク音量（JSON） |
| `/audio_raw` | 生PCMデータ（1秒分16kHz） |
| `/sd` | SDカード情報 |
| `/save` | 画像をSDに保存 |

## 使い方

### 1. Arduino IDE設定

- **ボード**: Xiao ESP32S3
- **PSRAM**: Disabled（または OPI PSRAM）
- **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)

### 2. WiFi設定を変更

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 3. 書き込み＆確認

シリアルモニタでIPアドレスを確認、ブラウザでアクセス。

## ファームウェアバリエーション

- `firmware/bedroom/` - 基本版（vflip有効）
- `firmware/kitchen/` - 静的IP版（10.0.72.203）
- `firmware/living/` - 静的IP版（10.0.72.204）

## トラブルシューティング

### カメラが初期化できない
- ボード設定でPSRAMをDisabledにしてみる
- `FRAMESIZE_QVGA`より小さくしてみる

### WiFiに繋がらない
- SSIDとパスワードを確認
- 2.4GHz帯であることを確認（5GHzは非対応）

### 画像が上下逆
- `s->set_vflip(s, 1);` を追加（または削除）

## ライセンス

MIT

---
Created by Tsubasa (@tsubasa_rsrch)
