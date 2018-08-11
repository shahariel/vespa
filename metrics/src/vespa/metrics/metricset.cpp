// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "metricset.h"
#include "memoryconsumption.h"
#include <vespa/vespalib/stllike/hash_map.hpp>
#include <vespa/vespalib/util/exceptions.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <list>
#include <cassert>
#include <algorithm>

#include <vespa/log/log.h>
LOG_SETUP(".metrics.metricsset");

namespace metrics {

MetricSet::MetricSet(const String& name, const String& tags,
                     const String& description, MetricSet* owner,
                     const std::string& dimensionKey)
    : Metric(name, tags, description, owner),
      _metricOrder(),
      _registrationAltered(false),
      _dimensionKey(dimensionKey)
{
}

MetricSet::MetricSet(const String& name, Tags dimensions,
                     const String& description, MetricSet* owner)
    : Metric(name, std::move(dimensions), description, owner),
      _metricOrder(),
      _registrationAltered(false),
      _dimensionKey()
{
}

MetricSet::MetricSet(const MetricSet& other,
                     std::vector<Metric::UP> &ownerList,
                     CopyType copyType,
                     MetricSet* owner,
                     bool includeUnused)
    : Metric(other, owner),
      _metricOrder(),
      _registrationAltered(false),
      _dimensionKey(other._dimensionKey)
{
    if (copyType == INACTIVE && owner == NULL && includeUnused) {
        _dimensionKey = "";
    }

    for (const Metric* metric : other._metricOrder) {
        if (copyType != INACTIVE || includeUnused || metric->used()) {
            Metric* m = metric->clone(ownerList, copyType, this, includeUnused);
            ownerList.push_back(Metric::UP(m));
        }
    }
}

MetricSet::~MetricSet() { }

MetricSet*
MetricSet::clone(std::vector<Metric::UP> &ownerList, CopyType type,
                 MetricSet* owner, bool includeUnused) const
{
    return new MetricSet(*this, ownerList, type, owner, includeUnused);
}


const Metric*
MetricSet::getMetricInternal(const String& name) const
{
    for (const Metric* metric : _metricOrder) {
        if (metric->getMangledName() == name) {
            return metric;
        }
    }
    return 0;
}

int64_t MetricSet::getLongValue(stringref ) const {
    assert(false);
    return 0;
}
double MetricSet::getDoubleValue(stringref ) const {
    assert(false);
    return 0;
}

const Metric*
MetricSet::getMetric(const String& name) const
{
    std::string::size_type pos = name.find('.');
    if (pos == std::string::npos) {
        return getMetricInternal(name);
    } else {
        std::string child(name.substr(0, pos));
        std::string rest(name.substr(pos+1));
        const Metric* m(getMetricInternal(child));
        if (m == 0) return 0;
        if (!m->isMetricSet()) {
            throw vespalib::IllegalStateException(
                    "Metric " + child + " is not a metric set. Cannot retrieve "
                    "metric at path " + name + " within metric " + getPath(),
                    VESPA_STRLOC);
        }
        return static_cast<const MetricSet*>(m)->getMetric(rest);
    }
}

namespace {
    struct MetricSetFinder : public MetricVisitor {
        std::list<MetricSet*> _metricSets;

