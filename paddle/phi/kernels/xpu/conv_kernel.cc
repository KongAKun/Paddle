// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "paddle/phi/kernels/conv_kernel.h"

#include "paddle/phi/backends/xpu/enforce_xpu.h"
#include "paddle/phi/core/kernel_registry.h"
#include "paddle/phi/kernels/cpu/conv_util.h"
#include "paddle/phi/kernels/xpu/xpu_api_wrapper.h"

namespace phi {

template <typename T, typename Context>
void ConvKernel(const Context& dev_ctx,
                const DenseTensor& input,
                const DenseTensor& filter,
                const std::vector<int>& strides,
                const std::vector<int>& paddings_t,
                const std::string& padding_algorithm,
                const std::vector<int>& dilations_t,
                int groups,
                const std::string& data_format,
                DenseTensor* out) {
  using XPUT = typename XPUTypeTrait<T>::Type;
  std::vector<int> paddings = paddings_t;
  std::vector<int> dilations = dilations_t;
  // The filter will be reshaped in the calculations,
  // so here use an assignment operation,
  // that avoids modifying the variable in the Scope.
  dev_ctx.template Alloc<T>(out);

  PADDLE_ENFORCE_EQ(
      data_format == "NDHWC",
      false,
      phi::errors::InvalidArgument(
          ("XPU does not support data_format is NDHWC in conv op.")));

  phi::DDim in_data_dims =
      phi::slice_ddim(input.dims(), 2, input.dims().size());
  phi::DDim filter_data_dims =
      phi::slice_ddim(filter.dims(), 2, filter.dims().size());
  std::vector<int> ksize = phi::vectorize<int>(filter_data_dims);
  UpdatePaddingAndDilation(
      &paddings, &dilations, padding_algorithm, in_data_dims, strides, ksize);

  int batch_size = static_cast<int>(input.dims()[0]);
  int img_c = static_cast<int>(input.dims()[1]);
  int img_h = static_cast<int>(input.dims()[2]);
  int img_w = static_cast<int>(input.dims()[3]);
  int f = static_cast<int>(filter.dims()[0]);
  bool is_nchw = true;
  if (data_format == "NHWC") {
    img_c = static_cast<int>(input.dims()[3]);
    img_h = static_cast<int>(input.dims()[1]);
    img_w = static_cast<int>(input.dims()[2]);
    is_nchw = false;
  }

  const XPUT* input_data = reinterpret_cast<const XPUT*>(input.data<T>());
  const XPUT* filter_data = reinterpret_cast<const XPUT*>(filter.data<T>());
  XPUT* output_data = reinterpret_cast<XPUT*>(out->data<T>());

  xpu::ctx_guard RAII_GUARD(dev_ctx.x_context());

  XPUT* filter_data_tmp;
  const XPUT* filter_data_ptr = filter_data;
  if (data_format == "NHWC") {
    filter_data_tmp = RAII_GUARD.alloc<XPUT>(filter.numel());
    PADDLE_ENFORCE_XDNN_NOT_NULL(filter_data_tmp);
    std::vector<int> filter_shape = phi::vectorize<int>(filter.dims());
    int r = xpu::transpose<XPUT>(dev_ctx.x_context(),
                                 filter_data,
                                 filter_data_tmp,
                                 filter_shape,
                                 {0, 2, 3, 1});
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "transpose");
    filter_data_ptr = reinterpret_cast<const XPUT*>(filter_data_tmp);
  }

