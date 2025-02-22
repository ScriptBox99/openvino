// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <ngraph/node.hpp>
#include <ngraph/variant.hpp>
#include "low_precision/lpt_visibility.hpp"
#include "low_precision/rt_info/precision_preserved_attribute.hpp"

namespace ngraph {

/**
 * @ingroup ie_transformation_common_api
 * @brief AvgPoolPrecisionPreservedAttribute is utility attribute which is used only during `AvgPool` operation precision
 * preserved property definition.
 *
 * For more details about the attribute, refer to
 * [AvgPoolPrecisionPreservedAttribute](@ref openvino_docs_IE_DG_lpt_AvgPoolPrecisionPreserved) page in the Inference Engine Developer Guide.
 */
class LP_TRANSFORMATIONS_API AvgPoolPrecisionPreservedAttribute : public PrecisionPreservedAttribute {
public:
    OPENVINO_RTTI("LowPrecision::AvgPoolPrecisionPreserved", "", ov::RuntimeAttribute, 0);
    using PrecisionPreservedAttribute::PrecisionPreservedAttribute;
    void merge(std::vector<ov::Any>& attributes);
    std::string to_string() const override;
};

} // namespace ngraph
