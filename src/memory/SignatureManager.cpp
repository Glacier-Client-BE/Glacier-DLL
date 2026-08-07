#include "SignatureManager.h"

#include "Memory.h"
#include "../util/Logger.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace glacier::memory {

void SignatureManager::addSignature(std::string name, std::string idaPattern) {
    m_sigs[std::move(name)] = Entry{ std::move(idaPattern), 0 };
}

void SignatureManager::addOffset(std::string name, std::ptrdiff_t offset) {
    m_offsets[std::move(name)] = offset;
}

std::size_t SignatureManager::scanAll() {
    const ModuleRange game = moduleRange();
    m_unresolved.clear();

    if (game.base == 0 || game.size == 0) {
        LOG_ERROR("could not determine game module range — no signatures resolvable");
        return 0;
    }

    // Flatten the map into an indexable vector so worker threads can claim work
    // with a single atomic increment. Pointers stay valid: nothing mutates the
    // map for the duration of the scan.
    std::vector<Entry*> work;
    std::vector<const std::string*> names;
    work.reserve(m_sigs.size());
    names.reserve(m_sigs.size());
    for (auto& [name, entry] : m_sigs) {
        work.push_back(&entry);
        names.push_back(&name);
    }

    std::atomic<std::size_t> next{ 0 };
    std::atomic<std::size_t> resolved{ 0 };
    std::mutex failMutex;
    std::vector<std::string> failures;

    const auto worker = [&] {
        for (;;) {
            const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= work.size()) return;

            if (auto hit = findSignature(work[i]->pattern, game)) {
                work[i]->address = *hit;
                resolved.fetch_add(1, std::memory_order_relaxed);
            } else {
                work[i]->address = 0;
                std::scoped_lock lock(failMutex);
                failures.push_back(*names[i]);
            }
        }
    };

    unsigned threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 1;
    threads = std::min<unsigned>(threads, static_cast<unsigned>(std::max<std::size_t>(work.size(), 1)));

    std::vector<std::thread> pool;
    pool.reserve(threads > 0 ? threads - 1 : 0);
    for (unsigned t = 1; t < threads; ++t) pool.emplace_back(worker);
    worker();   // this thread pulls its share too rather than idling
    for (auto& t : pool) t.join();

    // Sort so the failure list is stable run-to-run despite the racy append —
    // this output is a diagnostic users paste into bug reports.
    std::sort(failures.begin(), failures.end());
    m_unresolved = std::move(failures);

    for (const auto& name : m_unresolved) {
        LOG_WARN("signature not found: {}", name);
    }
    LOG_INFO("resolved {}/{} signatures across {} thread(s)",
             resolved.load(), m_sigs.size(), threads);

    if (!m_unresolved.empty()) {
        LOG_WARN("{} signature(s) unresolved — see docs/reverse-engineering.md; "
                 "this usually means the game build differs from the one Glacier targets",
                 m_unresolved.size());
    }
    return resolved.load();
}

std::uintptr_t SignatureManager::sig(std::string_view name) const {
    auto it = m_sigs.find(name);
    return it != m_sigs.end() ? it->second.address : 0;
}

std::ptrdiff_t SignatureManager::offset(std::string_view name) const {
    auto it = m_offsets.find(name);
    return it != m_offsets.end() ? it->second : 0;
}

} // namespace glacier::memory
