# 🛠️ Xiaomi Notification Fix Tool

Công cụ Windows mã nguồn mở hỗ trợ khắc phục tình trạng **trễ hoặc mất thông báo** trên Xiaomi / Redmi / POCO, đặc biệt với các máy sử dụng **ROM nội địa Trung Quốc**.

> Không cần root. Tool giao tiếp với điện thoại thông qua ADB và thay đổi một số cấu hình AppOps, Doze, Standby và dữ liệu nền.

## ✨ Tính năng

- 📱 Tự kiểm tra kết nối thiết bị Android qua ADB.
- 🔍 Quét ứng dụng đang có trên thiết bị.
- 📋 Đọc trạng thái của từng ứng dụng.
- 🚀 Hỗ trợ AppOps `10053` và `10008` cho cơ chế tự khởi động.
- 🔔 Cho phép `POST_NOTIFICATION` ở cấp package và UID.
- 🔋 Thêm ứng dụng vào **Doze whitelist** để giảm giới hạn chạy nền.
- ⚡ Đặt **Standby Bucket** của ứng dụng về `active`.
- 🌐 Có tùy chọn thêm UID vào whitelist dữ liệu nền.
- 💬 Chọn nhanh các ứng dụng chat / mạng xã hội.
- 💳 Chọn nhanh các ứng dụng ví điện tử / ngân hàng.
- 🔎 Tìm kiếm ứng dụng ngay trong danh sách.
- ☑️ Fix từng ứng dụng hoặc toàn bộ ứng dụng đã quét.
- 💾 Sao lưu trạng thái trước khi thay đổi.
- ↩️ Hoàn tác về trạng thái đã sao lưu.
- 🔄 Làm mới Xiaomi Security Center sau khi thay đổi.
- 📦 ADB được tích hợp trong file thực thi, không yêu cầu người dùng cài platform-tools riêng.
- 🖥️ Giao diện native C++ cho Windows.

## 📱 Yêu cầu

### Máy tính

- Windows 10 / 11
- Cáp USB hỗ trợ truyền dữ liệu
- Quyền sử dụng USB Debugging

### Điện thoại

Bật:

**Cài đặt → Tùy chọn nhà phát triển → Gỡ lỗi USB**

Khi kết nối lần đầu, chọn **Cho phép gỡ lỗi USB** trên điện thoại.

## 🚀 Cách sử dụng

1. Kết nối điện thoại với máy tính.
2. Bật USB Debugging.
3. Mở `ToolFixThongBaoXiaomi.exe`.
4. Kiểm tra kết nối ADB.
5. Nhấn **QUÉT & ĐỌC TRẠNG THÁI**.
6. Chọn ứng dụng cần xử lý.
7. Nhấn **BẮT ĐẦU FIX THÔNG BÁO**.
8. Có thể khởi động lại điện thoại sau khi hoàn tất để hệ thống áp dụng ổn định hơn.

### Chế độ chọn ứng dụng

Có thể:

- Chọn từng ứng dụng.
- Chọn toàn bộ ứng dụng đã quét.
- Chọn nhanh app chat.
- Chọn nhanh app ví / ngân hàng.
- Tìm kiếm theo tên package.

## ⚠️ Tùy chọn toàn hệ thống

Tool có một số tùy chọn áp dụng cho **toàn bộ máy**:

- Tắt App Standby.
- Tắt cơ chế đóng băng ứng dụng nền.
- Giữ dữ liệu di động luôn hoạt động.

Các tùy chọn này có thể giúp ứng dụng chạy nền ổn định hơn nhưng **có thể làm tăng mức tiêu thụ pin**.

Chúng không được bật mặc định.

## 💾 Sao lưu & hoàn tác

Tool hỗ trợ lưu trạng thái trước khi thay đổi.

Trong chế độ **HOÀN TÁC**, tool có thể khôi phục trạng thái từ bản sao lưu.

File sao lưu được lưu tại:

```text
%APPDATA%\XiaomiNotifRevert\trang-thai-goc.txt
```

