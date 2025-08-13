import os
import numpy as np
import tkinter as tk
from tkinter import messagebox, ttk
from PIL import Image, ImageTk
from typing import Optional

class DatasetVerifier:
    def __init__(self, root:tk.Tk):
        self.root = root
        self.root.title("数字标签验证工具")
        self.root.geometry("800x600")
        self.root.configure(bg="#f0f0f0")
        
        # 数据集路径
        self.dataset_dir = "dataset"
        self.verified_log = "verified.txt"  # 记录已确认的文件
        self.error_log = "errors.txt"        # 记录错误的文件
        
        # 创建必要的日志文件
        for log_file in [self.verified_log, self.error_log]:
            if not os.path.exists(log_file):
                with open(log_file, "w", encoding="utf-8") as _:
                    pass
        
        # 加载已确认和错误的文件记录
        self.verified_files = self.load_log_file(self.verified_log)
        self.error_files = self.load_log_file(self.error_log)
        
        # 当前状态变量
        self.current_label = 0  # 当前查看的标签（0-9）
        self.current_file_index = 0  # 当前文件索引
        self.files_by_label = self.load_files_by_label()  # 按标签分类的文件列表
        self.current_image = None  # 当前显示的图像
        
        # 创建界面
        self.create_widgets()
        
        # 显示第一个文件
        self.load_current_file()
        
        # 绑定快捷键
        self.bind_shortcuts()

    def load_log_file(self, filename:str)->list[str]:
        """加载日志文件内容"""
        if os.path.exists(filename):
            with open(filename, "r", encoding="utf-8") as f:
                return [line.strip() for line in f if line.strip()]
        return []

    def load_files_by_label(self)->dict[int,list[str]]:
        """按标签加载所有文件"""
        files_by_label:dict[int,list[str]] = {}
        for label in range(10):
            label_dir = os.path.join(self.dataset_dir, str(label))
            if os.path.exists(label_dir):
                # 获取该标签下的所有YUV文件，排除已处理的
                all_files = [f for f in os.listdir(label_dir) 
                           if f.lower().endswith(".yuv") and 
                           os.path.isfile(os.path.join(label_dir, f))]
                
                # 过滤已确认和已标记错误的文件
                unprocessed:list[str] = []
                for file in all_files:
                    file_path = os.path.join(str(label), file)
                    if file_path not in self.verified_files and file_path not in self.error_files:
                        unprocessed.append(file)
                
                files_by_label[label] = unprocessed
            else:
                files_by_label[label] = []
        return files_by_label

    def create_widgets(self):
        """创建界面组件"""
        # 顶部信息栏
        top_frame = tk.Frame(self.root, bg="#e0e0e0", height=50)
        top_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # 标签选择下拉框
        tk.Label(top_frame, text="当前标签:", bg="#e0e0e0", font=("SimHei", 10)).pack(side=tk.LEFT, padx=5, pady=10)
        self.label_combobox = ttk.Combobox(top_frame, values=list(range(10)), width=5, font=("SimHei", 10))#type:ignore
        self.label_combobox.current(0)  # 默认选择0
        self.label_combobox.bind("<<ComboboxSelected>>", self.on_label_change)
        self.label_combobox.pack(side=tk.LEFT, padx=5, pady=10)
        
        # 进度信息
        self.status_var = tk.StringVar()
        self.status_var.set("准备就绪")
        tk.Label(top_frame, textvariable=self.status_var, bg="#e0e0e0", font=("SimHei", 10), fg="#333333").pack(side=tk.LEFT, padx=20, pady=10)
        
        # 图像显示区域
        self.image_frame = tk.Frame(self.root, bg="white", width=400, height=400, relief=tk.SUNKEN, bd=2)
        self.image_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.image_frame.pack_propagate(False)  # 防止框架大小被内容改变
        
        # 图像标签
        self.image_label = tk.Label(self.image_frame, bg="white")
        self.image_label.place(relx=0.5, rely=0.5, anchor=tk.CENTER)
        
        # 文件名显示
        self.filename_var = tk.StringVar()
        self.filename_var.set("等待加载图像...")
        tk.Label(self.root, textvariable=self.filename_var, bg="#f0f0f0", font=("SimHei", 10), fg="#666666").pack(pady=5)
        
        # 修改标签功能
        modify_frame = tk.Frame(self.root, bg="#f0f0f0")
        modify_frame.pack(fill=tk.X, padx=10, pady=5)
        
        tk.Label(modify_frame, text="修改标签为:", bg="#f0f0f0", font=("SimHei", 10)).pack(side=tk.LEFT, padx=5)
        self.new_label_combobox = ttk.Combobox(modify_frame, values=list(range(10)), width=5, font=("SimHei", 10))#type:ignore
        self.new_label_combobox.current(0)
        self.new_label_combobox.pack(side=tk.LEFT, padx=5)
        
        self.modify_btn = tk.Button(modify_frame, text="修改标签", command=self.modify_label, 
                                  bg="#4CAF50", fg="white", font=("SimHei", 10))
        self.modify_btn.pack(side=tk.LEFT, padx=10)
        
        # 操作提示
        tips = "操作提示: ← 上一个 | → 下一个 | Enter 确认正确 | Backspace 删除错误 | 数字键0-9 切换标签 | 修改标签按钮可移动文件"
        tk.Label(self.root, text=tips, bg="#f0f0f0", font=("SimHei", 9), fg="#0066cc").pack(pady=10)

    def bind_shortcuts(self):
        """绑定快捷键"""
        # 方向键导航
        self.root.bind('<Left>', self.prev_file)#type:ignore
        self.root.bind('<Right>', self.next_file)#type:ignore
        self.root.bind('<Left>', self.prev_file)#type:ignore
        self.root.bind('<Right>', self.next_file)#type:ignore
        
        # 确认和删除
        self.root.bind('<Return>', self.confirm_correct)#type:ignore
        self.root.bind('<BackSpace>', self.mark_as_incorrect)#type:ignore
        
        # 数字键切换标签
        for num in range(10):
            self.root.bind(str(num), self.switch_label_by_key)#type:ignore

    def switch_label_by_key(self, event:tk.Event):
        """通过数字键切换标签"""
        new_label = int(event.char)
        self.label_combobox.current(new_label)
        self.on_label_change()

    def on_label_change(self, event:Optional[tk.Event]=None):
        """标签改变时的处理"""
        self.current_label = int(self.label_combobox.get())
        self.current_file_index = 0
        self.load_current_file()

    def load_current_file(self):
        """加载当前标签下的当前文件"""
        # 获取当前标签的文件列表
        current_files = self.files_by_label.get(self.current_label, [])
        
        # 更新状态信息
        total = len(current_files)
        if total == 0:
            self.status_var.set(f"标签 {self.current_label}: 没有待处理文件")
            self.image_label.config(image="")
            self.filename_var.set("")
            return
            
        # 确保索引在有效范围内
        if self.current_file_index >= total:
            self.current_file_index = total - 1
        if self.current_file_index < 0:
            self.current_file_index = 0
            
        # 更新状态信息
        self.status_var.set(f"标签 {self.current_label}: {self.current_file_index + 1}/{total} 个文件 | 已确认: {len(self.verified_files)} | 错误: {len(self.error_files)}")
        
        # 加载并显示YUV文件
        current_filename = current_files[self.current_file_index]
        file_path = os.path.join(self.dataset_dir, str(self.current_label), current_filename)
        self.filename_var.set(f"文件名: {current_filename}")
        
        # 转换YUV为RGB并显示
        try:
            rgb_image = self.yuv_to_rgb(file_path)
            pil_img = Image.fromarray(rgb_image)
            # 调整图像大小以便显示
            display_img = pil_img.resize((320, 320), Image.LANCZOS)#type:ignore
            self.current_image = ImageTk.PhotoImage(image=display_img)
            self.image_label.config(image=self.current_image)
        except Exception as e:
            messagebox.showerror("错误", f"无法加载图像: {str(e)}")
            self.image_label.config(image="")

    def yuv_to_rgb(self, file_path:str):
        """将80x80的YUV422文件转换为RGB"""
        # 读取YUV数据
        with open(file_path, "rb") as f:
            yuv_data = np.fromfile(f, dtype=np.uint8)
        
        # 验证文件大小 (80x80x2 = 12800字节)
        expected_size = 80 * 80 * 2
        if len(yuv_data) != expected_size:
            raise ValueError(f"文件大小不正确，预期{expected_size}字节，实际{len(yuv_data)}字节")
        
        # 重塑为80x80x2的数组
        yuv_array = yuv_data.reshape(80, 80, 2)
        
        # 转换为RGB
        rgb = np.zeros((80, 80, 3), dtype=np.uint8)
        
        # YUV422 to RGB转换
        y0 = yuv_array[:, ::2, 0].astype(np.float32)
        u = yuv_array[:, ::2, 1].astype(np.float32) - 128
        y1 = yuv_array[:, 1::2, 0].astype(np.float32)
        v = yuv_array[:, 1::2, 1].astype(np.float32) - 128
        
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

    def prev_file(self, event:Optional[tk.Event]=None):
        """查看上一个文件"""
        if self.current_file_index > 0:
            self.current_file_index -= 1
            self.load_current_file()
        else:
            messagebox.showinfo("提示", "已经是第一个文件了")

    def next_file(self, event:Optional[tk.Event]=None):
        """查看下一个文件"""
        current_files = self.files_by_label.get(self.current_label, [])
        if self.current_file_index < len(current_files) - 1:
            self.current_file_index += 1
            self.load_current_file()
        else:
            messagebox.showinfo("提示", "已经是最后一个文件了")

    def confirm_correct(self, event:Optional[tk.Event]=None):
        """确认当前文件标签正确"""
        current_files = self.files_by_label.get(self.current_label, [])
        if not current_files or self.current_file_index >= len(current_files):
            return
            
        # 记录已确认的文件
        current_filename = current_files[self.current_file_index]
        file_identifier = os.path.join(str(self.current_label), current_filename)
        
        if file_identifier not in self.verified_files:
            with open(self.verified_log, "a", encoding="utf-8") as f:
                f.write(f"{file_identifier}\n")
            self.verified_files.append(file_identifier)
        
        # 移至下一个文件
        self.next_file()

    def modify_label(self):
        """修改当前文件标签并移动到新标签文件夹"""
        current_files = self.files_by_label.get(self.current_label, [])
        if not current_files or self.current_file_index >= len(current_files):
            return
            
        current_filename = current_files[self.current_file_index]
        new_label = int(self.new_label_combobox.get())
        
        if new_label == self.current_label:
            messagebox.showinfo("提示", "新标签与当前标签相同")
            return
            
        # 源文件和目标文件路径
        src_path = os.path.join(self.dataset_dir, str(self.current_label), current_filename)
        dest_dir = os.path.join(self.dataset_dir, str(new_label))
        dest_path = os.path.join(dest_dir, current_filename)
        
        # 确保目标目录存在
        if not os.path.exists(dest_dir):
            os.makedirs(dest_dir)
            
        try:
            # 移动文件
            os.rename(src_path, dest_path)
            
            # 记录修改
            file_identifier = os.path.join(str(self.current_label), current_filename)
            new_file_identifier = os.path.join(str(new_label), current_filename)
            
            
            # 更新已确认列表
            if file_identifier in self.verified_files:
                self.verified_files.remove(file_identifier)
                self.verified_files.append(new_file_identifier)
                with open(self.verified_log, "w", encoding="utf-8") as f:
                    f.write("\n".join(self.verified_files) + "\n")
            
            # 更新错误列表
            if file_identifier in self.error_files:
                self.error_files.remove(file_identifier)
                self.error_files.append(new_file_identifier)
                with open(self.error_log, "w", encoding="utf-8") as f:
                    f.write("\n".join(self.error_files) + "\n")
            
            # 更新文件列表
            self.files_by_label[self.current_label].pop(self.current_file_index)
            if new_label not in self.files_by_label:
                self.files_by_label[new_label] = []
            self.files_by_label[new_label].append(current_filename)
            
            # 重新加载当前文件
            self.load_current_file()
            
            messagebox.showinfo("成功", f"已将文件 {current_filename} 从标签 {self.current_label} 移动到标签 {new_label}")
            
        except Exception as e:
            messagebox.showerror("错误", f"修改标签失败: {str(e)}")

    def mark_as_incorrect(self, event:Optional[tk.Event]=None):
        """标记当前文件标签错误并删除"""
        current_files = self.files_by_label.get(self.current_label, [])
        if not current_files or self.current_file_index >= len(current_files):
            return
            
        current_filename = current_files[self.current_file_index]
        file_identifier = os.path.join(str(self.current_label), current_filename)
        file_path = os.path.join(self.dataset_dir, str(self.current_label), current_filename)
        
        # 确认删除
        if messagebox.askyesno("确认", f"确定要删除标签为 {self.current_label} 的文件 {current_filename} 吗？"):
            try:
                # 删除文件
                if os.path.exists(file_path):
                    os.remove(file_path)
                
                # 记录错误文件
                if file_identifier not in self.error_files:
                    with open(self.error_log, "a", encoding="utf-8") as f:
                        f.write(f"{file_identifier}\n")
                    self.error_files.append(file_identifier)
                
                # 更新文件列表并重新加载
                self.files_by_label[self.current_label].pop(self.current_file_index)
                self.load_current_file()
                
            except Exception as e:
                messagebox.showerror("错误", f"删除文件失败: {str(e)}")


if __name__ == "__main__":
    root = tk.Tk()
    app = DatasetVerifier(root)
    root.mainloop()
