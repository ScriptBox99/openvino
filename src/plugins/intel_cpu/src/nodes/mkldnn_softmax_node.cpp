// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_softmax_node.h"

#include <string>
#include <mkldnn_types.h>
#include <mkldnn_extension_utils.h>
#include <memory_desc/cpu_memory_desc_utils.h>
#include <ngraph/opsets/opset1.hpp>
#include "memory_desc/dnnl_blocked_memory_desc.h"
#include <common/primitive_hashing_utils.hpp>

using namespace mkldnn;
using namespace MKLDNNPlugin;
using namespace InferenceEngine;

namespace {
struct SoftmaxKey {
    DnnlMemoryDescCPtr inp0;
    impl_desc_type implType;
    size_t axis;

    size_t hash() const;
    bool operator==(const SoftmaxKey& rhs) const;
};

size_t SoftmaxKey::hash() const {
    using namespace dnnl::impl;
    using namespace dnnl::impl::primitive_hashing;

    size_t seed = 0;

    seed = hash_combine(seed, get_md_hash(inp0->getDnnlDesc().data));
    seed = hash_combine(seed, implType);
    seed = hash_combine(seed, axis);
    return seed;
}

bool SoftmaxKey::operator==(const SoftmaxKey& rhs) const {
    bool retVal = true;
    if (inp0 != rhs.inp0) {
        retVal = retVal && inp0 && rhs.inp0 && inp0->getDnnlDesc() == rhs.inp0->getDnnlDesc();
    }

    retVal = retVal && implType == rhs.implType && axis == rhs.axis;
    return retVal;
}
}  // namespace

bool MKLDNNSoftMaxNode::isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept {
    try {
        if (!std::dynamic_pointer_cast<const ngraph::opset1::Softmax>(op)) {
            errorMessage = "Only opset1 Softmax operation is supported";
            return false;
        }
    } catch (...) {
        return false;
    }
    return true;
}

MKLDNNSoftMaxNode::MKLDNNSoftMaxNode(const std::shared_ptr<ngraph::Node>& op, const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr &cache) :
        MKLDNNNode(op, eng, cache) {
    std::string errorMessage;
    if (!isSupportedOperation(op, errorMessage)) {
        IE_THROW(NotImplemented) << errorMessage;
    }
    axis = ngraph::as_type_ptr<ngraph::op::v1::Softmax>(op)->get_axis();
}

void MKLDNNSoftMaxNode::getSupportedDescriptors() {
    if (descs.size())
        return;

    InferenceEngine::Precision precision = getOriginalInputPrecisionAtPort(0);
    if (precision != InferenceEngine::Precision::FP32 && precision != InferenceEngine::Precision::BF16)
        precision = InferenceEngine::Precision::FP32;
    auto inputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(precision);

    if (getParentEdges().size() != 1)
        IE_THROW() << "Incorrect number of input edges for layer " << getName();
    if (!getChildEdges().size())
        IE_THROW() << "Incorrect number of output edges for layer " << getName();

    const auto &inShape = getInputShapeAtPort(0);
    if (inShape.getRank() == 3) {
        auto in_candidate = std::make_shared<DnnlBlockedMemoryDesc>(inShape, inputDataType, memory::format_tag::abc);
        createDescriptor({in_candidate}, {});
    }

    for (auto format : getAvailableFormatsForDims(inShape)) {
        auto in_candidate = std::make_shared<DnnlBlockedMemoryDesc>(inShape, inputDataType, format);

        if (in_candidate->blocksExtended())
            continue;

        createDescriptor({in_candidate}, {});
    }
}

bool MKLDNNSoftMaxNode::created() const {
    return getType() == Softmax;
}

