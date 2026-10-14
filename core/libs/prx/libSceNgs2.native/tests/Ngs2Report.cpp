#include "Ngs2Test.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <vector>

extern "C" {
int APS5_VABI sceNgs2ReportRegisterHandler(std::uint32_t reportType, Ngs2ReportHandler handler, std::uintptr_t userData, std::uintptr_t* outHandle);
int APS5_VABI sceNgs2ReportUnregisterHandler(std::uintptr_t reportHandle);
}

namespace {

struct Received {
    std::uintptr_t userData;
    Ngs2ReportCpuLoadData data;
};

std::vector<Received> received;
std::uintptr_t selfHandle = 0;

void APS5_VABI OnCpuLoad(const Ngs2ReportDataHeader* header, std::uintptr_t userData) {
    Require(header != nullptr && header->size == sizeof(Ngs2ReportCpuLoadData));
    received.push_back({userData, *reinterpret_cast<const Ngs2ReportCpuLoadData*>(header)});
}

void APS5_VABI OnCpuLoadOnce(const Ngs2ReportDataHeader* header, std::uintptr_t userData) {
    OnCpuLoad(header, userData);
    Require(sceNgs2ReportUnregisterHandler(selfHandle) == SCE_NGS2_OK);
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void Render(std::uintptr_t system) {
    received.clear();
    std::vector<float> out(Grain * 2, 1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
}

}

int main() {
    std::uintptr_t handle = 0x1234;
    Require(sceNgs2ReportRegisterHandler(SCE_NGS2_REPORT_TYPE_CPU_LOAD, nullptr, 0, &handle) == SCE_NGS2_ERROR_INVALID_REPORT_HANDLER);
    Require(handle == 0x1234);
    Require(Throws([] { sceNgs2ReportRegisterHandler(SCE_NGS2_REPORT_TYPE_CPU_LOAD, OnCpuLoad, 0, nullptr); }));
    Require(Throws([&] { sceNgs2ReportRegisterHandler(1, OnCpuLoad, 0, &handle); }));
    Require(handle == 0x1234);
    Require(sceNgs2ReportUnregisterHandler(0) == SCE_NGS2_ERROR_INVALID_REPORT_HANDLE);
    Require(sceNgs2ReportUnregisterHandler(0x1234) == SCE_NGS2_ERROR_INVALID_REPORT_HANDLE);

    const auto system = CreateSystem();
    Mastering(system, 2);
    Render(system);
    Require(received.empty());

    std::uintptr_t first = 0;
    Require(sceNgs2ReportRegisterHandler(SCE_NGS2_REPORT_TYPE_CPU_LOAD, OnCpuLoad, 0x11, &first) == SCE_NGS2_OK && first != 0);
    Render(system);
    Require(received.size() == 1 && received[0].userData == 0x11);
    const auto& report = received[0].data;
    Require(report.header.handle == system && report.header.type == SCE_NGS2_REPORT_TYPE_CPU_LOAD && report.header.result == SCE_NGS2_OK);
    Require(report.process_ratio >= 0.0f && report.flush_ratio >= 0.0f && report.feedback_ratio == 0.0f);
    Require(std::isfinite(report.total_ratio) && report.total_ratio == report.process_ratio + report.flush_ratio);

    std::uintptr_t second = 0;
    Require(sceNgs2ReportRegisterHandler(SCE_NGS2_REPORT_TYPE_CPU_LOAD, OnCpuLoad, 0x22, &second) == SCE_NGS2_OK && second != 0 && second != first);
    Render(system);
    Require(received.size() == 2 && received[0].userData == 0x11 && received[1].userData == 0x22);

    Require(sceNgs2ReportUnregisterHandler(first) == SCE_NGS2_OK);
    Require(sceNgs2ReportUnregisterHandler(first) == SCE_NGS2_ERROR_INVALID_REPORT_HANDLE);
    Render(system);
    Require(received.size() == 1 && received[0].userData == 0x22);

    std::uintptr_t third = 0;
    Require(sceNgs2ReportRegisterHandler(SCE_NGS2_REPORT_TYPE_CPU_LOAD, OnCpuLoadOnce, 0x33, &selfHandle) == SCE_NGS2_OK);
    Require(sceNgs2ReportRegisterHandler(SCE_NGS2_REPORT_TYPE_CPU_LOAD, OnCpuLoad, 0x44, &third) == SCE_NGS2_OK);
    Render(system);
    Require(received.size() == 3 && received[1].userData == 0x33 && received[2].userData == 0x44);
    Render(system);
    Require(received.size() == 2 && received[0].userData == 0x22 && received[1].userData == 0x44);

    Require(sceNgs2ReportUnregisterHandler(second) == SCE_NGS2_OK);
    Require(sceNgs2ReportUnregisterHandler(third) == SCE_NGS2_OK);
    Render(system);
    Require(received.empty());
}
