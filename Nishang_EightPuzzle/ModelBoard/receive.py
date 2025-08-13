import serial
import threading
import re
import tkinter as tk
from tkinter import ttk
import numpy as np
from PIL import Image, ImageTk
import time
from typing import Optional
import os
import numpy as np
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import tensorflow as tf  # type:ignore
from typing import Any, Optional


class SerialImageViewer:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("YUV422串口图像接收器")
        self.root.geometry("1000x800")

        # 串口配置
        self.serial_port = None
        self.baud_rate = 2000000
        self.is_connected = False
        self.receiving_image = False  # 是否正在接收图像数据
        self.expected_size = 0  # 预期接收的图像数据大小
        self.current_image_data: bytes = b""  # 当前接收的图像数据
        self.current_pic_num = ""  # 当前图像编号
        self.image_width = 0  # 图像宽度(X)
        self.image_height = 0  # 图像高度(Y)
        self.text_buffer = b""  # 二进制缓冲区，用于检测图像起始标志

        self.model_path = "digit_recognition_yuv.tflite"  # TFLite模型路径
        self.interpreter = None  # TFLite解释器
        self.input_details = []  # 模型输入信息
        self.output_details = []  # 模型输出信息
        self.IMAGE_SHAPE = (80, 80)  # 模型输入尺寸（与训练一致）

        self.load_tflite_model()

        # 图像缓存 - 用于九宫格显示，现在同时存储图像和预测结果
        self.image_cache: dict[str, list[tuple[Image.Image, int]]] = (
            {}
        )  # 按序号缓存图片和预测结果 {序号: [(图片1, 预测1), (图片2, 预测2), ...]}
        self.current_display_num: Optional[str] = None  # 当前显示的序号

        # 初始化YUV转RGB的查找表（对应C代码中的四个数组）
        self.init_yuyv2rgb_tables()

        # 正则表达式匹配图像头格式
        self.image_header_pattern = re.compile(
            rb"pop picture (\d+): 0x([0-9a-fA-F]+), len: (\d+) "
            rb"CROP_WQVGA_X: (\d+) CROP_WQVGA_Y: (\d+)\r?\n"
        )

        # 创建UI
        self.create_widgets()

        # 启动接收线程
        self.receive_thread = threading.Thread(target=self.receive_data, daemon=True)
        self.receive_thread.start()


    def load_tflite_model(self):
        """加载TFLite模型并初始化解释器"""
        try:
            if not os.path.exists(self.model_path):
                messagebox.showwarning(
                    "模型缺失",
                    f"未找到模型文件: {self.model_path}\n请先运行训练脚本生成模型",
                )
                return

            # 初始化TFLite解释器
            self.interpreter = tf.lite.Interpreter(
                model_path=self.model_path, num_threads=16
            )
            self.interpreter.allocate_tensors()

            # 获取输入输出信息
            self.input_details: list[dict[str, Any]] = (
                self.interpreter.get_input_details()
            )
            self.output_details: list[dict[str, Any]] = (
                self.interpreter.get_output_details()
            )

            print(f"模型加载成功: {self.model_path}")
            # print(f"输入形状: {self.input_details[0]['shape']}")
            # print(f"输出形状: {self.output_details[0]['shape']}")

        except Exception as e:
            messagebox.showerror("模型加载失败", f"加载模型时出错: {str(e)}")


    def init_yuyv2rgb_tables(self):
        """初始化YUV转RGB的查找表（对应C代码中的四个静态数组）"""
        self.s_r_1370705v = np.zeros(256, dtype=np.int32)
        self.s_b_1732446u = np.zeros(256, dtype=np.int32)
        self.s_g_337633u = np.zeros(256, dtype=np.int32)
        self.s_g_698001v = np.zeros(256, dtype=np.int32)

        for i in range(256):
            # 对应C代码中的计算，保留整数精度
            self.s_r_1370705v[i] = int(1.370705 * (i - 128))
            self.s_b_1732446u[i] = int(1.732446 * (i - 128))
            self.s_g_337633u[i] = int(0.337633 * (i - 128))
            self.s_g_698001v[i] = int(0.698001 * (i - 128))

    def create_widgets(self):
        # 顶部控制区
        control_frame = ttk.Frame(self.root, padding="10")
        control_frame.pack(fill=tk.X)

        ttk.Label(control_frame, text="串口号:").pack(side=tk.LEFT, padx=5)
        self.port_entry = ttk.Entry(control_frame, width=10)
        self.port_entry.insert(0, "COM3")
        self.port_entry.pack(side=tk.LEFT, padx=5)

        ttk.Label(control_frame, text="波特率:").pack(side=tk.LEFT, padx=5)
        self.baud_entry = ttk.Entry(control_frame, width=10)
        self.baud_entry.insert(0, str(self.baud_rate))
        self.baud_entry.pack(side=tk.LEFT, padx=5)

        self.connect_btn = ttk.Button(
            control_frame, text="连接", command=self.toggle_connection
        )
        self.connect_btn.pack(side=tk.LEFT, padx=5)

        # 信息显示区
        self.info_text = tk.Text(self.root, height=25, wrap=tk.WORD)
        self.info_text.pack(fill=tk.X, padx=10, pady=25)
        self.info_text.config(state=tk.DISABLED)

        # 九宫格图像显示区
        self.grid_frame = ttk.Frame(self.root, relief=tk.SUNKEN, borderwidth=2)
        self.grid_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # 创建3x3的九宫格布局，每个单元格包含图像和预测结果
        self.image_labels: list[list[ttk.Label]] = []  # 图像标签
        self.prediction_labels: list[list[ttk.Label]] = []  # 预测结果标签
        
        for i in range(3):
            img_row: list[ttk.Label] = []
            pred_row: list[ttk.Label] = []
            for j in range(3):
                # 创建单元格容器
                cell_frame = ttk.Frame(self.grid_frame, borderwidth=1, relief=tk.SOLID)
                cell_frame.grid(row=i, column=j, padx=2, pady=2, sticky="nsew")
                
                # 图像标签
                img_label = ttk.Label(cell_frame)
                img_label.pack(fill=tk.BOTH, expand=True)
                img_row.append(img_label)
                
                # 预测结果标签（小窗口）
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

        # 状态栏
        self.status_var = tk.StringVar()
        self.status_var.set("未连接")
        status_bar = ttk.Label(
            self.root, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W
        )
        status_bar.pack(side=tk.BOTTOM, fill=tk.X)

    def toggle_connection(self):
        if self.is_connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        try:
            port = self.port_entry.get()
            baud = int(self.baud_entry.get())

            self.serial_port = serial.Serial(port=port, baudrate=baud, timeout=0.1)

            if self.serial_port.is_open:
                self.is_connected = True
                self.connect_btn.config(text="断开")
                self.status_var.set(f"已连接到 {port}，波特率 {baud}")
                self.log_console(f"成功连接到 {port}，波特率 {baud}")
        except Exception as e:
            self.log_console(f"连接失败: {str(e)}")
            self.status_var.set("连接失败")

    def disconnect(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()

        self.is_connected = False
        self.connect_btn.config(text="连接")
        self.status_var.set("未连接")
        self.log_console("已断开连接")

    def log_message(self, message: str):
        # 确保UI操作在主线程执行
        self.root.after(0, lambda: self._do_log(message))

    def log_console(self, message: str):
        print(message)

    def _do_log(self, message: str):
        # 实际日志操作（仅主线程调用）
        self.info_text.config(state=tk.NORMAL)
        self.info_text.insert(tk.END, message + "\n")
        self.info_text.see(tk.END)
        self.info_text.config(state=tk.DISABLED)

    def receive_data(self):
        while True:
            if self.is_connected and self.serial_port and self.serial_port.is_open:
                try:
                    if self.receiving_image:
                        # 正在接收图像数据，需要接收指定长度的数据
                        remaining = self.expected_size - len(self.current_image_data)
                        if remaining > 0:
                            # 读取剩余所需数据
                            data = self.serial_port.read(remaining)
                            self.current_image_data += data

                            # 检查是否已接收完所有数据
                            if len(self.current_image_data) == self.expected_size:
                                self.log_console(
                                    f"图像 {self.current_pic_num} 接收完成，"
                                    f"大小: {len(self.current_image_data)} 字节，"
                                    f"尺寸: {self.image_width}x{self.image_height}"
                                )
                                self.receiving_image = False
                                # 在主线程中处理图像
                                self.root.after(0, self.process_image)
                    else:
                        # 不在接收图像状态，寻找图像起始标志
                        if self.serial_port.in_waiting > 0:
                            self.text_buffer += self.serial_port.read(
                                self.serial_port.in_waiting
                            )

                            # 搜索图像起始标志（完整行）
                            match = self.image_header_pattern.search(self.text_buffer)
                            if match:
                                # 提取匹配到的头部信息
                                header = match.group(0).decode("utf-8").strip()
                                pic_num = match.group(1).decode("utf-8")
                                # pic_addr = match.group(2).decode('utf-8')
                                self.expected_size = int(match.group(3).decode("utf-8"))
                                self.image_width = int(
                                    match.group(4).decode("utf-8")
                                )  # X尺寸
                                self.image_height = int(
                                    match.group(5).decode("utf-8")
                                )  # Y尺寸

                                # 输出头部信息
                                self.log_console(f"发现图像头: {header}")

                                # 计算头部信息结束位置，准备接收图像数据
                                header_end = match.end()
                                # 剩余数据（可能包含部分图像数据）
                                remaining_data = self.text_buffer[header_end:]

                                # 初始化图像接收状态
                                self.receiving_image = True
                                self.current_pic_num = pic_num
                                self.current_image_data = (
                                    remaining_data  # 保存已收到的部分图像数据
                                )

                                # 清空缓冲区
                                self.text_buffer = b""

                                # 检查是否已经收到了全部图像数据
                                if len(self.current_image_data) >= self.expected_size:
                                    # 截断到预期大小
                                    self.current_image_data = self.current_image_data[
                                        : self.expected_size
                                    ]
                                    self.log_console(
                                        f"图像 {self.current_pic_num} 接收完成，"
                                        f"大小: {len(self.current_image_data)} 字节，"
                                        f"尺寸: {self.image_width}x{self.image_height}"
                                    )
                                    self.receiving_image = False
                                    self.root.after(0, self.process_image)
                            else:
                                self.log_message(
                                    f"收到数据：{self.text_buffer.decode('utf-8').strip()}"
                                )
                                # 没有找到完整的图像头，限制缓冲区大小防止溢出
                                if len(self.text_buffer) > 4096:  # 超过4KB则截断
                                    self.text_buffer = self.text_buffer[
                                        -2048:
                                    ]  # 保留最后2KB
                except Exception as e:
                    self.log_console(f"接收数据错误: {str(e)}")
                    self.receiving_image = False  # 出错时重置接收状态
                    self.text_buffer = b""  # 清空缓冲区
            # 子线程休眠，减少CPU占用
            time.sleep(0.001)

    def get_label(self, yuv_sub: np.ndarray[Any, np.dtype[np.uint8]]) -> int:
        """使用TFLite模型预测数字标签"""
        # 模型未加载则返回0
        if not self.interpreter:
            return 0

        try:
            # 1. 获取量化参数
            input_scale = self.input_details[0]["quantization"][0]
            input_zero_point = self.input_details[0]["quantization"][1]
            output_scale = self.output_details[0]["quantization"][0]
            output_zero_point = self.output_details[0]["quantization"][1]

            # 2. 预处理：转换为int8并量化
            yuv_input = yuv_sub.astype(np.float32) / 255.0  # 归一化到[0,1]
            yuv_input = yuv_input / input_scale + input_zero_point  # 量化
            yuv_input = np.round(yuv_input).astype(np.int8)  # 转换为int8 #type:ignore
            yuv_input = np.expand_dims(yuv_input, axis=0)  # 增加批次维度 #type:ignore

            # 3. 设置模型输入
            self.interpreter.set_tensor(  # type:ignore
                self.input_details[0]["index"], yuv_input
            )

            # 4. 执行推理
            self.interpreter.invoke()

            # 5. 获取输出并反量化
            output_data = self.interpreter.get_tensor(  # type:ignore
                self.output_details[0]["index"]
            )
            output_data = output_data.astype(np.float32)  # 转换为float32 #type:ignore
            output_data = (  # type:ignore
                output_data - output_zero_point
            ) * output_scale  # 反量化

            # 6. 取概率最大的类别（0-8）
            predicted_class: int = np.argmax(output_data[0])  # type:ignore
            print(f"预测 {predicted_class} 概率 {output_data[0][predicted_class]}")

            return predicted_class

        except Exception as e:
            print(f"模型预测错误: {str(e)}")
            return 0

    def yuyv_to_rgb(self, yuyv_data: bytes):
        """
        将YUYV422格式数据转换为RGB格式
        YUYV格式: 每4个字节表示2个像素 [Y0, U, Y1, V]
        """
        # 计算总像素数和预期数据长度（每个像素2字节）
        total_pixels = self.image_width * self.image_height
        expected_length = total_pixels * 2  # YUYV422每个像素占2字节

        if len(yuyv_data) != expected_length:
            self.log_console(
                f"YUV数据长度不匹配: 实际{len(yuyv_data)}字节，预期{expected_length}字节"
            )
            return None

        # 转换为numpy数组便于处理
        yuyv = np.frombuffer(yuyv_data, dtype=np.uint8)

        # 创建RGB数组（高度×宽度×3通道）
        rgb = np.zeros((self.image_height, self.image_width, 3), dtype=np.uint8)

        # 遍历处理每个YUYV块（4字节对应2个像素）
        pixel_idx = 0  # 当前处理的像素索引
        for i in range(0, len(yuyv), 4):
            if i + 3 >= len(yuyv):
                break  # 防止越界

            # 提取Y0, U, Y1, V（对应C代码中的y0, u, y1, v）
            y0 = yuyv[i]
            u = yuyv[i + 1]
            y1 = yuyv[i + 2]
            v = yuyv[i + 3]

            # 计算第一个像素的RGB值（对应C代码中第一个像素的计算）
            # R = Y0 + s_r_1370705v[V]
            r = y0 + self.s_r_1370705v[v]
            # G = Y0 - s_g_337633u[U] - s_g_698001v[V]
            g = y0 - self.s_g_337633u[u] - self.s_g_698001v[v]
            # B = Y0 + s_b_1732446u[U]
            b = y0 + self.s_b_1732446u[u]

            # 裁剪到0-255范围
            r = np.clip(r, 0, 255)
            g = np.clip(g, 0, 255)
            b = np.clip(b, 0, 255)

            # 赋值给RGB数组（注意像素索引是否在范围内）
            if pixel_idx < total_pixels:
                row = pixel_idx // self.image_width
                col = pixel_idx % self.image_width
                rgb[row, col] = [r, g, b]
                pixel_idx += 1

            # 计算第二个像素的RGB值（对应C代码中第二个像素的计算）
            r = y1 + self.s_r_1370705v[v]
            g = y1 - self.s_g_337633u[u] - self.s_g_698001v[v]
            b = y1 + self.s_b_1732446u[u]

            # 裁剪到0-255范围
            r = np.clip(r, 0, 255)
            g = np.clip(g, 0, 255)
            b = np.clip(b, 0, 255)

            # 赋值给RGB数组
            if pixel_idx < total_pixels:
                row = pixel_idx // self.image_width
                col = pixel_idx % self.image_width
                rgb[row, col] = [r, g, b]
                pixel_idx += 1

        return rgb

    def process_image(self):
        """处理接收到的图像，根据序号进行缓存和显示，同时保存预测结果"""
        try:
            # 验证图像尺寸是否有效
            if self.image_width <= 0 or self.image_height <= 0:
                self.log_console("无效的图像尺寸信息")
                return
            
            # 预测数字
            yuv_np = np.frombuffer(self.current_image_data, dtype=np.uint8)
            yuv_np.resize((80,80,2))
            prediction = self.get_label(yuv_np)
            self.log_console(f"预测标签: {prediction}")

            # 将YUV422转换为RGB
            rgb_data = self.yuyv_to_rgb(self.current_image_data)
            if rgb_data is None:
                return

            # 从RGB数组创建图像
            img = Image.fromarray(rgb_data)

            # 检查序号是否变化
            if self.current_pic_num != self.current_display_num:
                # 如果是新序号，检查是否需要清空缓存
                self.log_console(f"检测到新序号 {self.current_pic_num}，更新显示")
                self.current_display_num = self.current_pic_num
                self.image_cache[self.current_pic_num] = []  # 初始化新序号的缓存

            # 添加到缓存，最多缓存9张，同时存储图像和预测结果
            if self.current_pic_num in self.image_cache:
                if len(self.image_cache[self.current_pic_num]) >= 9:
                    self.log_console(
                        f"序号 {self.current_pic_num} 的缓存已达上限(9张)，替换最早的图片"
                    )
                    self.image_cache[self.current_pic_num].pop(0)  # 移除最早的图片
                self.image_cache[self.current_pic_num].append((img, prediction))
            else:
                self.image_cache[self.current_pic_num] = [(img, prediction)]

            # 更新九宫格显示
            self.update_grid_display()

        except Exception as e:
            self.log_console(f"处理图像错误: {str(e)}")

    def update_grid_display(self):
        """更新九宫格显示，同时显示图像和对应的预测结果"""
        if self.current_pic_num not in self.image_cache:
            return

        # 获取图像和预测结果的列表
        images_with_preds = self.image_cache[self.current_pic_num]

        # 显示缓存中的图片和预测结果，从左上角开始排列
        for idx, (img, pred) in enumerate(images_with_preds):
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
            self.image_labels[row][col].image = photo  # 保持引用 #type:ignore

            # 显示预测结果
            self.prediction_labels[row][col].config(
                text=f"预测: {pred}",
                foreground="blue" if pred != 0 else "black"  # 非0结果用蓝色突出显示
            )

        self.log_console(
            f"九宫格已更新，当前显示序号 {self.current_pic_num} 的 {len(images_with_preds)} 张图片"
        )


if __name__ == "__main__":
    root = tk.Tk()
    app = SerialImageViewer(root)
    root.mainloop()