# Bản Tùy Biến scrcpy (v4.0) - Hỗ Trợ Chụp Màn Hình & Android 16

Bản phân phối này là phiên bản tùy biến của **scrcpy v4.0**, được phát triển để bổ sung hai tính năng quan trọng:
1. **Chụp ảnh màn hình vào Clipboard nhanh bằng phím nóng (`Cmd+C` trên macOS / `Ctrl+C` trên Windows/Linux).**
2. **Tương thích hoàn toàn với Android 16 (Android W - API 36)**, sửa triệt để các lỗi crash liên quan đến `KeyCharacterMap`, `PowerManager` và `DisplayWindowListener` khi khởi động trên các dòng máy/trình giả lập Android mới nhất.
3. **Menu chọn thiết bị tương tác thông minh**: Tự động hiển thị danh sách các thiết bị/trình giả lập khi khởi động, hỗ trợ tự động bật máy ảo (boot emulator) và đợi hệ thống khởi động hoàn tất (`sys.boot_completed`) mới kết nối để tránh lỗi.

---

## 📋 Yêu cầu hệ thống (macOS)
Trước khi chạy phiên bản biên dịch sẵn, bạn cần cài đặt các thư viện đồ họa và âm thanh phụ thuộc thông qua **Homebrew**:

```bash
brew install sdl3 ffmpeg libusb
```

---

## 🚀 Hướng dẫn cài đặt & sử dụng nhanh

1. Tải về tệp nén phát hành: **`scrcpy-macos-v4.0-screenshot.zip`** từ kho lưu trữ này.
2. Giải nén tệp tin. Bạn sẽ nhận được các tệp sau trong thư mục giải nén:
   - `scrcpy` (Kịch bản chạy - launcher script)
   - `scrcpy-bin` (Tệp nhị phân client)
   - `scrcpy-server` (Tệp server chạy trên Android)
   - `scrcpy.png` (Biểu tượng ứng dụng)
3. Mở Terminal tại thư mục vừa giải nén và chạy lệnh:

```bash
./scrcpy
```

---

## ⚙️ Cài đặt để chạy từ bất kỳ đâu (Global Command)

Để có thể mở Terminal ở bất kỳ thư mục nào và gõ lệnh `scrcpy` để sử dụng ngay, bạn làm như sau:

1. Sao chép các tệp đã giải nén vào thư mục lệnh cục bộ của người dùng:
   ```bash
   mkdir -p ~/.local/bin
   cp scrcpy scrcpy-bin scrcpy-server scrcpy.png ~/.local/bin/
   ```

2. Đảm bảo thư mục `~/.local/bin` đã có trong biến môi trường `PATH` của bạn. Mở tệp cấu hình shell (ví dụ: `~/.zshrc` đối với macOS mặc định):
   ```bash
   nano ~/.zshrc
   ```
   Thêm dòng sau vào cuối tệp nếu chưa có:
   ```bash
   export PATH="$HOME/.local/bin:$PATH"
   ```
   Lưu lại và áp dụng cấu hình mới:
   ```bash
   source ~/.zshrc
   ```

3. Từ bây giờ, bạn chỉ cần gõ:
   ```bash
   scrcpy
   ```
   ở bất kỳ đâu để bắt đầu điều khiển thiết bị Android!

---

## 📸 Cách chụp màn hình vào Clipboard máy tính
1. Trong khi đang mở cửa sổ scrcpy để điều khiển điện thoại/máy ảo.
2. Nhấn tổ hợp phím **`Cmd + C`** (trên Mac) hoặc **`Ctrl + C`** (trên Windows/Linux).
3. Ảnh chụp màn hình hiện tại sẽ được lưu trực tiếp vào bộ nhớ tạm (Clipboard) của máy tính. Bạn có thể nhấn `Cmd+V` để dán ảnh vào bất kỳ ứng dụng nào (Messenger, Slack, Photoshop, Google Docs, Word...).
