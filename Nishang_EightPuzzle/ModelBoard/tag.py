import os
import numpy as np
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import uuid
import tensorflow as tf  # type:ignore
from typing import Any, Optional
import random

print(
    "可用GPU列表:", tf.config.experimental.list_physical_devices("GPU")  # type:ignore
)


class YUVMarker:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("YUV图像打标工具")
        self.root.geometry("700x700")

        self.model_path = "digit_recognition_yuv.tflite"  # TFLite模型路径
        self.interpreter = None  # TFLite解释器
        self.input_details = []  # 模型输入信息
        self.output_details = []  # 模型输出信息
        self.IMAGE_SHAPE = (80, 80)  # 模型输入尺寸（与训练一致）

        self.load_tflite_model()

        self.current_file: Optional[str] = None
        self.sub_images = []  # 存储RGB格式子图（用于显示）
        self.sub_yuvs = []  # 存储YUV422格式子图（用于保存）
        self.entry_widgets = []
        self.current_entry_index = 0  # 当前激活的输入框索引
        self.image_dir = "image"
        self.dataset_dir = "dataset"
        self.processed_log = "processed.txt"
        self.processed_files = self.load_processed_files()
        self.init_dataset_dirs()
        self.create_widgets()
        self.load_next_file()
        self.bind_shortcuts()  # 绑定快捷键

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

    def init_dataset_dirs(self):
        """初始化dataset下的1-9目录"""
        for i in range(10):
            dir_path = os.path.join(self.dataset_dir, str(i))
            if not os.path.exists(dir_path):
                os.makedirs(dir_path)

    def load_processed_files(self) -> list[str]:
        """读取已处理文件记录"""
        if os.path.exists(self.processed_log):
            with open(self.processed_log, "r", encoding="utf-8") as f:
                return [line.strip() for line in f if line.strip()]
        return []

    def save_processed_file(self, filename: str):
        """记录已处理文件"""
        with open(self.processed_log, "a", encoding="utf-8") as f:
            f.write(f"{filename}\n")
        self.processed_files.append(filename)

    def create_widgets(self):
        """创建界面组件"""
        # 标题和操作提示
        tk.Label(
            self.root,
            text="YUV422图像切割打标（240x240 → 9×80x80）",
            font=("SimHei", 11),
        ).pack(pady=5)

        tk.Label(
            self.root,
            text="操作：数字键(0-9)填充，Enter切换/提交，↓直接下一张",
            font=("SimHei", 9),
            fg="green",
        ).pack(pady=2)

        # 图像显示区域（九宫格）
        self.image_frame = tk.Frame(self.root)
        self.image_frame.pack(pady=5)

        # 输入框区域（与图像对应）
        self.entry_frame = tk.Frame(self.root)
        self.entry_frame.pack(pady=5)

        # 状态提示
        self.status_var = tk.StringVar()
        self.status_var.set("等待加载图像...")
        tk.Label(
            self.root, textvariable=self.status_var, fg="blue", font=("SimHei", 9)
        ).pack(pady=5)

    def bind_shortcuts(self):
        """绑定键盘快捷键"""
        for num in range(10):
            self.root.bind(str(num), self.handle_number_input)

        self.root.bind("<Up>", self.handle_last_line_tag)
        self.root.bind("<Down>", self.handle_next_line_tag)
        self.root.bind("<Right>", self.handle_next_tag)
        self.root.bind("<Left>", self.handle_last_tag)

        self.root.bind("<Return>", self.handle_next_image)

    def handle_number_input(self, event: tk.Event):
        """处理数字键输入"""
        if 0 <= self.current_entry_index < len(self.entry_widgets):
            current_entry = self.entry_widgets[self.current_entry_index]
            current_entry.delete(0, tk.END)
            current_entry.insert(0, event.char)
            self.highlight_current_entry()

    def handle_next_tag(self, event: tk.Event):
        """处理Enter键，切换到下一个输入框或提交"""
        if self.current_entry_index < len(self.entry_widgets) - 1:
            self.current_entry_index += 1
            self.highlight_current_entry()

    def handle_last_tag(self, event: tk.Event):
        if self.current_entry_index > 0:
            self.current_entry_index -= 1
            self.highlight_current_entry()

    def handle_next_line_tag(self, event: tk.Event):
        if self.current_entry_index < len(self.entry_widgets) - 3:
            self.current_entry_index += 3
            self.highlight_current_entry()

    def handle_last_line_tag(self, event: tk.Event):
        if self.current_entry_index > 2:
            self.current_entry_index -= 3
            self.highlight_current_entry()

    def handle_next_image(self, event: tk.Event):
        if self.current_file:
            # self.save_processed_file(self.current_file)
            # self.status_var.set(f"已保存：{self.current_file}")
            self.process_labels()

    def highlight_current_entry(self):
        """高亮显示当前激活的输入框"""
        for i, entry in enumerate(self.entry_widgets):
            if i == self.current_entry_index:
                entry.config(bg="#e1f5fe")
                entry.focus_set()
            else:
                entry.config(bg="white")

    def get_yuv_files(self) -> list[str]:
        """获取未处理的YUV文件"""
        if not os.path.exists(self.image_dir):
            os.makedirs(self.image_dir)
            return []
        all_files = [
            f
            for f in os.listdir(self.image_dir)
            if f.lower().endswith(".yuv")
            and os.path.isfile(os.path.join(self.image_dir, f))
        ]
        ret_files = [f for f in all_files if f not in self.processed_files]
        random.shuffle(ret_files)
        return ret_files

    def load_next_file(self):
        """加载下一个未处理的YUV文件"""
        yuv_files = self.get_yuv_files()
        if not yuv_files:
            self.status_var.set("所有文件已处理完成！")
            return

        self.current_file = yuv_files[0]
        file_path = os.path.join(self.image_dir, self.current_file)
        self.status_var.set(
            f"处理中：{self.current_file}（已完成{len(self.processed_files)}个）"
        )

        # 验证文件大小
        expected_size = 240 * 240 * 2
        file_size = os.path.getsize(file_path)
        if file_size != expected_size:
            messagebox.showerror(
                "错误",
                f"{self.current_file}大小不正确\n实际: {file_size}字节 预期: {expected_size}字节",
            )
            self.save_processed_file(self.current_file)
            self.load_next_file()
            return

        # 读取并处理YUV文件
        try:
            self.process_yuv_file(file_path)
        except Exception as e:
            messagebox.showerror("错误", f"处理失败：{str(e)}")
            self.save_processed_file(self.current_file)
            self.load_next_file()

    def process_yuv_file(self, file_path: str):
        """读取YUV文件并切割为9个子图"""
        # 读取原始YUV422数据
        with open(file_path, "rb") as f:
            yuv_data = np.fromfile(f, dtype=np.uint8)

        # 验证数据完整性
        expected_length = 240 * 240 * 2
        if len(yuv_data) != expected_length:
            raise ValueError(
                f"数据不完整，预期{expected_length}字节，实际{len(yuv_data)}字节"
            )

        # 重塑为240x240x2的YUV422数组
        yuv_array = yuv_data.reshape(240, 240, 2)

        # 转换为RGB用于显示
        rgb_image = self.yuyv_to_rgb(yuv_array)

        # 切割为3x3子图
        self.sub_images: list[np.ndarray[Any, np.dtype[np.uint8]]] = []
        self.sub_yuvs: list[np.ndarray[Any, np.dtype[np.uint8]]] = []
        for i in range(3):
            for j in range(3):
                y_start, y_end = i * 80, (i + 1) * 80
                x_start, x_end = j * 80, (j + 1) * 80

                # 保存RGB子图（用于显示）
                self.sub_images.append(rgb_image[y_start:y_end, x_start:x_end])

                # 提取YUV422子图（用于保存）
                yuv_sub = yuv_array[y_start:y_end, x_start:x_end, :]
                self.sub_yuvs.append(yuv_sub)

        # 显示子图并重置输入框索引
        self.current_entry_index = 0
        self.display_sub_images()
        self.highlight_current_entry()

    def yuyv_to_rgb(
        self, yuyv_array: np.ndarray[Any, np.dtype[np.uint8]]
    ) -> np.ndarray[Any, np.dtype[np.uint8]]:
        """YUYV格式转RGB（用于显示）"""
        height, width, _ = yuyv_array.shape
        rgb = np.zeros((height, width, 3), dtype=np.uint8)

        # 向量化操作优化转换
        y0 = yuyv_array[:, ::2, 0].astype(np.float32)
        u = yuyv_array[:, ::2, 1].astype(np.float32) - 128
        y1 = yuyv_array[:, 1::2, 0].astype(np.float32)
        v = yuyv_array[:, 1::2, 1].astype(np.float32) - 128

        # 计算RGB分量
        r0 = y0 + 1.402 * v
        g0 = y0 - 0.344136 * u - 0.714136 * v
        b0 = y0 + 1.772 * u

        r1 = y1 + 1.402 * v
        g1 = y1 - 0.344136 * u - 0.714136 * v
        b1 = y1 + 1.772 * u

        # 裁剪到0-255范围
        r0 = np.clip(r0, 0, 255).astype(np.uint8)
        g0 = np.clip(g0, 0, 255).astype(np.uint8)
        b0 = np.clip(b0, 0, 255).astype(np.uint8)

        r1 = np.clip(r1, 0, 255).astype(np.uint8)
        g1 = np.clip(g1, 0, 255).astype(np.uint8)
        b1 = np.clip(b1, 0, 255).astype(np.uint8)

        # 填充RGB数组
        rgb[:, ::2, 0] = r0
        rgb[:, ::2, 1] = g0
        rgb[:, ::2, 2] = b0

        rgb[:, 1::2, 0] = r1
        rgb[:, 1::2, 1] = g1
        rgb[:, 1::2, 2] = b1

        return rgb

    def display_sub_images(self):
        """显示9个子图和输入框（3x3网格）"""
        # 清空之前的组件
        for widget in self.image_frame.winfo_children():
            widget.destroy()
        for widget in self.entry_frame.winfo_children():
            widget.destroy()
        self.entry_widgets: list[tk.Entry] = []

        # 3x3网格显示
        print()
        for i in range(9):
            # 显示图像
            pil_img = Image.fromarray(self.sub_images[i])
            display_img = pil_img.resize((140, 140), Image.LANCZOS)  # type:ignore
            tk_img = ImageTk.PhotoImage(image=display_img)

            row, col = i // 3, i % 3
            img_label = tk.Label(
                self.image_frame, image=tk_img, borderwidth=1, relief="solid"
            )
            img_label.image = tk_img  # type:ignore
            img_label.grid(row=row, column=col, padx=3, pady=3)

            # 输入框（使用默认标签函数获取初始值）
            entry = tk.Entry(
                self.entry_frame, width=4, font=("SimHei", 10), justify="center"
            )
            entry.grid(row=row, column=col, padx=15, pady=2)

            # 调用默认标签函数获取初始值（核心修改）
            default_label = self.get_default_label(self.sub_yuvs[i])  # 传入当前子图
            entry.insert(0, str(default_label))  # 设置默认值
            self.entry_widgets.append(entry)
        print("可以开始确认")

    def get_default_label(self, yuv_sub: np.ndarray[Any, np.dtype[np.uint8]]) -> int:
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

    def process_labels(self):
        """处理标签并自动保存到对应目录，文件名自动生成"""
        # 验证输入
        labels: list[int] = []
        for i, entry in enumerate(self.entry_widgets):
            text = entry.get().strip()
            if not text.isdigit() or not (0 <= int(text) <= 9):
                messagebox.showerror("错误", f"第{i+1}个输入必须是0-9的数字")
                self.current_entry_index = i
                self.highlight_current_entry()
                return
            labels.append(int(text))

        # 保存标签为1-9的子图（YUV422格式）
        for i in range(9):
            label = labels[i]
            if 0 <= label <= 9:
                # 生成唯一文件名（使用UUID）
                unique_filename = f"{uuid.uuid4().hex}.yuv"
                # 保存路径：dataset/标签/唯一文件名
                save_path = os.path.join(self.dataset_dir, str(label), unique_filename)
                # 保存YUV422数据
                yuv_flat = self.sub_yuvs[i].flatten()
                yuv_flat.tofile(save_path)

        # 记录为已处理
        if self.current_file:
            self.save_processed_file(self.current_file)
            self.status_var.set(f"已完成：{self.current_file}")

        # 自动加载下一个
        self.load_next_file()


if __name__ == "__main__":
    root = tk.Tk()
    app = YUVMarker(root)
    root.mainloop()
