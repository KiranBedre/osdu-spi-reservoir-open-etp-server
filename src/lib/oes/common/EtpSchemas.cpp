// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

#include "EtpSchemas.h"
#include "CompressUtils.h"
#include "etp12/codegen/AvroSpec.i.ipp"

#include <oes/core/utils/Json.hpp>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Compiler.hh>
#include <avro/NodeImpl.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

std::map<std::string, avro::NodePtr> makeLatestSchemaCache(int major, int minor) {
    avro::ValidSchema schema = ::oes::common::getValidSchema(major, minor);
    assert(schema.root().get());
    assert(avro::AVRO_UNION == schema.root()->type());

    std::map<std::string, avro::NodePtr> cache;
    for (int i = 0; i < int(schema.root()->leaves()); ++i) {
        const avro::NodePtr& node = schema.root()->leafAt(i);
        cache.emplace(node->name().fullname(), node);
    }
    return cache;
}

bool isMapNode(const avro::NodePtr& node) {
    return node->type() == avro::AVRO_MAP;
}

bool isSymbolicNode(const avro::NodePtr& node) {
    return node->type() == avro::AVRO_SYMBOLIC;
}

bool hasSymbolicNodes(const avro::NodePtr& node) {
    if (isSymbolicNode(node)) {
        return true;
    }
    const int leaf_count = static_cast<int>(node->leaves());
    for (int i = 0; i < leaf_count; ++i) {
        const avro::NodePtr& leaf_node(node->leafAt(i));
        if (hasSymbolicNodes(leaf_node)) {
            return true;
        }
    }
    return false;
}

// TODO: Guard against nodes cycles
avro::NodePtr makeNodeShallow(const avro::NodePtr& n) {
    if (!hasSymbolicNodes(n)) {
        return n; // don't copy nodes, unless necessary
    }

    // Must copy composite nodes, so we can replace the symbolic children
    // Arrays and Maps have one element child type (leafAt(0)/leafAt(1) respectively)
    switch (n->type()) {
    case avro::AVRO_UNION:
        return avro::NodePtr(new avro::NodeUnion());
    case avro::AVRO_RECORD:
        return avro::NodePtr(new avro::NodeRecord());
    case avro::AVRO_ARRAY:
        return avro::NodePtr(new avro::NodeArray());
    case avro::AVRO_MAP:
        return avro::NodePtr(new avro::NodeMap());
    case avro::AVRO_SYMBOLIC:
        if (const avro::NodePtr& nn = avro::resolveSymbol(n)) {
            assert(!isSymbolicNode(nn));
            return makeNodeShallow(nn);
        } else {
            throw std::runtime_error(
                "Unresolved symbolic node: " +
                (n->hasName()? n->name().fullname(): "(Unnamed)")
            );
        }
    default:
        return n;
    }
}

static void resolveSymbolicNodes(avro::NodePtr& new_node, const avro::NodePtr& node) {
  //std::cout << "Resolving " << node->type() << " node "
  //    << (node->hasName()? node->name().fullname(): "(Unnamed)") << std::endl;

    if (new_node == node) {
        return; // leaf or non-composite nodes
    }
    if (new_node->locked()) {
        return; // composite nodes w/o symbolic children, thus used as-is
    }

    if (!node->isValid()) {
        throw std::runtime_error("Invalid node of type " + node->type());
    }

    if (node->hasName()) {
        const avro::Name& nm = node->name();
        new_node->setName(nm);
    }

    // The AVRO_STRING first node is implicitly added,
    // even when default constructed, so do not add it a 2nd time.
    const int leaf_count = static_cast<int>(node->leaves());
    assert(!isMapNode(node) || (leaf_count == 2));
    for (int i = isMapNode(node)? 1: 0; i < leaf_count; ++i) {
        const avro::NodePtr& leaf_node(node->leafAt(i));
        const avro::NodePtr& old_leaf = isSymbolicNode(leaf_node)?
            avro::resolveSymbol(leaf_node): leaf_node;

        avro::NodePtr new_leaf(makeNodeShallow(leaf_node));
        resolveSymbolicNodes(new_leaf, old_leaf);
        new_node->addLeaf(new_leaf);
    }

    // unions have no names
    const int name_count = static_cast<int>(node->names());
    assert(!name_count || (leaf_count == name_count));
    for (int i = 0; i < name_count; ++i) {
        const std::string& leaf_name(node->nameAt(i));
        new_node->addName(leaf_name);
    }
}

avro::NodePtr getSchemaNodeOfType12(const char* dotted_qname) {
    static const auto cache__ = makeLatestSchemaCache(1, 2);
    auto found = cache__.find(dotted_qname);
    if (found != cache__.end()) {
        return found->second;
    }
    return nullptr;
}

} // namespace

namespace oes::common {

/*!
 * \return empty string if not found.
 */
std::string getSpecFile(int major, int minor) {
    if (major == 1 && minor == 2) {
        const auto* z = etp12::AvroSchema::jsonz;
        const auto nz = static_cast<unsigned>(sizeof(etp12::AvroSchema::jsonz));
        const auto nu = static_cast<unsigned>(etp12::AvroSchema::usize);

        std::string uncompressed(nu, '\0');
        auto* u = reinterpret_cast<unsigned char*>(&uncompressed[0]);
        int rc = oes_eml_zlib_z2u(z, nz, u, nu, nullptr, 0);
        if (rc != 0) {
            return {}; // error
        }
        return uncompressed;
    }
    return {}; // not found
}

/*!
 * \return empty schema if not found.
 * \throws Exception derived from std::exception on found but invalid schema
 */
avro::ValidSchema getValidSchema(int major, int minor) {
    std::string spec = getSpecFile(major, minor);
    if (spec.empty()) {
        return avro::ValidSchema();
    }

    return avro::compileJsonSchemaFromString(spec);
}

void getLatestValidSchema(int& major, int& minor) {
    major = 1;
    minor = 2;
}

avro::ValidSchema getValidSchemaOfType12(const char* dotted_qname) {
    avro::NodePtr old_root = getSchemaNodeOfType12(dotted_qname);
    avro::NodePtr new_root(makeNodeShallow(old_root));
    resolveSymbolicNodes(new_root, old_root);
    return avro::ValidSchema(new_root);
}

} // namespace oes::common
