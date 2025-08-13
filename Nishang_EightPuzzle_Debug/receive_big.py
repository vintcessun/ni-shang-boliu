import threading
import tkinter as tk
from tkinter import ttk, messagebox
import numpy as np
from PIL import Image, ImageTk
import time
import os
import tensorflow as tf  # type:ignore
from typing import Any, Optional, List, Tuple, Dict
from aiohttp import web
import asyncio
import io

class HttpImageViewer:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("YUV422 HTTP图像接收器")
        self.root.geometry("1000x800")

        # 服务器配置
        self.server_running = False
        self.server_thread = None
        self.host = "0.0.0.0"
        self.port = 9898

        # 图像处理相关
        self.image_width = 240  # 固定为240x240
        self.image_height = 240
        self.sub_image_size = 80  # 每个子图像的尺寸 80x80

        # 模型相关
        self.model_path = "digit_recognition_yuv_big.h5"  # Keras模型路径
        self.model: Optional[tf.keras.Model] = None  # Keras模型
        self.IMAGE_SHAPE = (80, 80)  # 模型输入尺寸（与训练一致）

        self.predictions = []

        self.load_tflite_model()

        # 图像缓存 - 用于九宫格显示，存储图像和预测结果
        self.current_grid_images: List[Tuple[Image.Image, int]] = []  # 当前显示的九宫格图像和预测结果

        # 初始化YUV转RGB的查找表
        self.init_yuyv2rgb_tables()

        # 创建UI
        self.create_widgets()

        # 启动HTTP服务器
        self.start_server()

    def load_tflite_model(self):
        """加载Keras h5模型"""
        try:
            if not os.path.exists(self.model_path):
                messagebox.showwarning(
                    "模型缺失",
                    f"未找到模型文件: {self.model_path}\n请先运行训练脚本生成模型",
                )
                return

            # 加载Keras模型
            self.model = tf.keras.models.load_model(self.model_path)  # type:ignore
            print(f"模型加载成功: {self.model_path}")

        except Exception as e:
            messagebox.showerror("模型加载失败", f"加载模型时出错: {str(e)}")

    def init_yuyv2rgb_tables(self):
        """初始化YUV转RGB的查找表"""
        self.s_r_1370705v = np.zeros(256, dtype=np.int32)
        self.s_b_1732446u = np.zeros(256, dtype=np.int32)
        self.s_g_337633u = np.zeros(256, dtype=np.int32)
        self.s_g_698001v = np.zeros(256, dtype=np.int32)

        for i in range(256):
            self.s_r_1370705v[i] = int(1.370705 * (i - 128))
            self.s_b_1732446u[i] = int(1.732446 * (i - 128))
            self.s_g_337633u[i] = int(0.337633 * (i - 128))
            self.s_g_698001v[i] = int(0.698001 * (i - 128))

    def create_widgets(self):
        # 顶部控制区
        control_frame = ttk.Frame(self.root, padding="10")
        control_frame.pack(fill=tk.X)

        ttk.Label(control_frame, text=f"服务器地址: http://{self.host}:{self.port}").pack(side=tk.LEFT, padx=5)
        self.status_var = tk.StringVar()
        self.status_var.set("服务器启动中...")
        ttk.Label(control_frame, textvariable=self.status_var).pack(side=tk.LEFT, padx=5)

        # 信息显示区
        self.info_text = tk.Text(self.root, height=10, wrap=tk.WORD)
        self.info_text.pack(fill=tk.X, padx=10, pady=5)
        self.info_text.config(state=tk.DISABLED)

        # 九宫格图像显示区
        self.grid_frame = ttk.Frame(self.root, relief=tk.SUNKEN, borderwidth=2)
        self.grid_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # 创建3x3的九宫格布局，每个单元格包含图像和预测结果
        self.image_labels: List[List[ttk.Label]] = []  # 图像标签
        self.prediction_labels: List[List[ttk.Label]] = []  # 预测结果标签
        
        for i in range(3):
            img_row: List[ttk.Label] = []
            pred_row: List[ttk.Label] = []
            for j in range(3):
                # 创建单元格容器
                cell_frame = ttk.Frame(self.grid_frame, borderwidth=1, relief=tk.SOLID)
                cell_frame.grid(row=i, column=j, padx=2, pady=2, sticky="nsew")
                
                # 图像标签
                img_label = ttk.Label(cell_frame)
                img_label.pack(fill=tk.BOTH, expand=True)
                img_row.append(img_label)
                
                # 预测结果标签
                pred_label = ttk.Label(
                    cell_frame, 
                    text="", 
                    font=("Arial", 12, "bold"),
                    background="#f0f0f0",
                    padding=2
                )
                pred_label.pack(fill=tk.X)
                pred_row.append(pred_label)
                
            self.image_labels.append(img_row)
            self.prediction_labels.append(pred_row)

        # 设置网格权重，使单元格可以伸缩
        for i in range(3):
            self.grid_frame.grid_rowconfigure(i, weight=1)
            self.grid_frame.grid_columnconfigure(i, weight=1)

    def log_message(self, message: str):
        """在UI上显示日志信息"""
        self.root.after(0, lambda: self._do_log(message))

    def _do_log(self, message: str):
        """实际执行日志记录的函数，确保在主线程执行"""
        self.info_text.config(state=tk.NORMAL)
        self.info_text.insert(tk.END, message + "\n")
        self.info_text.see(tk.END)
        self.info_text.config(state=tk.DISABLED)

    def start_server(self):
        """启动HTTP服务器线程"""
        if not self.server_running:
            self.server_running = True
            self.server_thread = threading.Thread(target=self.run_server, daemon=True)
            self.server_thread.start()
            self.log_message(f"启动服务器线程，监听 {self.host}:{self.port}")

    def run_server(self):
        """运行aiohttp服务器"""
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
        app = web.Application()
        app.add_routes([web.post('/receive_yuv', self.receive_yuv)])
        
        runner = web.AppRunner(app)
        loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, self.host, self.port)
        
        try:
            self.root.after(0, lambda: self.status_var.set(f"服务器运行中: {self.host}:{self.port}"))
            self.log_message(f"服务器开始运行在 {self.host}:{self.port}")
            loop.run_until_complete(site.start())
            loop.run_forever()
        except Exception as e:
            self.log_message(f"服务器错误: {str(e)}")
            self.root.after(0, lambda: self.status_var.set(f"服务器错误"))
        finally:
            loop.run_until_complete(runner.cleanup())
            loop.close()

    async def receive_yuv(self, request: web.Request)->str:
        """处理接收YUV图像的POST请求"""
        try:
            # 读取原始数据
            data = await request.read()
            self.log_message(f"收到图像数据，大小: {len(data)} 字节")
            
            # 验证数据大小是否符合240x240 YUV422格式
            expected_size = self.image_width * self.image_height * 2  # 每个像素2字节
            if len(data) != expected_size:
                error_msg = f"数据大小不匹配，预期 {expected_size} 字节，实际 {len(data)} 字节"
                self.log_message(error_msg)
                return web.Response(text=error_msg, status=400)
            
            # 在主线程中处理图像
            self.root.after(0, lambda: self.process_image(data))
            
            return web.Response(text=",".join(self.predictions))
        except Exception as e:
            error_msg = f"处理请求错误: {str(e)}"
            self.log_message(error_msg)
            return web.Response(text=error_msg, status=500)

    def split_into_grid(self, yuv_data: bytes) -> List[bytes]:
        """将240x240的YUV422图像切割成9个80x80的子图像"""
        sub_images = []
        
        # 将字节数据转换为numpy数组
        yuv_np = np.frombuffer(yuv_data, dtype=np.uint8)
        yuv_np = yuv_np.reshape((self.image_height, self.image_width, 2))  # YUV422格式每个像素2字节
        
        # 切割成3x3的网格
        for i in range(3):
            for j in range(3):
                # 计算子图像的起始和结束坐标
                start_row = i * self.sub_image_size
                end_row = start_row + self.sub_image_size
                start_col = j * self.sub_image_size
                end_col = start_col + self.sub_image_size
                
                # 提取子图像
                sub_yuv = yuv_np[start_row:end_row, start_col:end_col, :]
                sub_images.append(sub_yuv.tobytes())
                
        return sub_images


    def batch_predict_labels(self, yuv_subs: list[np.ndarray[Any, np.dtype[np.uint8]]]) -> list[int]:
        """批量预测9个子图的标签"""
        if not hasattr(self, 'model') or self.model is None:
            return [0] * 9

        try:
            # 预处理所有子图
            inputs = np.array([sub.astype(np.float32) / 255.0 for sub in yuv_subs])
            
            # 批量预测
            predictions = self.model.predict(inputs)  # type:ignore
            
            # 获取每个子图的预测结果
            predicted_classes = [int(np.argmax(pred)) for pred in predictions]  # type:ignore
            
            # 打印预测结果
            for i, (pred, cls) in enumerate(zip(predictions, predicted_classes)):  # type:ignore
                print(f"子图{i+1} 预测 {cls} 概率 {pred[cls]}")
                
            return predicted_classes

        except Exception as e:
            print(f"批量预测错误: {str(e)}")
            return [0] * 9

    def get_label(self, yuv_sub: np.ndarray[Any, np.dtype[np.uint8]]) -> int:
        return self.batch_predict_labels([yuv_sub])[0]

    def yuyv_to_rgb(self, yuyv_data: bytes, width: int, height: int) -> Optional[np.ndarray]:
        """将YUYV422格式数据转换为RGB格式"""
        # 计算总像素数和预期数据长度（每个像素2字节）
        total_pixels = width * height
        expected_length = total_pixels * 2  # YUYV422每个像素占2字节

        if len(yuyv_data) != expected_length:
            self.log_message(
                f"YUV数据长度不匹配: 实际{len(yuyv_data)}字节，预期{expected_length}字节"
            )
            return None

        # 转换为numpy数组便于处理
        yuyv = np.frombuffer(yuyv_data, dtype=np.uint8)

        # 创建RGB数组（高度×宽度×3通道）
        rgb = np.zeros((height, width, 3), dtype=np.uint8)

        # 遍历处理每个YUYV块（4字节对应2个像素）
        pixel_idx = 0  # 当前处理的像素索引
        for i in range(0, len(yuyv), 4):
            if i + 3 >= len(yuyv):
                break  # 防止越界

            # 提取Y0, U, Y1, V
            y0 = yuyv[i]
            u = yuyv[i + 1]
            y1 = yuyv[i + 2]
            v = yuyv[i + 3]

            # 计算第一个像素的RGB值
            r = y0 + self.s_r_1370705v[v]
            g = y0 - self.s_g_337633u[u] - self.s_g_698001v[v]
            b = y0 + self.s_b_1732446u[u]

            # 裁剪到0-255范围
            r = np.clip(r, 0, 255)
            g = np.clip(g, 0, 255)
            b = np.clip(b, 0, 255)

            # 赋值给RGB数组
            if pixel_idx < total_pixels:
                row = pixel_idx // width
                col = pixel_idx % width
                rgb[row, col] = [r, g, b]
                pixel_idx += 1

            # 计算第二个像素的RGB值
            r = y1 + self.s_r_1370705v[v]
            g = y1 - self.s_g_337633u[u] - self.s_g_698001v[v]
            b = y1 + self.s_b_1732446u[u]

            # 裁剪到0-255范围
            r = np.clip(r, 0, 255)
            g = np.clip(g, 0, 255)
            b = np.clip(b, 0, 255)

            # 赋值给RGB数组
            if pixel_idx < total_pixels:
                row = pixel_idx // width
                col = pixel_idx % width
                rgb[row, col] = [r, g, b]
                pixel_idx += 1

        return rgb

    def process_image(self, yuv_data: bytes):
        """处理接收到的图像，切割成九宫格并进行预测"""
        try:
            self.log_message("开始处理图像...")
            
            # 将图像切割成9个子图像
            sub_images = self.split_into_grid(yuv_data)
            self.log_message(f"图像切割完成，得到 {len(sub_images)} 个子图像")
            
            yuv_datas = []
            img_datas = []
            for i, sub_img_data in enumerate(sub_images):
                # 转换为RGB以便显示
                rgb_data = self.yuyv_to_rgb(sub_img_data, self.sub_image_size, self.sub_image_size)
                if rgb_data is None:
                    self.log_message(f"子图像 {i} 转换失败")
                    continue
                
                # 从RGB数组创建图像
                img = Image.fromarray(rgb_data)
                
                # 准备预测用的YUV数据
                yuv_np = np.frombuffer(sub_img_data, dtype=np.uint8)
                yuv_np.resize((self.sub_image_size, self.sub_image_size, 2))
                
                # 预测数字
                yuv_datas.append(yuv_np)
                
                img_datas.append(img)
            
            self.predictions = self.batch_predict_labels(yuv_datas)

            # 处理每个子图像
            grid_results = []
            for i in range(len(img_datas)):
                grid_results.append((img_datas[i],self.predictions[i]))

            # 更新当前显示的九宫格图像
            self.current_grid_images = grid_results
            self.update_grid_display()
        except Exception as e:
            self.log_message(f"处理图像错误: {str(e)}")

    def update_grid_display(self):
        """更新九宫格显示，同时显示图像和对应的预测结果"""
        # 显示缓存中的图片和预测结果，按3x3排列
        for idx, (img, pred) in enumerate(self.current_grid_images):
            if idx >= 9:  # 只显示最多9张
                break

            row = idx // 3
            col = idx % 3

            # 调整图像大小以适应单元格
            cell_width = self.grid_frame.winfo_width() // 3
            cell_height = self.grid_frame.winfo_height() // 3
            img.thumbnail((cell_width, cell_height))

            # 显示图像
            photo = ImageTk.PhotoImage(image=img)
            self.image_labels[row][col].config(image=photo)
            self.image_labels[row][col].image = photo  # 保持引用

            # 显示预测结果
            self.prediction_labels[row][col].config(
                text=f"预测: {pred}",
                foreground="blue" if pred != 0 else "black"
            )

        self.log_message(f"九宫格已更新，显示 {len(self.current_grid_images)} 张图片")


if __name__ == "__main__":
    root = tk.Tk()
    app = HttpImageViewer(root)
    root.mainloop()