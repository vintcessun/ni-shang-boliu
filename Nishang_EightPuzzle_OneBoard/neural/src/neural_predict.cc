#include <math.h>
#include "neural_predict.h"
#include "neural_model.h"
#include "dso_problem.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "rtos.h"

#define DBG_TAG "NEURAL"
#include "log.h"

// TensorFlow Lite模型解释器和相关资源
namespace {
	tflite::MicroErrorReporter *error_reporter = nullptr;
	const tflite::Model* model = nullptr;
	tflite::MicroInterpreter* interpreter = nullptr;
	TfLiteTensor* input = nullptr;
	TfLiteTensor* output = nullptr;
	
	// 定义Tensor Arena大小 (根据模型大小调整)
}  // namespace

constexpr uint64_t kTensorArenaSize = 110 * 1024;
uint8_t *tensor_arena;

int neural_predict_init(void) {
	tensor_arena = (uint8_t*) pvPortMalloc(sizeof(uint8_t) * kTensorArenaSize);

	if (interpreter && input && output) {
		return 0;
	}

    tflite::InitializeTarget();

    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

	// 加载模型
	if (!neural_model_data) {
		printf("错误: 模型数据指针为空\n");
		return -1;
	}
	
	model = tflite::GetModel(neural_model_data);
	if (!model) {
		printf("错误: 模型加载失败\n");
		return -1;
	}
	
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		printf("模型版本错误: 期望 %d 实际 %d\n", 
			TFLITE_SCHEMA_VERSION, model->version());
		return -1;
	}
	
	printf("模型加载成功，大小: %d 字节\n", neural_model_size);

	// 注册所有操作
	static tflite::AllOpsResolver resolver;

	// 创建解释器
	static tflite::MicroInterpreter static_interpreter(
		model, resolver, tensor_arena, kTensorArenaSize,
		error_reporter);
	interpreter = &static_interpreter;

	printf("开始分配张量\n");
	// 分配张量
	TfLiteStatus allocate_status = interpreter->AllocateTensors();
	if (allocate_status != kTfLiteOk) {
		printf("获取张量错误: %d\n", allocate_status);
		printf("模型大小: %d\n", neural_model_size);
		printf("Tensor Arena大小: %d\n", kTensorArenaSize);
		return -2;
	}
	printf("分配成功\n");

	// 获取输入输出张量
	input = interpreter->input(0);
	output = interpreter->output(0);

	return 0;
}

int neural_predict(const uint8_t* yuv_data) {
	if (!interpreter || !input || !output) {
		return -1;
	}

	// 预处理YUV422数据 (80x80x2) - 确保正确处理int8量化
	if (input->type != kTfLiteInt8) {
		printf("错误: 输入张量类型应为int8\n");
		return -3;
	}
	
	// 检查量化参数
	if (input->params.scale == 0.0f) {
		printf("错误: 输入量化参数未正确设置\n");
		return -5;
	}
	
	// 量化处理: 按照Python相同流程处理
	int8_t* input_data = input->data.int8;
	double input_scale = input->params.scale;
	int32_t input_zero_point = input->params.zero_point;
	
	for (int i = 0; i < 80 * 80 * 2; i++) {
		// 1. 归一化到[0,1]
		double normalized = yuv_data[i] / 255.0f;
		// 2. 应用量化参数: /scale + zero_point
		double quantized = normalized / input_scale + input_zero_point;
		// 3. 四舍五入转为int8
		input_data[i] = round(quantized);
	}

	// 执行推理

	//LOG_I("开始推理");
	if (interpreter->Invoke() != kTfLiteOk) {
		return -2;
	}
	//LOG_I("推理结束");

	// 获取预测结果 (检查输出类型和量化参数)
	if (output->type != kTfLiteInt8) {
		printf("错误: 输出张量类型应为int8\n");
		return -4;
	}
	
	if (output->params.scale == 0.0f) {
		printf("错误: 输出量化参数未正确设置\n");
		return -6;
	}

	// 调试打印输出张量形状信息
	//printf("输出张量维度数: %d\n", output->dims->size);
	//for (int i = 0; i < output->dims->size; i++) {
	//	printf("维度 %d 大小: %d\n", i, output->dims->data[i]);
	//}
	
	int8_t* output_data = output->data.int8;
	double output_scale = output->params.scale;
	int32_t output_zero_point = output->params.zero_point;
	
	// 反量化处理输出
	double max_prob = -1000.0f;
	int prediction = 0;
	for (int i = 0; i < output->dims->data[1]; i++) {
		// 反量化: (output - zero_point) * scale
		double dequantized = (output_data[i] - output_zero_point) * output_scale;
		//printf("%d 预测概率为 %.4f\n", i, dequantized);
		if (dequantized > max_prob) {
			max_prob = dequantized;
			prediction = i;
		}
	}
	//printf("Predicted is %d (Value = %.4f)\n", prediction, max_prob);

	return prediction;
}