Nên sử dụng **SAO LƯU GỐC** trước khi thử các thay đổi lớn.

## 🔌 ADB

Tool có cơ chế tìm ADB theo nhiều nguồn:

1. ADB được chọn thủ công.
2. `platform-tools\adb.exe` cạnh tool.
3. ADB được nhúng trong file EXE và giải nén tạm.
4. Một số vị trí Android SDK / platform-tools phổ biến trên Windows.

ADB nhúng được giải nén vào:

```text
%LOCALAPPDATA%\XiaomiNotifTool
```

## 🧑‍💻 Build từ source

Project được viết bằng **C++17** và sử dụng **MinGW-w64**.

Cấu trúc:

```text
XiaomiNotifTool-CPP/
├── assets/
│   ├── adb/
│   │   ├── adb.exe
│   │   ├── AdbWinApi.dll
│   │   └── AdbWinUsbApi.dll
│   ├── app.ico
│   └── bg.png
├── src/
│   ├── adb.cpp
│   ├── adb.h
│   ├── core.cpp
│   ├── core.h
│   ├── main.cpp
│   ├── obf.h
│   └── res.h
├── build.ps1
└── resources.rc
```

### Build

Cài MinGW-w64 có `g++` và `windres`, sau đó chạy:

```powershell
.\build.ps1
```

File build sẽ nằm trong:

```text
build\ToolFixThongBaoXiaomi.exe
```

### Lưu ý về `build.ps1`

File `build.ps1` hiện đang chứa đường dẫn MinGW cụ thể của máy phát triển.

Nếu máy của bạn sử dụng đường dẫn MinGW khác, hãy sửa biến:

```powershell
$BIN = "ĐƯỜNG_DẪN_TỚI\mingw64\bin"
```

Sau đó chạy lại:

```powershell
.\build.ps1
```

## 🧩 Công nghệ

- C++17
- Win32 API
- MinGW-w64
- Windows Resource Compiler (`windres`)
- Android Debug Bridge (ADB)
- Android AppOps
- Doze / DeviceIdle
- Standby Bucket
- Windows native GUI

## 🔐 Quyền & an toàn

Tool **không yêu cầu root**.

Tool không cần cài đặt thêm chương trình vào điện thoại. Các thay đổi được thực hiện thông qua ADB với quyền mà Android Debugging cho phép.

Tuy nhiên, vì tool thay đổi các chính sách chạy nền của Android/Xiaomi, kết quả có thể khác nhau tùy:

- Phiên bản Android.
- Phiên bản HyperOS / MIUI.
- ROM khu vực.
- Ứng dụng.
- Chính sách riêng của từng thiết bị.

Không có gì đảm bảo mọi ứng dụng sẽ hoạt động giống hoàn toàn ROM quốc tế.

## 🤝 Đóng góp

Project được phát hành dưới dạng **mã nguồn mở**.

Bạn có thể:

- Đọc source.
- Kiểm tra cách tool hoạt động.
- Báo lỗi.
- Đề xuất cải tiến.
- Tạo Pull Request.
- Tự build từ source.

Nếu phát hiện lỗi hoặc vấn đề tương thích, hãy mở **Issue** và cung cấp:

```text
Model:
Android:
HyperOS / MIUI:
Ứng dụng bị lỗi:
Mô tả:
Log / ảnh lỗi:
```

## 📦 Release

File EXE dành cho người dùng cuối được phát hành tại GitHub Releases.

**BETA v1.3.0:**

https://github.com/DatCuCaiTv/FixTBXiaomiChina/releases/tag/v1.3.0_beta

## ❤️ Tác giả

**Phan Tấn Đạt**

Nếu tool hữu ích, bạn có thể ủng hộ tác giả:

```text
MB Bank
STK: 0982414903
Chủ tài khoản: Phan Tấn Đạt
```

---

### 📄 License

Source code được công khai để cộng đồng có thể xem và đóng góp.

Nếu bạn muốn sử dụng, sửa đổi hoặc phân phối lại project, vui lòng kiểm tra file `LICENSE` của repository để biết các điều khoản áp dụng.
