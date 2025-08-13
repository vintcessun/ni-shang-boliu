from aiohttp import web
import os
import datetime
import aiofiles
import numpy as np
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import ttk
import threading

# 确保image文件夹存在
os.makedirs("image", exist_ok=True)

# 全局变量用于图像显示更新
latest_image = None
image_update_event = threading.Event()

# 预计算转换表，对应C语言中的__s_r_1370705v, __s_g_337633u等
__s_r_1370705v = [int(1.370705 * (v - 128)) for v in range(256)]
__s_g_337633u = [int(0.337633 * (u - 128)) for u in range(256)]
__s_g_698001v = [int(0.698001 * (v - 128)) for v in range(256)]
__s_b_1732446u = [int(1.732446 * (u - 128)) for u in range(256)]

def yuyv2rgb565(input_data):
    """将YUV422 (YUYV) 转换为RGB565格式"""
    len_input = len(input_data)
    if len_input % 4 != 0:
        raise ValueError("输入数据长度必须是4的倍数")
    
    output_len = (len_input // 4) * 2 * 2  # 每4字节输入产生2个像素，每个像素2字节
    output_data = bytearray(output_len)
    
    for i in range(len_input // 4):
        # 提取Y0, U, Y1, V分量
        y0 = input_data[i * 4 + 0]
        u = input_data[i * 4 + 1]
        y1 = input_data[i * 4 + 2]
        v = input_data[i * 4 + 3]
        
        # 计算第一个像素的RGB值
        val = y0 + __s_r_1370705v[v]
        r = 0 if val < 0 else 255 if val > 255 else val
        val = y0 - __s_g_337633u[u] - __s_g_698001v[v]
        g = 0 if val < 0 else 255 if val > 255 else val
        val = y0 + __s_b_1732446u[u]
        b = 0 if val < 0 else 255 if val > 255 else val
        
        # 转换为RGB565并存储
        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        output_data[i * 4 + 0] = (rgb565 >> 8) & 0xFF  # 高8位
        output_data[i * 4 + 1] = rgb565 & 0xFF          # 低8位
        
        # 计算第二个像素的RGB值
        val = y1 + __s_r_1370705v[v]
        r = 0 if val < 0 else 255 if val > 255 else val
        val = y1 - __s_g_337633u[u] - __s_g_698001v[v]
        g = 0 if val < 0 else 255 if val > 255 else val
        val = y1 + __s_b_1732446u[u]
        b = 0 if val < 0 else 255 if val > 255 else val
        
        # 转换为RGB565并存储
        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        output_data[i * 4 + 2] = (rgb565 >> 8) & 0xFF  # 高8位
        output_data[i * 4 + 3] = rgb565 & 0xFF          # 低8位
    
    return output_data

def rgb565_to_rgb888(rgb565_data, width, height):
    """将RGB565数据转换为RGB888格式"""
    rgb888 = []
    for i in range(0, len(rgb565_data), 2):
        # 提取RGB565值
        high_byte = rgb565_data[i]
        low_byte = rgb565_data[i + 1]
        rgb565 = (high_byte << 8) | low_byte
        
        # 转换为RGB888
        r = ((rgb565 >> 11) & 0x1F) << 3
        g = ((rgb565 >> 5) & 0x3F) << 2
        b = (rgb565 & 0x1F) << 3
        
        rgb888.extend([r, g, b])
    
    # 转换为numpy数组并重塑为图像尺寸
    return np.array(rgb888, dtype=np.uint8).reshape((height, width, 3))

def update_display_window(root, panel, status_label):
    """更新显示窗口的函数，在主线程中运行"""
    global latest_image, image_update_event
    
    while True:
        # 等待新图像事件
        image_update_event.wait()
        image_update_event.clear()
        
        if latest_image is not None:
            # 更新图像显示
            img_tk = ImageTk.PhotoImage(image=latest_image)
            panel.config(image=img_tk)
            panel.image = img_tk  # 保持引用，防止被垃圾回收
            status_label.config(text=f"最后更新: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        
        # 处理窗口事件
        root.update_idletasks()
        root.update()

async def convert_yuv_to_png(yuv_data, png_filename, width, height):
    """将YUV数据转换为PNG图像并更新全局图像变量"""
    global latest_image, image_update_event
    try:
        # 转换YUV到RGB565
        rgb565_data = yuyv2rgb565(yuv_data)
        
        # 转换RGB565到RGB888
        rgb888_data = rgb565_to_rgb888(rgb565_data, width, height)
        
        # 创建图像对象
        img = Image.fromarray(rgb888_data)
        
        # 保存为PNG
        # img.save(png_filename)
        
        # 更新全局变量用于显示
        latest_image = img.copy()
        image_update_event.set()  # 触发显示更新
        
        # print(f"Successfully converted to {png_filename}")
        return True
    except Exception as e:
        print(f"Error converting YUV to PNG: {str(e)}")
        return False

async def receive_yuv(request):
    try:
        # 读取请求体中的二进制数据
        yuv_data = await request.read()  # 异步读取数据
        yuv_size = len(yuv_data)
        print(f"Received data size: {yuv_size} bytes")

        # 验证数据大小 - 240x240的YUV422应该是240*240*2=115200字节
        expected_width = 240
        expected_height = 240
        expected_size = expected_width * expected_height * 2  # YUV422每个像素2字节
        
        if yuv_size != expected_size:
            return web.Response(
                text=f"Invalid data size: {yuv_size}, expected {expected_size}",
                status=400
            )

        # 生成高精度时间戳作为文件名
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]  # 包含毫秒
        yuv_filename = f"image/{timestamp}.yuv"
        png_filename = f"image/{timestamp}.png"

        # 异步保存YUV文件
        async with aiofiles.open(yuv_filename, "wb") as f:
            await f.write(yuv_data)
        
        print(f"Successfully saved YUV422: {yuv_size} bytes to {yuv_filename}")
        
        # 转换为PNG并更新显示
        await convert_yuv_to_png(yuv_data, png_filename, expected_width, expected_height)
        
        return web.Response(text="YUV received and converted to PNG", status=200)
        
    except Exception as e:
        print(f"Error processing request: {str(e)}")
        return web.Response(text=f"Server error: {str(e)}", status=500)

def run_server():
    """运行aiohttp服务器的函数，将在单独线程中执行"""
    app = web.Application()
    app.add_routes([web.post('/receive_yuv', receive_yuv)])
    web.run_app(app, host='0.0.0.0', port=9898, handle_signals=True)

if __name__ == '__main__':
    # 创建显示窗口
    root = tk.Tk()
    root.title("YUV图像实时显示")
    root.geometry("600x500")  # 设置窗口初始大小
    
    # 创建状态标签
    status_label = ttk.Label(root, text="等待接收图像...", font=("Arial", 10))
    status_label.pack(pady=10)
    
    # 创建图像显示面板
    panel = ttk.Label(root)
    panel.pack(expand=True, fill=tk.BOTH, padx=10, pady=10)
    
    # 启动服务器线程
    server_thread = threading.Thread(target=run_server, daemon=True)
    server_thread.start()
    
    # 启动显示更新循环
    update_display_window(root, panel, status_label)
    
    # 启动Tkinter主循环
    root.mainloop()
