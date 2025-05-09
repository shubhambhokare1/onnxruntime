// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "gtest/gtest.h"

#include "test/providers/provider_test_utils.h"
#include "test/providers/run_options_config_keys.h"
#include "test/common/dnnl_op_test_utils.h"
#include "test/common/cuda_op_test_utils.h"
#include "test/common/tensor_op_test_utils.h"
#include "default_providers.h"

namespace onnxruntime {
namespace test {

namespace {

const onnxruntime::RunOptions run_options = []() {
  onnxruntime::RunOptions options{};
  ORT_THROW_IF_ERROR(options.config_options.AddConfigEntry(kOpTesterRunOptionsConfigTestTunableOp, "true"));
  return options;
}();

const constexpr auto run_with_tunable_op = &run_options;

}  // namespace

template <typename T>
struct MatMulTestData {
  std::string name;
  std::vector<int64_t> input0_dims;
  std::vector<int64_t> input1_dims;
  std::vector<int64_t> expected_dims;
  std::vector<T> expected_vals;
};

template <typename T>
std::vector<MatMulTestData<T>> GenerateTestCases() {
  std::vector<MatMulTestData<T>> test_cases;

  auto real_expected_vals = [](const std::vector<int32_t>& expected_vals) {
    if constexpr (std::is_same_v<T, int32_t>) {
      return expected_vals;
    } else if constexpr (std::is_same_v<T, MLFloat16>) {
      std::vector<MLFloat16> expected_vals_fp16(expected_vals.size());
      std::transform(expected_vals.begin(), expected_vals.end(), expected_vals_fp16.begin(),
                     [](int32_t num) { return MLFloat16(float(num)); });
      return expected_vals_fp16;
    } else {
      std::vector<T> real_expected_vals(expected_vals.size());
      std::transform(expected_vals.begin(), expected_vals.end(), real_expected_vals.begin(),
                     [](int32_t num) { return static_cast<T>(num); });
      return real_expected_vals;
    }
  };

  test_cases.push_back(
      {"test padding and broadcast A > B",
       {3, 1, 1, 2},
       {2, 2, 2},
       {3, 2, 1, 2},
       real_expected_vals({2, 3, 6, 7, 6, 11, 26, 31, 10, 19, 46, 55})});

  test_cases.push_back(
      {"test padding and broadcast B > A",
       {2, 3, 2},
       {3, 2, 2, 1},
       {3, 2, 3, 1},
       real_expected_vals({1, 3, 5, 33, 43, 53, 5, 23, 41, 85, 111, 137, 9, 43, 77, 137, 179, 221})});

  test_cases.push_back(
      {"test left 1D",
       {2},
       {3, 2, 1},
       {3, 1},
       real_expected_vals({1, 3, 5})});

  test_cases.push_back(
      {"test right 1D",
       {3, 1, 2},
       {2},
       {3, 1},
       real_expected_vals({1, 3, 5})});

  test_cases.push_back(
      {"test left 1D right 2D",
       {2},
       {2, 3},
       {3},
       real_expected_vals({3, 4, 5})});

  test_cases.push_back(
      {"test scalar output",
       {3},
       {3},
       {},
       real_expected_vals({5})});

  test_cases.push_back(
      {"test 2D",
       {3, 4},
       {4, 3},
       {3, 3},
       real_expected_vals({42, 48, 54, 114, 136, 158, 186, 224, 262})});

  test_cases.push_back(
      {"test 2D special",
       {2, 2, 3},
       {3, 4},
       {2, 2, 4},
       real_expected_vals({20, 23, 26, 29, 56, 68, 80, 92, 92, 113, 134, 155, 128, 158, 188, 218})});

  test_cases.push_back(
      {"test 2D special 2",
       {2, 2, 3},
       {1, 3, 4},
       {2, 2, 4},
       real_expected_vals({20, 23, 26, 29, 56, 68, 80, 92, 92, 113, 134, 155, 128, 158, 188, 218})});

  test_cases.push_back(
      {"test 2D special 3",
       {2, 6},
       {1, 1, 6, 1},
       {1, 1, 2, 1},
       real_expected_vals({55, 145})});

  test_cases.push_back(
      {"test 2D empty input",
       {3, 4},
       {4, 0},
       {3, 0},
       real_expected_vals({})});

  test_cases.push_back(
      {"test 3D batch",
       {3, 1, 3},
       {3, 3, 2},
       {3, 1, 2},
       real_expected_vals({
           // clang-format off
            10,  13,
           100, 112,
           298, 319,
           // clang-format on
       })});

  test_cases.push_back(
      {"test 4D batch",
       {2, 2, 1, 3},
       {2, 2, 3, 2},
       {2, 2, 1, 2},
       real_expected_vals({
           // clang-format off
            10,  13,
           100, 112,
           298, 319,
           604, 634,
           // clang-format on
       })});

#ifdef USE_WEBGPU
  test_cases.push_back(
      {"test 3D tensors with M = 1",
       {6, 1, 8},
       {1, 8, 3},
       {6, 1, 3},
       real_expected_vals({
           // clang-format off
            420, 448, 476,
            1092, 1184, 1276,
            1764, 1920, 2076,
            2436, 2656, 2876,
            3108, 3392, 3676,
            3780, 4128, 4476,
           // clang-format on
       })});

  test_cases.push_back(
      {"test 4D tensors with M = 1",
       {2, 3, 1, 8},
       {1, 1, 8, 3},
       {2, 3, 1, 3},
       real_expected_vals({420, 448, 476, 1092, 1184, 1276, 1764, 1920, 2076, 2436, 2656, 2876, 3108, 3392, 3676, 3780, 4128, 4476})});

  test_cases.push_back(
      {"test 4D tensors",
       {2, 3, 4, 3},
       {2, 3, 3, 5},
       {2, 3, 4, 5},
       real_expected_vals({
           // clang-format off
          25, 28, 31, 34, 37, 70, 82, 94, 106, 118, 115, 136, 157, 178, 199, 160, 190, 220,
          250, 280, 790, 829, 868, 907, 946, 970, 1018, 1066, 1114, 1162, 1150, 1207, 1264,
          1321, 1378, 1330, 1396, 1462, 1528, 1594, 2635, 2710, 2785, 2860, 2935, 2950, 3034,
          3118, 3202, 3286, 3265, 3358, 3451, 3544, 3637, 3580, 3682, 3784, 3886, 3988, 5560,
          5671, 5782, 5893, 6004, 6010, 6130, 6250, 6370, 6490, 6460, 6589, 6718, 6847, 6976,
          6910, 7048, 7186, 7324, 7462, 9565, 9712, 9859, 10006, 10153, 10150, 10306, 10462,
          10618, 10774, 10735, 10900, 11065, 11230, 11395, 11320, 11494, 11668, 11842, 12016,
          14650, 14833, 15016, 15199, 15382, 15370, 15562, 15754, 15946, 16138, 16090, 16291,
          16492, 16693, 16894, 16810, 17020, 17230, 17440, 17650
           // clang-format on
       })});

  // Test case: multiplies 2D broadcasted to 4D tensors
  test_cases.push_back(
      {"test 2D broadcasted to 4D tensors",
       {2, 4},
       {4, 3, 2, 4, 2},
       {4, 3, 2, 2, 2},
       real_expected_vals({
           // clang-format off
          28, 34, 76, 98, 76, 82, 252, 274, 124, 130, 428, 450, 172, 178, 604, 626,
          220, 226, 780, 802, 268, 274, 956, 978, 316, 322, 1132, 1154, 364, 370,
          1308, 1330, 412, 418, 1484, 1506, 460, 466, 1660, 1682, 508, 514, 1836,
          1858, 556, 562, 2012, 2034, 604, 610, 2188, 2210, 652, 658, 2364, 2386,
          700, 706, 2540, 2562, 748, 754, 2716, 2738, 796, 802, 2892, 2914, 844,
          850, 3068, 3090, 892, 898, 3244, 3266, 940, 946, 3420, 3442, 988, 994,
          3596, 3618, 1036, 1042, 3772, 3794, 1084, 1090, 3948, 3970, 1132, 1138,
          4124, 4146
           // clang-format on
       })});

  // Test case: multiplies 4D broadcasted to 5D tensors
  test_cases.push_back(
      {"test 4D broadcasted to 5D tensors",
       {3, 1, 2, 4},
       {4, 3, 2, 4, 2},
       {4, 3, 2, 2, 2},
       real_expected_vals({
           // clang-format off
            28, 34, 76, 98, 76, 82, 252, 274, 732, 770, 1036, 1090, 1036, 1074, 1468,
            1522, 2460, 2530, 3020, 3106, 3020, 3090, 3708, 3794, 316, 322, 1132,
            1154, 364, 370, 1308, 1330, 2556, 2594, 3628, 3682, 2860, 2898, 4060,
            4114, 5820, 5890, 7148, 7234, 6380, 6450, 7836, 7922, 604, 610, 2188,
            2210, 652, 658, 2364, 2386, 4380, 4418, 6220, 6274, 4684, 4722, 6652,
            6706, 9180, 9250, 11276, 11362, 9740, 9810, 11964, 12050, 892, 898, 3244,
            3266, 940, 946, 3420, 3442, 6204, 6242, 8812, 8866, 6508, 6546, 9244,
            9298, 12540, 12610, 15404, 15490, 13100, 13170, 16092, 16178

           // clang-format on
       })});

  // Test case: same ranks different broadcast small 1
  test_cases.push_back(
      {"test same ranks different broadcast small 1",
       {2, 1, 2, 2},
       {1, 2, 2, 1},
       {2, 2, 2, 1},
       real_expected_vals({1, 3, 3, 13, 5, 7, 23, 33})});

  // Test case: same ranks different broadcast larger 0
  test_cases.push_back(
      {"test same ranks different broadcast larger 0",
       {1, 2, 2, 8},
       {2, 1, 8, 1},
       {2, 2, 2, 1},
       real_expected_vals({140, 364, 588, 812, 364, 1100, 1836, 2572})});

  // Test case: same ranks different broadcast larger 1
  test_cases.push_back(
      {"test same ranks different broadcast larger 1",
       {2, 1, 2, 8},
       {1, 2, 8, 1},
       {2, 2, 2, 1},
       real_expected_vals({140, 364, 364, 1100, 588, 812, 1836, 2572})});
#endif
  return test_cases;
}

template <typename T>
void RunMatMulTest(int32_t opset_version, bool is_a_constant, bool is_b_constant) {
  for (auto t : GenerateTestCases<T>()) {
    SCOPED_TRACE("test case: " + t.name);

    OpTester test("MatMul", opset_version);

    int64_t size0 = TensorShape::FromExistingBuffer(t.input0_dims).SizeHelper(0, t.input0_dims.size());
    std::vector<T> input0_vals = ValueRange<T>(size0);
    test.AddInput<T>("A", t.input0_dims, input0_vals, is_a_constant);

    int64_t size1 = TensorShape::FromExistingBuffer(t.input1_dims).SizeHelper(0, t.input1_dims.size());
    std::vector<T> input1_vals = ValueRange<T>(size1);
    test.AddInput<T>("B", t.input1_dims, input1_vals, is_b_constant);

    test.AddOutput<T>("Y", t.expected_dims, t.expected_vals);

    // OpenVINO EP: Disabled temporarily matmul broadcasting not fully supported
    // Disable TensorRT because of unsupported data type
    // QNN EP: Crash during graph execution for QNN's CPU backend on QNN SDK 2.22. Not a problem for QNN's HTP backend.
    std::unordered_set<std::string> excluded_providers{kTensorrtExecutionProvider,
                                                       kOpenVINOExecutionProvider,
                                                       kQnnExecutionProvider};
    if (t.name == "test 2D empty input") {
      // NNAPI: currently fails for the "test 2D empty input" case
      excluded_providers.insert(kNnapiExecutionProvider);
    }

    // TODO:: Change MatMulNaive Shader to support these test cases webgpu
    std::unordered_set<std::string> webgpu_excluded_test_cases{
        "test left 1D",
        "test right 1D",
        "test 2D empty input"};

    // if test in webgpu_excluded_test_cases, add webgpu to excluded_providers
    if (webgpu_excluded_test_cases.find(t.name) != webgpu_excluded_test_cases.end()) {
      excluded_providers.insert(kWebGpuExecutionProvider);
    }

    test.ConfigExcludeEps(excluded_providers)
        .Config(run_with_tunable_op)
        .RunWithConfig();
  }
}

template <typename T>
void RunMatMulTest(int32_t opset_version) {
  RunMatMulTest<T>(opset_version, false, false);
}

TEST(MathOpTest, MatMulFloatType) {
  // TODO: Unskip when fixed #41968513
  if (DefaultDmlExecutionProvider().get() != nullptr) {
    GTEST_SKIP() << "Skipping because of the following error: Assertion failed: m_bufferTensorDesc.TotalTensorSizeInBytes >= ComputeByteSizeFromDimensions(nonBroadcastDimensions, dataType)";
  }
  RunMatMulTest<float>(7, false, false);
  // Note. Xnnpack only supports matmul when Matrix B is constant
  RunMatMulTest<float>(7, false, true);
}

#if defined(USE_CUDA) || defined(USE_ROCM) || defined(USE_COREML) || defined(USE_XNNPACK)
TEST(MathOpTest, MatMulFloat16) {
#ifdef USE_CUDA
  int min_cuda_architecture = 530;
  if (!HasCudaEnvironment(min_cuda_architecture)) {
    LOGS_DEFAULT(WARNING) << "Hardware NOT support FP16";
    return;
  }
#endif
  // TODO: Unskip when fixed #41968513
  if (DefaultDmlExecutionProvider().get() != nullptr) {
    GTEST_SKIP() << "Skipping because of the following error: Assertion failed: m_bufferTensorDesc.TotalTensorSizeInBytes >= ComputeByteSizeFromDimensions(nonBroadcastDimensions, dataType)";
  }
  RunMatMulTest<MLFloat16>(14, false, false);
  // Note. Xnnpack only supports matmul when Matrix B is constant
  RunMatMulTest<MLFloat16>(14, false, true);
}
#endif

TEST(MathOpTest, MatMulDoubleType) {
  RunMatMulTest<double>(7);
}

TEST(MathOpTest, MatMulInt32Type) {
  // Webgpu does not support int32 matmul
  if (DefaultWebGpuExecutionProvider().get() != nullptr) {
    GTEST_SKIP() << "Skipping because of the following error: Webgpu does not support int32 matmul";
  }
  RunMatMulTest<int32_t>(9);
}

TEST(MathOpTest, MatMulUint32Type) {
  // Webgpu does not support uint32 matmul
  if (DefaultWebGpuExecutionProvider().get() != nullptr) {
    GTEST_SKIP() << "Skipping because of the following error: Webgpu does not support uint32 matmul";
  }
  RunMatMulTest<uint32_t>(9);
}

TEST(MathOpTest, MatMulInt64Type) {
  RunMatMulTest<int64_t>(9);
}

TEST(MathOpTest, MatMulUint64Type) {
  RunMatMulTest<uint64_t>(9);
}

template <typename T>
void RunMatMulZeroKTest() {
  // test with empty inputs and zero filled output
  constexpr const std::array<T, 0> empty_input{};
  const std::vector<T> expected_output(4 * 4, T{});
  OpTester test("MatMul", 13);

  test.AddInput<T>("A", {4, 0}, empty_input);
  test.AddInput<T>("B", {0, 4}, empty_input);
  test.AddOutput<T>("Y", {4, 4}, expected_output);

  // No special case is implemented.
  test.ConfigExcludeEps({kCoreMLExecutionProvider, kNnapiExecutionProvider,
                         kDmlExecutionProvider, kDnnlExecutionProvider, kQnnExecutionProvider,
                         kOpenVINOExecutionProvider, kWebGpuExecutionProvider})
      .Config(run_with_tunable_op)
      .RunWithConfig();
}

TEST(MathOpTest, MatMulZeroKFloatType) {
  if (DefaultWebGpuExecutionProvider().get() != nullptr) {
    GTEST_SKIP() << "Skipping because of the following error: Webgpu does not support zero-sized tensor";
  }
  RunMatMulZeroKTest<float>();
}

TEST(MathOpTest, MatMulZeroKInt32Type) {
  if (DefaultWebGpuExecutionProvider().get() != nullptr) {
    GTEST_SKIP() << "Skipping because of the following error: Webgpu does not support zero-sized tensor";
  }
  RunMatMulZeroKTest<int32_t>();
}

#if defined(USE_CUDA) || defined(USE_ROCM) || defined(USE_COREML) || defined(USE_XNNPACK)
TEST(MathOpTest, MatMul_Float16) {
#ifdef USE_CUDA
  int min_cuda_architecture = 530;
  if (!HasCudaEnvironment(min_cuda_architecture)) {
    LOGS_DEFAULT(WARNING) << "Hardware NOT support FP16";
    return;
  }
#endif
  std::vector<float> A{1.0f, 2.0f, 3.0f, 4.0f,
                       -1.0f, -2.0f, -3.0f, -4.0f};
  std::vector<float> B(12, 1.0f);
  std::vector<float> Y{10.0f, 10.0f, 10.0f,
                       -10.0f, -10.0f, -10.0f};

  std::vector<MLFloat16> f_A(8);
  std::vector<MLFloat16> f_B(12);
  std::vector<MLFloat16> f_Y(6);
  ConvertFloatToMLFloat16(A.data(), f_A.data(), 8);
  ConvertFloatToMLFloat16(B.data(), f_B.data(), 12);
  ConvertFloatToMLFloat16(Y.data(), f_Y.data(), 6);

  auto run_test = [&](bool B_is_constant) {
    // it needs Matrix B as constant to test XNNPack
    OpTester test("MatMul", 14);
    test.AddInput<MLFloat16>("A", {2, 4}, f_A);
    test.AddInput<MLFloat16>("B", {4, 3}, f_B, B_is_constant);
    test.AddOutput<MLFloat16>("Y", {2, 3}, f_Y);
    test.ConfigExcludeEps({kTensorrtExecutionProvider})  // TensorRT: fp16 is not supported
        .Config(run_with_tunable_op)
        .RunWithConfig();
  };
  run_test(true);
  run_test(false);
}
#endif

#if defined(USE_CUDA) || defined(USE_ROCM) || defined(USE_DNNL)
TEST(MathOpTest, MatMul_bfloat16) {
#ifdef USE_CUDA
  int min_cuda_architecture = 530;
  if (!HasCudaEnvironment(min_cuda_architecture)) {
    LOGS_DEFAULT(WARNING) << "Hardware NOT support BFP16";
    return;
  }
#endif
#ifdef USE_DNNL
  if (!DnnlHasBF16Support()) {
    LOGS_DEFAULT(WARNING) << "Hardware does NOT support BF16";
    return;
  }
#endif
  OpTester test("MatMul", 14);

  test.AddInput<BFloat16>("A", {2, 4}, MakeBFloat16({1.0f, 2.0f, 3.0f, 4.0f, -1.0f, -2.0f, -3.0f, -4.0f}));
  test.AddInput<BFloat16>("B", {4, 3}, MakeBFloat16({1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f}));
  test.AddOutput<BFloat16>("Y", {2, 3}, MakeBFloat16({10.0f, 10.0f, 10.0f, -10.0f, -10.0f, -10.0f}));
  std::vector<std::unique_ptr<IExecutionProvider>> execution_providers;
  test.Config(run_with_tunable_op);
#ifdef USE_CUDA
  execution_providers.emplace_back(DefaultCudaExecutionProvider());
#elif USE_ROCM
  execution_providers.emplace_back(DefaultRocmExecutionProvider(/*test_tunable_op=*/true));
  test.ConfigEps(std::move(execution_providers))
      .RunWithConfig();

  execution_providers.clear();
  execution_providers.emplace_back(DefaultRocmExecutionProvider(/*test_tunable_op=*/false));
#elif USE_DNNL
  execution_providers.emplace_back(DefaultDnnlExecutionProvider());
#endif
  test.ConfigEps(std::move(execution_providers))
      .RunWithConfig();
}
#endif

#ifndef ENABLE_TRAINING
// Prepacking is disabled in full training build so no need to test the feature in a training build.
TEST(MathOpTest, MatMulSharedPrepackedWeights) {
  OpTester test("MatMul");

  std::vector<float> b_init_values(12, 1.0f);
  test.AddInput<float>("A", {2, 4},
                       {1.0f, 2.0f, 3.0f, 4.0f,
                        -1.0f, -2.0f, -3.0f, -4.0f});
  // B is to be an initializer for triggering pre-packing
  test.AddInput<float>("B", {4, 3}, b_init_values, true);

  test.AddOutput<float>("Y", {2, 3},
                        {10.0f, 10.0f, 10.0f,
                         -10.0f, -10.0f, -10.0f});

  OrtValue b;
  Tensor::InitOrtValue(DataTypeImpl::GetType<float>(), TensorShape({4, 3}),
                       b_init_values.data(), OrtMemoryInfo(CPU, OrtAllocatorType::OrtDeviceAllocator), b);

  SessionOptions so;
  // Set up B as a shared initializer to be shared between sessions
  ASSERT_EQ(so.AddInitializer("B", &b), Status::OK());

  // We want all sessions running using this OpTester to be able to share pre-packed weights if applicable
  test.EnableSharingOfPrePackedWeightsAcrossSessions();

  // Pre-packing is limited just to the CPU EP for now and we will only test the CPU EP
  // and we want to ensure that it is available in this build
  auto cpu_ep = []() -> std::vector<std::unique_ptr<IExecutionProvider>> {
    std::vector<std::unique_ptr<IExecutionProvider>> execution_providers;
    execution_providers.push_back(DefaultCpuExecutionProvider());
    return execution_providers;
  };

  size_t number_of_pre_packed_weights_counter_session_1 = 0;
  size_t number_of_shared_pre_packed_weights_counter = 0;

  // Session 1
  {
    test.Config(so)
        .Config(run_with_tunable_op)
        .ConfigEps(cpu_ep())
        .RunWithConfig(&number_of_pre_packed_weights_counter_session_1, &number_of_shared_pre_packed_weights_counter);
    // Assert that no pre-packed weights have been shared thus far
    ASSERT_EQ(number_of_shared_pre_packed_weights_counter, static_cast<size_t>(0));
  }

  auto number_of_elements_in_shared_prepacked_buffers_container =
      test.GetNumPrePackedWeightsShared();
  // Assert that the number of elements in the shared container
  // is the same as the number of weights that have been pre-packed
  ASSERT_EQ(number_of_pre_packed_weights_counter_session_1, number_of_elements_in_shared_prepacked_buffers_container);

  // On some platforms/architectures MLAS may choose to not do any pre-packing and the number of elements
  // that have been pre-packed will be zero in which case we do not continue with the testing
  // of "sharing" of pre-packed weights as there are no pre-packed weights to be shared at all.
  if (number_of_pre_packed_weights_counter_session_1 == 0)
    return;

  // Session 2
  {
    size_t number_of_pre_packed_weights_counter_session_2 = 0;
    test.Config(so)
        .Config(run_with_tunable_op)
        .ConfigEps(cpu_ep())
        .RunWithConfig(&number_of_pre_packed_weights_counter_session_2, &number_of_shared_pre_packed_weights_counter);

    // Assert that the same number of weights were pre-packed in both sessions
    ASSERT_EQ(number_of_pre_packed_weights_counter_session_1, number_of_pre_packed_weights_counter_session_2);

    // Assert that the number of pre-packed weights that were shared equals
    // the number of pre-packed weights in the second session
    ASSERT_EQ(number_of_pre_packed_weights_counter_session_2,
              static_cast<size_t>(number_of_shared_pre_packed_weights_counter));
  }
}

#endif

}  // namespace test
}  // namespace onnxruntime
