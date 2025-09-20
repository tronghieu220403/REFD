#ifndef HONEYPOT_H_
#define HONEYPOT_H_

#include <string>
#include <vector>

namespace honeypot {

    // HoneyPot is responsible for deploying honeypot files
    // into target directories from a source directory.
    class HoneyPot {
    public:
        // Initializes honeypots in the given target directories.
        // - target_dirs: list of directories where honeypots should be deployed.
        // - source_dir: directory containing the original honeypot files.
        static void Init(const std::vector<std::wstring>& target_dirs,
            const std::wstring& source_dir);
    };

}  // namespace honeypot

#endif  // HONEYPOT_H_
