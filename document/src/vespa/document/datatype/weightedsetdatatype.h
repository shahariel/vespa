// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
/**
 * \class document::WeightedSetDataType
 * \ingroup datatype
 *
 * \brief DataType describing a weighted set.
 *
 * Describes what can be stored and behaviour of weighted sets with this type.
 * The create if non-existing and remove if zero weight functionality, as used
 * in tagging, is a part of the type.
 */
#pragma once

#include "collectiondatatype.h"

namespace document {

class WeightedSetDataType : public CollectionDataType {
    bool _createIfNonExistent;
    bool _removeIfZero;

public:
    WeightedSetDataType() {}
    WeightedSetDataType(const DataType& nestedType, bool createIfNonExistent, bool removeIfZero);
    WeightedSetDataType(const DataType& nestedType, bool createIfNonExistent, bool removeIfZero, int id);

    /**
     * @return Whether values of this datatype will autogenerate entries if
     * operations that require an existing entries operates on non-existing
     * ones.
     */
    bool createIfNonExistent() const { return _createIfNonExistent; };
    /**
     * @return Whether values of this datatype will automatically
     *         remove entries with zero weight.
     */
    bool removeIfZero() const { return _removeIfZero; };

    std::unique_ptr<FieldValue> createFieldValue() const override;
    void print(std::ostream&, bool verbose, const std::string& indent) const override;
    bool operator==(const DataType& other) const override;
    WeightedSetDataType* clone() const override { return new WeightedSetDataType(*this); }
    void onBuildFieldPath(FieldPath & path, vespalib::stringref remainFieldName) const override;

    DECLARE_IDENTIFIABLE(WeightedSetDataType);
};

} // document

