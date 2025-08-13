# 修复过的文件夹

## 1.work文件夹

## 2.helloworld文件夹

## 3.peripherals/pwm_v2下多了一个pwm_config_channel_fixed文件夹是修复过的

## 4.pikapython只有命令行能够在上面跑，实际打包运行是不行的

## 5.tensorflowlite使用的版本很旧必须这样才能运行

```python
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
```

## 6.wifi/sta文件夹下可以正常使用auto_connect

## 7.peripherals/i2c文件夹下

i2c_40_bit实现了手工1转40然后能够正常使用

i2c_10_bit_fixed尝试使用库函数进行I2C发现不能使用

i2c_16_bit实现了手工1转16然后能够正常使用

## 8.putchar_getchar文件夹实现获得串口数据并且发送到串口

## 9.peripherals\spi\spi_dma_fixed实现spi两个板子传输信息

## 10.peripherals\pwm_v2\pwm_software实现软件pwm

## 11.peripherals\pwm_v2\pwm_software_40实现软件pwm并且加在一转40上面