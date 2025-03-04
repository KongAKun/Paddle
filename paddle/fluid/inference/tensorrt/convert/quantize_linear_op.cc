
/* Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/inference/tensorrt/convert/op_converter.h"

namespace paddle {
namespace inference {
namespace tensorrt {

class QuantizeLinearOpConverter : public OpConverter {
 public:
  void operator()(const framework::proto::OpDesc& op,
                  const framework::Scope& scope,
                  bool test_model) override {
#if IS_TRT_VERSION_GE(8000)
    VLOG(4) << "convert a quantize_linear op to tensorrt IQuantizeLayer";

    // Declare inputs and attributes
    framework::OpDesc op_desc(op, nullptr);
    auto* x = engine_->GetITensor(op_desc.Input("X")[0]);
    auto* scale_var = scope.FindVar(op_desc.Input("Scale")[0]);
    int axis = PADDLE_GET_CONST(int, op_desc.GetAttr("quant_axis"));

    // Create constant layer for scale
    PADDLE_ENFORCE_NOT_NULL(
        scale_var,
        platform::errors::NotFound("Can not find %s presistale var in scope.",
                                   op_desc.Input("Scale")[0]));
    auto* scale_t = scale_var->GetMutable<phi::DenseTensor>();
    int n_scale = scale_t->numel();
    std::vector<float> scale_data(n_scale, 0.0f);
    for (int i = 0; i < n_scale; ++i) {
      scale_data[i] = scale_t->data<float>()[i] / 127.0f;
    }
    nvinfer1::Dims scale_dim{1, { n_scale }};
    auto* scale = AddConstantLayer(scale_data.data(), scale_dim);

    // Add quantize layer
    auto* layer = TRT_ENGINE_ADD_LAYER(engine_, Quantize, *x, *scale);
    layer->setAxis(axis);
    auto output_name = op_desc.Output("Y")[0];
    RreplenishLayerAndOutput(
        layer, "quantize_linear", {output_name}, test_model);
#else
    PADDLE_THROW(
        platform::errors::Fatal("Paddle-TRT explicit quantization does not "
                                "support Paddle compiled with TRT < 8.0"));
#endif
  }
};

}  // namespace tensorrt
}  // namespace inference
}  // namespace paddle

REGISTER_TRT_OP_CONVERTER(quantize_linear, QuantizeLinearOpConverter);
