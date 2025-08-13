# 使用了 tensor 环境，在使用前执行这行命令
# conda activate tensor
import tensorflow as tf  # type:ignore
from tensorflow.keras import layers, models  # type:ignore
import numpy as np
import os
import glob
from sklearn.model_selection import train_test_split  # type:ignore
from sklearn.metrics.pairwise import cosine_similarity  # type:ignore
from typing import Any

print(
    "可用GPU列表:", tf.config.experimental.list_physical_devices("GPU")  # type:ignore
)

# 配置参数
IMAGE_SHAPE = (80, 80)  # 图像尺寸: 80x80
NUM_CLASSES = 10  # 数字0-9
BATCH_SIZE = 32
EPOCHS = 30
LEARNING_RATE = 0.001
DATASET_DIR = os.path.join(os.getcwd(), "dataset")  # 当前文件夹下的dataset目录
NEURAL_DIR = os.path.join(os.getcwd(), "neural")  # 存放生成的C文件的目录
SCALE = 0.8
DELETE_SIMILARITY = 0.75
RETRY_PROBILITY = 0.9


def load_yuv_dataset() -> (
    tuple[np.ndarray[Any, np.dtype[np.uint8]], np.ndarray[Any, Any]]
):
    """加载YUV422格式数据集，并保证1-9类别数量相同
    0不受限制，1-9取最少类别数量为基准，数量多的类别保留相似项
    """
    # 按类别存储图像和标签
    class_images: dict[int, list[np.ndarray[Any, np.dtype[np.float64]]]] = {
        i: [] for i in range(10)
    }  # 0-9

    # 检查数据集目录是否存在
    if not os.path.exists(DATASET_DIR):
        raise ValueError(f"数据集目录不存在: {DATASET_DIR}")

    # 遍历0-9每个数字的文件夹
    for digit in range(10):
        digit_folder = os.path.join(DATASET_DIR, str(digit))

        if not os.path.exists(digit_folder):
            print(f"警告: 数字{digit}的文件夹不存在 - {digit_folder}")
            continue

        # 获取所有YUV格式文件
        yuv_files = glob.glob(os.path.join(digit_folder, "*.yuv")) + glob.glob(
            os.path.join(digit_folder, "*.raw")
        )

        if not yuv_files:
            print(f"警告: 数字{digit}的文件夹中没有找到YUV文件 - {digit_folder}")
            continue

        # 处理每个YUV文件
        for file_path in yuv_files:
            try:
                # 读取YUV422原始数据 (80x80x2)
                with open(file_path, "rb") as f:
                    yuv_data = np.fromfile(f, dtype=np.uint8)

                expected_size = IMAGE_SHAPE[0] * IMAGE_SHAPE[1] * 2
                if len(yuv_data) != expected_size:
                    print(
                        f"警告: 文件{file_path}大小不正确，应为{expected_size}字节，实际为{len(yuv_data)}字节，已跳过"
                    )
                    continue

                # 重塑并归一化
                yuv_img = yuv_data.reshape((IMAGE_SHAPE[0], IMAGE_SHAPE[1], 2))
                yuv_img = yuv_img / 255.0

                class_images[digit].append(yuv_img)

            except Exception as e:
                print(f"处理文件{file_path}时出错: {str(e)}，已跳过")
                continue

    # --------------------------
    # 平衡0-9类别的数量
    # --------------------------

    # 提取0-9类别的样本
    other_classes = {i: class_images[i] for i in range(10)}

    # 计算0-9类别的最小样本数（作为平衡基准）
    min_count = min(len(imgs) for imgs in other_classes.values() if len(imgs) > 0)
    if min_count == 0:
        raise ValueError("0-9中存在无样本的类别，无法平衡数据集")

    print(f"0-9类别将平衡至每个类别{min_count*SCALE}个样本")

    # 对每个超过基准数量的类别进行筛选，保留相似样本
    balanced_images: list[np.ndarray[Any, np.dtype[np.float64]]] = []
    balanced_labels: list[int] = []

    for digit in range(10):
        imgs = other_classes[digit]
        current_count = len(imgs)

        if current_count <= min_count * SCALE:
            # 样本数不足，全部保留
            balanced_images.extend(imgs)
            balanced_labels.extend([digit] * current_count)
            continue

        # 样本数过多，通过前后对比去除相似项
        # --------------------------
        # 1. 计算所有样本的特征（保留Y通道关键特征）
        features: list[list[Any]] = []
        for img in imgs:
            y_channel = img[:, :, 0]  # 取亮度通道
            # 提取更细致的结构特征，提升相似性判断准确性
            feat = [
                np.mean(y_channel),  # 整体亮度
                np.var(y_channel),  # 亮度分布
                np.mean(y_channel[:20, :]),  # 顶部区域亮度
                np.mean(y_channel[20:40, :]),  # 中上区域亮度
                np.mean(y_channel[40:60, :]),  # 中下区域亮度
                np.mean(y_channel[60:80, :]),  # 底部区域亮度
                np.mean(y_channel[:, :40]),  # 左半区域亮度
                np.mean(y_channel[:, 40:]),  # 右半区域亮度
            ]
            features.append(feat)
        features = np.array(features)  # type:ignore

        # 2. 计算样本间的余弦相似度（值越大越相似）
        sim_matrix = cosine_similarity(features)  # type:ignore

        # 3. 生成样本索引对，按相似度从高到低排序（只保留i < j的成对组合）
        pairs: list[tuple[int, int, Any]] = []
        for i in range(len(imgs)):
            for j in range(i + 1, len(imgs)):
                pairs.append((i, j, sim_matrix[i][j]))

        # 按相似度降序排序，优先处理最相似的样本对
        pairs.sort(key=lambda x: x[2], reverse=True)

        # 4. 迭代去除相似样本，直到达到目标数量
        keep_mask = np.ones(len(imgs), dtype=bool)  # 标记需要保留的样本
        current_count = len(imgs)
        target_count = int(min_count * SCALE)
        similarity_threshold = DELETE_SIMILARITY  # 相似度高于此值视为"高度相似"

        # 遍历所有相似对，去除其中一个样本
        for i, j, sim in pairs:
            # 若已达到目标数量，停止处理
            if current_count <= target_count:
                break

            # 若两个样本都还在保留列表中，且相似度足够高
            if keep_mask[i] and keep_mask[j] and sim > similarity_threshold:
                # 优先保留索引较小的样本（或可改为随机保留）
                keep_mask[j] = False
                current_count -= 1

        # 5. 如果仍未达到目标数量，随机去除剩余样本
        if current_count > target_count:
            remaining_indices = np.where(keep_mask)[0]
            # 随机选择需要去除的样本
            remove_indices = np.random.choice(
                remaining_indices, size=current_count - target_count, replace=False
            )
            keep_mask[remove_indices] = False

        # 6. 提取最终保留的样本
        selected_imgs = [imgs[i] for i in range(len(imgs)) if keep_mask[i]]

        balanced_images.extend(selected_imgs)
        balanced_labels.extend([digit] * len(selected_imgs))

    # 合并0类和平衡后的1-9类
    all_images = balanced_images
    all_labels = balanced_labels

    # 打乱顺序
    combined: list[Any] = list(zip(all_images, all_labels))
    np.random.shuffle(combined)
    all_images, all_labels = zip(*combined)

    return np.array(all_images), np.array(all_labels)