void MKLDNNSoftMaxNode::initOptimalPrimitiveDescriptor() {
    auto selected_pd = getSelectedPrimitiveDescriptor();
    if (selected_pd == nullptr)
        IE_THROW() << "Preferable primitive descriptor is not set.";
    auto config = selected_pd->getConfig();
    if (isConfigDefined(config))
        return;

    if (config.inConfs.size() != 1 || config.outConfs.size() != 1 ||
            (config.inConfs[0].desc->isDefined() &&
                    config.outConfs[0].desc->isDefined() && !config.inConfs[0].desc->isCompatible(*config.outConfs[0].desc)))
        IE_THROW() << "Layer " << getName() << " has incorrect selected config!";

    if (config.inConfs[0].desc->isDefined()) {
        config.outConfs[0].desc = config.inConfs[0].desc;
    } else if (config.outConfs[0].desc->isDefined()) {
        config.inConfs[0].desc = config.outConfs[0].desc;
    } else {
        config.inConfs[0].desc = getDefinedInputDesc(config, 0);
        config.outConfs[0].desc = config.inConfs[0].desc;
    }

    initDescriptor(config);
}

void MKLDNNSoftMaxNode::createDescriptor(const std::vector<MemoryDescPtr> &inputDesc,
                                         const std::vector<MemoryDescPtr> &outputDesc) {
    auto inpDesc = inputDesc[0]->isDefined() ? inputDesc[0] : MemoryDescUtils::makeDummyDesc(*inputDesc[0]);
    DnnlMemoryDescPtr definedInpMemDesc = MemoryDescUtils::convertToDnnlMemoryDesc(inpDesc);
    auto in_candidate = definedInpMemDesc->getDnnlDesc();

    MKLDNNDescriptor desc(std::shared_ptr<softmax_forward::desc>(
            new softmax_forward::desc(prop_kind::forward_scoring, in_candidate, axis)));
    descs.push_back(desc);
}

void MKLDNNSoftMaxNode::prepareParams() {
    auto inpDesc = getParentEdgeAt(0)->getMemory().GetDescWithType<DnnlMemoryDesc>();
    const NodeDesc* selected_pd = getSelectedPrimitiveDescriptor();

    if (selected_pd == nullptr)
        IE_THROW() << "Preferable primitive descriptor is not set for node " << getName() << ".";

    SoftmaxKey key = {inpDesc, selected_pd->getImplementationType(), axis};
    auto engine = getEngine();
    auto builder = [&engine](const SoftmaxKey& key) -> std::shared_ptr<mkldnn::primitive> {
        softmax_forward::primitive_desc prim_desc;
        MKLDNNDescriptor desc(std::shared_ptr<softmax_forward::desc>(
            new softmax_forward::desc(prop_kind::forward_scoring, key.inp0->getDnnlDesc(), key.axis)));
        primitive_desc_iterator itpd = desc.createPrimitiveDescriptorIterator(engine);

        while (itpd) {
            impl_desc_type impl_type = parse_impl_name(itpd.impl_info_str());
            if (impl_type == key.implType ||
                // At least for oneDNN v2.4 the softmax primitive is optimized for the cases where the dimension of the
                // softmax axis is physically dense. There could be situations where it is not possible to detect the
                // optimized case in advance in case of dynamic shapes, but in runtime the shape could be suitable for
                // the optimized implementation, so we have to select the optimized one.
                (ref_any == key.implType && (impl_type & jit))) {
                prim_desc = itpd.get();
                break;
            }
            if (!itpd.next_impl())
                return nullptr;
        }
        return std::make_shared<softmax_forward>(prim_desc);
    };

    auto cache = getRuntimeCache();
    auto result = cache->getOrCreate(key, builder);

    if (!result.first) {
        IE_THROW() << "Primitive descriptor was not found for node " << getName() << ".";
    }

    prim = result.first;

    auto src = getParentEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
    auto dst = getChildEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
    primArgs = {{DNNL_ARG_SRC, src}, {DNNL_ARG_DST, dst}};
}

void MKLDNNSoftMaxNode::executeDynamicImpl(mkldnn::stream strm) {
    execute(strm);
}

std::vector<VectorDims> MKLDNNSoftMaxNode::shapeInfer() const {
    return {getParentEdgesAtPort(0).front()->getMemory().getStaticDims()};
}

REG_MKLDNN_PRIM_FOR(MKLDNNSoftMaxNode, Softmax);