  int fccal_type = FCCalcType<XPUT>();
  if (fccal_type == XPUFCCalcType::FC_INT32) {
    int r = xpu::conv2d<XPUT, XPUT, XPUT, int>(dev_ctx.x_context(),
                                               input_data,
                                               filter_data_ptr,
                                               output_data,
                                               batch_size,
                                               img_c,
                                               img_h,
                                               img_w,
                                               f,
                                               ksize,
                                               strides,
                                               paddings,
                                               dilations,
                                               groups,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               is_nchw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv2d");
  } else if (fccal_type == XPUFCCalcType::FC_FLOAT) {
    int r = xpu::conv2d<XPUT, XPUT, XPUT, float>(dev_ctx.x_context(),
                                                 input_data,
                                                 filter_data_ptr,
                                                 output_data,
                                                 batch_size,
                                                 img_c,
                                                 img_h,
                                                 img_w,
                                                 f,
                                                 ksize,
                                                 strides,
                                                 paddings,
                                                 dilations,
                                                 groups,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 is_nchw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv2d");
  } else if (fccal_type == XPUFCCalcType::FC_INT32_WITH_LL) {
    int r = xpu::conv2d<XPUT, XPUT, XPUT, int_with_ll_t>(dev_ctx.x_context(),
                                                         input_data,
                                                         filter_data_ptr,
                                                         output_data,
                                                         batch_size,
                                                         img_c,
                                                         img_h,
                                                         img_w,
                                                         f,
                                                         ksize,
                                                         strides,
                                                         paddings,
                                                         dilations,
                                                         groups,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         is_nchw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv2d");
  } else {
    int r = xpu::conv2d<XPUT, XPUT, XPUT, int16_t>(dev_ctx.x_context(),
                                                   input_data,
                                                   filter_data_ptr,
                                                   output_data,
                                                   batch_size,
                                                   img_c,
                                                   img_h,
                                                   img_w,
                                                   f,
                                                   ksize,
                                                   strides,
                                                   paddings,
                                                   dilations,
                                                   groups,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   is_nchw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv2d");
  }
}

template <typename T, typename Context>
void DepthwiseConvKernel(const Context& dev_ctx,
                         const DenseTensor& input,
                         const DenseTensor& filter,
                         const std::vector<int>& strides,
                         const std::vector<int>& paddings,
                         const std::string& padding_algorithm,
                         int groups,
                         const std::vector<int>& dilations,
                         const std::string& data_format,
                         DenseTensor* out) {
  ConvKernel<T, Context>(dev_ctx,
                         input,
                         filter,
                         strides,
                         paddings,
                         padding_algorithm,
                         dilations,
                         groups,
                         data_format,
                         out);
}

template <typename T, typename Context>
void Conv3DKernel(const Context& dev_ctx,
                  const DenseTensor& input,
                  const DenseTensor& filter,
                  const std::vector<int>& strides,
                  const std::vector<int>& paddings_t,
                  const std::string& padding_algorithm,
                  int groups,
                  const std::vector<int>& dilations_t,
                  const std::string& data_format,
                  DenseTensor* out) {
  using XPUT = typename XPUTypeTrait<T>::Type;
  std::vector<int> paddings = paddings_t;
  std::vector<int> dilations = dilations_t;
  // The filter will be reshaped in the calculations,
  // so here use an assignment operation,
  // that avoids modifying the variable in the Scope.
  dev_ctx.template Alloc<T>(out);

  phi::DDim in_data_dims =
      phi::slice_ddim(input.dims(), 2, input.dims().size());
  phi::DDim filter_data_dims =
      phi::slice_ddim(filter.dims(), 2, filter.dims().size());
  std::vector<int> ksize = phi::vectorize<int>(filter_data_dims);
  UpdatePaddingAndDilation(
      &paddings, &dilations, padding_algorithm, in_data_dims, strides, ksize);

  int batch_size = static_cast<int>(input.dims()[0]);
  int img_c = static_cast<int>(input.dims()[1]);
  int img_d = static_cast<int>(input.dims()[2]);
  int img_h = static_cast<int>(input.dims()[3]);
  int img_w = static_cast<int>(input.dims()[4]);
  int f = static_cast<int>(filter.dims()[0]);
  bool is_ncdhw = true;
  if (data_format == "NDHWC") {
    img_c = static_cast<int>(input.dims()[4]);
    img_d = static_cast<int>(input.dims()[1]);
    img_h = static_cast<int>(input.dims()[2]);
    img_w = static_cast<int>(input.dims()[3]);
    is_ncdhw = false;
  }

  XPUT* output_data = reinterpret_cast<XPUT*>(out->data<T>());
  const XPUT* filter_data = reinterpret_cast<const XPUT*>(filter.data<T>());
  const XPUT* input_data = reinterpret_cast<const XPUT*>(input.data<T>());

  xpu::ctx_guard RAII_GUARD(dev_ctx.x_context());

  XPUT* filter_data_tmp;
  const XPUT* filter_data_ptr = filter_data;
  if (data_format == "NDHWC") {
    filter_data_tmp = RAII_GUARD.alloc<XPUT>(filter.numel());
    PADDLE_ENFORCE_XDNN_NOT_NULL(filter_data_tmp);
    std::vector<int> filter_shape = phi::vectorize<int>(filter.dims());
    int r = xpu::transpose<XPUT>(dev_ctx.x_context(),
                                 filter_data,
                                 filter_data_tmp,
                                 filter_shape,
                                 {0, 2, 3, 4, 1});
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "transpose");
    filter_data_ptr = reinterpret_cast<const XPUT*>(filter_data_tmp);
  }

  int fccal_type = FCCalcType<XPUT>();
  if (fccal_type == XPUFCCalcType::FC_INT32) {
    int r = xpu::conv3d<XPUT, XPUT, XPUT, int>(dev_ctx.x_context(),
                                               input_data,
                                               filter_data_ptr,
                                               output_data,
                                               batch_size,
                                               img_c,
                                               img_d,
                                               img_h,
                                               img_w,
                                               f,
                                               ksize,
                                               strides,
                                               paddings,
                                               dilations,
                                               groups,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               is_ncdhw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv3d");
  } else if (fccal_type == XPUFCCalcType::FC_FLOAT) {
    int r = xpu::conv3d<XPUT, XPUT, XPUT, float>(dev_ctx.x_context(),
                                                 input_data,
                                                 filter_data_ptr,
                                                 output_data,
                                                 batch_size,
                                                 img_c,
                                                 img_d,
                                                 img_h,
                                                 img_w,
                                                 f,
                                                 ksize,
                                                 strides,
                                                 paddings,
                                                 dilations,
                                                 groups,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 is_ncdhw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv3d");

  } else if (fccal_type == XPUFCCalcType::FC_INT32_WITH_LL) {
    int r = xpu::conv3d<XPUT, XPUT, XPUT, int_with_ll_t>(dev_ctx.x_context(),
                                                         input_data,
                                                         filter_data_ptr,
                                                         output_data,
                                                         batch_size,
                                                         img_c,
                                                         img_d,
                                                         img_h,
                                                         img_w,
                                                         f,
                                                         ksize,
                                                         strides,
                                                         paddings,
                                                         dilations,
                                                         groups,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         is_ncdhw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv3d");
  } else {
    int r = xpu::conv3d<XPUT, XPUT, XPUT, int16_t>(dev_ctx.x_context(),
                                                   input_data,
                                                   filter_data_ptr,
                                                   output_data,
                                                   batch_size,
                                                   img_c,
                                                   img_d,
                                                   img_h,
                                                   img_w,
                                                   f,
                                                   ksize,
                                                   strides,
                                                   paddings,
                                                   dilations,
                                                   groups,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   is_ncdhw);
    PADDLE_ENFORCE_XDNN_SUCCESS(r, "conv3d");
  }
}

}  // namespace phi

PD_REGISTER_KERNEL(
    conv2d, XPU, ALL_LAYOUT, phi::ConvKernel, float, phi::dtype::float16) {}
PD_REGISTER_KERNEL(depthwise_conv2d,
                   XPU,
                   ALL_LAYOUT,
                   phi::DepthwiseConvKernel,
                   float,
                   phi::dtype::float16) {}
PD_REGISTER_KERNEL(
    conv3d, XPU, ALL_LAYOUT, phi::Conv3DKernel, float, phi::dtype::float16) {}
