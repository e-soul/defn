// Copyright (c) 2026 e-soul.org
// SPDX-License-Identifier: BSD-2-Clause

#include "sim_report.h"

#include <sstream>

namespace defn {

namespace {

void write_string(std::ostringstream &out, const std::string &value) {
    out << '"';
    for (const char character : value) {
        switch (character) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << character;
            break;
        }
    }
    out << '"';
}

void write_field(std::ostringstream &out, const char *key, const std::string &value) {
    write_string(out, key);
    out << ':';
    write_string(out, value);
}

template <typename Number> void write_field(std::ostringstream &out, const char *key, Number value) {
    write_string(out, key);
    out << ':' << value;
}

void write_field(std::ostringstream &out, const char *key, bool value) {
    write_string(out, key);
    out << ':' << (value ? "true" : "false");
}

void write_deployments(std::ostringstream &out, const std::vector<SimDeploymentStat> &deployments) {
    out << R"("deployments":[)";
    bool first = true;
    for (const SimDeploymentStat &deployment : deployments) {
        out << (first ? "" : ",") << '{';
        write_field(out, "unit_id", deployment.unit_id);
        out << ',';
        write_field(out, "count", deployment.count);
        out << ',';
        write_field(out, "total_energy", deployment.total_energy);
        out << '}';
        first = false;
    }
    out << ']';
}

void write_per_unit(std::ostringstream &out, const std::vector<SimUnitStat> &per_unit) {
    out << R"("per_unit":[)";
    bool first = true;
    for (const SimUnitStat &unit : per_unit) {
        out << (first ? "" : ",") << '{';
        write_field(out, "unit_id", unit.unit_id);
        out << ',';
        write_field(out, "spawned", unit.spawned);
        out << ',';
        write_field(out, "damage_dealt", unit.damage_dealt);
        out << ',';
        write_field(out, "damage_taken", unit.damage_taken);
        out << ',';
        write_field(out, "kills", unit.kills);
        out << ',';
        write_field(out, "deaths", unit.deaths);
        out << ',';
        write_field(out, "mean_lifespan_s", unit.mean_lifespan_seconds);
        out << '}';
        first = false;
    }
    out << ']';
}

void write_leaks(std::ostringstream &out, const std::vector<SimLeakEvent> &leaks) {
    out << R"("leak_events":[)";
    bool first = true;
    for (const SimLeakEvent &leak : leaks) {
        out << (first ? "" : ",") << '{';
        write_field(out, "t", leak.time_seconds);
        out << ',';
        write_field(out, "unit_id", leak.unit_id);
        out << ',';
        write_field(out, "damage", leak.damage);
        out << '}';
        first = false;
    }
    out << ']';
}

void write_front_line(std::ostringstream &out, const std::vector<float> &trace) {
    out << R"("front_line_trace":[)";
    bool first = true;
    for (const float sample : trace) {
        out << (first ? "" : ",") << sample;
        first = false;
    }
    out << ']';
}

} // namespace

std::string to_jsonl(const SimMatchReport &report) {
    std::ostringstream out;
    out << '{';
    write_field(out, "level_id", report.level_id);
    out << ',';
    write_field(out, "seed", report.seed);
    out << ',';
    write_field(out, "policy", report.policy);
    out << ',';
    write_field(out, "decided", report.decided);
    out << ',';
    write_field(out, "victory", report.victory);
    out << ',';
    write_field(out, "clear_time_s", report.clear_time_seconds);
    out << ',';
    write_field(out, "remaining_integrity", report.remaining_integrity);
    out << ',';
    write_field(out, "base_health", report.base_health);
    out << ',';
    write_field(out, "base_max_health", report.base_max_health);
    out << ',';
    write_field(out, "kill_score", report.kill_score);
    out << ',';
    write_field(out, "level_score", report.level_score);
    out << ',';
    write_field(out, "energy_idle_integral", report.energy_idle_integral);
    out << ',';
    write_field(out, "peak_concurrent_enemies", report.peak_concurrent_enemies);
    out << ',';
    write_field(out, "peak_window_5s", report.peak_window_5s);
    out << ',';
    write_field(out, "deployments_total", report.deployments_total);
    out << ',';
    write_field(out, "energy_spent", report.energy_spent);
    out << ',';
    write_field(out, "camera_scroll_events", report.camera_scroll_events);
    out << ',';
    write_deployments(out, report.deployments);
    out << ',';
    write_per_unit(out, report.per_unit);
    out << ',';
    write_front_line(out, report.front_line_trace);
    out << ',';
    write_leaks(out, report.leak_events);
    out << '}';
    return out.str();
}

} // namespace defn
