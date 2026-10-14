#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

namespace {

struct Ngs2Report {
    std::uint32_t type;
    Ngs2ReportHandler handler;
    std::uintptr_t userData;
};

std::vector<std::unique_ptr<Ngs2Report>>& Reports() {
    static std::vector<std::unique_ptr<Ngs2Report>> reports;
    return reports;
}

}

void Ngs2ReportCpuLoad(Ngs2Handle system, const Ngs2RenderTimes& times, std::uint32_t grain, std::uint32_t sampleRate) {
    std::lock_guard lock(Ngs2Mutex());
    std::vector<Ngs2Report> targets;
    for (const auto& report : Reports()) {
        if (report->type == SCE_NGS2_REPORT_TYPE_CPU_LOAD) targets.push_back(*report);
    }
    if (targets.empty()) return;
    const double period = static_cast<double>(grain) / sampleRate;
    Ngs2ReportCpuLoadData data{};
    data.header = {sizeof(Ngs2ReportCpuLoadData), system, SCE_NGS2_REPORT_TYPE_CPU_LOAD, SCE_NGS2_OK};
    data.process_ratio = static_cast<float>(times.process / period);
    data.flush_ratio = static_cast<float>(times.flush / period);
    data.feedback_ratio = 0.0f;
    data.total_ratio = data.process_ratio + data.flush_ratio + data.feedback_ratio;
    for (const auto& target : targets) target.handler(&data.header, target.userData);
}

Ngs2Handle Ngs2AddReport(std::uint32_t type, Ngs2ReportHandler handler, std::uintptr_t userData) {
    std::lock_guard lock(Ngs2Mutex());
    auto& report = Reports().emplace_back(std::make_unique<Ngs2Report>(Ngs2Report{type, handler, userData}));
    return reinterpret_cast<Ngs2Handle>(report.get());
}

bool Ngs2RemoveReport(Ngs2Handle handle) {
    std::lock_guard lock(Ngs2Mutex());
    auto& reports = Reports();
    const auto found = std::find_if(reports.begin(), reports.end(), [&](const auto& report) {
        return reinterpret_cast<Ngs2Handle>(report.get()) == handle;
    });
    if (found == reports.end()) return false;
    reports.erase(found);
    return true;
}