def build_model(input_shape: Any) -> Any:
    inputs = layers.Input(shape=input_shape)  # type:ignore

    # 优化的轻量级模型结构
    # 第一层: 深度可分离卷积 + BatchNorm
    x = layers.SeparableConv2D(  # type:ignore
        8, (3, 3), padding="same", use_bias=False
    )(inputs)
    x = layers.BatchNormalization()(x)  # type:ignore
    x = layers.LeakyReLU(alpha=0.1)(x)  # type:ignore
    x = layers.MaxPooling2D((2, 2))(x)  # type:ignore

    # 第二层: 深度可分离卷积 + BatchNorm + 残差连接
    residual = x  # type:ignore
    x = layers.SeparableConv2D(  # type:ignore
        16, (3, 3), padding="same", use_bias=False
    )(x)
    x = layers.BatchNormalization()(x)  # type:ignore
    x = layers.LeakyReLU(alpha=0.1)(x)  # type:ignore
    x = layers.MaxPooling2D((2, 2))(x)  # type:ignore

    # 第三层: 深度可分离卷积 + BatchNorm
    x = layers.SeparableConv2D(  # type:ignore
        32, (3, 3), padding="same", use_bias=False
    )(x)
    x = layers.BatchNormalization()(x)  # type:ignore
    x = layers.LeakyReLU(alpha=0.1)(x)  # type:ignore
    x = layers.GlobalAveragePooling2D()(x)  # type:ignore

    # 全连接层
    x = layers.Dense(32, activation="relu")(x)  # type:ignore
    x = layers.Dropout(0.2)(x)  # type:ignore

    # 输出层
    outputs = layers.Dense(NUM_CLASSES, activation="softmax")(x)  # type:ignore

    model = models.Model(inputs=inputs, outputs=outputs)  # type:ignore

    # 编译模型（使用标准损失函数）
    model.compile(  # type:ignore
        optimizer=tf.keras.optimizers.Adam(learning_rate=LEARNING_RATE),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(),
        metrics=["accuracy"],
    )

    return model  # type:ignore


