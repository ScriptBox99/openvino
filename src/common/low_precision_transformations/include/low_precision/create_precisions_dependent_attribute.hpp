// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>
#include <vector>

#include <ngraph/node.hpp>
#include <ngraph/variant.hpp>
#include <ngraph/pattern/op/wrap_type.hpp>

#include <low_precision/lpt_visibility.hpp>
#include <ngraph/pass/graph_rewrite.hpp>
#include <ngraph/opsets/opset1.hpp>
#include "rt_info/precision_preserved_attribute.hpp"
#include "network_helper.hpp"
#include "lpt_itt.hpp"

namespace ngraph {
namespace pass {
namespace low_precision {

template <typename AttributeType, typename OperationType>
class CreatePrecisionsDependentAttribute;

}  // namespace low_precision
}  // namespace pass
}  // namespace ngraph

/**
 * @ingroup ie_transformation_common_api
 * @brief CreatePrecisionsDependentAttribute transformation marks OperationType operations by
 * PrecisionPreservedAttribute and AttributeType attributes with the same shared part.
 *
 * For more details about the transformation, refer to
 * [CreatePrecisionsDependentAttribute](@ref openvino_docs_IE_DG_lpt_CreatePrecisionsDependentAttribute) page
 * in the Inference Engine Developer Guide.
 */
template <typename AttributeType, typename OperationType>
class ngraph::pass::low_precision::CreatePrecisionsDependentAttribute : public ngraph::pass::MatcherPass {
public:
    CreatePrecisionsDependentAttribute() {
        auto operation = pattern::wrap_type<OperationType>();

        ngraph::graph_rewrite_callback callback = [&](pattern::Matcher& m) {
            auto node = m.get_match_root();
            if (transformation_callback(node)) {
                return false;
            }

            {
                OV_ITT_SCOPE(FIRST_INFERENCE, itt::domains::LPT_LT, "CreatePrecisionsDependentAttribute");
                auto &rt = node->get_rt_info();

                const auto precisionPreservedAttribute = PrecisionPreservedAttribute(false);
                rt[PrecisionPreservedAttribute::get_type_info_static()] = precisionPreservedAttribute;
                const auto &targetSharedValue = precisionPreservedAttribute.attribute->sharedValue;

                const auto attribute = AttributeType{};
                rt[AttributeType::get_type_info_static()] = attribute;

                ngraph::pass::low_precision::NetworkHelper::reassign<AttributeType>(
                    targetSharedValue,
                    {
                        attribute.attribute,
                        precisionPreservedAttribute.attribute
                    });
            }
            return true;
        };

        auto matcher = std::make_shared<ngraph::pattern::Matcher>(operation, "CreatePrecisionsDependentAttribute");
        this->register_matcher(matcher, callback);
    }
};
