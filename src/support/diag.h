#pragma once
// Minimal diagnostics for the Coco frontend (docs/COCO_PLAN.md §11.2).
#include <cstdint>
#include <string>
#include <vector>

namespace coco {

struct Diag {
    uint32_t line = 0;
    uint32_t col = 1;
    std::string message;
};

class DiagEngine {
public:
    void report(uint32_t line, uint32_t col, std::string msg) {
        diags_.push_back(Diag{line, col, std::move(msg)});
    }
    const std::vector<Diag>& diags() const { return diags_; }
    bool ok() const { return diags_.empty(); }
    size_t count() const { return diags_.size(); }

private:
    std::vector<Diag> diags_;
};

} // namespace coco