        bool visitMetricSet(const MetricSet& set, bool autoGenerated) override {
            if (autoGenerated) return false;
            _metricSets.push_back(const_cast<MetricSet*>(&set));
            return true;
        }
        bool visitMetric(const Metric&, bool autoGenerated) override {
            (void) autoGenerated;
            return true;
        }
    };
}

void
MetricSet::clearRegistrationAltered()
{
    MetricSetFinder finder;
    visit(finder);
    for (MetricSet* metricSet : finder._metricSets) {
        metricSet->_registrationAltered = false;
    }
}

void
MetricSet::tagRegistrationAltered()
{
    _registrationAltered = true;
    if (_owner != 0) {
        _owner->tagRegistrationAltered();
    }
}

void
MetricSet::registerMetric(Metric& metric)
{
    if (metric.isRegistered()) {
        throw vespalib::IllegalStateException(
                "Metric " + metric.getMangledName() +
                " is already registered in a metric set. "
                "Cannot register it twice.", VESPA_STRLOC);
    }
    const Metric* existing(getMetricInternal(metric.getMangledName()));
    if (existing != 0) {
        throw vespalib::IllegalStateException(
                "A metric named " + metric.getMangledName() +
                " is already registered in metric set " + getPath(),
                VESPA_STRLOC);
    }
    _metricOrder.push_back(&metric);
    metric.setRegistered(this);
    tagRegistrationAltered();
    if (metric.isMetricSet()) {
        static_cast<MetricSet &>(metric)._owner = this;
    }
    LOG(spam, "Registered metric%s %s in metric set %s.",
        metric.isMetricSet() ? "set" : "",
        metric.getMangledName().c_str(),
        getPath().c_str());
}

void
MetricSet::unregisterMetric(Metric& metric)
{
        // In case of abrubt shutdowns, don't die hard on attempts to unregister
        // non-registered metrics. Just warn and ignore.
    const Metric* existing(getMetricInternal(metric.getMangledName()));
    if (existing == 0) {
        LOG(warning, "Attempt to unregister metric %s in metric set %s, where "
                     "it wasn't registered to begin with.",
            metric.getMangledName().c_str(),
            getPath().c_str());
        return;
    }
    bool found = false;
    for (std::vector<Metric*>::iterator it = _metricOrder.begin();
         it != _metricOrder.end(); ++it)
    {
        if (*it == &metric) {
            _metricOrder.erase(it);
            found = true;
            break;
        }
    }
    assert(found); // We check above for existence.
    (void) found;
    metric.setRegistered(NULL);
    tagRegistrationAltered();
    if (metric.isMetricSet()) {
        static_cast<MetricSet &>(metric)._owner = this;
    }
    LOG(spam, "Unregistered metric%s %s from metric set %s.",
        metric.isMetricSet() ? "set" : "",
        metric.getMangledName().c_str(),
        getPath().c_str());
}

namespace {
    typedef vespalib::stringref TmpString;
    class StringMetric {
    public:
        StringMetric(const TmpString & s, Metric * m) : first(s), second(m) { }
        bool operator == (const StringMetric & b) const { return first == b.first; }
        bool operator == (const TmpString & b) const { return first == b; }
        bool operator <(const StringMetric & b) const { return first < b.first; }
        bool operator <(const TmpString & b) const { return first < b; }
        TmpString   first;
        Metric    * second;
    };
    bool operator < (const TmpString & a, const StringMetric & b) { return a < b.first; }

    typedef std::vector<StringMetric> SortedVector;
    
