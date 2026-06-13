#pragma once

#include "../proxy_imgui_gui.hpp"
#include <spdlog/sinks/base_sink.h>
#include <mutex>

namespace utils {

template<typename Mutex>
class GuiSink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t buf;
        this->formatter_->format(msg, buf);
        std::string line(buf.data(), buf.size());
        
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        AppendLog(line);
    }
    void flush_() override {}
};

using GuiSink_mt = GuiSink<std::mutex>;
using GuiSink_st = GuiSink<spdlog::details::null_mutex>;

} 
