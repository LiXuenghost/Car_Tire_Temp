# import serial
# import time

# mlx90640To = []
# count = 0

# try:
#     # 打開串口，例如 COM3，波特率 115200
#     ser = serial.Serial('COM7', 115200, timeout=1)
    
#     # 等待串口初始化
#     time.sleep(2) 
    
#     print("正在讀取串口數據...")
#     while True:
#         if ser.in_waiting > 0:  # 檢查是否有數據傳入
#             data = ser.readline().decode('utf-8').rstrip() # 讀取一行並解碼
#             if(data == '0'):
#                 data = ser.readline().decode('utf-8').rstrip()
#                 while(data != '1'): 
#                     mlx90640To.append(list(map(int, data.split())))
#                     print(f"{data}")
#                     data = ser.readline().decode('utf-8').rstrip()
#                 mlx90640To.clear()
#             # print("\n")
#             # for i in range(len(mlx90640To)):
                
            
            
# except Exception as e:
#     print(f"錯誤: {e}")
# finally:
#     if 'ser' in locals() and ser.is_open:
#         ser.close()
#         print("串口已關閉")


import serial
import time
import numpy as np
import cv2

# =========================
# MLX90640 設定
# =========================

ROWS = 24
COLS = 32

# 溫度範圍
TEMP_MIN = 0
TEMP_MAX = 100

# 視窗大小
DISPLAY_WIDTH = 640
DISPLAY_HEIGHT = 480

# color bar 大小
BAR_WIDTH = 80

# =========================
# 開啟 Serial
# =========================

ser = serial.Serial(
    'COM7',
    115200,
    timeout=1
)

time.sleep(2)

print("Serial Connected")

# =========================
# 建立 Color Bar
# =========================

def create_color_bar():

    # 建立 0~255 漸層
    gradient = np.linspace(
        255,
        0,
        DISPLAY_HEIGHT
    ).astype(np.uint8)

    gradient = np.tile(
        gradient.reshape(DISPLAY_HEIGHT, 1),
        (1, BAR_WIDTH)
    )

    # 套用熱像 colormap
    color_bar = cv2.applyColorMap(
        gradient,
        cv2.COLORMAP_INFERNO
    )

    # 加上溫度文字
    for i in range(0, 101, 10):

        y = int(DISPLAY_HEIGHT - (i / 100) * DISPLAY_HEIGHT)

        cv2.putText(
            color_bar,
            f"{i}",
            (5, y),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (255, 255, 255),
            1
        )

    return color_bar

# 固定 color bar
color_bar = create_color_bar()

# =========================
# Main Loop
# =========================

mlx90640To = []

while True:

    try:

        # =========================
        # 等待 frame start
        # =========================

        while True:

            data = ser.readline().decode(
                'utf-8',
                errors='ignore'
            ).strip()

            print("RAW:", data)

            if data == '0':
                print("Frame Start")
                break

        mlx90640To.clear()

        # =========================
        # 接收 frame
        # =========================

        while True:

            data = ser.readline().decode(
                'utf-8',
                errors='ignore'
            ).strip()

            print(data)

            # frame end
            if data == '1':
                print("Frame End")
                break

            try:

                row = list(map(float, data.split()))

                if len(row) == COLS:
                    mlx90640To.append(row)

            except Exception as e:

                print("Parse Error:", e)

        # =========================
        # 完整 frame
        # =========================

        if len(mlx90640To) == ROWS:

            temp_array = np.array(
                mlx90640To,
                dtype=np.float32
            )

            # =========================
            # 顯示最高溫
            # =========================

            max_temp = np.max(temp_array)

            y, x = np.unravel_index(
                np.argmax(temp_array),
                temp_array.shape
            )

            print(f"Max Temp: {max_temp:.1f}")

            # =========================
            # 固定 0~100 normalize
            # =========================

            normalized = (
                (temp_array - TEMP_MIN)
                / (TEMP_MAX - TEMP_MIN)
                * 255
            )

            normalized = np.clip(
                normalized,
                0,
                255
            ).astype(np.uint8)

            # =========================
            # 套用熱像色彩
            # =========================

            thermal = cv2.applyColorMap(
                normalized,
                cv2.COLORMAP_INFERNO
            )

            # =========================
            # 放大畫面
            # =========================

            thermal = cv2.resize(
                thermal,
                (DISPLAY_WIDTH, DISPLAY_HEIGHT),
                interpolation=cv2.INTER_CUBIC
            )

            # =========================
            # 顯示最高溫文字
            # =========================

            cv2.putText(
                thermal,
                f"Max Temp: {max_temp:.1f} C",
                (10, 35),
                cv2.FONT_HERSHEY_SIMPLEX,
                1,
                (255, 255, 255),
                2
            )

            # =========================
            # 顯示 Hot Spot
            # =========================

            hotspot_x = int(
                x * DISPLAY_WIDTH / COLS
            )

            hotspot_y = int(
                y * DISPLAY_HEIGHT / ROWS
            )

            cv2.circle(
                thermal,
                (hotspot_x, hotspot_y),
                10,
                (255, 255, 255),
                2
            )

            # =========================
            # 合併 color bar
            # =========================

            final_display = np.hstack(
                (thermal, color_bar)
            )

            # =========================
            # 顯示 GUI
            # =========================

            cv2.imshow(
                "MLX90640 Thermal Camera",
                final_display
            )

            key = cv2.waitKey(1)

            # ESC 離開
            if key == 27:
                break

    except KeyboardInterrupt:

        break

    except Exception as e:

        print("Error:", e)

# =========================
# Cleanup
# =========================

ser.close()

cv2.destroyAllWindows()

print("Program Closed")