def export_model_to_c(tflite_model_data: bytes, output_dir: str):
    """
    将TFLite模型导出为C语言源文件和头文件
    模型数据将存储为一个常量字节数组，便于嵌入式环境使用
    """
    # 创建输出目录
    include_dir = os.path.join(output_dir, "include")
    src_dir = os.path.join(output_dir, "src")

    os.makedirs(include_dir, exist_ok=True)
    os.makedirs(src_dir, exist_ok=True)

    # 头文件路径和源文件路径
    source_path = os.path.join(src_dir, "neural_model.c")

    # 生成源文件，将模型数据存储为C数组
    with open(source_path, "w", encoding="utf-8") as f:
        f.write('#include "neural_model.h"\n\n')
        f.write("/* 数字识别模型数据 - 自动生成 */\n")
        f.write("const uint8_t neural_model_data[] = {\n")

        # 将模型数据转换为C数组格式
        for i, byte in enumerate(tflite_model_data):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 16 == 0:
                f.write("\n")

        f.write("\n};\n\n")
        f.write(f"const size_t neural_model_size = sizeof(neural_model_data);\n\n")

        # 实现获取模型数据的函数
        f.write("const uint8_t* get_neural_model_data(void) {\n")
        f.write("    return neural_model_data;\n")
        f.write("}\n\n")

        f.write("size_t get_neural_model_size(void) {\n")
        f.write("    return neural_model_size;\n")
        f.write("}\n")

    print(f"模型已导出为C文件:")
    print(f"源文件: {source_path}")


def main() -> None:
    try:
        # 加载并预处理数据
        print(f"从 {DATASET_DIR} 加载YUV422数据集...")
        images, labels = load_yuv_dataset()

        # 划分训练集和验证集 (80%训练, 20%验证)
        X_train, X_val, y_train, y_val = train_test_split(  # type:ignore
            images, labels, test_size=0.2, random_state=42, stratify=labels
        )

        print(
            f"加载完成 - 训练集: {X_train.shape[0]} 张图像, 验证集: {X_val.shape[0]} 张图像"  # type:ignore
        )
        print(f"图像形状: {X_train.shape[1:]} (高度, 宽度, 通道数)")  # type:ignore

        # 构建模型 - 输入形状为 (80, 80, 2)
        model = build_model((IMAGE_SHAPE[0], IMAGE_SHAPE[1], 2))
        model.summary()

        # 添加回调函数
        callbacks = [  # type:ignore
            tf.keras.callbacks.EarlyStopping(
                patience=5, restore_best_weights=True, verbose=1
            ),
            tf.keras.callbacks.ModelCheckpoint(
                "best_yuv_model.h5", save_best_only=True
            ),
            tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=3, verbose=1),
        ]

        # 训练模型
        print("开始训练模型...")
        history = model.fit(  # type:ignore
            X_train,
            y_train,
            batch_size=BATCH_SIZE,
            epochs=EPOCHS,
            validation_data=(X_val, y_val),
            callbacks=callbacks,
        )

        # 评估模型
        val_loss, val_acc = model.evaluate(X_val, y_val)  # type:ignore
        print(f"验证集准确率: {val_acc:.4f}")

        if val_acc < RETRY_PROBILITY:
            return main()

        # 转换为TensorFlow Lite模型并使用全整数量化
        converter = tf.lite.TFLiteConverter.from_keras_model(model)  # type:ignore

        # 设置全整数量化
        converter.optimizations = [tf.lite.Optimize.DEFAULT]  # type:ignore
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]

        # 设置输入输出为int8类型
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8

        # 提供代表性数据集用于校准量化参数
        def representative_dataset():  # type:ignore
            for i in range(100):
                yield [X_train[i : i + 1].astype(np.float32)]  # type:ignore

        converter.representative_dataset = representative_dataset  # type:ignore

        tflite_model = converter.convert()  # type:ignore

        # 保存TFLite模型
        tflite_file = "digit_recognition_yuv.tflite"
        with open(tflite_file, "wb") as f:
            f.write(tflite_model)  # type:ignore

        print(
            f"TensorFlow Lite模型已保存为: {tflite_file} (大小: {len(tflite_model)}字节)"  # type:ignore
        )

        # 导出为C语言文件
        export_model_to_c(tflite_model, NEURAL_DIR)  # type:ignore

    except Exception as e:
        print(f"程序出错: {str(e)}")


if __name__ == "__main__":
    main()
