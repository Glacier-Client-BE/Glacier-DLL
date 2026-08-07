#include "SignatureManager.h"

#include "GameVersion.h"
#include "Memory.h"
#include "../util/Logger.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace glacier::memory {

bool SignatureManager::checkAboveOrEqual(int major, int minor, int patch) {
    const auto& v = gameVersion();
    // Unreadable version -> assume the newest table. Seeding an old table
    // against a new game would be the worse failure: patterns would resolve to
    // outdated addresses instead of visibly not resolving at all.
    if (!v.valid) return true;
    return v.atLeast(major, minor, patch);
}

void SignatureManager::addSignature(std::string name, std::string idaPattern,
                                    int derefOffset, TargetKind kind) {
    m_sigs[std::move(name)] = Entry{ std::move(idaPattern), derefOffset, kind, 0 };
}

void SignatureManager::addOffset(std::string name, std::ptrdiff_t offset) {
    m_offsets[std::move(name)] = offset;
}

std::size_t SignatureManager::scanAll() {
    const ModuleRange game = moduleRange();
    m_unresolved.clear();

    // Report the build up front. When something misbehaves later, this line is
    // the first thing worth knowing, and it costs nothing.
    const auto& version = gameVersion();
    LOG_INFO("game build {} (Glacier targets {}.{}.{})",
             version.toString(), kTargetMajor, kTargetMinor, kTargetPatch);

    if (version.valid && !version.sameFeatureRelease(kTargetMajor, kTargetMinor, kTargetPatch)) {
        // Not an error: the table often still works across nearby builds, and
        // refusing to run would be worse than trying. But an unexplained
        // failure after this point is very likely explained by this line.
        LOG_WARN("game build {}.{}.{} differs from the targeted {}.{}.{} — "
                 "signatures may not resolve, and offsets may read wrong data. "
                 "See docs/reverse-engineering.md",
                 version.major, version.minor, version.patch,
                 kTargetMajor, kTargetMinor, kTargetPatch);
    }

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
                // A call-site pattern still has to be followed to the function
                // it references. A target that lands outside the module means
                // the displacement was decoded from the wrong instruction, so
                // treat it as unresolved rather than hand back a wild pointer.
                std::uintptr_t target = *hit;
                if (work[i]->derefOffset >= 0) {
                    target = offsetFromSig(target, work[i]->derefOffset);

                    // Any four bytes decode to *some* address, so a pattern
                    // that matched the wrong instruction still produces a
                    // confident-looking pointer. Being inside the module is not
                    // enough — the module spans .text, .rdata and .data alike.
                    //
                    // What separates a real hit from a plausible one is landing
                    // in the *right kind* of memory, and that differs by entry:
                    // a function must be executable, while a global must not be
                    // (a "global" inside .text is a misdecoded displacement).
                    // Checking every deref against the executable rule is what
                    // broke Platform_GameCore, which points at a data global.
                    //
                    // This matters more than it looks for the code case: that
                    // address gets a jump written over it, and if it was not
                    // code the result is a hung or corrupted game with nothing
                    // in the log to say why.
                    const bool executable = isExecutable(target);
                    const char* problem = nullptr;
                    if (!game.contains(target)) {
                        problem = " (matched, but its target is outside the module)";
                    } else if (work[i]->kind == TargetKind::Code && !executable) {
                        problem = " (matched, but its target is not executable code — "
                                  "the call-site pattern resolved to the wrong place)";
                    } else if (work[i]->kind == TargetKind::Data && executable) {
                        problem = " (matched, but its target is executable code where a "
                                  "data global was expected — the displacement was decoded "
                                  "from the wrong instruction)";
                    }
                    if (problem) {
                        work[i]->address = 0;
                        std::scoped_lock lock(failMutex);
                        failures.push_back(*names[i] + problem);
                        continue;
                    }
                }
                work[i]->address = target;
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
