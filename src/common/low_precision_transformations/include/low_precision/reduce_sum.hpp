// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "low_precision/reduce_base_transformation.hpp"

#include <memory>
#include <ngraph/ngraph.hpp>
#include "layer_transformation.hpp"

namespace ngraph {
namespace pass {
namespace low_precision {

/**
 * @ingroup ie_transformation_common_api
 * @brief ReduceSumTransformation propagates dequantization operations through ReduceSum operation.
 *
 * For more details about the transformation, refer to
 * [ReduceSumTransformation](@ref openvino_docs_IE_DG_lpt_ReduceSumTransformation) page
 * in the Inference Engine Developer Guide.
 */
class LP_TRANSFORMATIONS_API ReduceSumTransformation : public ReduceBaseTransformation {
public:
    NGRAPH_RTTI_DECLARATION;
    ReduceSumTransformation(const Params& params = Params());
    bool isPrecisionPreserved(std::shared_ptr<Node> reduce) const noexcept override;
    bool canBeTransformed(const TransformationContext& context, std::shared_ptr<Node> reduce) const override;

protected:
    void changeDequantizationValues(
        const std::shared_ptr<Node>& reduce,
        FakeQuantizeDequantization& dequantization) const override;
    bool getUpdatePrecision(const std::shared_ptr<Node>& reduce) const override;
};

} // namespace low_precision
} // namespace pass
} // namespace ngraph
