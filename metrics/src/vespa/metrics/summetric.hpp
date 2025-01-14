// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "summetric.h"
#include "metricset.h"
#include "memoryconsumption.h"
#include <vespa/vespalib/util/exceptions.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <ostream>
#include <cassert>
#include <algorithm>

namespace metrics {

template<typename AddendMetric>
bool
SumMetric<AddendMetric>::visit(MetricVisitor& visitor,
                               bool tagAsAutoGenerated) const
{
    (void) tagAsAutoGenerated;
    if (_metricsToSum.empty()) return true;
    std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
    return sum.second->visit(visitor, true);
}

template<typename AddendMetric>
SumMetric<AddendMetric>::SumMetric(const String& name, Tags tags,
                                   const String& description, MetricSet* owner)
    : Metric(name, tags, description, owner),
      _startValue(),
      _metricsToSum()
{
}

template<typename AddendMetric>
SumMetric<AddendMetric>::SumMetric(const SumMetric<AddendMetric>& other,
                                   std::vector<Metric::UP> &ownerList,
                                   MetricSet* owner)
    : Metric(other, owner),
      _startValue(other._startValue),
      _metricsToSum()
{
    (void) ownerList;
    if (other._owner == 0) {
        throw vespalib::IllegalStateException(
                "Cannot copy a sum metric not registered in a metric set, as "
                "we need to use parent to detect new metrics to point to.",
                VESPA_STRLOC);
    }
    if (owner == 0) {
        throw vespalib::IllegalStateException(
                "Cannot copy a sum metric directly. One needs to at least "
                "include metric set above it in order to include metrics "
                "summed.", VESPA_STRLOC);
    }
    std::vector<String> parentPath(other._owner->getPathVector());
    _metricsToSum.reserve(other._metricsToSum.size());
    for(const AddendMetric* m : _metricsToSum) {
        std::vector<String> addendPath(m->getPathVector());
        MetricSet* newAddendParent = owner;
        for (uint32_t i=parentPath.size(), n=addendPath.size() - 1; i<n; ++i) {
            Metric* child = newAddendParent->getMetric(addendPath[i]);
            if (child == 0) {
                throw vespalib::IllegalStateException(
                        "Metric " + addendPath[i] + " in metric set "
                        + newAddendParent->getPath() + " was expected to "
                        "exist. This sounds like a bug.", VESPA_STRLOC);
            }
            if (!child->isMetricSet()) {
                throw vespalib::IllegalStateException(
                        "Metric " + addendPath[i] + " in metric set "
                        + newAddendParent->getPath() + " was expected to be a "
                        "metric set. This sounds like a bug.", VESPA_STRLOC);
            }
            newAddendParent = static_cast<MetricSet*>(child);
        }
        Metric* child = newAddendParent->getMetric(addendPath[addendPath.size() - 1]);
        if (child == 0) {
            throw vespalib::IllegalStateException(
                    "Metric " + addendPath[addendPath.size() - 1] + " in "
                    "metric set " + newAddendParent->getPath() + " was "
                    "expected to exist. This sounds like a bug.", VESPA_STRLOC);
        }
        AddendMetric* am(dynamic_cast<AddendMetric*>(child));
        if (am == 0) {
            throw vespalib::IllegalStateException(
                    "Metric " + child->getPath() + " is of wrong type for sum "
                    + other.getPath() + ". This sounds like a bug.",
                    VESPA_STRLOC);
        }
        _metricsToSum.push_back(am);
    }
}

template<typename AddendMetric>
SumMetric<AddendMetric>::~SumMetric() { }

template<typename AddendMetric>
Metric*
SumMetric<AddendMetric>::clone(std::vector<Metric::UP> &ownerList,
                               CopyType copyType, MetricSet* owner,
                               bool includeUnused) const
{
    (void) includeUnused;
    if (_metricsToSum.empty() && _startValue.get() == 0) {
        abort();
        throw vespalib::IllegalStateException(
                "Attempted to clone sum metric without any children or start value. "
                "This is currently illegal, to avoid needing to be able to "
                "construct a metric of appropriate type without having a "
                "template. (Hard to know how to construct any metric.",
                VESPA_STRLOC);
    }
    if (copyType == CLONE) {
        return new SumMetric<AddendMetric>(*this, ownerList, owner);
    }
        // Else we're generating an inactive copy by evaluating sum
    typename std::vector<const AddendMetric*>::const_iterator it(
            _metricsToSum.begin());
        // Clone start value or first child and use as accumulator
        // As the metric cloned will have wrong info, we have to wait to
        // register it in parent until we have fixed that.
    Metric *m = 0;
    if (_startValue.get() != 0) {
        m = _startValue->getStartValue().clone(ownerList, INACTIVE, 0, true);
    } else {
        m = (**it).clone(ownerList, INACTIVE, 0, true);
        ++it;
    }
    m->setName(getName());
    m->setDescription(getDescription());
    m->setTags(getTags());
    if (owner != 0) owner->registerMetric(*m);
        // Add the others to the metric cloned.
    for (; it != _metricsToSum.end(); ++it) {
        (**it).addToPart(*m);
    }
    return m;
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::addToPart(Metric& m) const
{
    if (!m.is_sum_metric()) {
        std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
        sum.second->addToPart(m);
    }
}

template<typename AddendMetric>
bool
SumMetric<AddendMetric>::is_sum_metric() const
{
    return true;
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::addToSnapshot(
        Metric& m, std::vector<Metric::UP> &ownerList) const
{
    if (isAddendType(&m)) {
        // If the type to add to is an addend metric, it is part of an inactive
        // copy we need to add data to.
        std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
        sum.second->addToSnapshot(m, ownerList);
    }
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::addTo(Metric& m,
                               std::vector<Metric::UP> *ownerList) const
{
    if (ownerList == 0) {
        std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
        sum.second->addToPart(m);
    } else {
        if (isAddendType(&m)) {
            // If the type to add to is an addend metric, it is part of an
            // inactive copy we need to add data to.
            std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
            sum.second->addToSnapshot(m, *ownerList);
        }
    }
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::addMetricToSum(const AddendMetric& metric)
{
    if (_owner == 0) {
        throw vespalib::IllegalStateException(
                "Sum metric needs to be registered in a parent metric set "
                "prior to adding metrics to sum.", VESPA_STRLOC);
    }
    std::vector<String> sumParentPath(_owner->getPathVector());
    std::vector<String> addedPath(metric.getPathVector());
    bool error = false;
    if (addedPath.size() <= sumParentPath.size()) {
        error = true;
    } else for (uint32_t i=0; i<sumParentPath.size(); ++i) {
        if (sumParentPath[i] != addedPath[i]) {
            error = true;
            break;
        }
    }
    if (error) {
        throw vespalib::IllegalStateException(
                "Metric added to sum is required to be a child of the sum's "
                "direct parent metric set. (Need not be a direct child) "
                "Metric set " + metric.getPath() + " is not a child of "
                + _owner->getPath(), VESPA_STRLOC);
    }
    std::vector<const AddendMetric*> metrics(_metricsToSum.size() + 1);
    for (uint32_t i=0; i<_metricsToSum.size(); ++i) {
        metrics[i] = _metricsToSum[i];
    }
    metrics[metrics.size() - 1] = &metric;
    _metricsToSum.swap(metrics);
        // Ensure we don't use extra memory
    assert(_metricsToSum.capacity() == _metricsToSum.size());
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::removeMetricFromSum(const AddendMetric &metric)
{
    _metricsToSum.erase(std::remove(_metricsToSum.begin(), _metricsToSum.end(), &metric));
}

template<typename AddendMetric>
std::pair<std::vector<Metric::UP>, Metric::UP>
SumMetric<AddendMetric>::generateSum() const
{
    std::pair<std::vector<Metric::UP>, Metric::UP> retVal;
    Metric* m = clone(retVal.first, INACTIVE, 0, true);
    m->setRegistered(_owner);
    retVal.second.reset(m);
    return retVal;
}

template<typename AddendMetric>
int64_t
SumMetric<AddendMetric>::getLongValue(stringref id) const
{
    std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
    if (sum.second.get() == 0) return 0;
    return sum.second->getLongValue(id);
}

template<typename AddendMetric>
double
SumMetric<AddendMetric>::getDoubleValue(stringref id) const
{
    std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
    if (sum.second.get() == 0) return 0.0;
    return sum.second->getDoubleValue(id);
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::print(std::ostream& out, bool verbose,
                               const std::string& indent,
                               uint64_t secondsPassed) const
{
    std::pair<std::vector<Metric::UP>, Metric::UP> sum(generateSum());
    if (sum.second.get() == 0) return;
    sum.second->print(out, verbose, indent, secondsPassed);
}

template<typename AddendMetric>
bool
SumMetric<AddendMetric>::used() const
{
    for(const AddendMetric* m : _metricsToSum) {
        if (m->used()) return true;
    }
    return false;
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::addMemoryUsage(MemoryConsumption& mc) const
{
    ++mc._sumMetricCount;
    mc._sumMetricMeta += sizeof(SumMetric<AddendMetric>) - sizeof(Metric)
                       + _metricsToSum.capacity() * sizeof(Metric*);
    Metric::addMemoryUsage(mc);
}

template<typename AddendMetric>
void
SumMetric<AddendMetric>::printDebug(std::ostream& out,
                                    const std::string& indent) const
{
    out << "sum ";
    Metric::printDebug(out, indent);
    out << " {";
    for(const AddendMetric* m : _metricsToSum) {
        out << "\n" << indent << "  ";
        m->printDebug(out, indent + "  ");
    }
    out << "}";
}

template<typename AddendMetric>
bool
SumMetric<AddendMetric>::isAddendType(const Metric* m) const
{
    // If metric to addend it a metric set, we can only check if target also is
    // a metric set, as other type will be lost when going inactive. Is there a
    // way to do this without using an actual instance?
    if (_metricsToSum.empty() && _startValue.get() == 0) {
        throw vespalib::IllegalStateException(
                "Attempted to verify addend type for sum metric without any "
                "children or start value.", VESPA_STRLOC);
    }
    const Metric* wantedType;
    if (!_metricsToSum.empty()) {
        wantedType = _metricsToSum[0];
    } else {
        wantedType = &_startValue->getStartValue();
    }
    if (wantedType->isMetricSet()) {
        return (m->isMetricSet());
    } else {
        return (dynamic_cast<const AddendMetric*>(m) != 0);
    }
}

} // metrics

