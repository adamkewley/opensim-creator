#include "Perf.hpp"

#include "src/Utils/Algorithms.hpp"
#include "src/Utils/SynchronizedValue.hpp"

#include <unordered_map>
#include <mutex>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>


static int64_t GenerateID(char const* label, char const* filename, unsigned int line)
{
    return static_cast<int64_t>(osc::HashOf(std::string{label}, std::string{filename}, line));
}

static osc::SynchronizedValue<std::unordered_map<int64_t, osc::PerfMeasurement>>& GetMeasurementStorage()
{
    static osc::SynchronizedValue<std::unordered_map<int64_t, osc::PerfMeasurement>> g_Measurements;
    return g_Measurements;
}


// public API

int64_t osc::AllocateMeasurementID(char const* label, char const* filename, unsigned int line)
{
    int64_t id = GenerateID(label, filename, line);

    auto guard = GetMeasurementStorage().lock();
    guard->emplace(std::piecewise_construct, std::tie(id), std::tie(id, label, filename, line));
    return id;
}

void osc::SubmitMeasurement(int64_t id, PerfClock::time_point start, PerfClock::time_point end)
{
    auto guard = GetMeasurementStorage().lock();
    auto it = guard->find(id);

    if (it != guard->end())
    {
        it->second.submit(start, end);
    }
}

void osc::ClearPerfMeasurements()
{
    auto guard = GetMeasurementStorage().lock();

    for (auto& [id, data] : *guard)
    {
        data.clear();
    }
}

int osc::GetAllMeasurements(std::vector<PerfMeasurement>& appendOut)
{
    auto guard = GetMeasurementStorage().lock();
    int i = 0;
    for (auto const& [id, measurement] : *guard)
    {
        appendOut.push_back(measurement);
        ++i;
    }
    return i;
}