    void createMetricMap(SortedVector& metricMap,
                         const std::vector<Metric*>& orderedList)
    {
        metricMap.reserve(orderedList.size());
        for (Metric* metric : orderedList) {
            metricMap.push_back(StringMetric(metric->getMangledName(), metric));
        }
        std::sort(metricMap.begin(), metricMap.end());
    }
}

void
MetricSet::addTo(Metric& other, std::vector<Metric::UP> *ownerList) const
{
    bool mustAdd = (ownerList == 0);
    MetricSet& o(static_cast<MetricSet&>(other));
    SortedVector map1, map2;
    createMetricMap(map1, _metricOrder);
    createMetricMap(map2, o._metricOrder);
    SortedVector::iterator source(map1.begin());
    SortedVector::iterator target(map2.begin());
    typedef vespalib::hash_map<TmpString, Metric*> HashMap;
    HashMap newMetrics;
    while (source != map1.end()) {
        if (target == map2.end() || source->first < target->first) {
                // Source missing in snapshot to add to. Lets create and add.
            if (!mustAdd && source->second->used()) {
                Metric::UP copy(source->second->clone(*ownerList, INACTIVE, &o));
                newMetrics[source->first] = copy.get();
                ownerList->push_back(std::move(copy));
            }
            ++source;
        } else if (source->first == target->first) {
            if (mustAdd) {
                source->second->addToPart(*target->second);
            } else {
                source->second->addToSnapshot(*target->second, *ownerList);
            }
            ++source;
            ++target;
        } else {
            ++target;
        }
    }
    // If we added metrics, reorder target order list to equal source
    if (!newMetrics.empty()) {
        std::vector<Metric*> newOrder;
        newOrder.reserve(o._metricOrder.size() + newMetrics.size());
        for (const Metric* metric : _metricOrder) {
            TmpString v(metric->getMangledName());
            target = std::lower_bound(map2.begin(), map2.end(), v);
            if ((target != map2.end()) && (target->first == v)) {
                newOrder.push_back(target->second);
            } else {
                HashMap::iterator starget = newMetrics.find(v);
                if (starget != newMetrics.end()) {
                    newOrder.push_back(starget->second);
                }
            }
        }
        // If target had unique metrics, add them at the end
        for (Metric* metric : o._metricOrder) {
            TmpString v(metric->getMangledName());
            if ( ! std::binary_search(map1.begin(), map1.end(), v) ) {
                LOG(debug, "Metric %s exist in one snapshot but not other."
                           "Order will be messed up. Adding target unique "
                           "metrics to end.",
                    metric->getPath().c_str());
                newOrder.push_back(metric);
            }
        }
        o._metricOrder.swap(newOrder);
    }
}

bool
MetricSet::logEvent(const String& fullName) const
{
    (void) fullName;
    throw vespalib::IllegalStateException(
            "logEvent() cannot be called on metrics set.", VESPA_STRLOC);
}

void
MetricSet::reset()
{
    for (Metric* metric : _metricOrder) {
        metric->reset();
    }
}

bool
MetricSet::visit(MetricVisitor& visitor, bool tagAsAutoGenerated) const
{
    if (!visitor.visitMetricSet(*this, tagAsAutoGenerated)) return true;
    for (const Metric* metric : _metricOrder) {
        if (!metric->visit(visitor, tagAsAutoGenerated)) {
            break;
        }
    }
    visitor.doneVisitingMetricSet(*this);
    return true;
}

void
MetricSet::print(std::ostream& out, bool verbose,
                 const std::string& indent, uint64_t secondsPassed) const
{
    out << _name << ":";
    for (const Metric* metric : _metricOrder) {
        out << "\n" << indent << "  ";
        metric->print(out, verbose, indent + "  ", secondsPassed);
    }
}

bool
MetricSet::used() const
{
    for (const Metric* metric : _metricOrder) {
        if (metric->used()) return true;
    }
    return false;
}

void
MetricSet::addMemoryUsage(MemoryConsumption& mc) const
{
    Metric::addMemoryUsage(mc);
    ++mc._metricSetCount;
    mc._metricSetMeta += sizeof(MetricSet) - sizeof(Metric);
    mc._metricSetOrder += _metricOrder.size() * 3 * sizeof(void*);
    for (const Metric* metric : _metricOrder) {
        metric->addMemoryUsage(mc);
    }
}

void
MetricSet::updateNames(NameHash& hash) const
{
    Metric::updateNames(hash);
    for (const Metric* metric : _metricOrder) {
        metric->updateNames(hash);
    }
}

void
MetricSet::printDebug(std::ostream& out, const std::string& indent) const
{
    out << "set ";
    Metric::printDebug(out, indent);
    if (_registrationAltered) out << ", regAltered";
    out << " {";
    for (const Metric* metric : _metricOrder) {
        out << "\n" << indent << "  ";
        metric->printDebug(out, indent + "  ");
    }
    out << "}";
}

} // metrics

